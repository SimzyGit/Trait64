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
        std::cout << checks << " storage checks passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
