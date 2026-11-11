// SPDX-License-Identifier: AGPL-3.0-or-later
#include "database.h"
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QUuid>
#include <stdexcept>

namespace t64 {
namespace {
void run(QSqlQuery &q) {
    if (!q.exec())
        throw std::runtime_error(q.lastError().text().toStdString());
}
} // namespace
void Database::upgradeV4(int originalVersion) {
    if (originalVersion >= 4)
        return;
    if (originalVersion > 0)
        backup(path_ + ".before-v4-" + QUuid::createUuid().toString(QUuid::WithoutBraces) + ".sqlite");
    sql("SAVEPOINT schema4");
    try {
        sql("CREATE TABLE sources(id INTEGER PRIMARY KEY,slug TEXT NOT NULL UNIQUE,title TEXT NOT NULL,"
            "url TEXT NOT NULL,license TEXT NOT NULL,games INTEGER NOT NULL DEFAULT 0)");
        sql("CREATE TABLE source_games(source_id INTEGER NOT NULL REFERENCES sources(id) ON DELETE CASCADE,"
            "game_id INTEGER NOT NULL REFERENCES games(id) ON DELETE CASCADE,PRIMARY KEY(source_id,game_id)) "
            "WITHOUT ROWID");
        sql("CREATE INDEX source_games_game ON source_games(game_id,source_id)");
        sql("CREATE TABLE import_history(id INTEGER PRIMARY KEY,source_id INTEGER REFERENCES sources(id) ON "
            "DELETE SET NULL,"
            "imported TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ','now')),"
            "filename TEXT NOT NULL,sha256 TEXT NOT NULL,processed INTEGER NOT NULL,added INTEGER NOT NULL)");
        sql("CREATE INDEX import_history_recent ON import_history(id DESC)");
        sql("CREATE INDEX games_eco_recent ON games(deleted,eco,updated DESC,id DESC)");
        sql("CREATE TRIGGER source_link_add AFTER INSERT ON source_games BEGIN UPDATE sources SET "
            "games=games+COALESCE((SELECT deleted=0 FROM games WHERE id=NEW.game_id),0) WHERE "
            "id=NEW.source_id; END");
        sql("CREATE TRIGGER source_link_remove BEFORE DELETE ON source_games BEGIN UPDATE sources SET "
            "games=games-COALESCE((SELECT deleted=0 FROM games WHERE id=OLD.game_id),0) WHERE "
            "id=OLD.source_id; END");
        sql("CREATE TRIGGER source_game_remove BEFORE DELETE ON games BEGIN DELETE FROM source_games WHERE "
            "game_id=OLD.id; END");
        sql("CREATE TRIGGER source_game_trash AFTER UPDATE OF deleted ON games WHEN OLD.deleted<>NEW.deleted "
            "BEGIN UPDATE sources SET games=games+(NEW.deleted=0)-(OLD.deleted=0) WHERE id IN(SELECT "
            "source_id FROM source_games WHERE game_id=NEW.id); END");
        sql("PRAGMA user_version=4");
        sql("RELEASE schema4");
    } catch (...) {
        sql("ROLLBACK TO schema4");
        sql("RELEASE schema4");
        throw;
    }
}
// Un recueil est une source comme une autre : un nom, des parties, et la recherche de
// la bibliothèque qui sait déjà filtrer dessus. Seule manquait la marque qui le
// distingue d'une base téléchargée, pour que l'onglet Études sache lesquelles montrer.
void Database::upgradeV11(int originalVersion) {
    if (originalVersion >= 11)
        return;
    if (originalVersion > 0)
        backup(path_ + ".before-v11-" + QUuid::createUuid().toString(QUuid::WithoutBraces) + ".sqlite");
    sql("SAVEPOINT schema11");
    try {
        if (!db_.record("sources").contains("kind"))
            sql("ALTER TABLE sources ADD COLUMN kind TEXT NOT NULL DEFAULT ''");
        sql("PRAGMA user_version=11");
        sql("RELEASE schema11");
    } catch (...) {
        sql("ROLLBACK TO schema11");
        sql("RELEASE schema11");
        throw;
    }
}
qint64 Database::registerSource(const QVariantMap &source) {
    if (source.value("id").toString().isEmpty())
        return 0;
    QSqlQuery q(db_);
    q.prepare("INSERT INTO sources(slug,title,url,license,kind) VALUES(?,?,?,?,?) ON CONFLICT(slug) DO "
              "UPDATE SET title=excluded.title,url=excluded.url,license=excluded.license,"
              "kind=excluded.kind");
    q.addBindValue(source["id"].toString().left(200));
    q.addBindValue(source["title"].toString().left(300));
    q.addBindValue(source.value("source", QString("")).toString().left(2048));
    q.addBindValue(source.value("license", QString("")).toString().left(1000));
    q.addBindValue(source.value("kind", QString("")).toString().left(40));
    run(q);
    q.prepare("SELECT id FROM sources WHERE slug=?");
    q.addBindValue(source["id"].toString().left(200));
    run(q);
    if (!q.next())
        throw std::runtime_error("Source introuvable.");
    return q.value(0).toLongLong();
}
void Database::moveSourcePath(const QString &from, const QString &to) {
    // Un fichier, ou tout ce qui vit sous un dossier : renommer le dossier déplace ses
    // fichiers, et leurs recueils doivent suivre. La comparaison par préfixe se fait à la
    // main — LIKE prendrait « _ » et « % » des noms de fichiers pour des jokers.
    QSqlQuery q(db_);
    q.prepare("UPDATE sources SET url = ? || substr(url, length(?) + 1) "
              "WHERE url = ? OR substr(url, 1, length(?) + 1) = ? || '/'");
    q.addBindValue(to);
    q.addBindValue(from);
    q.addBindValue(from);
    q.addBindValue(from);
    q.addBindValue(from);
    run(q);
}
void Database::linkSource(qint64 source, qint64 game) {
    if (!source)
        return;
    QSqlQuery q(db_);
    q.prepare("INSERT OR IGNORE INTO source_games VALUES(?,?)");
    q.addBindValue(source);
    q.addBindValue(game);
    run(q);
}
void Database::recordImport(qint64 source, const QString &file, const QString &digest, int processed,
                            int added) {
    QSqlQuery q(db_);
    q.prepare("INSERT INTO import_history(source_id,filename,sha256,processed,added) VALUES(?,?,?,?,?)");
    q.addBindValue(source ? QVariant(source) : QVariant());
    q.addBindValue(file);
    q.addBindValue(digest);
    q.addBindValue(processed);
    q.addBindValue(added);
    run(q);
}
QVariantList Database::sources() const {
    QSqlQuery q(db_);
    q.prepare("SELECT s.id,s.slug,s.title,s.url,s.license,s.games,s.kind FROM sources s ORDER BY s.title");
    run(q);
    QVariantList rows;
    while (q.next())
        rows << QVariantMap{{"id", q.value(0)},     {"slug", q.value(1)},    {"title", q.value(2)},
                            {"source", q.value(3)}, {"license", q.value(4)}, {"games", q.value(5)},
                            {"kind", q.value(6)}};
    return rows;
}
QVariantList Database::importHistory() const {
    QSqlQuery q(db_);
    q.prepare("SELECT h.id,COALESCE(s.title,''),h.imported,h.filename,h.sha256,h.processed,h.added "
              "FROM import_history h LEFT JOIN sources s ON s.id=h.source_id ORDER BY h.id DESC LIMIT 200");
    run(q);
    QVariantList rows;
    while (q.next())
        rows << QVariantMap{{"id", q.value(0)},   {"title", q.value(1)},  {"date", q.value(2)},
                            {"file", q.value(3)}, {"sha256", q.value(4)}, {"processed", q.value(5)},
                            {"added", q.value(6)}};
    return rows;
}
void Database::exportSource(QIODevice &output, qint64 source) const {
    QSqlQuery q(db_);
    q.setForwardOnly(true);
    q.prepare("SELECT g.pgn FROM source_games s JOIN games g ON g.id=s.game_id WHERE s.source_id=? AND "
              "g.deleted=0 ORDER BY g.id");
    q.addBindValue(source);
    run(q);
    while (q.next()) {
        const auto bytes = q.value(0).toString().toUtf8() + '\n';
        if (output.write(bytes) != bytes.size())
            throw std::runtime_error(output.errorString().toStdString());
    }
    if (q.lastError().isValid())
        throw std::runtime_error(q.lastError().text().toStdString());
}
} // namespace t64
