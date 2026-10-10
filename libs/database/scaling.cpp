// SPDX-License-Identifier: AGPL-3.0-or-later
#include "database.h"
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>
namespace t64 {
void Database::upgradeV3(int originalVersion) {
    if (originalVersion >= 3) {
        enhanced_ = true;
        return;
    }
    if (originalVersion > 0)
        backup(path_ + ".before-v3-" + QUuid::createUuid().toString(QUuid::WithoutBraces) + ".sqlite");
    sql("SAVEPOINT schema3");
    try {
        sql("CREATE INDEX games_recent ON games(deleted,updated DESC,id DESC)");
        sql("CREATE INDEX games_white_rating ON games(deleted,white,white_elo)");
        sql("CREATE INDEX games_black_rating ON games(deleted,black,black_elo)");
        sql("CREATE INDEX games_white_recent ON games(deleted,white,updated DESC,id DESC)");
        sql("CREATE INDEX games_black_recent ON games(deleted,black,updated DESC,id DESC)");
        sql("CREATE INDEX games_collection ON games(deleted,collection,updated DESC,id DESC)");
        sql("CREATE INDEX games_date ON games(deleted,date DESC,id DESC)");
        sql("CREATE INDEX review_log_date ON review_log(reviewed)");
        sql("CREATE INDEX cards_due ON cards(due,id)");
        sql("CREATE TABLE library_counts(id INTEGER PRIMARY KEY CHECK(id=1),games INTEGER,favorites "
            "INTEGER,whiteWins INTEGER,draws INTEGER,blackWins INTEGER)");
        sql("INSERT INTO library_counts SELECT "
            "1,COUNT(*),COALESCE(SUM(favorite=1),0),COALESCE(SUM(result='1-0'),0),COALESCE(SUM(result='1/2-1/"
            "2'),0),COALESCE(SUM(result='0-1'),0) FROM games WHERE deleted=0");
        sql("CREATE TABLE player_counts(name TEXT PRIMARY KEY,games INTEGER NOT NULL)");
        sql("INSERT INTO player_counts SELECT name,COUNT(*) FROM (SELECT white name FROM games WHERE "
            "deleted=0 UNION ALL SELECT black FROM games WHERE deleted=0) GROUP BY name");
        sql("CREATE INDEX player_counts_rank ON player_counts(games DESC,name)");
        auto delta = [&](QString ref, int sign) {
            QString d = QString::number(sign) + "*(" + ref + ".deleted=0)";
            QString text = "UPDATE library_counts SET games=games+" + d + ",favorites=favorites+" + d + "*(" +
                           ref + ".favorite=1),whiteWins=whiteWins+" + d + "*(" + ref +
                           ".result='1-0'),draws=draws+" + d + "*(" + ref +
                           ".result='1/2-1/2'),blackWins=blackWins+" + d + "*(" + ref +
                           ".result='0-1') WHERE id=1;";
            for (auto field : {"white", "black"})
                text += "INSERT INTO player_counts(name,games) VALUES(COALESCE(" + ref + "." + field +
                        ",'')," + d + ") ON CONFLICT(name) DO UPDATE SET games=games+excluded.games;";
            return text;
        };
        sql("CREATE TRIGGER game_totals_insert AFTER INSERT ON games BEGIN " + delta("NEW", 1) + " END");
        sql("CREATE TRIGGER game_totals_delete AFTER DELETE ON games BEGIN " + delta("OLD", -1) + " END");
        sql("CREATE TRIGGER game_totals_update AFTER UPDATE OF white,black,deleted,favorite,result ON games "
            "BEGIN " +
            delta("OLD", -1) + delta("NEW", 1) + " END");
        sql("CREATE TABLE opening_edges(game_id INTEGER REFERENCES games(id) ON DELETE CASCADE,fen TEXT NOT "
            "NULL,uci TEXT NOT NULL,san TEXT NOT NULL,PRIMARY KEY(game_id,fen,uci)) WITHOUT ROWID");
        sql("CREATE TABLE opening_stats(fen TEXT NOT NULL,uci TEXT NOT NULL,san TEXT NOT NULL,count INTEGER "
            "NOT NULL,white INTEGER NOT NULL,draw INTEGER NOT NULL,black INTEGER NOT NULL,PRIMARY "
            "KEY(fen,uci)) WITHOUT ROWID");
        sql("INSERT INTO opening_edges SELECT game_id,fen,uci,MIN(san) FROM positions WHERE uci<>'' GROUP BY "
            "game_id,fen,uci");
        sql("INSERT INTO opening_stats SELECT "
            "e.fen,e.uci,MIN(e.san),COUNT(*),SUM(g.result='1-0'),SUM(g.result='1/2-1/2'),SUM(g.result='0-1') "
            "FROM opening_edges e JOIN games g ON g.id=e.game_id WHERE g.deleted=0 GROUP BY e.fen,e.uci");
        sql("CREATE TRIGGER edge_game_delete BEFORE DELETE ON games BEGIN DELETE FROM opening_edges WHERE "
            "game_id=OLD.id; END");
        sql("CREATE TRIGGER edge_add AFTER INSERT ON opening_edges BEGIN INSERT INTO opening_stats SELECT "
            "NEW.fen,NEW.uci,NEW.san,1,(result='1-0'),(result='1/2-1/2'),(result='0-1') FROM games WHERE "
            "id=NEW.game_id AND deleted=0 ON CONFLICT(fen,uci) DO UPDATE SET "
            "count=count+1,white=white+excluded.white,draw=draw+excluded.draw,black=black+excluded.black; "
            "END");
        sql("CREATE TRIGGER edge_remove BEFORE DELETE ON opening_edges BEGIN UPDATE opening_stats SET "
            "count=count-1,white=white-COALESCE((SELECT result='1-0' FROM games WHERE "
            "id=OLD.game_id),0),draw=draw-COALESCE((SELECT result='1/2-1/2' FROM games WHERE "
            "id=OLD.game_id),0),black=black-COALESCE((SELECT result='0-1' FROM games WHERE "
            "id=OLD.game_id),0) WHERE fen=OLD.fen AND uci=OLD.uci AND (SELECT deleted FROM games WHERE "
            "id=OLD.game_id)=0; DELETE FROM opening_stats WHERE fen=OLD.fen AND uci=OLD.uci AND count<=0; "
            "END");
        // Update contributions once per game/position/move, including repetition and trash/restore.
        sql("CREATE TRIGGER edge_game_update AFTER UPDATE OF result,deleted ON games WHEN "
            "OLD.result<>NEW.result OR OLD.deleted<>NEW.deleted BEGIN INSERT INTO opening_stats SELECT "
            "fen,uci,san,(NEW.deleted=0)-(OLD.deleted=0),(NEW.deleted=0 AND NEW.result='1-0')-(OLD.deleted=0 "
            "AND OLD.result='1-0'),(NEW.deleted=0 AND NEW.result='1/2-1/2')-(OLD.deleted=0 AND "
            "OLD.result='1/2-1/2'),(NEW.deleted=0 AND NEW.result='0-1')-(OLD.deleted=0 AND OLD.result='0-1') "
            "FROM opening_edges WHERE game_id=NEW.id ON CONFLICT(fen,uci) DO UPDATE SET "
            "count=count+excluded.count,white=white+excluded.white,draw=draw+excluded.draw,black=black+"
            "excluded.black; DELETE FROM opening_stats WHERE count<=0 AND (fen,uci) IN "
            "(SELECT fen,uci FROM opening_edges WHERE game_id=NEW.id); END");
        // Portable fallback when the distribution's SQLite lacks the FTS5 trigram tokenizer.
        QSqlQuery fts(db_);
        fts_ = fts.exec("CREATE VIRTUAL TABLE games_fts USING "
                        "fts5(white,black,event,eco,collection,date,content='games',content_rowid='id',"
                        "tokenize='trigram')");
        if (fts_) {
            sql("INSERT INTO games_fts(games_fts) VALUES('rebuild')");
            const QString cols = "white,black,event,eco,collection,date";
            sql("CREATE TRIGGER fts_insert AFTER INSERT ON games BEGIN INSERT INTO games_fts(rowid," + cols +
                ") VALUES(NEW.id,NEW.white,NEW.black,NEW.event,NEW.eco,NEW.collection,NEW.date); END");
            const QString remove =
                "INSERT INTO games_fts(games_fts,rowid," + cols +
                ") VALUES('delete',OLD.id,OLD.white,OLD.black,OLD.event,OLD.eco,OLD.collection,OLD.date);";
            sql("CREATE TRIGGER fts_delete AFTER DELETE ON games BEGIN " + remove + " END");
            sql("CREATE TRIGGER fts_update AFTER UPDATE OF " + cols + " ON games BEGIN " + remove +
                " INSERT INTO games_fts(rowid," + cols +
                ") VALUES(NEW.id,NEW.white,NEW.black,NEW.event,NEW.eco,NEW.collection,NEW.date); END");
        }
        sql("PRAGMA user_version=3");
        sql("RELEASE schema3");
        enhanced_ = true;
    } catch (...) {
        sql("ROLLBACK TO schema3");
        sql("RELEASE schema3");
        throw;
    }
}
} // namespace t64
