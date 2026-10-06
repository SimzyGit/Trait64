// SPDX-License-Identifier: AGPL-3.0-or-later
#include "database.h"
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>
#include <stdexcept>

namespace t64 {
namespace {
void check(bool ok, const QString &error) {
    if (!ok)
        throw std::runtime_error(error.toStdString());
}
void run(QSqlQuery &q) {
    if (!q.exec())
        throw std::runtime_error((q.lastQuery() + ": " + q.lastError().text()).toStdString());
}
QString s(const std::string &text) {
    return QString::fromStdString(text);
}
QString tag(const Game &g, const char *name) {
    const auto it = g.tags.find(name);
    return it == g.tags.end() ? QString() : s(it->second);
}
} // namespace
Database::Database(const QString &path) : connection_(QUuid::createUuid().toString()), path_(path) {
    try {
        QDir().mkpath(QFileInfo(path).absolutePath());
        db_ = QSqlDatabase::addDatabase("QSQLITE", connection_);
        db_.setDatabaseName(path);
        db_.setConnectOptions("QSQLITE_BUSY_TIMEOUT=5000");
        if (!db_.open())
            throw std::runtime_error(db_.lastError().text().toStdString());
        QSqlQuery version(db_);
        version.exec("PRAGMA user_version");
        version.next();
        const int v = version.value(0).toInt();
        version.finish();
        check(v <= 1, "Cette bibliothèque nécessite une version plus récente de Trait64.");
        QSqlQuery appId(db_);
        appId.exec("PRAGMA application_id");
        appId.next();
        check((v == 0 && appId.value(0).toInt() == 0) || (v == 1 && appId.value(0).toInt() == 1412830276),
              "Ce fichier n’est pas une bibliothèque Trait64.");
        appId.finish();
        if (v == 0) {
            check(db_.tables().isEmpty(), "Base SQLite inconnue : ouverture refusée.");
            if (!db_.transaction())
                throw std::runtime_error(db_.lastError().text().toStdString());
            try {
                sql("CREATE TABLE games(id INTEGER PRIMARY KEY, uuid TEXT NOT NULL UNIQUE, white TEXT, black "
                    "TEXT, event TEXT, date TEXT, result TEXT, eco TEXT, collection TEXT DEFAULT '', "
                    "favorite "
                    "INTEGER DEFAULT 0, deleted INTEGER DEFAULT 0, pgn TEXT NOT NULL, digest TEXT NOT NULL, "
                    "updated TEXT NOT NULL)");
                sql("CREATE INDEX games_digest ON games(digest)");
                sql("CREATE INDEX games_players ON games(white,black)");
                sql("CREATE INDEX games_filter ON games(deleted,result,eco,date)");
                sql("CREATE TABLE positions(game_id INTEGER NOT NULL REFERENCES games(id) ON DELETE CASCADE, "
                    "ply "
                    "INTEGER NOT NULL, fen TEXT NOT NULL, uci TEXT, san TEXT, PRIMARY KEY(game_id,ply))");
                sql("CREATE INDEX positions_fen ON positions(fen)");
                sql("CREATE TABLE cards(id INTEGER PRIMARY KEY, title TEXT NOT NULL, fen TEXT NOT NULL, "
                    "moves "
                    "TEXT NOT NULL, due TEXT NOT NULL, interval INTEGER DEFAULT 0, reviews INTEGER DEFAULT "
                    "0, "
                    "successes INTEGER DEFAULT 0)");
                sql("PRAGMA application_id=1412830276");
                sql("PRAGMA user_version=1");
                if (!db_.commit())
                    throw std::runtime_error(db_.lastError().text().toStdString());
            } catch (...) {
                db_.rollback();
                throw;
            }
        }
        sql("PRAGMA foreign_keys=ON");
        sql("PRAGMA journal_mode=WAL");
        sql("PRAGMA synchronous=FULL");
    } catch (...) {
        db_.close();
        db_ = QSqlDatabase();
        QSqlDatabase::removeDatabase(connection_);
        throw;
    }
}
Database::~Database() {
    db_.close();
    db_ = QSqlDatabase();
    QSqlDatabase::removeDatabase(connection_);
}
void Database::sql(const QString &query) const {
    QSqlQuery q(db_);
    if (!q.exec(query))
        throw std::runtime_error((query + ": " + q.lastError().text()).toStdString());
}

qint64 Database::save(const Game &game, qint64 id, const QString &collection) {
    const auto pgn = s(game.pgn());
    const auto digest =
        QString::fromLatin1(QCryptographicHash::hash(pgn.toUtf8(), QCryptographicHash::Sha256).toHex());
    if (!id) {
        QSqlQuery q(db_);
        q.prepare("SELECT id FROM games WHERE digest=? AND deleted=0 LIMIT 1");
        q.addBindValue(digest);
        run(q);
        if (q.next())
            return q.value(0).toLongLong();
    }
    sql("SAVEPOINT save_game");
    try {
        QSqlQuery q(db_);
        if (id)
            q.prepare("UPDATE games SET "
                      "white=?,black=?,event=?,date=?,result=?,eco=?,pgn=?,digest=?,updated=? WHERE id=?");
        else
            q.prepare(
                "INSERT INTO games(white,black,event,date,result,eco,pgn,digest,updated,uuid,collection) "
                "VALUES(?,?,?,?,?,?,?,?,?,?,?)");
        for (const char *key : {"White", "Black", "Event", "Date", "Result", "ECO"})
            q.addBindValue(tag(game, key));
        q.addBindValue(pgn);
        q.addBindValue(digest);
        q.addBindValue(QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
        if (id)
            q.addBindValue(id);
        else {
            q.addBindValue(QUuid::createUuid().toString(QUuid::WithoutBraces));
            q.addBindValue(collection);
        }
        run(q);
        if (!id)
            id = q.lastInsertId().toLongLong();
        check(q.numRowsAffected() > 0, "Partie introuvable lors de la sauvegarde.");
        QSqlQuery del(db_);
        del.prepare("DELETE FROM positions WHERE game_id=?");
        del.addBindValue(id);
        run(del);
        auto line = game.mainline();
        QSqlQuery pos(db_);
        pos.prepare("INSERT INTO positions(game_id,ply,fen,uci,san) VALUES(?,?,?,?,?)");
        for (size_t i = 0; i < line.size(); ++i) {
            pos.bindValue(0, id);
            pos.bindValue(1, int(i));
            pos.bindValue(2, s(positionKey(game.nodes[line[i]].fen)));
            pos.bindValue(3, i + 1 < line.size() ? s(game.nodes[line[i + 1]].uci) : QString());
            pos.bindValue(4, i + 1 < line.size() ? s(game.nodes[line[i + 1]].san) : QString());
            run(pos);
        }
        sql("RELEASE save_game");
        return id;
    } catch (...) {
        sql("ROLLBACK TO save_game");
        sql("RELEASE save_game");
        throw;
    }
}
int Database::importGames(const std::vector<Game> &games) {
    int before = stats()["games"].toInt();
    if (!db_.transaction())
        throw std::runtime_error(db_.lastError().text().toStdString());
    try {
        for (const auto &g : games)
            save(g);
        if (!db_.commit())
            throw std::runtime_error(db_.lastError().text().toStdString());
    } catch (...) {
        db_.rollback();
        throw;
    }
    return stats()["games"].toInt() - before;
}
Game Database::load(qint64 id) const {
    QSqlQuery q(db_);
    q.prepare("SELECT pgn FROM games WHERE id=?");
    q.addBindValue(id);
    run(q);
    check(q.next(), "Partie introuvable.");
    auto games = parsePgn(q.value(0).toString().toStdString());
    check(games.size() == 1, "Partie enregistrée invalide.");
    return std::move(games[0]);
}
QVariantList Database::search(const QString &query, const QString &result, bool favorite,
                              const QString &position, bool trash) const {
    QString statement = "SELECT g.id,g.white,g.black,g.event,g.date,g.result,g.eco,g.collection,g.favorite "
                        "FROM games g WHERE g.deleted=?";
    QVariantList args{trash ? 1 : 0};
    if (!query.isEmpty()) {
        statement += " AND (g.white LIKE ? OR g.black LIKE ? OR g.event LIKE ? OR g.eco LIKE ? OR "
                     "g.collection LIKE ? OR g.date LIKE ?)";
        for (int i = 0; i < 6; ++i)
            args << "%" + query + "%";
    }
    if (!result.isEmpty()) {
        statement += " AND g.result=?";
        args << result;
    }
    if (favorite)
        statement += " AND g.favorite=1";
    if (!position.isEmpty()) {
        statement += " AND EXISTS(SELECT 1 FROM positions p WHERE p.game_id=g.id AND p.fen=?)";
        args << s(positionKey(position.toStdString()));
    }
    statement += " ORDER BY g.updated DESC,g.id DESC LIMIT 1000";
    QSqlQuery q(db_);
    q.prepare(statement);
    for (const auto &a : args)
        q.addBindValue(a);
    run(q);
    QVariantList rows;
    while (q.next())
        rows << QVariantMap{{"id", q.value(0)},    {"white", q.value(1)},      {"black", q.value(2)},
                            {"event", q.value(3)}, {"date", q.value(4)},       {"result", q.value(5)},
                            {"eco", q.value(6)},   {"collection", q.value(7)}, {"favorite", q.value(8)}};
    return rows;
}
QVariantMap Database::stats() const {
    QVariantMap stats;
    for (const auto &[key, query] : std::vector<std::pair<QString, QString>>{
             {"games", "SELECT COUNT(*) FROM games WHERE deleted=0"},
             {"favorites", "SELECT COUNT(*) FROM games WHERE favorite=1 AND deleted=0"},
             {"cards", "SELECT COUNT(*) FROM cards"},
             {"due", "SELECT COUNT(*) FROM cards WHERE due<=date('now')"},
             {"reviews", "SELECT COALESCE(SUM(reviews),0) FROM cards"},
             {"successes", "SELECT COALESCE(SUM(successes),0) FROM cards"},
             {"whiteWins", "SELECT COUNT(*) FROM games WHERE result='1-0' AND deleted=0"},
             {"blackWins", "SELECT COUNT(*) FROM games WHERE result='0-1' AND deleted=0"},
             {"draws", "SELECT COUNT(*) FROM games WHERE result='1/2-1/2' AND deleted=0"}}) {
        QSqlQuery q(db_);
        if (!q.exec(query))
            throw std::runtime_error(q.lastError().text().toStdString());
        q.next();
        stats[key] = q.value(0);
    }
    return stats;
}
QVariantList Database::explorer(const QString &position) const {
    QSqlQuery q(db_);
    q.prepare("SELECT p.uci,p.san,COUNT(DISTINCT p.game_id),COUNT(DISTINCT CASE WHEN g.result='1-0' THEN "
              "g.id END),COUNT(DISTINCT CASE WHEN g.result='1/2-1/2' THEN g.id END),COUNT(DISTINCT CASE WHEN "
              "g.result='0-1' THEN g.id END) FROM positions p JOIN games g ON g.id=p.game_id WHERE p.fen=? "
              "AND p.uci<>'' AND g.deleted=0 GROUP BY p.uci,p.san ORDER BY COUNT(DISTINCT p.game_id) DESC");
    q.addBindValue(s(positionKey(position.toStdString())));
    run(q);
    QVariantList out;
    while (q.next())
        out << QVariantMap{{"uci", q.value(0)},   {"san", q.value(1)},  {"count", q.value(2)},
                           {"white", q.value(3)}, {"draw", q.value(4)}, {"black", q.value(5)}};
    return out;
}
void Database::favorite(qint64 id, bool value) {
    QSqlQuery q(db_);
    q.prepare("UPDATE games SET favorite=? WHERE id=?");
    q.addBindValue(value);
    q.addBindValue(id);
    run(q);
}
void Database::trash(qint64 id, bool value) {
    QSqlQuery q(db_);
    q.prepare("UPDATE games SET deleted=? WHERE id=?");
    q.addBindValue(value);
    q.addBindValue(id);
    run(q);
}
void Database::collection(qint64 id, const QString &value) {
    QSqlQuery q(db_);
    q.prepare("UPDATE games SET collection=? WHERE id=?");
    q.addBindValue(value.left(200));
    q.addBindValue(id);
    run(q);
}
QString Database::exportPgn() const {
    QSqlQuery q(db_);
    if (!q.exec("SELECT pgn FROM games WHERE deleted=0 ORDER BY id"))
        throw std::runtime_error(q.lastError().text().toStdString());
    QString out;
    while (q.next())
        out += q.value(0).toString() + '\n';
    return out;
}
void Database::backup(const QString &path) {
    check(QFileInfo(path).absoluteFilePath() != QFileInfo(path_).absoluteFilePath() &&
              (!QFileInfo(path).exists() ||
               QFileInfo(path).canonicalFilePath() != QFileInfo(path_).canonicalFilePath()),
          "Choisissez un autre fichier pour la sauvegarde.");
    const QString temp = path + "." + QUuid::createUuid().toString(QUuid::WithoutBraces) + ".tmp";
    QSqlQuery q(db_);
    q.prepare("VACUUM INTO ?");
    q.addBindValue(temp);
    run(q);
    QFile in(temp);
    QSaveFile out(path);
    try {
        check(in.open(QIODevice::ReadOnly) && out.open(QIODevice::WriteOnly),
              "Impossible de créer la sauvegarde.");
        while (!in.atEnd()) {
            auto bytes = in.read(1024 * 1024);
            check(!bytes.isEmpty() || in.error() == QFileDevice::NoError, in.errorString());
            check(out.write(bytes) == bytes.size(), out.errorString());
        }
        check(out.commit(), out.errorString());
        in.close();
        QFile::remove(temp);
    } catch (...) {
        in.close();
        QFile::remove(temp);
        throw;
    }
}
QString Database::integrity() const {
    QSqlQuery q(db_);
    if (!q.exec("PRAGMA integrity_check"))
        throw std::runtime_error(q.lastError().text().toStdString());
    QStringList out;
    while (q.next())
        out << q.value(0).toString();
    return out.join('\n');
}
void Database::addCard(const QString &title, const QString &fen, const QString &moves) {
    Game g(fen.toStdString());
    int node = 0;
    const auto sequence = moves.split(' ', Qt::SkipEmptyParts);
    check(!sequence.empty() && sequence.size() <= 40, "Une carte nécessite entre 1 et 40 demi-coups.");
    for (const auto &m : sequence)
        node = g.play(node, m.toStdString());
    QSqlQuery q(db_);
    q.prepare("INSERT INTO cards(title,fen,moves,due) VALUES(?,?,?,date('now'))");
    q.addBindValue(title.left(200));
    q.addBindValue(fen);
    q.addBindValue(sequence.join(' '));
    run(q);
}
QVariantList Database::cards() const {
    QSqlQuery q(db_);
    check(
        q.exec(
            "SELECT id,title,fen,moves,due,interval,reviews,successes FROM cards ORDER BY due,id LIMIT 1000"),
        q.lastError().text());
    QVariantList out;
    while (q.next())
        out << QVariantMap{{"id", q.value(0)},      {"title", q.value(1)},    {"fen", q.value(2)},
                           {"moves", q.value(3)},   {"due", q.value(4)},      {"interval", q.value(5)},
                           {"reviews", q.value(6)}, {"successes", q.value(7)}};
    return out;
}
void Database::review(qint64 id, bool success) {
    QSqlQuery q(db_);
    q.prepare("UPDATE cards SET interval=CASE WHEN ? THEN MIN(365,MAX(1,interval*2)) ELSE 1 "
              "END,reviews=reviews+1,successes=successes+? WHERE id=?");
    q.addBindValue(success);
    q.addBindValue(success ? 1 : 0);
    q.addBindValue(id);
    run(q);
    QSqlQuery d(db_);
    d.prepare("UPDATE cards SET due=date('now','+'||interval||' days') WHERE id=?");
    d.addBindValue(id);
    run(d);
}
void Database::deleteCard(qint64 id) {
    QSqlQuery q(db_);
    q.prepare("DELETE FROM cards WHERE id=?");
    q.addBindValue(id);
    run(q);
}
} // namespace t64
