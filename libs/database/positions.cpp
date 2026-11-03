// SPDX-License-Identifier: AGPL-3.0-or-later
// Schema 8: positions are indexed by their 64-bit Zobrist key instead of FEN text.
#include "database.h"
#include "game.h"
#include <QSqlError>
#include <QSqlQuery>
#include <QSaveFile>
#include <QUuid>
#include <algorithm>
#include <stdexcept>

namespace t64 {
namespace {
void check(bool ok, const QString &error) {
    if (!ok) throw std::runtime_error(error.toStdString());
}
void run(QSqlQuery &q) {
    if (!q.exec()) throw std::runtime_error(q.lastError().text().toStdString());
}
// Opening statistics triggers of schema 3, keyed by position hash.
const char *openingTriggers[] = {
    "CREATE TRIGGER edge_game_delete BEFORE DELETE ON games BEGIN DELETE FROM opening_edges WHERE "
    "game_id=OLD.id; END",
    "CREATE TRIGGER edge_add AFTER INSERT ON opening_edges BEGIN INSERT INTO opening_stats SELECT "
    "NEW.key,NEW.uci,NEW.san,1,(result='1-0'),(result='1/2-1/2'),(result='0-1') FROM games WHERE "
    "id=NEW.game_id AND deleted=0 ON CONFLICT(key,uci) DO UPDATE SET "
    "count=count+1,white=white+excluded.white,draw=draw+excluded.draw,black=black+excluded.black; END",
    "CREATE TRIGGER edge_remove BEFORE DELETE ON opening_edges BEGIN UPDATE opening_stats SET "
    "count=count-1,white=white-COALESCE((SELECT result='1-0' FROM games WHERE "
    "id=OLD.game_id),0),draw=draw-COALESCE((SELECT result='1/2-1/2' FROM games WHERE "
    "id=OLD.game_id),0),black=black-COALESCE((SELECT result='0-1' FROM games WHERE "
    "id=OLD.game_id),0) WHERE key=OLD.key AND uci=OLD.uci AND (SELECT deleted FROM games WHERE "
    "id=OLD.game_id)=0; DELETE FROM opening_stats WHERE key=OLD.key AND uci=OLD.uci AND count<=0; END",
    "CREATE TRIGGER edge_game_update AFTER UPDATE OF result,deleted ON games WHEN "
    "OLD.result<>NEW.result OR OLD.deleted<>NEW.deleted BEGIN INSERT INTO opening_stats SELECT "
    "key,uci,san,(NEW.deleted=0)-(OLD.deleted=0),(NEW.deleted=0 AND NEW.result='1-0')-(OLD.deleted=0 "
    "AND OLD.result='1-0'),(NEW.deleted=0 AND NEW.result='1/2-1/2')-(OLD.deleted=0 AND "
    "OLD.result='1/2-1/2'),(NEW.deleted=0 AND NEW.result='0-1')-(OLD.deleted=0 AND OLD.result='0-1') "
    "FROM opening_edges WHERE game_id=NEW.id ON CONFLICT(key,uci) DO UPDATE SET "
    "count=count+excluded.count,white=white+excluded.white,draw=draw+excluded.draw,black=black+"
    "excluded.black; DELETE FROM opening_stats WHERE count<=0 AND (key,uci) IN "
    "(SELECT key,uci FROM opening_edges WHERE game_id=NEW.id); END"};
} // namespace

qint64 Database::hashKey(const std::string &fen) { return static_cast<qint64>(positionHash(fen)); }
qint64 Database::pawnsKey(const std::string &fen) { return static_cast<qint64>(pawnKey(fen)); }

QVariantMap Database::novelty(const Game &game, qint64 self) const {
    QSqlQuery stats(db_), own(db_);
    stats.prepare("SELECT uci,san,count FROM opening_stats WHERE key=? ORDER BY count DESC");
    own.prepare("SELECT 1 FROM games WHERE id=? AND deleted=0");
    own.addBindValue(self);
    run(own);
    const bool counted = self > 0 && own.next();
    own.finish();
    const auto line = game.mainline();
    for (size_t i = 0; i + 1 < line.size(); ++i) {
        const auto &node = game.nodes[line[i]];
        const auto &next = game.nodes[line[i + 1]];
        stats.bindValue(0, nodeKey(node));
        run(stats);
        qint64 reached = 0, played = 0;
        QVariantList known;
        while (stats.next()) {
            const bool mine = counted && stats.value(0).toString() == QString::fromStdString(next.uci);
            const qint64 count = stats.value(2).toLongLong() - (mine ? 1 : 0);
            if (count <= 0) continue;
            reached += count;
            if (stats.value(0).toString() == QString::fromStdString(next.uci)) played = count;
            else if (known.size() < 3) known << QVariantMap{{"san", stats.value(1)}, {"count", count}};
        }
        if (reached == 0) // the previous move already left every known game
            return {{"found", false}, {"reason", "unknownStart"}};
        if (played == 0)
            return {{"found", true}, {"node", line[i + 1]}, {"ply", int(i)}, {"san", QString::fromStdString(next.san)},
                    {"reached", reached}, {"known", known}};
    }
    return {{"found", false}, {"reason", "known"}};
}

int Database::exportPolyglot(const QString &path, int minGames) const {
    // Schema-8 keys are Polyglot keys, so the statistics map onto book entries directly.
    struct Entry {
        quint64 key;
        quint16 move, weight;
    };
    std::vector<Entry> entries;
    QSqlQuery q(db_);
    q.setForwardOnly(true);
    q.prepare("SELECT key,uci,san,count FROM opening_stats WHERE count>=?");
    q.addBindValue(std::max(1, minGames));
    run(q);
    while (q.next()) {
        const auto uci = q.value(1).toString(), san = q.value(2).toString();
        if (uci.size() < 4) continue;
        auto square = [](QStringView s) { return (s[1].unicode() - '1') * 8 + (s[0].unicode() - 'a'); };
        const int from = square(QStringView(uci).mid(0, 2));
        int to = square(QStringView(uci).mid(2, 2));
        // Polyglot stores castling as the king taking its own rook.
        if (san.startsWith("O-O")) to = (from / 8) * 8 + (san.startsWith("O-O-O") ? 0 : 7);
        int promotion = 0;
        if (uci.size() == 5) promotion = QString("nbrq").indexOf(uci[4]) + 1;
        if (from < 0 || from > 63 || to < 0 || to > 63 || promotion < 0) continue;
        const auto clamped = quint16(std::clamp(q.value(3).toInt(), 1, 65535));
        entries.push_back({quint64(q.value(0).toLongLong()), quint16(to | from << 6 | promotion << 12), clamped});
    }
    std::sort(entries.begin(), entries.end(), [](const Entry &a, const Entry &b) {
        return a.key != b.key ? a.key < b.key : a.weight > b.weight;
    });
    QSaveFile file(path);
    check(file.open(QIODevice::WriteOnly), file.errorString());
    QByteArray bytes;
    bytes.reserve(int(entries.size() * 16));
    for (const auto &e : entries) {
        for (int shift = 56; shift >= 0; shift -= 8) bytes.append(char(e.key >> shift & 0xff));
        bytes.append(char(e.move >> 8)).append(char(e.move & 0xff));
        bytes.append(char(e.weight >> 8)).append(char(e.weight & 0xff));
        bytes.append(4, '\0'); // learn field
    }
    check(file.write(bytes) == bytes.size() && file.commit(), file.errorString());
    return int(entries.size());
}

void Database::upgradeV8(int originalVersion) {
    if (originalVersion >= 8) {
        keyed_ = true;
        return;
    }
    if (originalVersion > 0)
        backup(path_ + ".before-v8-" + QUuid::createUuid().toString(QUuid::WithoutBraces) + ".sqlite");
    sql("SAVEPOINT schema8");
    try {
        // FEN text -> hash, computed once per distinct position. SQL then converts every table
        // without re-reading a single PGN.
        sql("CREATE TEMP TABLE keymap(fen TEXT PRIMARY KEY,key INTEGER NOT NULL,pawns INTEGER NOT NULL) WITHOUT ROWID");
        QSqlQuery distinct(db_), insert(db_);
        distinct.setForwardOnly(true);
        check(distinct.exec("SELECT fen FROM all_positions UNION SELECT fen FROM positions"),
              distinct.lastError().text());
        insert.prepare("INSERT INTO keymap VALUES(?,?,?)");
        while (distinct.next()) {
            const auto fen = distinct.value(0).toString();
            insert.bindValue(0, fen);
            insert.bindValue(1, hashKey(fen.toStdString()));
            insert.bindValue(2, pawnsKey(fen.toStdString()));
            run(insert);
        }
        distinct.finish();
        for (const char *name : {"edge_game_delete", "edge_add", "edge_remove", "edge_game_update"})
            sql(QString("DROP TRIGGER IF EXISTS ") + name);
        sql("CREATE TABLE positions8(game_id INTEGER NOT NULL REFERENCES games(id) ON DELETE CASCADE,ply "
            "INTEGER NOT NULL,key INTEGER NOT NULL,uci TEXT,san TEXT,PRIMARY KEY(game_id,ply)) WITHOUT ROWID");
        sql("INSERT INTO positions8 SELECT p.game_id,p.ply,k.key,p.uci,p.san FROM positions p JOIN keymap k "
            "ON k.fen=p.fen");
        sql("CREATE TABLE all_positions8(game_id INTEGER NOT NULL REFERENCES games(id) ON DELETE CASCADE,node "
            "INTEGER NOT NULL,key INTEGER NOT NULL,material TEXT NOT NULL,pawns INTEGER NOT NULL,PRIMARY "
            "KEY(game_id,node)) WITHOUT "
            "ROWID");
        sql("INSERT INTO all_positions8 SELECT a.game_id,a.node,k.key,a.material,k.pawns FROM all_positions a JOIN "
            "keymap k ON k.fen=a.fen");
        sql("CREATE TABLE opening_edges8(game_id INTEGER REFERENCES games(id) ON DELETE CASCADE,key INTEGER "
            "NOT NULL,uci TEXT NOT NULL,san TEXT NOT NULL,PRIMARY KEY(game_id,key,uci)) WITHOUT ROWID");
        sql("INSERT INTO opening_edges8 SELECT game_id,key,uci,MIN(san) FROM positions8 WHERE uci<>'' GROUP "
            "BY game_id,key,uci");
        sql("CREATE TABLE opening_stats8(key INTEGER NOT NULL,uci TEXT NOT NULL,san TEXT NOT NULL,count "
            "INTEGER NOT NULL,white INTEGER NOT NULL,draw INTEGER NOT NULL,black INTEGER NOT NULL,PRIMARY "
            "KEY(key,uci)) WITHOUT ROWID");
        sql("INSERT INTO opening_stats8 SELECT "
            "e.key,e.uci,MIN(e.san),COUNT(*),SUM(g.result='1-0'),SUM(g.result='1/2-1/2'),SUM(g.result='0-1') "
            "FROM opening_edges8 e JOIN games g ON g.id=e.game_id WHERE g.deleted=0 GROUP BY e.key,e.uci");
        for (const char *table : {"positions", "all_positions", "opening_edges", "opening_stats"}) {
            sql(QString("DROP TABLE ") + table);
            sql(QString("ALTER TABLE ") + table + "8 RENAME TO " + table);
        }
        sql("CREATE INDEX positions_key ON positions(key)");
        sql("CREATE INDEX all_positions_key ON all_positions(key)");
        sql("CREATE INDEX all_positions_material ON all_positions(material)");
        sql("CREATE INDEX all_positions_pawns ON all_positions(pawns)");
        for (const char *trigger : openingTriggers) sql(trigger);
        sql("DROP TABLE keymap");
        sql("PRAGMA user_version=8");
        sql("RELEASE schema8");
    } catch (...) {
        sql("ROLLBACK TO schema8");
        sql("RELEASE schema8");
        throw;
    }
    keyed_ = true;
    // Return the pages freed by the text keys to the file system once.
    if (originalVersion > 0) sql("VACUUM");
}
} // namespace t64
