// SPDX-License-Identifier: AGPL-3.0-or-later
// Synthetic one-move games: measures indexed reads, not long-PGN import throughput.
#include "database.h"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <algorithm>
#include <iostream>
#include <vector>
void must(bool ok, const QString &message) {
    if (!ok)
        throw std::runtime_error(message.toStdString());
}
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    try {
        const int count = argc > 1 ? QString::fromLocal8Bit(argv[1]).toInt() : 100000;
        must(count >= 1000 && count <= 2000000, "Use 1,000 to 2,000,000 synthetic games");
        QTemporaryDir temp;
        const QString path = temp.path() + "/synthetic.sqlite";
        t64::Database db(path);
        const auto start = QString::fromStdString(chess::constants::STARTPOS);
        const auto key = QString::fromStdString(t64::positionKey(chess::constants::STARTPOS));
        auto writer = QSqlDatabase::addDatabase("QSQLITE", "benchmark-writer");
        writer.setDatabaseName(path);
        must(writer.open(), writer.lastError().text());
        QElapsedTimer clock;
        clock.start();
        {
            QSqlQuery q(writer), edge(writer);
            must(q.prepare("INSERT INTO "
                           "games(id,uuid,white,black,event,date,result,eco,collection,pgn,digest,updated,"
                           "white_elo,black_elo,plies) "
                           "VALUES(?,?,?,?,?,'2026.10.10','*','B00','Synthetic',?,?,?,2000,2100,1)"),
                 q.lastError().text());
            must(edge.prepare("INSERT INTO opening_edges VALUES(?,?, 'e2e4','e4')"), edge.lastError().text());
            must(writer.transaction(), writer.lastError().text());
            for (int i = 1; i <= count; ++i) {
                const auto white = QString("Player %1").arg(i % 10000, 5, 10, QChar('0'));
                const auto black = QString("Opponent %1").arg((i * 7) % 10000, 5, 10, QChar('0'));
                const auto event = QString("Fixture %1").arg(i);
                const auto pgn = QString("[White \"%1\"]\n[Black \"%2\"]\n[Event \"%3\"]\n\n1.e4 *")
                                     .arg(white, black, event);
                q.bindValue(0, i);
                q.bindValue(1, QString::number(i));
                q.bindValue(2, white);
                q.bindValue(3, black);
                q.bindValue(4, event);
                q.bindValue(5, pgn);
                q.bindValue(6, QString::number(i));
                q.bindValue(7, "2026-10-10T00:00:00Z");
                must(q.exec(), q.lastError().text());
                edge.bindValue(0, i);
                edge.bindValue(1, key);
                must(edge.exec(), edge.lastError().text());
                if (i % 10000 == 0) {
                    must(writer.commit(), writer.lastError().text());
                    must(writer.transaction(), writer.lastError().text());
                    if (i % 100000 == 0)
                        std::cerr << i << " rows prepared\n";
                }
            }
            must(writer.commit(), writer.lastError().text());
            must(q.exec("PRAGMA wal_checkpoint(TRUNCATE)"), q.lastError().text());
        }
        const auto seed = clock.elapsed();
        writer.close();
        writer = QSqlDatabase();
        QSqlDatabase::removeDatabase("benchmark-writer");
        QJsonObject report{
            {"fixture", "Synthetic metadata and one opening edge per game; not a full PGN/position corpus"},
            {"games", count},
            {"seed_ms", seed},
            {"database_bytes", QFileInfo(path).size()}};
        auto measure = [&](const QString &name, auto operation) {
            std::vector<double> samples;
            for (int i = 0; i < 11; ++i) {
                clock.restart();
                operation();
                samples.push_back(clock.nsecsElapsed() / 1e6);
            }
            std::sort(samples.begin(), samples.end());
            report[name] = QJsonObject{{"median_ms", samples[5]}, {"max_ms", samples.back()}};
        };
        measure("library_page", [&] { must(db.search().size() == 1000, "First page count"); });
        measure("trigram_substring", [&] { must(!db.search("ayer 00999").empty(), "Substring lookup"); });
        measure("exact_player", [&] {
            must(db.search({}, {}, false, {}, false, {{"white", "Player 00999"}, {"exactNames", true}})
                         .size() == (count - 999) / 10000 + 1,
                 "Exact player count");
        });
        measure("statistics", [&] { must(db.stats()["games"].toInt() == count, "Materialized game count"); });
        measure("opening_tree", [&] {
            must(db.explorer(start).first().toMap()["count"].toInt() == count, "Materialized opening count");
        });
        measure("top_players", [&] { must(db.players().size() == 1000, "Top player count"); });
        std::cout << QJsonDocument(report).toJson().constData();
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
