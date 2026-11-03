// SPDX-License-Identifier: AGPL-3.0-or-later
// Schema 8: positions are indexed by their 64-bit Zobrist key instead of FEN text.
#include "database.h"
#include "game.h"
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>
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
        sql("CREATE TEMP TABLE keymap(fen TEXT PRIMARY KEY,key INTEGER NOT NULL) WITHOUT ROWID");
        QSqlQuery distinct(db_), insert(db_);
        distinct.setForwardOnly(true);
        check(distinct.exec("SELECT fen FROM all_positions UNION SELECT fen FROM positions"),
              distinct.lastError().text());
        insert.prepare("INSERT INTO keymap VALUES(?,?)");
        while (distinct.next()) {
            const auto fen = distinct.value(0).toString();
            insert.bindValue(0, fen);
            insert.bindValue(1, hashKey(fen.toStdString()));
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
            "INTEGER NOT NULL,key INTEGER NOT NULL,material TEXT NOT NULL,PRIMARY KEY(game_id,node)) WITHOUT "
            "ROWID");
        sql("INSERT INTO all_positions8 SELECT a.game_id,a.node,k.key,a.material FROM all_positions a JOIN "
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
