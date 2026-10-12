// SPDX-License-Identifier: AGPL-3.0-or-later
#include "archive.h"
#include "database.h"
#include <QBuffer>
#include <QCoreApplication>
#include <QFile>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <iostream>
int checks = 0;
void check(bool ok, const char *text) {
    ++checks;
    if (!ok)
        throw std::runtime_error(text);
}
template <class F> void rejects(F f) {
    bool bad = false;
    try {
        f();
    } catch (const std::exception &) {
        bad = true;
    }
    check(bad, "Invalid data accepted");
}
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    QTemporaryDir dir;
    try {
        t64::Database db(dir.path() + "/test.sqlite");
        auto games = t64::parsePgn("[White \"Alice\"]\n[Black \"Bob\"]\n[Result \"1-0\"]\n1.e4 e5 2.Nf3 "
                                   "(2.Bc4) Nc6 1-0\n\n[White \"Carol\"]\n1.d4 d5 *");
        check(db.importGames(games) == 2, "Import games");
        check(db.importGames(games) == 0, "Deduplicate exact PGN");
        QBuffer exported;
        exported.open(QIODevice::WriteOnly);
        db.exportPgn(exported);
        check(t64::parsePgn(exported.data().toStdString()).size() == 2, "PGN export streams to a device");
        rejects([&] { db.validateOutputPath(db.path()); });
        rejects([&] { db.validateOutputPath(db.path() + "-wal"); });
        check(QFile::link(db.path(), dir.path() + "/alias.pgn"), "Create export alias fixture");
        rejects([&] { db.validateOutputPath(dir.path() + "/alias.pgn"); });
        auto rows = db.search("Alice");
        check(rows.size() == 1, "Search player");
        auto id = rows[0].toMap()["id"].toLongLong();
        auto loaded = db.load(id);
        check(loaded.pgn() == games[0].pgn(), "Store full variation tree");
        db.favorite(id, true);
        db.collection(id, "Préparation");
        check(db.search("Préparation", {}, true).size() == 1, "Favorite collection filter");
        check(db.search("' OR 1=1 --").size() == 0, "SQL bound queries");
        check(db.explorer(QString::fromStdString(chess::constants::STARTPOS)).size() == 2,
              "Opening explorer");
        check(db.search({}, {}, false, QString::fromStdString(games[0].nodes[1].fen)).size() == 1,
              "Search exact position");
        loaded.nodes[1].comment = "Updated note";
        db.save(loaded, id);
        check(db.load(id).nodes[1].comment == "Updated note", "Update stable game ID");
        db.trash(id, true);
        check(db.search("Alice").empty(), "Trash hides games");
        check(db.search("Alice", {}, false, {}, true).size() == 1, "Trash recovery list");
        db.trash(id, false);
        db.addCard("First moves", QString::fromStdString(chess::constants::STARTPOS), "e2e4 e7e5 g1f3");
        auto cardId = db.cards()[0].toMap()["id"].toLongLong();
        db.review(cardId, true);
        check(db.stats()["reviews"].toInt() == 1, "Training persists");
        check(db.cards()[0].toMap()["interval"].toInt() == 1, "First SRS interval");
        db.review(cardId, true);
        check(db.cards()[0].toMap()["interval"].toInt() == 2, "SRS interval doubles");
        db.review(cardId, false);
        check(db.cards()[0].toMap()["interval"].toInt() == 1, "Failure resets interval");
        rejects([&] { db.addCard("bad", QString::fromStdString(chess::constants::STARTPOS), "e2e5"); });
        db.backup(dir.path() + "/backup.sqlite");
        t64::Database backup(dir.path() + "/backup.sqlite");
        check(backup.stats()["games"].toInt() == 2 && backup.cards().size() == 1, "Full SQLite backup");
        check(db.integrity() == "ok", "Database integrity");
        rejects([&] { db.backup(db.path()); });
        const auto archive = dir.path() + "/test.t64";
        const auto first = QByteArray::fromStdString(games[0].pgn());
        t64::Archive::append(archive, first);
        check(t64::Archive::read(archive) == first, "T64 initial generation");
        const auto second = QByteArray::fromStdString(games[1].pgn());
        t64::Archive::append(archive, second);
        check(t64::Archive::read(archive) == second, "T64 second generation");
        QFile file(archive);
        check(file.open(QIODevice::ReadWrite), "Open archive fixture");
        file.resize(file.size() - 8);
        file.close();
        check(t64::Archive::read(archive) == first, "T64 recovers after incomplete payload");
        t64::Archive::append(archive, second);
        check(t64::Archive::read(archive) == second, "T64 append after recovery");
        file.open(QIODevice::ReadWrite);
        file.seek(256 + 64);
        file.write("torn", 4);
        file.close();
        check(t64::Archive::read(archive) == first, "T64 recovers torn directory");
        file.open(QIODevice::ReadWrite);
        file.seek(16);
        file.write(QByteArray(8, 'x'));
        file.close();
        rejects([&] { t64::Archive::read(archive); });
        auto variation =
            t64::parsePgn("[White \"Expert\"]\n[Black \"Rival\"]\n[WhiteElo \"2200\"]\n[BlackElo "
                          "\"2300\"]\n[Date \"2025.06.20\"]\n1.e4 e5 (1...c5 2.Nf3) *")
                .at(0);
        auto variantId = db.save(variation);
        int vn = variation.nodes[1].children[1];
        check(db.search({}, {}, false, QString::fromStdString(variation.nodes[vn].fen)).size() == 1,
              "Search indexes side variations");
        QVariantMap filters{
            {"white", "Expert"}, {"minElo", 2100}, {"dateFrom", "2025.01.01"}, {"dateTo", "2025.12.31"}};
        check(db.search({}, {}, false, {}, false, filters).size() == 1,
              "Combined player, Elo and date filters");
        filters["minElo"] = 2400;
        check(db.search({}, {}, false, {}, false, filters).empty(), "Elo excludes lower-rated games");
        auto profile = db.player("Expert");
        check(profile["games"].toInt() == 1 && profile["unfinished"].toInt() == 1,
              "Player profile excludes unfinished from score");
        auto annotated = variation;
        annotated.nodes[1].comment = "Different note";
        db.save(annotated);
        check(!db.duplicates().empty(), "Move-sequence duplicates ignore comments");
        variation.removeVariation(vn);
        db.save(variation, variantId);
        check(db.rebuildIndexes() == 4, "Rebuild all indexes transactionally");
        check(db.stats()["todayReviews"].toInt() == 3, "Review history tracks today's work");
        db.deleteCard(cardId);
        check(db.stats()["todayReviews"].toInt() == 3, "Deleting card preserves activity history");
        // Migration of a genuine v1 layout must back up, preserve data and rebuild variant indexes.
        const auto oldPath = dir.path() + "/old.sqlite";
        db.backup(oldPath);
        {
            auto old = QSqlDatabase::addDatabase("QSQLITE", "old-fixture");
            old.setDatabaseName(oldPath);
            check(old.open(), "Open migration fixture");
            QSqlQuery q(old);
            check(q.exec("SELECT name,type FROM sqlite_master WHERE type IN ('trigger','index') AND (name "
                         "LIKE 'source_%' OR name LIKE 'game_totals_%' OR name LIKE 'edge_%' OR name LIKE "
                         "'fts_%' OR name IN "
                         "('games_eco_recent','games_recent','games_white_rating','games_black_rating','"
                         "games_white_recent','"
                         "games_black_recent','games_collection','"
                         "games_date','review_log_date','cards_due','player_counts_rank'))"),
                  "Find v3 migration fixture objects");
            QList<QPair<QString, QString>> extra;
            while (q.next())
                extra << qMakePair(q.value(0).toString(), q.value(1).toString());
            q.finish();
            for (const auto &object : extra)
                check(q.exec("DROP " + object.second + " " + object.first), "Remove v3 fixture object");
            for (const auto &table :
                 QStringList{"source_games", "sources", "import_history", "games_fts", "library_counts",
                             "player_counts", "opening_edges", "opening_stats"})
                check(q.exec("DROP TABLE IF EXISTS " + table), "Remove v3 table");

            for (const auto &sql :
                 QStringList{"DROP TABLE all_positions", "DROP TABLE review_log",
                             "DROP INDEX games_fingerprint", "ALTER TABLE games DROP COLUMN white_elo",
                             "ALTER TABLE games DROP COLUMN black_elo", "ALTER TABLE games DROP COLUMN plies",
                             "ALTER TABLE games DROP COLUMN fingerprint", "PRAGMA user_version=1"})
                check(q.exec(sql), "Downgrade fixture schema");
            q.finish();
            old.close();
        }
        QSqlDatabase::removeDatabase("old-fixture");
        {
            t64::Database migrated(oldPath);
            check(migrated.stats()["games"].toInt() == 4, "Migration preserves games");
            check(migrated.search({}, {}, false, QString::fromStdString(annotated.nodes[vn].fen)).size() == 1,
                  "Migration rebuilds variant positions");
            check(migrated.integrity() == "ok", "Migrated database integrity");
        }
        const auto streamPath = dir.path() + "/stream.pgn";
        QFile streamFile(streamPath);
        check(streamFile.open(QIODevice::WriteOnly), "Open streaming fixture");
        for (int i = 0; i < 15; ++i)
            streamFile.write(QByteArray("[Event \"Stream ") + QByteArray::number(i) + "\"]\n1.c4 e5 *\n");
        streamFile.close();
        const int beforeImport = db.stats()["games"].toInt();
        rejects([&] { db.importPgnFile(streamPath, [](int n) { return n < 5; }); });
        check(db.stats()["games"].toInt() == beforeImport, "Canceled streaming import rolls back all games");
        check(db.importPgnFile(streamPath, [](int) { return true; }) == 15,
              "Streaming importer commits complete file");
        // Materialized totals must remain equivalent to the raw rows after every write path.
        {
            const auto path = dir.path() + "/counters.sqlite";
            t64::Database counter(path);
            auto g = t64::parsePgn("[White \"RepeatedPlayer\"]\n[Black \"Opponent\"]\n[Result "
                                   "\"1-0\"]\n1.Nf3 Nf6 2.Ng1 Ng8 3.Nf3 Nf6 1-0")
                         .at(0);
            const auto id = counter.save(g);
            const auto start = QString::fromStdString(chess::constants::STARTPOS);
            check(counter.explorer(start).first().toMap()["count"].toInt() == 1,
                  "Repeated positions count once per game in materialized opening tree");
            check(counter.explorer(start).first().toMap()["white"].toInt() == 1,
                  "Opening result initialized");
            counter.favorite(id, true);
            counter.favorite(id, true);
            check(counter.stats()["favorites"].toInt() == 1, "Idempotent favorite totals");
            g.tags["Result"] = "0-1";
            g.tags["White"] = "NewPlayer";
            counter.save(g, id);
            check(counter.stats()["whiteWins"].toInt() == 0 && counter.stats()["blackWins"].toInt() == 1,
                  "Result edit adjusts cached totals");
            auto edge = counter.explorer(start).first().toMap();
            check(edge["white"].toInt() == 0 && edge["black"].toInt() == 1 && edge["count"].toInt() == 1,
                  "Result edit does not duplicate opening edges");
            check(counter.search("peated").empty() && counter.search("wPlay").size() == 1,
                  "Trigram substring search follows metadata edits");
            check(counter.search({}, {}, false, {}, false, {{"white", "New"}, {"exactNames", true}}).empty(),
                  "Exact player search excludes prefixes");
            check(counter.search({}, {}, false, {}, false, {{"white", "NewPlayer"}, {"exactNames", true}})
                          .size() == 1,
                  "Exact player search finds full name");
            counter.trash(id, true);
            counter.trash(id, true);
            check(counter.stats()["games"].toInt() == 0 && counter.stats()["favorites"].toInt() == 0 &&
                      counter.players().empty() && counter.explorer(start).empty(),
                  "Trash removes every materialized contribution exactly once");
            counter.trash(id, false);
            counter.trash(id, false);
            check(counter.stats()["games"].toInt() == 1 && counter.players().size() == 2 &&
                      counter.explorer(start).first().toMap()["black"].toInt() == 1,
                  "Restore recovers totals and player/opening statistics");
            counter.rebuildIndexes();
            check(counter.explorer(start).first().toMap()["count"].toInt() == 1,
                  "Reindex keeps opening statistics consistent");
            auto raw = QSqlDatabase::addDatabase("QSQLITE", "counter-raw");
            raw.setDatabaseName(path);
            check(raw.open(), "Open counter verification connection");
            {
                QSqlQuery q(raw);
                check(q.exec("PRAGMA foreign_keys=ON") && q.exec("DELETE FROM games"), "Delete fixture rows");
            }
            check(counter.stats()["games"].toInt() == 0 && counter.explorer(start).empty(),
                  "Hard delete and foreign-key cascades preserve totals");
            raw.close();
            raw = QSqlDatabase();
            QSqlDatabase::removeDatabase("counter-raw");
        }
        {
            t64::Database sources(dir.path() + "/sources.sqlite");
            const auto pgn = dir.path() + "/source.pgn";
            QFile file(pgn);
            check(file.open(QIODevice::WriteOnly), "Source fixture");
            file.write("[White \"Champion\"]\n[Black \"Rival\"]\n[ECO \"B90\"]\n1.e4 c5 *\n");
            file.close();
            QVariantMap source{{"id", "champion"},
                               {"title", "Champion"},
                               {"source", "https://example.org"},
                               {"license", "test"},
                               {"sha256", "abc"}};
            check(sources.importPgnFile(pgn, [](int) { return true; }, source) == 1, "Import first source");
            source["id"] = "rival";
            source["title"] = "Rival";
            check(sources.importPgnFile(
                      pgn, [](int) { return true; }, source) == 0,
                  "Deduplicate across sources");
            check(sources.sources().size() == 2 && sources.sources()[1].toMap()["games"].toInt() == 1,
                  "Duplicate retains membership in both bases");
            const auto sourceId = sources.sources()[1].toMap()["id"].toLongLong();
            auto rows = sources.search({}, {}, false, {}, false,
                                       {{"sourceId", sourceId},
                                        {"player", "Rival"},
                                        {"colour", "black"},
                                        {"ecoFrom", "B80"},
                                        {"ecoTo", "B99"}});
            check(rows.size() == 1, "Combined base, colour, player and ECO range");
            check(sources
                      .search({}, {}, false, {}, false,
                              {{"openingRequested", true}, {"openingCodes", QStringList{"C00"}}})
                      .empty(),
                  "Opening code filter excludes other families");
            check(sources.search({}, {}, false, {}, false,
                                 {{"openingRequested", true}, {"openingCodes", QStringList{"B90"}}})
                          .size() == 1,
                  "Opening family filter");
            sources.trash(rows[0].toMap()["id"].toLongLong(), true);
            check(sources.sources()[0].toMap()["games"].toInt() == 0, "Trash updates base counts");
            sources.trash(rows[0].toMap()["id"].toLongLong(), false);
            check(sources.sources()[0].toMap()["games"].toInt() == 1, "Restoration updates base counts");
            QBuffer out;
            out.open(QIODevice::WriteOnly);
            sources.exportSource(out, sourceId);
            check(t64::parsePgn(out.data().toStdString()).size() == 1, "Export selected base");
            check(sources.importHistory().size() == 2 &&
                      sources.importHistory()[0].toMap()["added"].toInt() == 0,
                  "History includes duplicate import");
            source["id"] = "cancelled";
            rejects([&] { sources.importPgnFile(pgn, [](int n) { return n == 0; }, source); });
            check(sources.sources().size() == 2 && sources.importHistory().size() == 2 &&
                      sources.integrity() == "ok",
                  "Cancelled import rolls back memberships and history");
        }
        std::cout << checks << " storage checks passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
