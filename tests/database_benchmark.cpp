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
        // La même clé que celle dont l'explorateur se sert. L'index des positions est passé
        // d'une clé textuelle à un entier haché ; le banc était resté à l'ancienne, si bien
        // que ses arêtes n'étaient jamais retrouvées et que l'arbre d'ouvertures sortait vide.
        const auto key = static_cast<qint64>(t64::positionHash(chess::constants::STARTPOS));
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
                           "VALUES(?,?,?,?,?,'2026.10.10','*','','Synthetic',?,?,?,2000,2100,1)"),
                 q.lastError().text());
            must(edge.prepare(
                     "INSERT INTO opening_stats VALUES(?,'e2e4','e4',1,0,0,0) ON CONFLICT(key,uci) DO "
                     "UPDATE SET count=count+1"),
                 edge.lastError().text());
            // Le banc n'écrivait que les arêtes : il ne mesurait donc rien de ce qui passe
            // par `positions`, que tout vrai import remplit — dossier de position et
            // classement des ouvertures compris.
            QSqlQuery position(writer);
            must(position.prepare("INSERT INTO positions(game_id,ply,key,uci,san) VALUES(?,?,?,?,?)"),
                 position.lastError().text());
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
                position.bindValue(0, i);
                position.bindValue(1, 0);
                position.bindValue(2, key);
                position.bindValue(3, "e2e4");
                position.bindValue(4, "e4");
                must(position.exec(), position.lastError().text());
                edge.bindValue(0, key);
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
        const QVariantMap deepCursor{{"updated", "2026-10-10T00:00:00Z"}, {"id", count / 10 + 1}};
        QVariantList cursorPage;
        measure("deep_cursor_200", [&] {
            cursorPage = db.search({}, {}, false, {}, false, {{"cursor", deepCursor}, {"limit", 200}});
            must(cursorPage.size() == std::min(200, count / 10), "Deep cursor page count");
        });
        measure("deep_offset_200", [&] {
            must(db.search({}, {}, false, {}, false, {{"offset", count - count / 10}, {"limit", 200}}) ==
                     cursorPage,
                 "Deep OFFSET and cursor give the same rows");
        });
        measure("trigram_substring", [&] { must(!db.search("ayer 00999").empty(), "Substring lookup"); });
        measure("exact_player", [&] {
            must(db.search({}, {}, false, {}, false, {{"white", "Player 00999"}, {"exactNames", true}})
                         .size() == (count - 999) / 10000 + 1,
                 "Exact player count");
        });
        measure("statistics", [&] { must(db.stats()["games"].toInt() == count, "Materialized game count"); });
        measure("opening_tree", [&] {
            const auto tree = db.explorer(start);
            // first() sur une liste vide est un comportement indéfini : le banc plantait
            // au lieu de dire ce qui manquait.
            must(!tree.empty(), "L'explorateur d'ouvertures ne renvoie aucune ligne");
            must(tree.first().toMap()["count"].toInt() == count, "Materialized opening count");
        });
        measure("top_players", [&] { must(db.players().size() == 1000, "Top player count"); });
        // Le dossier d'une position : sur la position de départ, toutes les parties y
        // sont passées. C'est le pire cas, et c'est celui qu'un joueur ouvre d'abord.
        measure("opening_report", [&] {
            const auto report = db.openingReport(start);
            must(report["total"].toLongLong() == count, "Opening report counts every game");
            must(!report["models"].toList().isEmpty(), "Opening report names model games");
        });
        // Le classement des ouvertures sur une base entière : aucune partie du banc n'a
        // de code, elles sont donc toutes candidates. `measure` répète l'opération, et la
        // seconde passe n'a plus rien à faire : on mesure donc les deux cas séparément,
        // le second étant celui qui suit chaque import d'une base déjà classée.
        QVariantMap table;
        table[QString::number(qint64(t64::positionHash(start.toStdString())))] = "A00";
        clock.restart();
        const int classified = db.classifyOpenings(table);
        report["classify_openings"] = QJsonObject{{"median_ms", clock.nsecsElapsed() / 1e6}};
        must(classified == count, "Every game without a code is classified");
        // La comparaison répertoire/parties tourne sur le fil de l'interface : si elle
        // coûte cher sur une grande base, c'est la fenêtre qui se fige.
        const auto repertoire = db.createRepertoire("Banc", true, t64::parsePgn("1.e4 e5 *").front());
        measure("repertoire_gap", [&] {
            const auto gap = db.repertoireGap(repertoire, {"Player 00999"});
            must(gap["games"].toLongLong() > 0, "The player's games are found");
        });
        measure("classify_openings_idle",
                [&] { must(db.classifyOpenings(table) == 0, "A library already classified is left alone"); });
        std::cout << QJsonDocument(report).toJson().constData();
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
