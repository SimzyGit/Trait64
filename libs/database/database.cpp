// SPDX-License-Identifier: AGPL-3.0-or-later
#include "database.h"
#include "cancellation.h"
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
QString materialKey(QStringView pieces) {
    // Keep the schema-2 key byte-for-byte, without compiling a regular
    // expression for every indexed position. Only FEN piece letters count.
    QString key;
    key.reserve(32);
    for (const auto piece : pieces) {
        switch (piece.unicode()) {
        case 'p':
        case 'r':
        case 'n':
        case 'b':
        case 'q':
        case 'k':
        case 'P':
        case 'R':
        case 'N':
        case 'B':
        case 'Q':
        case 'K':
            key.append(piece);
        }
    }
    std::sort(key.begin(), key.end());
    return key;
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
        check(v <= 11, "Cette bibliothèque nécessite une version plus récente de Trait64.");
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
        upgradeV5(v);
        upgradeV6(v);
        upgradeV7(v);
        upgradeV8(v);
        upgradeV9(v);
        upgradeV10(v);
        upgradeV11(v);
        createPuzzleTables();
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
        // With no ANALYZE statistics SQLite can prefer an index starting with
        // deleted, scanning every active game for each new PGN (quadratic import).
        // This mandatory schema-1 index bounds the lookup to the matching digest.
        q.prepare("SELECT id FROM games INDEXED BY games_digest WHERE digest=? AND deleted=0 LIMIT 1");
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
        // Before migration 8 has run (only while upgrading an old library) keys are still FEN text.
        const QString column = keyed_ ? "key" : "fen";
        pos.prepare("INSERT INTO positions(game_id,ply," + column + ",uci,san) VALUES(?,?,?,?,?)");
        auto positionValue = [&](const Node &node) {
            return keyed_ ? QVariant(nodeKey(node)) : QVariant(s(positionKey(node.fen)));
        };
        for (size_t i = 0; i < line.size(); ++i) {
            pos.bindValue(0, id);
            pos.bindValue(1, int(i));
            pos.bindValue(2, positionValue(game.nodes[line[i]]));
            pos.bindValue(3, i + 1 < line.size() ? s(game.nodes[line[i + 1]].uci) : QString());
            pos.bindValue(4, i + 1 < line.size() ? s(game.nodes[line[i + 1]].san) : QString());
            run(pos);
        }
        if (enhanced_) {
            QSqlQuery edges(db_);
            edges.prepare("INSERT INTO opening_edges SELECT game_id," + column +
                          ",uci,MIN(san) FROM positions "
                          "WHERE game_id=? AND uci<>'' GROUP BY game_id," +
                          column + ",uci");
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
        all.prepare(keyed_ ? "INSERT INTO all_positions(game_id,node,key,material,pawns) VALUES(?,?,?,?,?)"
                           : "INSERT INTO all_positions(game_id,node,fen,material) VALUES(?,?,?,?)");
        std::vector<int> pending{0};
        while (!pending.empty()) {
            int n = pending.back();
            pending.pop_back();
            const auto &node = game.nodes[n];
            const auto material = materialKey(s(node.fen).section(' ', 0, 0));
            all.bindValue(0, id);
            all.bindValue(1, n);
            all.bindValue(2, positionValue(node));
            all.bindValue(3, material);
            if (keyed_)
                all.bindValue(4, pawnsKey(node.fen));
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
                              const QString &position, bool trash, const QVariantMap &filters,
                              const std::shared_ptr<std::atomic_bool> &canceled) const {
    const QueryCancellation cancellation(db_, canceled);
    const bool fullText = fts_ && query.size() >= 3 && !query.contains('%') && !query.contains('_');
    QString statement = "SELECT g.id,g.white,g.black,g.event,g.date,g.result,g.eco,g.collection,g.favorite,"
                        "g.updated,(g.white_elo+g.black_elo),g.plies,g.white_elo,g.black_elo ";
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
        // Same pawn structure (any other pieces) or the exact position.
        const bool pawns = filters.value("pawnStructure").toBool();
        statement += QString(" AND g.id IN(SELECT game_id FROM all_positions WHERE ") +
                     (pawns ? "pawns" : "key") + "=?)";
        args << (pawns ? pawnsKey(position.toStdString()) : hashKey(position.toStdString()));
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
        statement += " AND g.id IN(SELECT game_id FROM all_positions WHERE material=?)";
        args << materialKey(filters["material"].toString());
    }
    struct SortField {
        QString column, key;
        bool ascending;
    };
    const auto sort = filters.value("sort").toString();
    QList<SortField> order;
    if (sort == "date")
        order = {{"g.date", "date", false}};
    else if (sort == "players")
        order = {{"g.white", "white", true}, {"g.black", "black", true}};
    else if (sort == "elo")
        order = {{"(g.white_elo+g.black_elo)", "rating", false}};
    else if (sort == "length")
        order = {{"g.plies", "plies", true}};
    else
        order = {{"g.updated", "updated", false}};
    order << SortField{"g.id", "id", false};
    const auto cursor = filters.value("cursor").toMap();
    const bool recentCursor = !cursor.isEmpty() && order[0].key == "updated";
    if (!cursor.isEmpty()) {
        for (const auto &field : order)
            check(cursor.contains(field.key), "Curseur de recherche incomplet.");
        if (!recentCursor) {
            // Match SQLite's NULL ordering and use id as a stable tie-breaker.
            QStringList alternatives;
            for (int i = 0; i < order.size(); ++i) {
                QStringList terms;
                for (int j = 0; j < i; ++j) {
                    terms << order[j].column + " IS ?";
                    args << cursor[order[j].key];
                }
                const auto &field = order[i];
                const auto value = cursor[field.key];
                if (value.isNull())
                    terms << (field.ascending ? field.column + " IS NOT NULL" : "0");
                else {
                    terms << (field.ascending ? field.column + ">?"
                                              : "(" + field.column + "<? OR " + field.column + " IS NULL)");
                    args << value;
                }
                alternatives << "(" + terms.join(" AND ") + ")";
            }
            statement += " AND (" + alternatives.join(" OR ") + ")";
        }
    }
    QStringList ordering;
    for (const auto &field : order)
        ordering << field.column + (field.ascending ? " ASC" : " DESC");
    const auto orderedLimit = " ORDER BY " + ordering.join(',') + " LIMIT ?";
    const int limit = std::clamp(filters.value("limit", 1000).toInt(), 1, 1001);
    if (recentCursor) {
        // SQLite may seek on updated only for the row-value comparison
        // (updated,id)<(?,?). Large imports often share a timestamp. Two
        // disjoint, bounded ranges explicitly seek on updated+id and merge
        // at most 2*limit rows without scanning all earlier ties.
        const auto base = statement;
        const auto baseArgs = args;
        statement = "SELECT * FROM (" + base + " AND g.updated=? AND g.id<?" + orderedLimit +
                    ") "
                    "UNION ALL SELECT * FROM (" +
                    base + " AND g.updated<?" + orderedLimit +
                    ") "
                    "ORDER BY updated DESC,id DESC LIMIT ?";
        args << cursor["updated"] << cursor["id"] << limit;
        args += baseArgs;
        args << cursor["updated"] << limit << limit;
    } else {
        statement += orderedLimit;
        args << limit;
    }
    if (cursor.isEmpty() && filters.value("offset").toInt() > 0) {
        statement += " OFFSET ?";
        args << filters["offset"].toInt();
    }
    QSqlQuery q(db_);
    q.prepare(statement);
    for (const auto &a : args)
        q.addBindValue(a);
    run(q);
    QVariantList rows;
    while (q.next())
        rows << QVariantMap{{"id", q.value(0)},        {"white", q.value(1)},      {"black", q.value(2)},
                            {"event", q.value(3)},     {"date", q.value(4)},       {"result", q.value(5)},
                            {"eco", q.value(6)},       {"collection", q.value(7)}, {"favorite", q.value(8)},
                            {"updated", q.value(9)},   {"rating", q.value(10)},    {"plies", q.value(11)},
                            {"whiteElo", q.value(12)}, {"blackElo", q.value(13)}};
    check(!q.lastError().isValid(), q.lastError().text());
    check(!canceled || !canceled->load(), "Recherche annulée.");
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
    q.prepare("SELECT uci,san,count,white,draw,black FROM opening_stats WHERE key=? AND count>0 ORDER BY "
              "count DESC");
    q.addBindValue(hashKey(position.toStdString()));
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
QVariantMap Database::openingReport(const QString &fen, int models,
                                    const std::shared_ptr<std::atomic_bool> &canceled) const {
    QueryCancellation cancellation(db_, canceled);
    const auto key = hashKey(fen.toStdString());
    // Les tranches sont celles que l'on distingue réellement : le club, le joueur de
    // tournoi, le maître. Une partie dont un seul Elo manque n'est pas rangée de force.
    const QString band = "CASE WHEN g.white_elo<=0 OR g.black_elo<=0 THEN '' "
                         "WHEN (g.white_elo+g.black_elo)/2<2000 THEN 'club' "
                         "WHEN (g.white_elo+g.black_elo)/2<2300 THEN 'tournoi' ELSE 'maitre' END";
    QSqlQuery q(db_);
    q.prepare("SELECT p.uci,MIN(p.san)," + band +
              " AS band,COUNT(*),SUM(g.result='1-0'),SUM(g.result='1/2-1/2'),SUM(g.result='0-1'),"
              "MAX(substr(g.date,1,4)) FROM positions p JOIN games g ON g.id=p.game_id"
              " WHERE p.key=? AND p.uci<>'' AND g.deleted=0 GROUP BY p.uci,band");
    q.addBindValue(key);
    run(q);
    // Un coup par ligne, ses tranches rassemblées : l'agrégat SQL en rend une par
    // tranche, et c'est le coup que l'on veut lire d'un bloc.
    QMap<QString, QVariantMap> byMove;
    QVariantMap bandTotals;
    qint64 total = 0, white = 0, draw = 0, black = 0;
    while (q.next()) {
        const auto uci = q.value(0).toString();
        auto move = byMove.value(uci);
        const qint64 count = q.value(3).toLongLong(), w = q.value(4).toLongLong(),
                     d = q.value(5).toLongLong(), b = q.value(6).toLongLong();
        move["uci"] = uci;
        move["san"] = q.value(1);
        move["count"] = move["count"].toLongLong() + count;
        move["white"] = move["white"].toLongLong() + w;
        move["draw"] = move["draw"].toLongLong() + d;
        move["black"] = move["black"].toLongLong() + b;
        if (q.value(7).toString() > move["last"].toString())
            move["last"] = q.value(7);
        const auto name = q.value(2).toString();
        if (!name.isEmpty()) {
            auto bands = move["bands"].toMap();
            bands[name] = bands[name].toLongLong() + count;
            move["bands"] = bands;
            bandTotals[name] = bandTotals[name].toLongLong() + count;
        }
        byMove[uci] = move;
        total += count;
        white += w;
        draw += d;
        black += b;
    }
    QVariantList moves;
    for (const auto &move : byMove)
        moves.append(move);
    // Du plus joué au moins joué : c'est l'ordre dans lequel on lit une position.
    std::sort(moves.begin(), moves.end(), [](const QVariant &a, const QVariant &b) {
        return a.toMap()["count"].toLongLong() > b.toMap()["count"].toLongLong();
    });
    QVariantList best;
    if (models > 0) {
        QSqlQuery games(db_);
        games.prepare("SELECT g.id,g.white,g.black,g.result,g.date,g.event,g.white_elo,g.black_elo"
                      " FROM positions p JOIN games g ON g.id=p.game_id"
                      " WHERE p.key=? AND g.deleted=0 GROUP BY g.id"
                      " ORDER BY (g.white_elo+g.black_elo) DESC,g.date DESC LIMIT ?");
        games.addBindValue(key);
        games.addBindValue(std::clamp(models, 1, 50));
        run(games);
        while (games.next())
            best.append(QVariantMap{{"id", games.value(0)},
                                    {"white", games.value(1)},
                                    {"black", games.value(2)},
                                    {"result", games.value(3)},
                                    {"date", games.value(4)},
                                    {"event", games.value(5)},
                                    {"whiteElo", games.value(6)},
                                    {"blackElo", games.value(7)}});
    }
    return {{"total", total}, {"whiteWins", white},  {"draws", draw}, {"blackWins", black},
            {"moves", moves}, {"bands", bandTotals}, {"models", best}};
}
QVariantList Database::positionStatistics(const QString &fen, const QVariantMap &filters,
                                          const std::shared_ptr<std::atomic_bool> &canceled) const {
    QueryCancellation cancellation(db_, canceled);
    const int from = filters["fromYear"].toInt(), to = filters["toYear"].toInt();
    const int min = filters["minElo"].toInt(), max = filters["maxElo"].toInt();
    check(from >= 0 && from <= 9999 && to >= 0 && to <= 9999 && (!from || !to || from <= to),
          "Période invalide.");
    check(min >= 0 && min <= 4000 && max >= 0 && max <= 4000 && (!min || !max || min <= max),
          "Plage Elo invalide.");
    const auto colour = filters.value("colour", "both").toString();
    check(colour == "white" || colour == "black" || colour == "both", "Couleur invalide.");
    QString statement =
        "SELECT DISTINCT p.game_id,p.uci,p.san,g.result,g.white_elo,g.black_elo,g.date "
        "FROM positions p JOIN games g ON g.id=p.game_id WHERE p.key=? AND p.uci<>'' AND g.deleted=0";
    QVariantList args{hashKey(fen.toStdString())};
    const auto player = filters["player"].toString().trimmed();
    if (!player.isEmpty()) {
        if (colour == "both") {
            statement += " AND (g.white=? OR g.black=?)";
            args << player << player;
        } else {
            statement += " AND g." + colour + "=?";
            args << player;
        }
    }
    if (filters["sourceId"].toLongLong() > 0) {
        statement += " AND EXISTS(SELECT 1 FROM source_games s WHERE s.source_id=? AND s.game_id=g.id)";
        args << filters["sourceId"].toLongLong();
    }
    if (from || to)
        statement += " AND substr(g.date,1,4) GLOB '[0-9][0-9][0-9][0-9]'";
    if (from) {
        statement += " AND substr(g.date,1,4)>=?";
        args << QString::number(from).rightJustified(4, '0');
    }
    if (to) {
        statement += " AND substr(g.date,1,4)<=?";
        args << QString::number(to).rightJustified(4, '0');
    }
    if (min || max)
        statement += " AND g.white_elo>0 AND g.black_elo>0";
    if (min) {
        statement += " AND (g.white_elo+g.black_elo)/2.0>=?";
        args << min;
    }
    if (max) {
        statement += " AND (g.white_elo+g.black_elo)/2.0<=?";
        args << max;
    }
    statement =
        "WITH selected AS (" + statement +
        ") SELECT uci,MIN(san),COUNT(*),SUM(result='1-0'),"
        "SUM(result='1/2-1/2'),SUM(result='0-1'),AVG(CASE WHEN white_elo>0 AND black_elo>0 THEN "
        "(white_elo+black_elo)/2.0 END),SUM(white_elo>0 AND black_elo>0),"
        "MAX(CASE WHEN substr(date,1,4) GLOB '[0-9][0-9][0-9][0-9]' THEN substr(date,1,4) END),MIN(game_id) "
        "FROM selected GROUP BY uci ORDER BY COUNT(*) DESC,uci";
    QSqlQuery q(db_);
    q.setForwardOnly(true);
    q.prepare(statement);
    for (const auto &arg : args)
        q.addBindValue(arg);
    run(q);
    QVariantList rows;
    while (q.next())
        rows << QVariantMap{{"uci", q.value(0)},        {"san", q.value(1)},   {"count", q.value(2)},
                            {"white", q.value(3)},      {"draw", q.value(4)},  {"black", q.value(5)},
                            {"averageElo", q.value(6)}, {"rated", q.value(7)}, {"lastYear", q.value(8)},
                            {"example", q.value(9)}};
    check(!q.lastError().isValid(), q.lastError().text());
    return rows;
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
int Database::classifyOpenings(const QVariantMap &ecoByPosition, const RepertoireProgress &progress) {
    // Sans ce classement, `eco` ne valait que ce que le PGN portait en balise, et la
    // plupart des bases n'en portent aucune : recherche, statistiques et rangement par
    // ouverture restaient vides sans que rien ne le signale.
    sql("SAVEPOINT classify_openings");
    try {
        sql("CREATE TEMP TABLE IF NOT EXISTS opening_positions(key INTEGER PRIMARY KEY, eco TEXT NOT NULL)");
        sql("DELETE FROM opening_positions");
        QSqlQuery q(db_);
        q.prepare("INSERT OR IGNORE INTO opening_positions(key,eco) VALUES(?,?)");
        qint64 seen = 0;
        for (auto it = ecoByPosition.cbegin(); it != ecoByPosition.cend(); ++it) {
            check(!progress || progress(++seen), "Classement interrompu.");
            bool number = false;
            const auto key = it.key().toLongLong(&number);
            if (!number)
                continue;
            q.addBindValue(key);
            q.addBindValue(it.value().toString().left(8));
            run(q);
        }
        // `positions` ne tient que la ligne principale, indexée par (game_id, ply) : le
        // code retenu est celui de la position d'ouverture la plus profonde atteinte,
        // comme le font ChessBase et SCID. Une seule passe, sans rejouer les coups.
        q.prepare(
            "UPDATE games SET eco=(SELECT o.eco FROM positions p JOIN opening_positions o ON o.key=p.key"
            " WHERE p.game_id=games.id ORDER BY p.ply DESC LIMIT 1)"
            " WHERE deleted=0 AND COALESCE(eco,'')=''"
            " AND EXISTS(SELECT 1 FROM positions p JOIN opening_positions o ON o.key=p.key"
            " WHERE p.game_id=games.id)");
        run(q);
        const int touched = q.numRowsAffected();
        sql("RELEASE classify_openings");
        return touched;
    } catch (...) {
        sql("ROLLBACK TO classify_openings");
        sql("RELEASE classify_openings");
        throw;
    }
}
int Database::groupByOpening(const QVariantMap &labels, const RepertoireProgress &progress) {
    sql("SAVEPOINT group_openings");
    try {
        sql("CREATE TEMP TABLE IF NOT EXISTS opening_labels(eco TEXT PRIMARY KEY, label TEXT NOT NULL)");
        sql("DELETE FROM opening_labels");
        QSqlQuery q(db_);
        q.prepare("INSERT OR REPLACE INTO opening_labels(eco,label) VALUES(?,?)");
        qint64 seen = 0;
        for (auto it = labels.cbegin(); it != labels.cend(); ++it) {
            check(!progress || progress(++seen), "Regroupement interrompu.");
            q.addBindValue(it.key());
            q.addBindValue(it.value().toString().left(200));
            run(q);
        }
        // Un seul parcours de la bibliothèque, l'étiquette venant d'une table indexée.
        // L'index des parties commence par « deleted, result » : filtrer sur le seul code
        // ECO relirait toutes les parties, et une mise à jour par code en ferait autant
        // de fois qu'il y a de codes.
        q.prepare("UPDATE games SET collection=(SELECT label FROM opening_labels WHERE eco=games.eco)"
                  " WHERE deleted=0 AND eco IN (SELECT eco FROM opening_labels)");
        run(q);
        const int touched = q.numRowsAffected();
        sql("RELEASE group_openings");
        return touched;
    } catch (...) {
        sql("ROLLBACK TO group_openings");
        sql("RELEASE group_openings");
        throw;
    }
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
    for (const auto &base : QStringList{path_, QFileInfo(path_).canonicalFilePath()})
        for (const auto &suffix :
             QStringList{"", "-wal", "-shm", "-journal", ".import.lock", ".analysis.lock"})
            check(resolved(path) != resolved(base + suffix),
                  "L’export ne peut pas remplacer la bibliothèque ouverte, ses journaux ou ses verrous.");
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
QString Database::integrity(const std::shared_ptr<std::atomic_bool> &canceled) const {
    const QueryCancellation cancellation(db_, canceled);
    QSqlQuery q(db_);
    if (!q.exec("PRAGMA integrity_check"))
        throw std::runtime_error(q.lastError().text().toStdString());
    QStringList out;
    while (q.next())
        out << q.value(0).toString();
    check(!q.lastError().isValid(), q.lastError().text());
    check(!canceled || !canceled->load(), "Vérification interrompue.");
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
