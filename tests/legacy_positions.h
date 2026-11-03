// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
// Rewrites a schema-8 library the way schema 7 stored positions (FEN text keys), so tests
// can exercise the real 7 -> 8 migration and older downgrade fixtures.
#include "database.h"
#include <QSqlError>
#include <QSqlQuery>
#include <stdexcept>

inline void downgradePositionsToV7(const QString &path, const QString &connection = "positions-v7") {
    std::map<qint64, t64::Game> games;
    {
        t64::Database library(path);
        for (const auto &value : library.search()) {
            const auto id = value.toMap()["id"].toLongLong();
            games.emplace(id, library.load(id));
        }
    }
    {
        auto db = QSqlDatabase::addDatabase("QSQLITE", connection);
        db.setDatabaseName(path);
        if (!db.open()) throw std::runtime_error("Cannot open v7 fixture");
        QSqlQuery q(db);
        auto exec = [&](const QString &sql) {
            if (!q.exec(sql)) throw std::runtime_error((sql + ": " + q.lastError().text()).toStdString());
        };
        exec("BEGIN");
        for (const char *trigger : {"edge_game_delete", "edge_add", "edge_remove", "edge_game_update"})
            exec(QString("DROP TRIGGER IF EXISTS ") + trigger);
        exec("CREATE TABLE positions7(game_id INTEGER NOT NULL REFERENCES games(id) ON DELETE CASCADE,ply INTEGER "
             "NOT NULL,fen TEXT NOT NULL,uci TEXT,san TEXT,PRIMARY KEY(game_id,ply))");
        exec("CREATE TABLE all_positions7(game_id INTEGER NOT NULL REFERENCES games(id) ON DELETE CASCADE,node "
             "INTEGER NOT NULL,fen TEXT NOT NULL,material TEXT NOT NULL,PRIMARY KEY(game_id,node))");
        QSqlQuery insert(db), material(db);
        material.prepare("SELECT material FROM all_positions WHERE game_id=? AND node=?");
        for (const auto &[id, game] : games) {
            const auto line = game.mainline();
            insert.prepare("INSERT INTO positions7 VALUES(?,?,?,?,?)");
            for (size_t i = 0; i < line.size(); ++i) {
                insert.addBindValue(id);
                insert.addBindValue(int(i));
                insert.addBindValue(QString::fromStdString(t64::positionKey(game.nodes[line[i]].fen)));
                insert.addBindValue(i + 1 < line.size() ? QString::fromStdString(game.nodes[line[i + 1]].uci) : QString());
                insert.addBindValue(i + 1 < line.size() ? QString::fromStdString(game.nodes[line[i + 1]].san) : QString());
                if (!insert.exec()) throw std::runtime_error("v7 positions");
            }
            insert.prepare("INSERT INTO all_positions7 VALUES(?,?,?,?)");
            for (size_t n = 0; n < game.nodes.size(); ++n) {
                material.addBindValue(id);
                material.addBindValue(int(n));
                if (!material.exec() || !material.next()) throw std::runtime_error("v7 material");
                insert.addBindValue(id);
                insert.addBindValue(int(n));
                insert.addBindValue(QString::fromStdString(t64::positionKey(game.nodes[n].fen)));
                insert.addBindValue(material.value(0));
                if (!insert.exec()) throw std::runtime_error("v7 all positions");
            }
        }
        material.finish();
        for (const char *table : {"positions", "all_positions", "opening_edges", "opening_stats"})
            exec(QString("DROP TABLE ") + table);
        exec("ALTER TABLE positions7 RENAME TO positions");
        exec("ALTER TABLE all_positions7 RENAME TO all_positions");
        exec("CREATE INDEX positions_fen ON positions(fen)");
        exec("CREATE INDEX all_positions_fen ON all_positions(fen)");
        exec("CREATE INDEX all_positions_material ON all_positions(material)");
        exec("CREATE TABLE opening_edges(game_id INTEGER REFERENCES games(id) ON DELETE CASCADE,fen TEXT NOT "
             "NULL,uci TEXT NOT NULL,san TEXT NOT NULL,PRIMARY KEY(game_id,fen,uci)) WITHOUT ROWID");
        exec("CREATE TABLE opening_stats(fen TEXT NOT NULL,uci TEXT NOT NULL,san TEXT NOT NULL,count INTEGER "
             "NOT NULL,white INTEGER NOT NULL,draw INTEGER NOT NULL,black INTEGER NOT NULL,PRIMARY "
             "KEY(fen,uci)) WITHOUT ROWID");
        exec("INSERT INTO opening_edges SELECT game_id,fen,uci,MIN(san) FROM positions WHERE uci<>'' GROUP BY "
             "game_id,fen,uci");
        exec("INSERT INTO opening_stats SELECT e.fen,e.uci,MIN(e.san),COUNT(*),SUM(g.result='1-0'),"
             "SUM(g.result='1/2-1/2'),SUM(g.result='0-1') FROM opening_edges e JOIN games g ON g.id=e.game_id "
             "WHERE g.deleted=0 GROUP BY e.fen,e.uci");
        exec("PRAGMA user_version=7");
        exec("COMMIT");
        q.finish();
        db.close();
    }
    QSqlDatabase::removeDatabase(connection);
}
