// SPDX-License-Identifier: AGPL-3.0-or-later
#include "database.h"
#include <QBuffer>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>
#include <algorithm>
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
        check(v <= 4, "Cette bibliothèque nécessite une version plus récente de Trait64.");
        QSqlQuery appId(db_);
        appId.exec("PRAGMA application_id");
        appId.next();
        check((v == 0 && appId.value(0).toInt() == 0) || (v >= 1 && appId.value(0).toInt() == 1412830276),
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
        if (v < 2) {
            if (v == 1)
                backup(path_ + ".before-v2-" + QUuid::createUuid().toString(QUuid::WithoutBraces) +
                       ".sqlite");
            check(db_.transaction(), db_.lastError().text());
            try {
                sql("ALTER TABLE games ADD COLUMN white_elo INTEGER DEFAULT 0");
                sql("ALTER TABLE games ADD COLUMN black_elo INTEGER DEFAULT 0");
                sql("ALTER TABLE games ADD COLUMN plies INTEGER DEFAULT 0");
                sql("ALTER TABLE games ADD COLUMN fingerprint TEXT DEFAULT ''");
                sql("CREATE INDEX games_fingerprint ON games(fingerprint)");
                sql("CREATE TABLE all_positions(game_id INTEGER NOT NULL REFERENCES games(id) ON DELETE "
                    "CASCADE, node INTEGER NOT NULL, fen TEXT NOT NULL, material TEXT NOT NULL, PRIMARY "
                    "KEY(game_id,node))");
                sql("CREATE INDEX all_positions_fen ON all_positions(fen)");
                sql("CREATE INDEX all_positions_material ON all_positions(material)");
                sql("CREATE TABLE review_log(id INTEGER PRIMARY KEY, card_id INTEGER REFERENCES cards(id) ON "
                    "DELETE SET NULL, reviewed TEXT NOT NULL, success INTEGER NOT NULL)");
                sql("PRAGMA user_version=2");
                if (v == 1)
                    rebuildIndexes();
                check(db_.commit(), db_.lastError().text());
            } catch (...) {
                db_.rollback();
                throw;
            }
        }
        upgradeV3(v);
        upgradeV4(v);
        fts_ = db_.tables().contains("games_fts");
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
        if (enhanced_) {
            QSqlQuery edges(db_);
            edges.prepare("DELETE FROM opening_edges WHERE game_id=?");
            edges.addBindValue(id);
            run(edges);
        }
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
        if (enhanced_) {
            QSqlQuery edges(db_);
            edges.prepare("INSERT INTO opening_edges SELECT game_id,fen,uci,MIN(san) FROM positions WHERE "
                          "game_id=? AND uci<>'' GROUP BY game_id,fen,uci");
            edges.addBindValue(id);
            run(edges);
        }
        QSqlQuery metadata(db_);
        metadata.prepare("UPDATE games SET white_elo=?,black_elo=?,plies=?,fingerprint=? WHERE id=?");
        metadata.addBindValue(tag(game, "WhiteElo").toInt());
        metadata.addBindValue(tag(game, "BlackElo").toInt());
        metadata.addBindValue(int(line.size()) - 1);
        QByteArray fingerprint =
            QByteArray::fromStdString(game.nodes[0].fen) + (game.chess960 ? "960" : "standard");
        for (int n : line)
            fingerprint += ' ' + QByteArray::fromStdString(game.nodes[n].uci);
        metadata.addBindValue(
            QString::fromLatin1(QCryptographicHash::hash(fingerprint, QCryptographicHash::Sha256).toHex()));
        metadata.addBindValue(id);
        run(metadata);
        QSqlQuery erase(db_);
        erase.prepare("DELETE FROM all_positions WHERE game_id=?");
        erase.addBindValue(id);
        run(erase);
        QSqlQuery all(db_);
        all.prepare("INSERT INTO all_positions(game_id,node,fen,material) VALUES(?,?,?,?)");
        std::vector<int> pending{0};
        while (!pending.empty()) {
            int n = pending.back();
            pending.pop_back();
            const auto &node = game.nodes[n];
            QString material = s(node.fen).section(' ', 0, 0);
            material.remove(QRegularExpression("[^prnbqkPRNBQK]"));
            std::sort(material.begin(), material.end());
            all.bindValue(0, id);
            all.bindValue(1, n);
            all.bindValue(2, s(positionKey(node.fen)));
            all.bindValue(3, material);
            run(all);
            pending.insert(pending.end(), node.children.begin(), node.children.end());
        }
        sql("RELEASE save_game");
        return id;
    } catch (...) {
        sql("ROLLBACK TO save_game");
        sql("RELEASE save_game");
        throw;
    }
}
int Database::importGames(const std::vector<Game> &games, const std::function<bool(int)> &progress) {
    int before = stats()["games"].toInt();
    if (!db_.transaction())
        throw std::runtime_error(db_.lastError().text().toStdString());
    try {
        int count = 0;
        for (const auto &g : games) {
            check(!progress || progress(count), "Import interrompu : aucune partie ajoutée.");
            save(g);
            ++count;
        }
        check(!progress || progress(count), "Import interrompu : aucune partie ajoutée.");
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
                              const QString &position, bool trash, const QVariantMap &filters) const {
    const bool fullText = fts_ && query.size() >= 3 && !query.contains('%') && !query.contains('_');
    QString statement = "SELECT g.id,g.white,g.black,g.event,g.date,g.result,g.eco,g.collection,g.favorite ";
    QVariantList args;
    if (fullText) {
        // FTS drives the lookup: ORDER BY must not make SQLite scan every recent game.
        statement += "FROM games_fts CROSS JOIN games g ON g.id=games_fts.rowid WHERE games_fts MATCH ? AND "
                     "g.deleted=?";
        QString term = query;
        term.replace('"', "\"\"");
        args << '"' + term + '"' << (trash ? 1 : 0);
    } else {
        statement += "FROM games g WHERE g.deleted=?";
        args << (trash ? 1 : 0);
    }
    if (!fullText && !query.isEmpty()) {
        statement += " AND (g.white LIKE ? OR g.black LIKE ? OR g.event LIKE ? OR g.eco LIKE ? OR "
                     "g.collection LIKE ? OR g.date LIKE ?)";
        for (int i = 0; i < 6; ++i)
            args << "%" + query + "%";
    }
    if (filters.value("sourceId").toLongLong() > 0) {
        statement += " AND g.id IN(SELECT game_id FROM source_games WHERE source_id=?)";
        args << filters["sourceId"].toLongLong();
    }
    if (!filters.value("player").toString().isEmpty()) {
        const auto name = filters["player"].toString();
        const auto colour = filters["colour"].toString();
        const bool exact = filters["exactNames"].toBool();
        const auto op = exact ? "=?" : " LIKE ?";
        if (colour == "white" || colour == "black") {
            statement += " AND g." + colour + op;
            args << (exact ? name : "%" + name + "%");
        } else {
            statement += " AND (g.white" + QString(op) + " OR g.black" + op + ")";
            args << (exact ? name : "%" + name + "%") << (exact ? name : "%" + name + "%");
        }
    }
    const auto codes = filters.value("openingCodes").toStringList();
    if (filters.value("openingRequested").toBool()) {
        QStringList placeholders;
        for (const auto &code : codes) {
            placeholders << "?";
            args << code;
        }
        statement += codes.empty() ? " AND 0" : " AND g.eco IN(" + placeholders.join(',') + ")";
    }
    for (const auto &bound : QStringList{"ecoFrom", "ecoTo"}) {
        const auto code = filters.value(bound).toString().trimmed().toUpper();
        if (!code.isEmpty()) {
            check(QRegularExpression("^[A-E][0-9]{2}$").match(code).hasMatch(),
                  "Code ECO invalide (A00 à E99).");
            statement += bound == "ecoFrom" ? " AND g.eco>=?" : " AND g.eco<=?";
            args << code;
        }
    }
    if (!result.isEmpty()) {
        statement += " AND g.result=?";
        args << result;
    }
    if (favorite)
        statement += " AND g.favorite=1";
    if (!position.isEmpty()) {
        statement += " AND g.id IN(SELECT game_id FROM all_positions WHERE fen=?)";
        args << s(positionKey(position.toStdString()));
    }
    for (const auto &key : QStringList{"white", "black", "event", "eco", "collection"})
        if (!filters.value(key).toString().isEmpty()) {
            const bool exact = filters.value("exactNames").toBool();
            statement += " AND g." + key + (exact ? "=?" : " LIKE ?");
            args << (exact ? filters[key].toString() : "%" + filters[key].toString() + "%");
        }
    for (const auto &key : QStringList{"dateFrom", "dateTo"})
        if (!filters.value(key).toString().isEmpty()) {
            statement += key == "dateFrom" ? " AND g.date>=?" : " AND g.date<=?";
            args << filters[key].toString();
        }
    if (filters.value("minElo").toInt() > 0) {
        statement += " AND g.white_elo>=? AND g.black_elo>=?";
        args << filters["minElo"] << filters["minElo"];
    }
    if (filters.value("maxElo").toInt() > 0) {
        statement += " AND g.white_elo<=? AND g.black_elo<=? AND g.white_elo>0 AND g.black_elo>0";
        args << filters["maxElo"] << filters["maxElo"];
    }
    if (filters.value("maxPlies").toInt() > 0) {
        statement += " AND g.plies<=?";
        args << filters["maxPlies"];
    }
    if (!filters.value("material").toString().isEmpty()) {
        QString material = filters["material"].toString();
        material.remove(QRegularExpression("[^prnbqkPRNBQK]"));
        std::sort(material.begin(), material.end());
        statement += " AND g.id IN(SELECT game_id FROM all_positions WHERE material=?)";
        args << material;
    }
    const QMap<QString, QString> order{{"date", "g.date DESC"},
                                       {"players", "g.white,g.black"},
                                       {"elo", "(g.white_elo+g.black_elo) DESC"},
                                       {"length", "g.plies ASC"}};
    statement += " ORDER BY " + order.value(filters.value("sort").toString(), "g.updated DESC") +
                 ",g.id DESC LIMIT 1000 OFFSET ?";
    args << std::max(0, filters.value("offset").toInt());
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
             {"games", "SELECT games FROM library_counts WHERE id=1"},
             {"favorites", "SELECT favorites FROM library_counts WHERE id=1"},
             {"cards", "SELECT COUNT(*) FROM cards"},
             {"due", "SELECT COUNT(*) FROM cards WHERE due<=date('now','localtime')"},
             {"reviews", "SELECT COALESCE(SUM(reviews),0) FROM cards"},
             {"todayReviews", "SELECT COUNT(*) FROM review_log WHERE reviewed>=date('now','localtime') AND "
                              "reviewed<date('now','localtime','+1 day')"},
             {"weekReviews",
              "SELECT COUNT(*) FROM review_log WHERE reviewed>=date('now','localtime','-6 days')"},
             {"successes", "SELECT COALESCE(SUM(successes),0) FROM cards"},
             {"whiteWins", "SELECT whiteWins FROM library_counts WHERE id=1"},
             {"blackWins", "SELECT blackWins FROM library_counts WHERE id=1"},
             {"draws", "SELECT draws FROM library_counts WHERE id=1"}}) {
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
    q.prepare("SELECT uci,san,count,white,draw,black FROM opening_stats WHERE fen=? AND count>0 ORDER BY "
              "count DESC");
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
    QBuffer buffer;
    buffer.open(QIODevice::WriteOnly);
    exportPgn(buffer);
    return QString::fromUtf8(buffer.data());
}
void Database::exportPgn(QIODevice &output) const {
    QSqlQuery q(db_);
    q.setForwardOnly(true);
    if (!q.exec("SELECT pgn FROM games WHERE deleted=0 ORDER BY id"))
        throw std::runtime_error(q.lastError().text().toStdString());
    while (q.next()) {
        const auto bytes = q.value(0).toString().toUtf8() + '\n';
        check(output.write(bytes) == bytes.size(), output.errorString());
    }
    check(!q.lastError().isValid(), q.lastError().text());
}
void Database::validateOutputPath(const QString &path) const {
    auto resolved = [](const QString &p) {
        const QFileInfo f(p);
        if (f.exists())
            return f.canonicalFilePath();
        const auto parent = f.dir().canonicalPath();
        return parent.isEmpty() ? f.absoluteFilePath() : parent + '/' + f.fileName();
    };
    for (const auto &suffix : QStringList{"", "-wal", "-shm", "-journal"})
        check(resolved(path) != resolved(path_ + suffix),
              "L’export ne peut pas remplacer la bibliothèque ouverte ni ses journaux.");
}
void Database::backup(const QString &path) {
    validateOutputPath(path);
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
    q.prepare("INSERT INTO cards(title,fen,moves,due) VALUES(?,?,?,date('now','localtime'))");
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
    sql("SAVEPOINT review_card");
    try {
        QSqlQuery q(db_);
        q.prepare("UPDATE cards SET interval=CASE WHEN ? THEN MIN(365,MAX(1,interval*2)) ELSE 1 "
                  "END,reviews=reviews+1,successes=successes+? WHERE id=?");
        q.addBindValue(success);
        q.addBindValue(success ? 1 : 0);
        q.addBindValue(id);
        run(q);
        check(q.numRowsAffected() == 1, "Carte introuvable.");
        QSqlQuery d(db_);
        d.prepare("UPDATE cards SET due=date('now','localtime','+'||interval||' days') WHERE id=?");
        d.addBindValue(id);
        run(d);
        QSqlQuery log(db_);
        log.prepare(
            "INSERT INTO review_log(card_id,reviewed,success) VALUES(?,datetime('now','localtime'),?)");
        log.addBindValue(id);
        log.addBindValue(success);
        run(log);
        sql("RELEASE review_card");
    } catch (...) {
        sql("ROLLBACK TO review_card");
        sql("RELEASE review_card");
        throw;
    }
}
void Database::deleteCard(qint64 id) {
    QSqlQuery q(db_);
    q.prepare("DELETE FROM cards WHERE id=?");
    q.addBindValue(id);
    run(q);
}
} // namespace t64
