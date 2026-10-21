// SPDX-License-Identifier: AGPL-3.0-or-later
#include "database.h"
#include <QJsonDocument>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>
#include <algorithm>
#include <stdexcept>

namespace t64 {
namespace {
void require(bool ok, const QString &error) {
    if (!ok)
        throw std::runtime_error(error.toStdString());
}
void run(QSqlQuery &q) {
    if (!q.exec())
        throw std::runtime_error(q.lastError().text().toStdString());
}
QString json(const QVariant &value) {
    return QString::fromUtf8(QJsonDocument::fromVariant(value).toJson(QJsonDocument::Compact));
}
QVariant decoded(const QVariant &value) {
    QJsonParseError error;
    auto doc = QJsonDocument::fromJson(value.toString().toUtf8(), &error);
    require(error.error == QJsonParseError::NoError, "Résultat d’analyse illisible.");
    return doc.toVariant();
}
} // namespace
void Database::upgradeV7(int originalVersion) {
    if (originalVersion >= 7)
        return;
    if (originalVersion > 0)
        backup(path_ + ".before-v7-" + QUuid::createUuid().toString(QUuid::WithoutBraces) + ".sqlite");
    sql("SAVEPOINT schema7");
    try {
        sql("CREATE TABLE analysis_jobs(id INTEGER PRIMARY KEY,title TEXT NOT NULL,pgn TEXT NOT NULL,"
            "config TEXT NOT NULL,identity TEXT NOT NULL DEFAULT '{}',variations INTEGER NOT NULL,"
            "status TEXT NOT NULL DEFAULT 'queued' CHECK(status IN "
            "('queued','running','paused','completed','failed')),total INTEGER NOT NULL,"
            "done INTEGER NOT NULL DEFAULT 0,error TEXT NOT NULL DEFAULT '',"
            "created TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ','now')),"
            "updated TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ','now')))");
        sql("CREATE INDEX analysis_pending ON analysis_jobs(status,id)");
        sql("CREATE TABLE analysis_results(job INTEGER NOT NULL REFERENCES analysis_jobs(id) ON DELETE "
            "CASCADE,ordinal INTEGER NOT NULL,node INTEGER NOT NULL,fen TEXT NOT NULL,lines TEXT NOT NULL,"
            "cached INTEGER NOT NULL,PRIMARY KEY(job,ordinal)) WITHOUT ROWID");
        sql("CREATE TABLE analysis_cache(key TEXT PRIMARY KEY,lines TEXT NOT NULL,"
            "used TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ','now'))) WITHOUT ROWID");
        sql("CREATE INDEX analysis_cache_used ON analysis_cache(used,key)");
        sql("PRAGMA user_version=7");
        sql("RELEASE schema7");
    } catch (...) {
        sql("ROLLBACK TO schema7");
        sql("RELEASE schema7");
        throw;
    }
}
qint64 Database::createAnalysisJob(const QString &pgn, const QVariantMap &config, bool variations,
                                   int total) {
    require(pgn.size() <= 16 * 1024 * 1024 && total > 0 && total <= 100000, "Étude trop volumineuse.");
    QSqlQuery count(db_);
    require(count.exec("SELECT COUNT(*) FROM analysis_jobs") && count.next(), count.lastError().text());
    require(count.value(0).toInt() < 500, "500 analyses conservées : exportez puis retirez les anciennes.");
    count.finish();
    auto games = parsePgn(pgn.toStdString());
    require(games.size() == 1, "Une analyse doit contenir exactement une partie.");
    const auto &tags = games.front().tags;
    auto tag = [&](const char *name) {
        auto it = tags.find(name);
        return it == tags.end() ? QString("?") : QString::fromStdString(it->second);
    };
    QSqlQuery q(db_);
    q.prepare("INSERT INTO analysis_jobs(title,pgn,config,variations,total) VALUES(?,?,?,?,?)");
    q.addBindValue((tag("White") + " — " + tag("Black")).left(400));
    q.addBindValue(pgn);
    q.addBindValue(json(config));
    q.addBindValue(variations);
    q.addBindValue(total);
    run(q);
    return q.lastInsertId().toLongLong();
}
QVariantList Database::analysisJobs() const {
    QSqlQuery q(db_);
    q.prepare("SELECT id,title,status,total,done,error,created,updated,variations,config FROM analysis_jobs "
              "ORDER BY id DESC");
    run(q);
    QVariantList rows;
    while (q.next())
        rows << QVariantMap{{"id", q.value(0)},         {"title", q.value(1)},
                            {"status", q.value(2)},     {"total", q.value(3)},
                            {"done", q.value(4)},       {"error", q.value(5)},
                            {"created", q.value(6)},    {"updated", q.value(7)},
                            {"variations", q.value(8)}, {"config", decoded(q.value(9))}};
    return rows;
}
QVariantMap Database::analysisJob(qint64 id, bool includePgn) const {
    QSqlQuery q(db_);
    q.prepare(QString("SELECT ") + (includePgn ? "pgn" : "''") +
              ",config,identity,status,total,done,variations,error FROM analysis_jobs WHERE id=?");
    q.addBindValue(id);
    run(q);
    require(q.next(), "Analyse introuvable.");
    return {{"id", id},
            {"pgn", q.value(0)},
            {"config", decoded(q.value(1))},
            {"identity", decoded(q.value(2))},
            {"status", q.value(3)},
            {"total", q.value(4)},
            {"done", q.value(5)},
            {"variations", q.value(6)},
            {"error", q.value(7)}};
}
QVariantList Database::analysisResults(qint64 id, int offset, int limit) const {
    QSqlQuery q(db_);
    q.prepare("SELECT ordinal,node,fen,lines,cached FROM analysis_results WHERE job=? ORDER BY ordinal "
              "LIMIT ? OFFSET ?");
    q.addBindValue(id);
    q.addBindValue(std::clamp(limit, 1, 201));
    q.addBindValue(std::max(0, offset));
    run(q);
    QVariantList rows;
    while (q.next())
        rows << QVariantMap{{"ordinal", q.value(0)},
                            {"node", q.value(1)},
                            {"fen", q.value(2)},
                            {"lines", decoded(q.value(3))},
                            {"cached", q.value(4)}};
    return rows;
}
void Database::setAnalysisStatus(qint64 id, const QString &status, const QString &error) {
    require(QStringList{"queued", "running", "paused", "completed", "failed"}.contains(status),
            "État d’analyse invalide.");
    QSqlQuery q(db_);
    q.prepare("UPDATE analysis_jobs SET status=?,error=?,updated=strftime('%Y-%m-%dT%H:%M:%fZ','now') "
              "WHERE id=? AND (?!='completed' OR done=total)");
    q.addBindValue(status);
    q.addBindValue(error.isNull() ? QStringLiteral("") : error.left(2000));
    q.addBindValue(id);
    q.addBindValue(status);
    run(q);
    require(q.numRowsAffected() == 1, "Analyse introuvable ou incomplète.");
}
void Database::setAnalysisIdentity(qint64 id, const QVariantMap &identity) {
    const auto existing = analysisJob(id, false)["identity"].toMap();
    require(existing.isEmpty() || json(existing) == json(identity),
            "Le moteur ou ses ressources ont changé. Créez une nouvelle analyse.");
    QSqlQuery q(db_);
    q.prepare("UPDATE analysis_jobs SET identity=? WHERE id=?");
    q.addBindValue(json(identity));
    q.addBindValue(id);
    run(q);
}
void Database::recoverAnalyses() {
    // Caller must hold the library's analysis lock. An interrupted search is recomputed in full.
    sql("UPDATE analysis_jobs SET status='paused',error='Interruption détectée : reprise disponible.' "
        "WHERE status='running'");
}
void Database::forgetAnalysis(qint64 id) {
    QSqlQuery q(db_);
    q.prepare("DELETE FROM analysis_jobs WHERE id=? AND status!='running'");
    q.addBindValue(id);
    run(q);
    require(q.numRowsAffected() == 1, "Mettez l’analyse en pause avant de la retirer.");
}
void Database::saveAnalysisResult(qint64 id, int ordinal, int node, const QString &fen,
                                  const QVariantList &lines, const QString &cacheKey, bool cached) {
    require(!lines.empty(), "Aucune évaluation exacte reçue.");
    const auto data = json(lines);
    require(data.size() <= 128 * 1024, "Réponse d’analyse trop volumineuse.");
    sql("SAVEPOINT analysis_result");
    try {
        const auto job = analysisJob(id, false);
        require(job["status"] == "running" && ordinal >= 0 && ordinal < job["total"].toInt(),
                "Analyse inactive ou position invalide.");
        if (ordinal < job["done"].toInt()) {
            sql("RELEASE analysis_result");
            return; // Retrying an already committed result must not advance the checkpoint twice.
        }
        require(ordinal == job["done"].toInt(), "Point de reprise incohérent.");
        QSqlQuery q(db_);
        q.prepare("INSERT INTO analysis_results(job,ordinal,node,fen,lines,cached) VALUES(?,?,?,?,?,?)");
        for (const auto &v : QVariantList{id, ordinal, node, fen, data, cached})
            q.addBindValue(v);
        run(q);
        q.prepare(
            "UPDATE analysis_jobs SET done=done+1,updated=strftime('%Y-%m-%dT%H:%M:%fZ','now') WHERE id=?");
        q.addBindValue(id);
        run(q);
        if (!cacheKey.isEmpty()) {
            q.prepare("INSERT INTO analysis_cache(key,lines) VALUES(?,?) ON CONFLICT(key) DO UPDATE SET "
                      "lines=excluded.lines,used=strftime('%Y-%m-%dT%H:%M:%fZ','now')");
            q.addBindValue(cacheKey);
            q.addBindValue(data);
            run(q);
            sql("DELETE FROM analysis_cache WHERE key IN (SELECT key FROM analysis_cache ORDER BY used "
                "DESC,key DESC LIMIT -1 OFFSET 10000)");
        }
        sql("RELEASE analysis_result");
    } catch (...) {
        sql("ROLLBACK TO analysis_result");
        sql("RELEASE analysis_result");
        throw;
    }
}
QVariantList Database::cachedAnalysis(const QString &key) const {
    if (key.isEmpty())
        return {};
    QSqlQuery q(db_);
    q.prepare("SELECT lines FROM analysis_cache WHERE key=?");
    q.addBindValue(key);
    run(q);
    return q.next() ? decoded(q.value(0)).toList() : QVariantList();
}
void Database::clearAnalysisCache() {
    sql("DELETE FROM analysis_cache");
}
} // namespace t64
