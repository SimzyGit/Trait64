// SPDX-License-Identifier: AGPL-3.0-or-later
#include "archive.h"
#include "cancellation.h"
#include "chessbase.h"
#include "croissant.h"
#include "crosstable.h"
#include "database.h"
#include "legacy_positions.h"
#include "moveorder.h"
#include "names.h"
#include "polyglot.h"
#include "scid.h"
#include <QBuffer>
#include <QCoreApplication>
#include <QDataStream>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QScopeGuard>
#include <QSet>
#include <QSqlDriver>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QTemporaryDir>
#include <cstring>
#include <iostream>
#include <random>
#include <source_location>
#include <sqlite3.h>
#include <thread>
int checks = 0;
void check(bool ok, const char *text) {
    ++checks;
    if (!ok)
        throw std::runtime_error(text);
}
// Vingt et un appels partageaient le même message : un échec ne disait pas lequel avait
// laissé passer la donnée invalide. La ligne d'appel est jointe automatiquement.
template <class F> void rejects(F f, std::source_location where = std::source_location::current()) {
    bool bad = false;
    try {
        f();
    } catch (const std::exception &) {
        bad = true;
    }
    if (!bad) {
        ++checks;
        throw std::runtime_error("Invalid data accepted at line " + std::to_string(where.line()));
    }
    check(bad, "Invalid data accepted");
}
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    QTemporaryDir dir;
    try {
        {
            // No ANALYZE statistics: this is the normal state of a fresh import.
            // Count SQLite VM work on the real save() lookup, not wall-clock time
            // or a separately reconstructed query that could drift from production.
            t64::Database large(dir.filePath("dedup.sqlite"));
            QSqlDatabase connection;
            for (const auto &name : QSqlDatabase::connectionNames()) {
                auto candidate = QSqlDatabase::database(name);
                if (candidate.databaseName() == large.path())
                    connection = candidate;
            }
            QSqlQuery seed(connection);
            check(seed.exec("WITH RECURSIVE n(x) AS (VALUES(1) UNION ALL SELECT x+1 FROM n WHERE x<4096) "
                            "INSERT INTO games(uuid,white,black,event,date,result,pgn,digest,updated) "
                            "SELECT 'seed-'||x,'White','Black','','2026.10.22','*','*','seed-'||x,"
                            "'2026-10-22T00:00:00Z' FROM n"),
                  "Seed a populated library without query-planner statistics");
            auto native = connection.driver()->handle();
            check(native.isValid() && QByteArray(native.typeName()) == "sqlite3*", "Native SQLite handle");
            auto *handle = *static_cast<sqlite3 **>(native.data());
            // Ce bloc appelle SQLite directement sur la connexion de Qt. Quand le pilote
            // embarque sa propre copie de la bibliothèque — les binaires officiels de Qt
            // sur macOS et Windows — mêler les deux plante. On le constate et on passe,
            // au lieu de faire tomber la suite sur une limite connue de la plateforme.
            const bool oneLibrary = t64::QueryCancellation::sameLibrary(connection);
            if (!oneLibrary) {
                QSqlQuery driverVersion(connection);
                driverVersion.exec("SELECT sqlite_version()");
                std::cout << "Skipping native-handle checks: the Qt driver reports SQLite "
                          << (driverVersion.next() ? driverVersion.value(0).toString().toStdString()
                                                   : std::string("(unavailable)"))
                          << " while this build links " << sqlite3_libversion() << '\n';
            }
            if (oneLibrary) {
                struct LookupWork {
                    int calls = 0, maximum = 0;
                } work;
                sqlite3_trace_v2(
                    handle, SQLITE_TRACE_PROFILE,
                    [](unsigned, void *context, void *statement, void *) {
                        auto *stmt = static_cast<sqlite3_stmt *>(statement);
                        const char *sql = sqlite3_sql(stmt);
                        if (sql && std::strncmp(sql, "SELECT id FROM games ", 21) == 0 &&
                            std::strstr(sql, "digest=")) {
                            auto &work = *static_cast<LookupWork *>(context);
                            ++work.calls;
                            work.maximum = std::max(work.maximum,
                                                    sqlite3_stmt_status(stmt, SQLITE_STMTSTATUS_VM_STEP, 0));
                        }
                        return 0;
                    },
                    &work);
                qint64 first = 0, duplicate = 0, restored = 0;
                const auto game = t64::parsePgn("[White \"Real game\"]\n1.e4 e5 *").front();
                try {
                    first = large.save(game);
                    duplicate = large.save(game);
                    large.trash(first, true);
                    restored = large.save(game);
                } catch (...) {
                    sqlite3_trace_v2(handle, 0, nullptr, nullptr);
                    throw;
                }
                sqlite3_trace_v2(handle, 0, nullptr, nullptr);
                check(work.calls == 3 && work.maximum < 100,
                      "Digest lookups stay bounded despite thousands of unrelated active games");
                check(first == duplicate && restored != first && large.load(first).pgn() == game.pgn() &&
                          large.load(restored).pgn() == game.pgn() && large.stats()["games"].toInt() == 4097,
                      "Bounded deduplication preserves active copies and games in the trash");
                auto canceled = std::make_shared<std::atomic_bool>(true);
                rejects([&] { large.integrity(canceled); });
                canceled->store(false);
                // Cancel after SQLite has begun preparing the actual integrity check,
                // without sleeps or a race against the speed of this machine.
                sqlite3_set_authorizer(
                    handle,
                    [](void *flag, int action, const char *name, const char *, const char *, const char *) {
                        if (action == SQLITE_PRAGMA && name && std::strcmp(name, "integrity_check") == 0)
                            static_cast<std::atomic_bool *>(flag)->store(true);
                        return SQLITE_OK;
                    },
                    canceled.get());
                QString interruption;
                try {
                    large.integrity(canceled);
                } catch (const std::exception &e) {
                    interruption = QString::fromUtf8(e.what());
                }
                sqlite3_set_authorizer(handle, nullptr, nullptr);
                check(canceled->load() && interruption.contains("interrupt", Qt::CaseInsensitive),
                      "SQLite interrupts an executing integrity check, never reporting partial success");
                check(large.integrity() == "ok" && large.stats()["games"].toInt() == 4097,
                      "Cancellation leaves data intact and removes the temporary progress handler");
            }
        }
        {
            t64::Database materialDb(dir.filePath("material.sqlite"));
            const auto specimens =
                t64::parsePgn("[Event \"Capture and variation\"]\n1.e4 d5 2.exd5 (2.e5) *\n\n"
                              "[SetUp \"1\"]\n[FEN \"8/P6k/8/8/8/8/6p1/4K3 w - - 0 1\"]\n"
                              "1.a8=N g1=R+ *\n\n"
                              "[SetUp \"1\"]\n[FEN \"4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 1\"]\n"
                              "1.exd6 *\n\n"
                              "[Variant \"Chess960\"]\n[SetUp \"1\"]\n"
                              "[FEN \"4k3/8/8/8/8/8/8/RK5R w HA - 0 1\"]\n1.O-O *");
            QMap<QString, QSet<qint64>> matching;
            QSet<QPair<qint64, qint64>> expected;
            for (const auto &game : specimens) {
                const auto id = materialDb.save(game);
                for (size_t node = 0; node < game.nodes.size(); ++node) {
                    // L'ancienne clé en toutes lettres reste la référence de la recherche.
                    auto key = QString::fromStdString(game.nodes[node].fen).section(' ', 0, 0);
                    key.remove(QRegularExpression("[^prnbqkPRNBQK]"));
                    std::sort(key.begin(), key.end());
                    expected.insert({id, t64::Database::materialCode(key)});
                    matching[key].insert(id);
                }
            }
            auto verify = [&] {
                QSqlDatabase connection;
                for (const auto &name : QSqlDatabase::connectionNames()) {
                    auto candidate = QSqlDatabase::database(name);
                    if (candidate.databaseName() == materialDb.path())
                        connection = candidate;
                }
                QSqlQuery query(connection);
                check(query.exec("SELECT game_id,material FROM game_material"),
                      "Read persisted material index");
                QSet<QPair<qint64, qint64>> actual;
                while (query.next())
                    actual.insert({query.value(0).toLongLong(), query.value(1).toLongLong()});
                check(actual == expected, "Material signatures cover variants, special moves and colours");
                for (auto it = matching.cbegin(); it != matching.cend(); ++it) {
                    auto input = it.key();
                    std::reverse(input.begin(), input.end());
                    input += QString::fromUtf8(" 123/.-É♔");
                    QSet<qint64> ids;
                    for (const auto &row : materialDb.search({}, {}, false, {}, false, {{"material", input}}))
                        ids.insert(row.toMap()["id"].toLongLong());
                    check(ids == it.value(),
                          "Material search keeps counts, colours, variants and ignored characters");
                }
            };
            verify();
            check(materialDb.rebuildIndexes() == int(specimens.size()), "Rebuild material fixtures");
            verify();
            rejects(
                [&] { materialDb.rebuildIndexes([&](int count) { return count < int(specimens.size()); }); });
            verify();
            check(materialDb.integrity() == "ok", "Material indexes remain valid after canceled rebuild");
        }
        t64::Database db(dir.path() + "/test.sqlite");
        auto games = t64::parsePgn("[White \"Alice\"]\n[Black \"Bob\"]\n[Result \"1-0\"]\n1.e4 e5 2.Nf3 "
                                   "(2.Bc4) Nc6 1-0\n\n[White \"Carol\"]\n1.d4 d5 *");
        check(db.importGames(games) == 2, "Import games");
        check(db.importGames(games) == 0, "Deduplicate exact PGN");
        {
            // Lieu et ronde : rangés à l'import, relus par la liste, triables.
            t64::Database places(dir.filePath("places.sqlite"));
            places.importGames(t64::parsePgn("[White \"A\"]\n[Black \"B\"]\n[Site \"Wijk aan Zee\"]\n"
                                             "[Round \"7\"]\n1.e4 *\n\n[White \"C\"]\n[Black \"D\"]\n"
                                             "[Site \"Biel\"]\n[Round \"2\"]\n1.d4 *"));
            const auto rows = places.search({}, {}, false, {}, false, {{"sort", "site"}});
            check(rows.size() == 2 && rows[0].toMap()["site"] == "Biel" && rows[1].toMap()["round"] == "7",
                  "Site and round are stored and sortable");
            // Une bibliothèque du schéma 17 les reprend de l'en-tête PGN de ses parties.
            {
                auto raw = QSqlDatabase::addDatabase("QSQLITE", "places-v17");
                raw.setDatabaseName(dir.filePath("places.sqlite"));
                check(raw.open(), "Open the library behind its back");
                QSqlQuery q(raw);
                check(q.exec("UPDATE games SET site='',round=''") && q.exec("PRAGMA user_version=17"),
                      "Library set back to schema 17");
            }
            QSqlDatabase::removeDatabase("places-v17");
        }
        {
            t64::Database places(dir.filePath("places.sqlite"));
            const auto rows = places.search({}, {}, false, {}, false, {{"sort", "site"}});
            check(rows.size() == 2 && rows[1].toMap()["site"] == "Wijk aan Zee" &&
                      rows[0].toMap()["round"] == "2",
                  "Migration 18 reads site and round from the stored PGN");
        }
        QBuffer exported;
        exported.open(QIODevice::WriteOnly);
        db.exportPgn(exported);
        check(t64::parsePgn(exported.data().toStdString()).size() == 2, "PGN export streams to a device");
        {
            // Un bilan de masse ne doit refaire que ce qui manque : la profondeur atteinte
            // est retenue par partie, et une demande moins exigeante ne relance rien.
            const auto waiting = db.recentGamesToReview(2, 18);
            check(waiting.size() == 2, "Fresh games are all waiting for a review");
            const auto gameId = waiting.first().toLongLong();
            const auto job = db.createAnalysisJob(QString::fromStdString(db.load(gameId).pgn()),
                                                  QVariantMap{{"depth", 18}}, false, 1, gameId);
            db.setAnalysisStatus(job, "running");
            db.saveAnalysisResult(job, 0, 0, "fen", QVariantList{QVariantMap{{"rank", 1}}});
            db.setAnalysisStatus(job, "completed");
            check(db.recentGamesToReview(2, 18).size() == 1, "A finished review is not queued again");
            check(db.recentGamesToReview(2, 12).size() == 1, "A shallower review changes nothing");
            check(db.recentGamesToReview(2, 24).size() == 2, "A deeper review covers the game again");
        }
        {
            // Tableau de bord : le bilan d'une partie est conservé, puis agrégé sous les
            // pseudos d'un joueur, du côté où il se trouvait.
            t64::Database board(dir.filePath("dashboard.sqlite"));
            auto played = t64::parsePgn(
                "[White \"Alice\"]\n[Black \"Bob\"]\n[Date \"2026.09.10\"]\n1.e4 e5 2.Nf3 Nc6 *\n\n"
                "[White \"Bob\"]\n[Black \"Alice\"]\n[Date \"2026.10.02\"]\n1.d4 d5 2.c4 e6 *");
            check(board.importGames(played) == 2, "Seed two games for the dashboard");
            check(board.reviewSummary({"Alice"})["games"].toInt() == 0,
                  "Without a stored review nothing is aggregated");
            // Les évaluations sont comptées du point de vue des Blancs : Alice tient
            // l'équilibre dans la première partie, Bob lâche prise dans la seconde.
            const auto rows = board.search({});
            for (const auto &row : rows) {
                const auto id = row.toMap()["id"].toLongLong();
                const auto game = board.load(id);
                const bool aliceIsWhite = row.toMap()["white"].toString() == "Alice";
                const auto job =
                    board.createAnalysisJob(QString::fromStdString(game.pgn()), QVariantMap{{"depth", 18}},
                                            false, int(game.mainline().size()), id);
                board.setAnalysisStatus(job, "running");
                const auto line = game.mainline();
                // Les deux parties doivent être dissemblables : avec les mêmes
                // évaluations et les couleurs échangées, les deux joueurs obtiendraient
                // par symétrie exactement la même note, et la vérification ne prouverait
                // rien. Ici Bob faute dans les deux, une fois avec les Noirs, une fois
                // avec les Blancs — attendu : Alice 99,8 contre Bob 42,3.
                const double blackErrs[] = {20, 20, 600, 590, 610};
                const double whiteErrs[] = {20, 20, 20, -600, -590};
                const double *evaluations = aliceIsWhite ? blackErrs : whiteErrs;
                for (size_t i = 0; i < line.size(); ++i)
                    board.saveAnalysisResult(
                        job, int(i), line[i], QString::number(int(i)),
                        QVariantList{QVariantMap{
                            {"rank", 1}, {"scoreType", "cp"}, {"cp", evaluations[std::min(i, size_t(4))]}}});
                board.setAnalysisStatus(job, "completed");
            }
            const auto alice = board.reviewSummary({"Alice"});
            check(alice["games"].toInt() == 2 && alice["moves"].toLongLong() > 0,
                  "Both games count for a player who played each colour");
            check(alice["all"].isValid() && alice["opening"].isValid(),
                  "The aggregate carries an overall and an opening mark");
            check(!alice["endgame"].isValid(), "A phase no game reached stays unmeasured");
            const auto bob = board.reviewSummary({"Bob"});
            check(bob["games"].toInt() == 2 && alice["all"].toDouble() > 95 && bob["all"].toDouble() < 60,
                  "Each player is scored from the side he actually played");
            check(board.reviewSummary({"alice"})["games"].toInt() == 2,
                  "Nicknames are matched without regard to case");
            check(board.reviewSummary({})["games"].toInt() == 0, "No nickname means nothing to measure");
            check(board.gamesCountFor({"alice"}) >= 2 && board.gamesCountFor({}) == 0,
                  "A profile counts all its games, reviewed or not");
            {
                // Le bilan d'un profil de plusieurs centaines de parties : aucun plafond ne
                // doit l'arrêter en route. Les bilans terminés au-delà des 200 derniers
                // sont retirés, leurs chiffres restant dans game_reviews.
                const auto gameId = board.gamesToReviewFor({"alice"}, 99).first().toLongLong();
                const auto study = t64::parsePgn("1.e4 e5 *").front();
                const auto pgn = QString::fromStdString(study.pgn());
                const QVariantMap config{{"depth", 1}};
                qint64 last = 0;
                for (int i = 0; i < 600; ++i) {
                    last = board.createAnalysisJob(pgn, config, false, 3, gameId);
                    if (i < 590) {
                        board.setAnalysisStatus(last, "running");
                        for (int ordinal = 0; ordinal < 3; ++ordinal)
                            board.saveAnalysisResult(last, ordinal, ordinal,
                                                     QString::fromStdString(study.nodes[ordinal].fen),
                                                     {QVariantMap{{"rank", 1},
                                                                  {"score", 0.},
                                                                  {"cp", 0},
                                                                  {"scoreType", "cp"},
                                                                  {"depth", 1}}});
                        board.setAnalysisStatus(last, "completed");
                    }
                }
                check(last > 0 && board.pendingAnalyses() == 10 && board.analysisJobs().size() <= 211,
                      "Six hundred game reviews queue without hitting the 500-analysis cap");
            }
            {
                // Les clés stratégiques : un Gambit de la Dame, variante d'échange, garde la
                // structure Carlsbad bien après le seizième demi-coup.
                t64::Database keys(dir.filePath("themes.sqlite"));
                keys.importGames(
                    t64::parsePgn("[White \"Carl\"]\n[Black \"Sbad\"]\n\n1. d4 d5 2. c4 e6 3. Nc3 Nf6 4. "
                                  "cxd5 exd5 5. Bg5 Be7 6. e3 c6 7. Bd3 Nbd7 8. Qc2 O-O 9. Nf3 "
                                  "Re8 10. O-O Nf8 11. h3 g6 *"));
                const auto theme = [&](const char *key) {
                    return keys.search({}, {}, false, {}, false, {{"theme", key}}).size();
                };
                check(theme("carlsbad") == 1 && theme("isolatedQueenPawn") == 0 &&
                          keys.gamesWithoutThemes() == 0,
                      "Games are classified by the strategic keys they kept");
            }
            check(board.accuracyEloSamples().empty(),
                  "Games without ratings, or too short, do not calibrate the estimated strength");
            const auto history = board.reviewHistory({"Alice"}, 12);
            check(history.size() == 2 && history.first().toMap()["period"].toString() == "2026.10",
                  "History is grouped by month, most recent first");
            check(history.first().toMap()["games"].toInt() == 1, "Each month counts its own games");
            // Le bouton « analyser le profil » doit désigner exactement les parties que le
            // tableau de bord ne sait pas encore compter : mêmes pseudos, même casse.
            check(board.gamesToReviewFor({"Alice"}, 18).isEmpty(),
                  "A game already reviewed that deep is not queued again");
            check(board.gamesToReviewFor({"Alice"}, 24).size() == 2,
                  "A deeper review queues the games again");
            check(board.gamesToReviewFor({"alice"}, 24).size() == 2,
                  "Games to review are matched without regard to case");
            check(board.gamesToReviewFor({"Carol"}, 24).isEmpty(), "Another player's games are left alone");
            check(board.gamesToReviewFor({}, 24).isEmpty(), "No nickname means nothing to queue");
        }
        {
            // L'écart entre un répertoire et les parties réellement jouées : personne
            // d'autre ne peut le dire, il faut les deux au même endroit.
            t64::Database mine(dir.filePath("gap.sqlite"));
            const auto prep = t64::parsePgn("1.e4 e5 2.Nf3 Nc6 3.Bb5 *").front();
            const auto id = mine.createRepertoire("Espagnole", true, prep);
            auto game = [](const char *w, const char *b, const char *moves, const char *result) {
                return QString("[Event \"x\"]\n[White \"%1\"]\n[Black \"%2\"]\n[Result \"%3\"]\n"
                               "[Date \"2026.01.01\"]\n\n%4 %3\n\n")
                    .arg(w, b, result, moves);
            };
            const auto pgn =
                // Suit la préparation jusqu'au bout.
                game("Simzy", "X", "1.e4 e5 2.Nf3 Nc6 3.Bb5 a6", "1-0") +
                // La quitte au troisième coup : Bc4 au lieu de Bb5.
                game("Simzy", "Y", "1.e4 e5 2.Nf3 Nc6 3.Bc4 Bc5", "0-1") +
                // N'y entre jamais.
                game("Simzy", "Z", "1.d4 d5 2.c4 e6", "1/2-1/2") +
                // Celle d'un autre joueur ne doit pas compter.
                game("Autre", "W", "1.e4 e5 2.Nf3 Nc6 3.Bc4", "1-0") +
                // Simzy avec les Noirs : le répertoire est celui des Blancs.
                game("V", "Simzy", "1.e4 e5 2.Nf3 Nc6 3.Bc4", "1-0");
            check(mine.importGames(t64::parsePgn(pgn.toStdString())) == 5, "Seed five games");
            const auto gap = mine.repertoireGap(id, {"Simzy"});
            check(gap["games"].toLongLong() == 3, "Only the player's games with that colour are counted");
            check(gap["touched"].toLongLong() == 2, "Two of them reached the repertoire");
            check(gap["left"].toLongLong() == 1, "One of them left the preparation");
            const auto rows = gap["rows"].toList();
            check(rows.size() == 1, "One row per game that left, not per position");
            const auto row = rows.first().toMap();
            check(row["black"] == "Y", "The row names the game that left");
            check(row["move"].toInt() == 3 && row["played"] == "Bc4",
                  "The row says at which move and with which move the player left");
            check(row["expected"].toStringList() == QStringList{"Bb5"},
                  "And what the repertoire had prepared instead");
            // Un joueur inconnu de la base n'est pas une erreur : c'est zéro.
            const auto none = mine.repertoireGap(id, {"Personne"});
            check(none["games"].toLongLong() == 0 && none["rows"].toList().isEmpty(),
                  "A name no game carries reports nothing rather than failing");
            check(mine.repertoireGap(id, {})["games"].toLongLong() == 0,
                  "No name at all reports nothing rather than every game");
        }
        {
            // Dossier d'une position : ce que la bibliothèque sait de ce qui s'y joue.
            t64::Database desk(dir.filePath("report.sqlite"));
            auto game = [](const char *w, const char *b, const char *result, const char *we, const char *be,
                           const char *moves, const char *year) {
                return QString("[Event \"x\"]\n[White \"%1\"]\n[Black \"%2\"]\n[Result \"%3\"]\n"
                               "[WhiteElo \"%4\"]\n[BlackElo \"%5\"]\n[Date \"%6.01.01\"]\n\n%7 %3\n\n")
                    .arg(w, b, result, we, be, year, moves);
            };
            const auto pgn = game("A", "B", "1-0", "2500", "2480", "1.e4 c5 2.Nf3", "2024") +
                             game("C", "D", "0-1", "2400", "2390", "1.e4 c5 2.Nf3", "2023") +
                             game("E", "F", "1/2-1/2", "1800", "1750", "1.e4 c5 2.Nf3", "2022") +
                             game("G", "H", "1-0", "1900", "1850", "1.e4 c5 2.d4", "2021") +
                             game("I", "J", "1-0", "0", "0", "1.e4 c5 2.c3", "2020");
            check(desk.importGames(t64::parsePgn(pgn.toStdString())) == 5, "Seed five games on one position");
            const QString sicilian = "rnbqkbnr/pp1ppppp/8/2p5/4P3/8/PPPP1PPP/RNBQKBNR w KQkq - 0 2";
            const auto report = desk.openingReport(sicilian);
            check(report["total"].toLongLong() == 5, "Every game through the position is counted");
            check(report["whiteWins"].toLongLong() == 3 && report["draws"].toLongLong() == 1 &&
                      report["blackWins"].toLongLong() == 1,
                  "The result split is the one of the games that reached it");
            const auto moves = report["moves"].toList();
            check(moves.size() == 3, "One line per move played from the position");
            check(moves[0].toMap()["san"] == "Nf3" && moves[0].toMap()["count"].toLongLong() == 3,
                  "Moves come most played first");
            // La tranche de force : trois Nf3, deux chez les maîtres, un au club.
            const auto bands = moves[0].toMap()["bands"].toMap();
            check(bands["maitre"].toLongLong() == 2 && bands["club"].toLongLong() == 1,
                  "A move is split by the strength of those who played it");
            check(!bands.contains(""), "A game missing an Elo is counted, but ranked in no band");
            check(moves[0].toMap()["last"].toString() == "2024",
                  "The most recent year the move was played is kept");
            const auto models = report["models"].toList();
            check(models.size() == 5 && models[0].toMap()["white"] == "A",
                  "Model games come best rated first");
            // Une position que personne n'a jouée n'est pas une erreur : c'est zéro.
            const auto empty = desk.openingReport("8/8/8/4k3/8/8/4K3/8 w - - 0 1");
            check(empty["total"].toLongLong() == 0 && empty["moves"].toList().isEmpty() &&
                      empty["models"].toList().isEmpty(),
                  "A position no game reached reports nothing rather than failing");
            // Le dossier gardé en base sert tel quel, et disparaît dès qu'une partie change.
            {
                auto raw = QSqlDatabase::addDatabase("QSQLITE", "report-cache");
                raw.setDatabaseName(dir.filePath("report.sqlite"));
                check(raw.open(), "Open the library a second time");
                QByteArray payload;
                QDataStream out(&payload, QIODevice::WriteOnly);
                out << QVariantMap{{"total", 99}};
                QSqlQuery q(raw);
                q.prepare("INSERT INTO report_cache(key,models,payload) SELECT key,6,? FROM positions WHERE "
                          "uci='g1f3' LIMIT 1");
                q.addBindValue(payload);
                check(q.exec(), "Store a report in the cache");
                raw.close();
            }
            QSqlDatabase::removeDatabase("report-cache");
            check(desk.openingReport(sicilian)["total"].toLongLong() == 99, "A cached report is read back");
            desk.importGames(
                t64::parsePgn(game("K", "L", "1-0", "0", "0", "1.e4 c5 2.Nf3", "2025").toStdString()));
            check(desk.openingReport(sicilian)["total"].toLongLong() == 6,
                  "A new game drops the cached report");
            check(t64::Database::cleanTags({" a  b ", "A B", "c,d", "", QString(60, 'x')}) ==
                      QStringList({"a b", "cd", QString(40, 'x')}),
                  "Tags are tidied: spaces, case-insensitive duplicates, commas and length");
        }
        {
            // Préparer un adversaire : graphies, base à son nom, arbre et faiblesses.
            t64::Database prep(dir.filePath("opponent.sqlite"));
            QString pgn;
            const auto add = [&](const QString &w, const QString &b, const QString &result,
                                 const QString &eco, const QString &date, const QString &moves) {
                pgn += QString("[White \"%1\"]\n[Black \"%2\"]\n[Result \"%3\"]\n[ECO \"%4\"]\n[Date "
                               "\"%5\"]\n\n%6 %3\n\n")
                           .arg(w, b, result, eco, date, moves);
            };
            for (int i = 0; i < 6; ++i)
                add("Rival, Jean", QString("X%1").arg(i), i < 5 ? "0-1" : "1-0", "C50",
                    "2024.01.0" + QString::number(i + 1), "1.e4 e5 2.Nf3 Nc6 3.Bc4");
            add("Rival,J.", "Y", "1-0", "D02", "2025.03.01", "1.d4 d5 2.Nf3");
            add("Z", "Jean Rival", "1/2-1/2", "B20", "2025.04.01", "1.e4 c5");
            prep.importGames(t64::parsePgn(pgn.toStdString()));
            const auto names = prep.playerSpellings("Rival, Jean");
            check(names.size() == 3, "Every spelling of a player is found");
            const auto root = prep.opponentTree(names, true, {});
            const auto first = root["moves"].toList();
            check(root["games"].toInt() == 7 && first.size() == 2 && first[0].toMap()["san"] == "e4" &&
                      first[0].toMap()["games"].toInt() == 6 && first[0].toMap()["score"].toInt() == 17 &&
                      first[1].toMap()["last"] == "2025.03.01",
                  "His tree gives each move with its games, score and last date");
            const auto deeper = prep.opponentTree(names, true, {"e2e4", "e7e5"});
            check(deeper["hisTurn"].toBool() && deeper["moves"].toList().first().toMap()["san"] == "Nf3",
                  "The tree follows a line and knows whose move it is");
            const auto weak = prep.opponentWeaknesses(names);
            check(weak.size() == 1 && weak.first().toMap()["eco"] == "C50" &&
                      weak.first().toMap()["score"].toInt() == 17,
                  "A weakness is an opening he plays often and scores little in");
            // Statistiques personnelles : cadence, ouverture, semaines.
            t64::Database mine(dir.filePath("stats.sqlite"));
            QString games;
            for (int i = 0; i < 4; ++i)
                games +=
                    QString(
                        "[White \"Moi\"]\n[Black \"A%1\"]\n[Result \"%2\"]\n[ECO \"B20\"]\n[Date "
                        "\"2026.11.%3\"]\n"
                        "[WhiteElo \"%4\"]\n[BlackElo \"1800\"]\n[TimeControl \"180+2\"]\n\n1.e4 c5 %2\n\n")
                        .arg(i)
                        .arg(i < 3 ? "1-0" : "0-1")
                        .arg(10 + i)
                        .arg(1900 + 10 * i);
            games += "[White \"B\"]\n[Black \"moi\"]\n[Result \"1/2-1/2\"]\n[Date "
                     "\"2026.06.01\"]\n[TimeControl \"600+5\"]\n\n1.d4 d5 1/2-1/2\n\n";
            mine.importGames(t64::parsePgn(games.toStdString()));
            const auto stats = mine.personalStats({"Moi"}, QDate(2026, 11, 20));
            const auto cadences = stats["cadences"].toList();
            check(cadences.size() == 2 && cadences[0].toMap()["cadence"] == "blitz" &&
                      cadences[0].toMap()["rating"].toInt() == 1930 &&
                      cadences[0].toMap()["recent"].toInt() == 1915 &&
                      cadences[1].toMap()["cadence"] == "rapid",
                  "Strength is read per time control from the ratings in the games");
            const auto opening = stats["openings"].toList().first().toMap();
            check(opening["eco"] == "B20" && opening["score"].toInt() == 75 &&
                      opening["performance"].toInt() == 2000,
                  "Performance per opening follows the FIDE formula");
            const auto weeks = stats["weeks"].toList();
            check(weeks.size() == 12 && weeks.last().toMap()["games"].toInt() == 0 &&
                      weeks[10].toMap()["games"].toInt() == 4,
                  "Activity is counted week by week");
            const auto base =
                prep.collectPlayerGames({{"id", "prep:fide:1"}, {"title", "Préparation : Rival"}}, names);
            check(prep.search({}, {}, false, {}, false, {{"sourceId", base}}).size() == 8,
                  "His games are gathered in a preparation base");
        }
        {
            // Doublons : même suite de coups et mêmes joueurs, l'un annoté, l'autre daté.
            t64::Database twins(dir.filePath("twins.sqlite"));
            twins.importGames(t64::parsePgn(
                "[White \"Kasparov\"]\n[Black \"Karpov\"]\n[Result \"1-0\"]\n[Date \"????.??.??\"]\n\n"
                "1.e4 {Le roi des coups} e5 2.Nf3 (2.f4 Bc5) 1-0\n\n"
                "[White \"Kasparov\"]\n[Black \"Karpov\"]\n[Result \"1-0\"]\n[Date \"1985.10.15\"]\n"
                "[Event \"Moscou\"]\n\n1.e4 e5 2.Nf3 1-0\n\n"
                "[White \"Autre\"]\n[Black \"Joueur\"]\n[Result \"1-0\"]\n\n1.e4 e5 2.Nf3 1-0\n\n"));
            const auto merged = twins.mergeDuplicates();
            check(merged["groups"].toInt() == 1 && merged["removed"].toInt() == 1,
                  "Only games with the same players are merged, not every game with the same moves");
            const auto kept = twins.search();
            check(kept.size() == 2, "The merged duplicate leaves the library");
            bool annotated = false;
            for (const auto &row : kept) {
                const auto game = twins.load(row.toMap()["id"].toLongLong());
                if (game.tags.at("White") == "Kasparov")
                    annotated = game.tags.at("Date") == "1985.10.15" && game.tags.at("Event") == "Moscou" &&
                                game.pgn().find("Le roi des coups") != std::string::npos &&
                                game.pgn().find("f4") != std::string::npos;
            }
            check(annotated, "The kept game has the comments, the variation and the missing headers");
            // Comparaison : la base « twins » contre celle du dossier d'ouverture.
            const auto diff = twins.compareWith(dir.filePath("report.sqlite"));
            check(diff["mine"].toInt() == 2 && diff["theirs"].toInt() == 6 && diff["common"].toInt() == 0 &&
                      diff["onlyTheirs"].toInt() == 6 && diff["players"].toList().size() == 10,
                  "Two libraries are compared game by game, with the players missing here");
            bool refused = false;
            try {
                twins.compareWith(dir.filePath("nowhere.sqlite"));
            } catch (const std::exception &) {
                refused = !QFile::exists(dir.filePath("nowhere.sqlite"));
            }
            check(refused, "Comparing with a missing file creates nothing");
        }
        {
            // Réparation : une base dont des pages sont écrasées se recopie dans une base saine.
            const auto damaged = dir.filePath("damaged.sqlite");
            {
                t64::Database seed(damaged);
                QString pgn;
                for (int i = 0; i < 40; ++i)
                    pgn += QString("[White \"W%1\"]\n[Black \"B%1\"]\n[Result \"1-0\"]\n\n1.e4 e5 2.Nf3 Nc6 "
                                   "3.Bb5 a6 4.Ba4 Nf6 5.O-O Be7 %2 1-0\n\n")
                               .arg(i)
                               .arg(i % 2 ? "6.Re1 b5" : "6.d3 b5");
                seed.importGames(t64::parsePgn(pgn.toStdString()));
            }
            const auto healthy = t64::Database::salvage(damaged, dir.filePath("healthy.sqlite"));
            check(healthy["games"].toInt() == 40 && healthy["lost"].toInt() == 0 &&
                      healthy["integrity"] == "ok",
                  "A sound library is copied whole");
            QFile file(damaged);
            check(file.open(QIODevice::ReadWrite) && file.size() > 40 * 4096, "The library spans many pages");
            for (int page : {6, 9, 12}) {
                file.seek(qint64(page) * 4096 + 100);
                file.write(QByteArray(3000, '\xA5'));
            }
            file.close();
            const auto saved = t64::Database::salvage(damaged, dir.filePath("saved.sqlite"));
            check(saved["integrity"] == "ok" && saved["games"].toInt() > 0,
                  "A damaged library is rebuilt from what can still be read");
        }
        {
            // Classer les ouvertures : `eco` ne venait que de la balise PGN, et la plupart
            // des bases n'en portent aucune. Le classement passe par l'empreinte de
            // position que la base indexe déjà — encore faut-il que celle d'une position
            // à quatre champs soit la même que celle d'un FEN complet.
            t64::Database shelf(dir.filePath("classify.sqlite"));
            check(
                shelf.importGames(t64::parsePgn("[Event \"sicilienne\"]\n\n1.e4 c5 2.Nf3 d6 *\n\n"
                                                "[Event \"balise\"]\n[ECO \"Z99\"]\n\n1.e4 c5 2.Nf3 d6 *\n\n"
                                                "[Event \"inconnue\"]\n\n1.a4 h5 2.Ra3 Rh6 *")) == 3,
                "Seed a game to classify, one already tagged, and one off the books");
            // La clé est celle du FEN à quatre champs, comme dans positions.tsv.
            const QString afterD6 = "rnbqkbnr/pp2pppp/3p4/2p5/4P3/5N2/PPPP1PPP/RNBQKB1R w KQkq -";
            const QString afterC5 = "rnbqkbnr/pp1ppppp/8/2p5/4P3/8/PPPP1PPP/RNBQKBNR w KQkq -";
            QVariantMap table;
            table[QString::number(qint64(t64::positionHash(afterC5.toStdString())))] = "B20";
            table[QString::number(qint64(t64::positionHash(afterD6.toStdString())))] = "B50";
            check(shelf.classifyOpenings(table) == 1, "Only the untagged game that matches is classified");
            auto ecoOf = [&](const QString &event) {
                for (const auto &row : shelf.search({}))
                    if (row.toMap()["event"] == event)
                        return row.toMap()["eco"].toString();
                return QString();
            };
            check(ecoOf("sicilienne") == "B50",
                  "The deepest opening position reached decides the code, not the first");
            check(ecoOf("balise") == "Z99", "A code already in the PGN is left alone");
            check(ecoOf("inconnue").isEmpty(), "A game off the books keeps no code rather than a wrong one");
            check(shelf.classifyOpenings(table) == 0, "Running it again classifies nothing more");
            bool stopped = false;
            rejects([&] {
                shelf.classifyOpenings(table, [&](qint64 n) {
                    stopped = true;
                    return n < 2;
                });
            });
            check(stopped, "A canceled classification stops rather than finishing quietly");
        }
        {
            // Ranger la bibliothèque par famille d'ouverture : une seule passe, et seules
            // les parties dont le code figure dans la table sont touchées.
            t64::Database shelf(dir.filePath("openings.sqlite"));
            const QString sicilian = "B20", queens = "D00";
            check(shelf.importGames(t64::parsePgn("[Event \"a\"]\n[ECO \"B20\"]\n\n1.e4 c5 *\n\n"
                                                  "[Event \"b\"]\n[ECO \"D00\"]\n\n1.d4 d5 *\n\n"
                                                  "[Event \"c\"]\n\n1.c4 e5 *")) == 3,
                  "Seed games with and without an ECO code");
            check(shelf.groupByOpening({{sicilian, sicilian + " · Famille"}}) == 1,
                  "Only the games whose code is listed are ranged");
            auto collectionOf = [&](const QString &event) {
                for (const auto &row : shelf.search({}))
                    if (row.toMap()["event"] == event)
                        return row.toMap()["collection"].toString();
                return QString();
            };
            check(collectionOf("a") == sicilian + " · Famille", "A ranged game carries its family label");
            check(collectionOf("b").isEmpty(), "A game outside the table keeps its collection");
            check(collectionOf("c").isEmpty(), "A game without an ECO code is left alone");
            // Relancer doit réétiqueter, non empiler : l'opération est rejouable.
            check(shelf.groupByOpening({{sicilian, sicilian + " · Autre"}}) == 1,
                  "Grouping again relabels the same games");
            check(collectionOf("a") == sicilian + " · Autre", "The new label replaces the previous one");
            bool stopped = false;
            rejects([&] {
                shelf.groupByOpening({{sicilian, "x"}, {queens, "y"}}, [&](qint64 n) {
                    stopped = true;
                    return n < 2;
                });
            });
            check(stopped && collectionOf("a") == sicilian + " · Autre",
                  "A canceled grouping leaves every label untouched");
        }
        rejects([&] { db.validateOutputPath(db.path()); });
        rejects([&] { db.validateOutputPath(db.path() + "-wal"); });
        // Un lien symbolique vers la base ne doit pas servir de cible d'export : il
        // permettrait d'écraser la bibliothèque en passant par un autre nom. Sous
        // Windows, QFile::link produit un raccourci « .lnk » et non un lien symbolique
        // — la notion visée n'y existe pas sous cette forme, et créer un vrai lien
        // symbolique y demande des privilèges que la machine d'intégration n'a pas.
#ifdef Q_OS_WIN
        std::cout << "Skipping symlink-alias checks: QFile::link creates a shortcut on Windows\n";
#else
        check(QFile::link(db.path(), dir.path() + "/alias.pgn"), "Create export alias fixture");
        rejects([&] { db.validateOutputPath(dir.path() + "/alias.pgn"); });
        {
            t64::Database alias(dir.path() + "/alias.pgn");
            rejects([&] { alias.validateOutputPath(db.path() + ".analysis.lock"); });
            rejects([&] { alias.validateOutputPath(db.path() + ".import.lock"); });
            rejects([&] { alias.validateOutputPath(db.path() + "-wal"); });
        }
#endif
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
        check(db.cards()[0].toMap()["interval"].toInt() == 3, "Second SRS step is three days");
        db.review(cardId, true);
        check(db.cards()[0].toMap()["interval"].toInt() == 8, "An always-known card grows by 2.5");
        db.review(cardId, false);
        check(db.cards()[0].toMap()["interval"].toInt() == 1, "Failure resets interval");
        db.review(cardId, true);
        db.review(cardId, true);
        // Après l'échec : 1 → 3 jours, puis 5 réussites sur 6, facteur 1,3 + 1,2 × 5/6 = 2,3,
        // donc 3 → 7 jours au lieu de 8 pour une carte jamais ratée.
        check(db.cards()[0].toMap()["interval"].toInt() == 7, "A card once failed grows more slowly");
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
        const auto beforeCanceledRebuild = db.exportPgn();
        rejects([&] { db.rebuildIndexes([](int count) { return count < 4; }); });
        check(db.exportPgn() == beforeCanceledRebuild && db.integrity() == "ok",
              "Cancellation at the final reindex checkpoint preserves the library");
        check(db.stats()["todayReviews"].toInt() == 6, "Review history tracks today's work");
        db.deleteCard(cardId);
        check(db.stats()["todayReviews"].toInt() == 6, "Deleting card preserves activity history");
        // Migration of a genuine v1 layout must back up, preserve data and rebuild variant indexes.
        // Schema 7 -> 8: FEN text keys become Zobrist keys, with a backup and identical results.
        {
            const auto v7Path = dir.path() + "/v7.sqlite";
            db.backup(v7Path);
            const auto variant = QString::fromStdString(annotated.nodes[vn].fen);
            const auto start = QString::fromStdString(std::string(chess::constants::STARTPOS));
            const auto searchBefore = db.search({}, {}, false, variant).size();
            const auto explorerBefore = db.explorer(start);
            downgradePositionsToV7(v7Path);
            const auto pawnBefore =
                db.search({}, {}, false, variant, false, {{"pawnStructure", true}}).size();
            const auto legacySize = QFileInfo(v7Path).size();
            {
                t64::Database migrated(v7Path);
                check(migrated.search({}, {}, false, variant).size() == searchBefore && searchBefore > 0,
                      "Migration 8 keeps variation position search");
                check(migrated.explorer(start) == explorerBefore && !explorerBefore.empty(),
                      "Migration 8 keeps opening statistics exactly");
                check(migrated.integrity() == "ok", "Migration 8 integrity");
                check(migrated.search({}, {}, false, variant, false, {{"pawnStructure", true}}).size() ==
                              pawnBefore &&
                          pawnBefore >= searchBefore,
                      "Migration 8 computes pawn structure keys");
                auto copy = migrated.load(migrated.search().first().toMap()["id"].toLongLong());
                copy.tags["Event"] = "After migration";
                migrated.save(copy);
                check(migrated.explorer(start).first().toMap()["count"].toInt() ==
                          explorerBefore.first().toMap()["count"].toInt() + 1,
                      "Opening statistics triggers work on hashed keys");
            }
            check(!QDir(dir.path()).entryList({"v7.sqlite.before-v8-*.sqlite"}).empty(),
                  "Backup before migration 8");
            QSqlDatabase::removeDatabase("v8-check");
            {
                auto v8 = QSqlDatabase::addDatabase("QSQLITE", "v8-check");
                v8.setDatabaseName(v7Path);
                check(v8.open(), "Open migrated library");
                QSqlQuery q(v8);
                check(q.exec("PRAGMA user_version") && q.next() && q.value(0).toInt() == 18,
                      "Schema version 15");
                // La marque qui distingue un recueil d'études d'une base téléchargée.
                check(q.exec("SELECT kind FROM sources LIMIT 1"), "Sources carry their kind");
                check(q.exec("SELECT typeof(key) FROM positions LIMIT 1") && q.next() &&
                          q.value(0).toString() == "integer",
                      "Positions are stored as integers");
                q.finish();
                v8.close();
            }
            QSqlDatabase::removeDatabase("v8-check");
            std::cout << "v7 fixture " << legacySize << " bytes, v8 " << QFileInfo(v7Path).size()
                      << " bytes\n";
        }
        {
            // Same pawns, different pieces: only the structure search links the two games.
            t64::Database pawns(dir.path() + "/pawns.sqlite");
            for (const auto *pgn : {"[Event \"A\"]\n\n1. e4 e5 2. Nf3 Nc6 *",
                                    "[Event \"B\"]\n\n1. e4 e5 2. Nc3 Nf6 *", "[Event \"C\"]\n\n1. d4 d5 *"})
                pawns.save(t64::parsePgn(pgn).front());
            const auto target =
                QString::fromStdString(t64::parsePgn("1. e4 e5 2. Nf3 Nc6 *").front().nodes.back().fen);
            check(pawns.search({}, {}, false, target).size() == 1 &&
                      pawns.search({}, {}, false, target, false, {{"pawnStructure", true}}).size() == 2,
                  "Pawn structure search finds games with other piece placements");
        }
        {
            t64::Database scout(dir.path() + "/dossier.sqlite");
            const char *games[] = {"[White \"Kim\"]\n[Black \"A\"]\n[Result \"1-0\"]\n[WhiteElo "
                                   "\"1800\"]\n[Date \"2026.01.01\"]\n\n1. e4 c5 2. Nf3 d6 3. d4 cxd4 1-0",
                                   "[White \"Kim\"]\n[Black \"B\"]\n[Result \"0-1\"]\n[WhiteElo "
                                   "\"1850\"]\n[Date \"2026.02.01\"]\n\n1. e4 c5 2. Nf3 d6 3. d4 cxd4 0-1",
                                   "[White \"C\"]\n[Black \"Kim\"]\n[Result \"1-0\"]\n[BlackElo "
                                   "\"1900\"]\n[Date \"2026.03.01\"]\n\n1. d4 d5 2. c4 e6 1-0",
                                   "[White \"D\"]\n[Black \"Kim\"]\n[Result \"1-0\"]\n[BlackElo "
                                   "\"1880\"]\n[Date \"2026.04.01\"]\n\n1. d4 d5 2. c4 e6 1-0",
                                   "[White \"E\"]\n[Black \"Kim\"]\n[Result \"1-0\"]\n[BlackElo "
                                   "\"1870\"]\n[Date \"2026.05.01\"]\n\n1. d4 d5 2. c4 e6 1-0"};
            for (const auto *pgn : games) {
                auto game = t64::parsePgn(pgn).front();
                game.tags["ECO"] = game.nodes[1].uci == "e2e4" ? "B54" : "D30";
                scout.save(game);
            }
            const auto kim = scout.player("Kim");
            check(kim["links"].toMap().isEmpty(), "A player seen only in local games has no online links");
            scout.save(
                t64::parsePgn("[Site \"https://lichess.org/abcd1234\"]\n[White \"Kim\"]\n[Black \"F\"]\n"
                              "[WhiteFideId \"12345678\"]\n[Result \"*\"]\n\n1. e4 *")
                    .front());
            const auto links = scout.player("Kim")["links"].toMap();
            check(links["lichess"] == "https://lichess.org/@/Kim" && !links.contains("chesscom") &&
                      links["fide"] == "https://ratings.fide.com/profile/12345678",
                  "A Lichess game gives the account link, a FIDE number the FIDE profile");
            // Le style de Kim : ses cinq parties finies sont décisives ; la seule prise (cxd4)
            // est de l'adversaire, aucun roque.
            const auto style = scout.player("Kim")["style"].toMap();
            check(style["averageMoves"].toInt() > 0 && style["decisive"].toInt() == 100 &&
                      style["captures"].toDouble() == 0 && style["oppositeCastling"].toInt() == 0,
                  "A player's style counts decisive games and castling sides");
            // Le bilan d'une sélection : groupé par année, résultat et tranche d'Elo moyen.
            qint64 counted = 0, kimFirst = 0;
            for (const auto &row : scout.search("Kim", {}, false, {}, false, {{"summary", true}})) {
                const auto r = row.toMap();
                counted += r["count"].toLongLong();
                if (r["year"] == "2026" && r["result"] == "1-0" && r["elo"].toInt() == 0)
                    kimFirst += r["count"].toLongLong();
            }
            check(counted == 6 && kimFirst == 4,
                  "A selection summary counts every matching game by year and result");
            // Une manœuvre : le cavalier g1-f3-d4-f3, d'autres coups entre les siens.
            scout.save(t64::parsePgn("[White \"Man\"]\n[Black \"Oeuvre\"]\n[Result \"*\"]\n\n"
                                     "1. Nf3 d5 2. g3 e5 3. Nd4 e4 4. Nf3 *")
                           .front());
            const auto maneuver = [&](const QString &piece, const QStringList &squares) {
                return scout
                    .search({}, {}, false, {}, false, {{"maneuverPiece", piece}, {"maneuverSteps", squares}})
                    .size();
            };
            check(maneuver("N", {"g1", "f3", "d4", "f3"}) == 1 && maneuver("", {"f3", "d4"}) == 1,
                  "A knight manoeuvre is found across other moves");
            check(maneuver("N", {"f3", "d4", "e6"}) == 0 && maneuver("B", {"f3", "d4"}) == 0 &&
                      maneuver("P", {"d7", "d5"}) == 4 && maneuver("P", {"d2", "d4"}) == 5,
                  "The piece and every square must match, pawns included");
            // Les médailles : un bit par médaille, filtrées comme le reste.
            const auto manoeuvre = scout.search("Oeuvre").first().toMap()["id"].toLongLong();
            scout.setMedals(manoeuvre, (1 << 3) | (1 << 8));
            const auto medalled = scout.search({}, {}, false, {}, false, {{"medal", 1 << 8}});
            check(medalled.size() == 1 &&
                      medalled.first().toMap()["medals"].toInt() == ((1 << 3) | (1 << 8)) &&
                      scout.search({}, {}, false, {}, false, {{"medal", 1}}).isEmpty(),
                  "Medals are kept per game and filter the library");
            // Les finales atteintes : une finale de tours (pions compris), puis une tour
            // contre un cavalier après l'échange d'une tour.
            scout.save(t64::parsePgn("[White \"Fin\"]\n[Black \"Ale\"]\n[SetUp \"1\"]\n"
                                     "[FEN \"r3k3/p7/8/8/8/8/8/R3K1N1 w - - 0 1\"]\n[Result \"0-1\"]\n\n"
                                     "1. Rxa7 Rxa7 0-1")
                           .front());
            const auto endgame = [&](const QString &kind) {
                return scout.search({}, {}, false, {}, false, {{"endgame", kind}}).size();
            };
            check(endgame("rookMinor") == 1 && endgame("rookMinorRook") == 1 && endgame("rooks") == 0 &&
                      endgame("queens") == 0,
                  "Endgames are classified by the material each side keeps, either way round");
            // Les probabilités de finale : ce cavalier contre une tour, perdu une fois.
            const auto odds = scout.materialResults(
                t64::Database::materialCode(std::string_view("4k3/r7/8/8/8/8/8/4K1N1")));
            check(odds["games"].toInt() == 1 && odds["black"].toInt() == 1,
                  "The endgame odds count the games that reached this material, by result");
            check(odds["byElo"].toList().size() == 3 &&
                      odds["byElo"].toList()[1].toMap()["games"].toInt() == 0,
                  "Games without ratings stay out of the trend by rating gap");
            // L'explorateur de plans, sur la structure d4-d5 : les manœuvres de chaque camp tant
            // qu'elle tient, puis la rupture qui la change (2.c4 dans trois parties, 3...e6 ici).
            scout.save(t64::parsePgn("[White \"Pla\"]\n[Black \"Nes\"]\n[Result \"1-0\"]\n\n"
                                     "1. d4 d5 2. Nf3 Nf6 3. Bf4 e6 4. e3 1-0")
                           .front());
            const auto plans = scout.plans("rnbqkbnr/ppp1pppp/8/3p4/3P4/8/PPP1PPPP/RNBQKBNR w KQkq - 0 2");
            const auto planWhite = plans["white"].toList(), planBlack = plans["black"].toList();
            const auto row = [](const QVariantList &rows, const QString &move) {
                for (const auto &r : rows)
                    if (r.toMap()["move"] == move)
                        return r.toMap();
                return QVariantMap{};
            };
            check(
                plans["games"].toInt() == 4 && planWhite.first().toMap()["move"] == "c4" &&
                    planWhite.first().toMap()["rupture"].toBool() &&
                    row(planWhite, "Bf4")["games"].toInt() == 1 &&
                    row(planWhite, "Bf4")["score"].toInt() == 100 &&
                    row(planBlack, "Nf6")["games"].toInt() == 1 && row(planBlack, "e6")["rupture"].toBool() &&
                    row(planWhite, "e3").isEmpty(),
                "The plan explorer counts each side's manoeuvres while the structure holds, then its break");
            // Les positions semblables : 3.Bg5 au lieu de 3.Bf4, un fou déplacé, soit deux cases.
            {
                const auto similar = scout.similarPositions(
                    "rnbqkb1r/ppp1pppp/5n2/3p2B1/3P4/5N2/PPP1PPPP/RN1QKB1R b KQkq - 3 3");
                bool near = false, sorted = true;
                for (int i = 0; i < similar.size(); ++i) {
                    const auto r = similar[i].toMap();
                    near = near || (r["white"] == "Pla" && r["distance"].toInt() == 2 &&
                                    r["fen"].toString().startsWith("rnbqkb1r/ppp1pppp/5n2/3p4/3P1B2/"));
                    sorted = sorted &&
                             (i == 0 || similar[i - 1].toMap()["distance"].toInt() <= r["distance"].toInt());
                }
                check(near && sorted, "Similar positions find the bishop on another square, closest first");
                check(scout.similarPositions("4k3/8/8/8/8/8/8/4K2R w - - 0 1").isEmpty(),
                      "A material no game reached finds nothing");
            }
            const auto white = kim["whiteLines"].toList(), black = kim["blackLines"].toList();
            check(white.size() == 1 && white[0].toMap()["line"] == "1. e4 c5 2. Nf3 d6 3. d4 cxd4" &&
                      white[0].toMap()["count"] == 2 && white[0].toMap()["score"] == 50,
                  "Dossier groups the player's white lines with their score");
            check(black.size() == 1 && black[0].toMap()["line"] == "1. d4 d5 2. c4 e6" &&
                      black[0].toMap()["score"] == 0,
                  "Dossier separates black lines");
            const auto weakest = kim["weakest"].toList();
            check(!weakest.isEmpty() && weakest[0].toMap()["label"] == "Noirs · D30" &&
                      weakest[0].toMap()["score"] == 0,
                  "Weakest opening needs three finished games and comes first");
            const auto phases = kim["phases"].toList();
            check(phases.size() == 3 && phases[0].toMap()["wins"] == 1 && phases[0].toMap()["losses"] == 4,
                  "Results are split by game length");
            const auto elo = kim["eloHistory"].toList();
            check(elo.size() == 5 && elo.first().toMap()["elo"] == 1800 && elo.last().toMap()["elo"] == 1870,
                  "Rating history is oldest first");
        }
        {
            t64::Database theory(dir.path() + "/novelty.sqlite");
            theory.save(t64::parsePgn("1. e4 e5 2. Nf3 Nc6 *").front());
            theory.save(t64::parsePgn("1. e4 e5 2. Nf3 Nf6 *").front());
            const auto study = t64::parsePgn("1. e4 e5 2. Nf3 d6 3. d4 *").front();
            auto found = theory.novelty(study);
            const auto known = found["known"].toList();
            check(found["found"].toBool() && found["ply"] == 3 && found["san"] == "d6" &&
                      found["reached"] == 2 && known.size() == 2,
                  "Novelty: first move never played from a known position, with the known replies");
            const auto saved = theory.save(study);
            found = theory.novelty(theory.load(saved), saved);
            check(found["found"].toBool() && found["san"] == "d6",
                  "A saved game does not count as its own precedent");
            check(theory.novelty(t64::parsePgn("1. e4 e5 *").front())["reason"] == "known",
                  "Fully known line has no novelty");
            theory.save(t64::parsePgn("1. e4 e5 2. Nf3 Nc6 3. Bc4 Bc5 4. O-O Nf6 *").front());
            theory.save(t64::parsePgn("1. e4 e5 2. Nf3 Nc6 3. Bc4 Bc5 4. O-O d6 *").front());
            const auto bookPath = dir.path() + "/library.bin";
            check(theory.exportPolyglot(bookPath, 2) > 0, "Library exports a Polyglot book");
            t64::PolyglotBook book;
            check(book.open(bookPath.toStdString()), "Exported book opens");
            const auto first = book.moves(chess::Board());
            check(first.size() == 1 && chess::uci::moveToUci(first[0].move) == "e2e4" && first[0].weight == 5,
                  "Book weights are the number of games");
            chess::Board castle("r1bqk1nr/pppp1ppp/2n5/2b1p3/2B1P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 4 4");
            const auto castling = book.moves(castle);
            check(castling.size() == 1 && castling[0].move.typeOf() == chess::Move::CASTLING,
                  "Castling is written the Polyglot way and reads back as castling");
            const auto sparse = theory.exportPolyglot(dir.path() + "/sparse.bin", 3);
            check(sparse > 0 && sparse < theory.exportPolyglot(dir.path() + "/all.bin", 1),
                  "Rare moves can be left out");
            check(theory.novelty(t64::parsePgn("1. a3 *").front())["ply"] == 0,
                  "A first move can be the novelty");
            check(theory.novelty(
                      t64::parsePgn("[SetUp \"1\"]\n[FEN \"8/8/8/8/8/8/6k1/4K2R w K - 0 1\"]\n\n1. Ke2 *")
                          .front())["reason"] == "unknownStart",
                  "An unknown starting position is reported as such");
        }
        // Une longue partie traverse la migration : son milieu de jeu, indexé position par
        // position dans l'ancien schéma, doit rester trouvable par signature dans le nouveau.
        t64::Game marathon;
        marathon.tags["White"] = "Marathon";
        for (int i = 0, node = 0; i < 90; ++i) {
            chess::Movelist moves;
            chess::movegen::legalmoves(moves, marathon.boardOf(node));
            if (moves.empty())
                break;
            node = marathon.play(node, chess::uci::moveToUci(moves[(i * 5) % moves.size()]));
        }
        const auto marathonLine = marathon.mainline();
        const auto marathonId = db.save(marathon);
        const auto marathonLate = QString::fromStdString(marathon.nodes[marathonLine[70]].fen);
        const auto treeBefore = db.explorer(QString::fromStdString(chess::constants::STARTPOS));
        const auto oldPath = dir.path() + "/old.sqlite";
        db.backup(oldPath);
        downgradePositionsToV7(oldPath, "old-v7");
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
                 QStringList{"analysis_results", "analysis_jobs", "analysis_cache", "repertoire_reviews",
                             "repertoire_moves", "repertoire_positions", "repertoires", "import_jobs",
                             "source_games", "sources", "import_history", "games_fts", "library_counts",
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
            check(migrated.stats()["games"].toInt() == 5, "Migration preserves games");
            const auto ids = [&](const QVariantList &rows) {
                QSet<qint64> out;
                for (const auto &row : rows)
                    out.insert(row.toMap()["id"].toLongLong());
                return out;
            };
            check(ids(migrated.search({}, {}, false, marathonLate)) == QSet<qint64>{marathonId},
                  "Migration keeps a middlegame position findable without its index row");
            check(ids(migrated.search({}, {}, false, marathonLate, false, {{"pawnStructure", true}}))
                          .contains(marathonId) &&
                      ids(migrated.search({}, {}, false, {}, false,
                                          {{"material", marathonLate.section(' ', 0, 0)}}))
                          .contains(marathonId),
                  "Migration carries material and pawn structure into the signatures");
            check(migrated.explorer(QString::fromStdString(chess::constants::STARTPOS)) == treeBefore,
                  "Migration rebuilds the same opening tree");
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
        {
            // Un compte en ligne mêle souvent des variantes : une partie de Crazyhouse au
            // milieu ne doit pas faire échouer l'import des autres.
            const auto mixed = dir.path() + "/mixed.pgn";
            QFile file(mixed);
            check(file.open(QIODevice::WriteOnly), "Open mixed-variant fixture");
            file.write(
                "[Event \"Rated blitz\"]\n[White \"Simzy\"]\n[Black \"B\"]\n1.e4 e5 2.Nf3 Nc6 1-0\n\n"
                "[Event \"Rated crazyhouse\"]\n[Variant \"Crazyhouse\"]\n[White \"Simzy\"]\n[Black \"C\"]\n"
                "1.e4 d5 2.exd5 Qxd5 3.Nc3 Qd8 4.N@f3 *\n\n"
                "[Event \"Rated rapid\"]\n[White \"D\"]\n[Black \"Simzy\"]\n1.d4 Nf6 2.c4 e6 0-1\n");
            file.close();
            int skipped = -1;
            const int before = db.stats()["games"].toInt();
            check(db.importPgnFile(
                      mixed, [](int) { return true; }, {}, &skipped) == 2 &&
                      skipped == 1 && db.stats()["games"].toInt() == before + 2,
                  "An unsupported variant is set aside instead of aborting the whole import");
            QFile only(dir.path() + "/variant-only.pgn");
            check(only.open(QIODevice::WriteOnly), "Open variant-only fixture");
            only.write("[Variant \"Atomic\"]\n1.e4 e5 *\n");
            only.close();
            bool told = false;
            try {
                db.importPgnFile(only.fileName(), [](int) { return true; });
            } catch (const std::exception &e) {
                told = QString::fromUtf8(e.what()).contains("variantes");
            }
            check(told, "A file of variants only says why nothing was imported");
        }
        {
            // Le répertoire d'un adversaire : ses lignes régulières et ses choix par année.
            check(
                t64::Database::openingMoves(
                    "[Event \"x\"]\n1. e4 {[%clk 0:03:00]} e5 (1... c5 2. Nf3) 2. Nf3!? $1 Nc6 3. Bb5 a6 1-0",
                    4) == QStringList({"e4", "e5", "Nf3", "Nc6"}),
                "Opening moves skip tags, comments, variations, numbers and NAGs");
            t64::Database rep(dir.path() + "/repertoire-player.sqlite");
            const auto file = dir.path() + "/repertoire-player.pgn";
            QFile out(file);
            check(out.open(QIODevice::WriteOnly), "Open opponent fixture");
            out.write(
                "[White \"Opp\"]\n[Black \"A\"]\n[Date \"2023.01.01\"]\n[Result \"1-0\"]\n1.e4 e5 2.Nf3 Nc6 "
                "1-0\n\n"
                "[White \"Opp\"]\n[Black \"B\"]\n[Date \"2024.01.01\"]\n[Result \"1/2-1/2\"]\n1.e4 e5 2.Nf3 "
                "Nc6 1/2-1/2\n\n"
                "[White \"Opp\"]\n[Black \"C\"]\n[Date \"2025.01.01\"]\n[Result \"0-1\"]\n1.d4 d5 0-1\n\n"
                "[White \"D\"]\n[Black \"Opp\"]\n[Date \"2025.02.01\"]\n[Result \"0-1\"]\n1.e4 c5 2.Nf3 d6 "
                "0-1\n");
            out.close();
            rep.importPgnFile(file, [](int) { return true; });
            const auto r = rep.playerRepertoire("Opp", 4);
            const auto white = r["white"].toList(), black = r["black"].toList(), years = r["years"].toList();
            check(white.size() == 2 && white[0].toMap()["line"] == "1.e4 e5 2.Nf3 Nc6" &&
                      white[0].toMap()["games"] == 2 && white[0].toMap()["score"] == 75 &&
                      black.size() == 1 && black[0].toMap()["line"] == "1.e4 c5 2.Nf3 d6" &&
                      black[0].toMap()["score"] == 100,
                  "An opponent's regular lines come with their frequency and score");
            check(years.size() == 3 && years[2].toMap()["year"] == "2025" &&
                      years[2].toMap()["white"].toMap()["move"] == "1.d4" &&
                      years[2].toMap()["black"].toMap()["move"] == "1.e4 c5" &&
                      years[0].toMap()["white"].toMap()["move"] == "1.e4",
                  "An opponent's choices are followed year by year");
        }
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
        // Index allégé : quarante demi-coups indexés, le reste retrouvé par signature.
        {
            const auto path = dir.path() + "/light.sqlite";
            t64::Database light(path);
            // Une partie de cent demi-coups, toujours la même : le k-ième coup légal.
            const auto longGame = [](int stride, const char *white) {
                t64::Game g;
                g.tags["White"] = white;
                int node = 0;
                for (int i = 0; i < 100; ++i) {
                    chess::Movelist moves;
                    chess::movegen::legalmoves(moves, g.boardOf(node));
                    if (moves.empty())
                        break;
                    node = g.play(node, chess::uci::moveToUci(moves[(i * stride) % moves.size()]));
                }
                return g;
            };
            auto g = longGame(7, "Long");
            const auto line = g.mainline();
            check(line.size() > 80, "The fixture game is long enough to cross the index horizon");
            // Deux variantes : l'une tôt, dans l'index ; l'autre bien après l'horizon.
            const auto branch = [&](int ply) {
                chess::Movelist moves;
                chess::movegen::legalmoves(moves, g.boardOf(line[ply]));
                for (const auto m : moves)
                    if (chess::uci::moveToUci(m) != g.nodes[line[ply + 1]].uci)
                        return g.play(line[ply], chess::uci::moveToUci(m));
                return -1;
            };
            const int earlySide = branch(5), lateSide = branch(60);
            const auto id = light.save(g);
            light.save(t64::parsePgn("[White \"Short\"]\n1.e4 e5 2.Nf3 *").at(0));
            const auto fen = [&](int node) { return QString::fromStdString(g.nodes[node].fen); };
            const auto found = [&](const QString &position, const QVariantMap &filters = {}) {
                QSet<qint64> ids;
                for (const auto &row : light.search({}, {}, false, position, false, filters))
                    ids.insert(row.toMap()["id"].toLongLong());
                return ids;
            };
            {
                auto raw = QSqlDatabase::addDatabase("QSQLITE", "light-raw");
                raw.setDatabaseName(path);
                check(raw.open(), "Open light index verification connection");
                QSqlQuery q(raw);
                q.prepare("SELECT COUNT(*) FROM positions WHERE game_id=?");
                q.addBindValue(id);
                check(q.exec() && q.next() && q.value(0).toInt() == t64::Database::indexedPlies,
                      "Only the first plies of a game are indexed position by position");
                q.finish();
                raw.close();
            }
            QSqlDatabase::removeDatabase("light-raw");
            check(found(fen(line[10])) == QSet<qint64>{id}, "An opening position is found by the index");
            check(found(fen(line[75])) == QSet<qint64>{id},
                  "A position past the horizon is found by replaying the games that share its signature");
            check(found(fen(earlySide)) == QSet<qint64>{id} && found(fen(lateSide)) == QSet<qint64>{id},
                  "Variations are found on both sides of the horizon");
            check(found(fen(line[75]), {{"pawnStructure", true}}).contains(id),
                  "Pawn structure search covers the whole game");
            check(found({}, {{"material", fen(line[90]).section(' ', 0, 0)}}).contains(id),
                  "Material search covers the whole game");
            {
                // Placement partiel : trois pièces prises telles qu'elles sont au 75e demi-coup.
                const auto pieces = [](const QString &position, int count) {
                    QStringList tokens;
                    int square = 0;
                    for (const QChar c : position.section(' ', 0, 0)) {
                        if (c == '/')
                            continue;
                        if (c.isDigit()) {
                            square += c.digitValue();
                            continue;
                        }
                        if (tokens.size() < count)
                            tokens
                                << QString("%1%2%3").arg(c).arg(QChar('a' + square % 8)).arg(8 - square / 8);
                        ++square;
                    }
                    return tokens.join(' ');
                };
                const auto late = pieces(fen(line[75]), 3);
                check(found({}, {{"placement", late}}).contains(id),
                      "A partial placement is found wherever it occurs in the game");
                check(found({}, {{"placement", "Ka8 ka1 Qh5 qh4 Pa7 pa2"}}).isEmpty(),
                      "A placement no game ever reached finds nothing");
                // CQL : la même position décrite autrement, puis une requête refusée.
                check(found({}, {{"cql", late}}).contains(id) &&
                          found({}, {{"cql", "Ka8 ka1 Qh5 qh4"}}).isEmpty(),
                      "A CQL query finds the games where one position matches");
                bool cqlRefused = false;
                try {
                    found({}, {{"cql", "sequence {mate}"}});
                } catch (const std::exception &e) {
                    cqlRefused = QString::fromUtf8(e.what()).startsWith("Requête CQL");
                }
                check(cqlRefused, "An unreadable CQL query says so");
                const auto wanted = t64::Database::parsePlacement("Ke1, pd5");
                check(t64::Database::placementMatches(wanted, "4k3/8/8/3p4/8/8/8/4K3 w - - 0 1") &&
                          !t64::Database::placementMatches(wanted, "4k3/8/8/3P4/8/8/8/4K3 w - - 0 1") &&
                          !t64::Database::placementMatches(wanted, "4k3/8/8/3p4/8/8/8/3K4 w - - 0 1"),
                      "A placement asks for that piece, of that colour, on that square");
                for (const char *bad : {"", "Ke9", "Xe1", "e1", "Ke1 Qe1"}) {
                    bool refused = false;
                    try {
                        t64::Database::parsePlacement(bad);
                    } catch (const std::exception &) {
                        refused = true;
                    }
                    check(refused, "An unreadable placement is refused, not guessed");
                }
            }
            // Même matériel, mêmes pions, autre position : la signature propose, le rejeu écarte.
            auto other = longGame(11, "Other");
            const auto otherLine = other.mainline();
            check(found(QString::fromStdString(other.nodes[otherLine[75]].fen)).isEmpty(),
                  "A position no game reached is not found");
            // Une partie ajoutée après coup doit sortir à la recherche suivante.
            auto twin = g;
            twin.tags["White"] = "Twin";
            const auto twinId = light.save(twin);
            check(found(fen(line[75])) == QSet<qint64>({id, twinId}),
                  "A later search sees games saved since the previous one");
            check(light.novelty(g, id)["reason"] == "known",
                  "Past the indexed plies a line is not reported as leaving the library");
            // Rejouer la partie raccourcie retire ses anciennes signatures.
            auto cut = t64::parsePgn("[White \"Twin\"]\n1.d4 d5 *").at(0);
            light.save(cut, twinId);
            check(found(fen(line[75])) == QSet<qint64>{id} && found(fen(line[10])) == QSet<qint64>{id},
                  "Editing a game removes it from the positions it no longer reaches");
            check(light.integrity() == "ok", "Light index integrity");
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
            {
                // Une étude importée sous une étiquette : ses « joueurs » (des chapitres)
                // restent hors de l'onglet Joueurs, comme les noms de remplissage ; ses
                // parties portent l'étiquette comme collection.
                t64::Database shelf(dir.path() + "/labels.sqlite");
                const auto course = dir.path() + "/course.pgn";
                QFile lesson(course);
                check(lesson.open(QIODevice::WriteOnly), "Course fixture");
                lesson.write("[White \"Chapitre 1 : les finales\"]\n[Black \"?\"]\n1.e4 *\n\n"
                             "[White \"Blancs\"]\n[Black \"Noirs\"]\n1.d4 *\n");
                lesson.close();
                QFile played(dir.path() + "/played.pgn");
                check(played.open(QIODevice::WriteOnly), "Played fixture");
                played.write("[White \"Carlsen, Magnus\"]\n[Black \"Chapitre 1 : les finales\"]\n1.c4 *\n");
                played.close();
                shelf.importPgnFile(course, [](int) { return true; },
                                    {{"id", "import:cours"},
                                     {"title", "Cours"},
                                     {"kind", "import"},
                                     {"purpose", "study"},
                                     {"collection", "Cours"}});
                shelf.importPgnFile(dir.path() + "/played.pgn", [](int) { return true; });
                QStringList names;
                for (const auto &p : shelf.players())
                    names << p.toMap()["name"].toString() + "=" + p.toMap()["games"].toString();
                names.sort();
                check(names == QStringList{"Carlsen, Magnus=1", "Chapitre 1 : les finales=1"},
                      "Study names and placeholders stay out of the players; a real game still counts");
                check(shelf.search({}, {}, false, {}, false, {{"collection", "Cours"}}).size() == 2 ||
                          shelf.search("Chapitre").first().toMap()["collection"] == "Cours",
                      "Imported games carry their label as collection");
                const auto courseId = shelf.sources().first().toMap()["id"].toLongLong();
                check(shelf.sources().first().toMap()["purpose"] == "study", "The source keeps its purpose");
                shelf.setSourcePurpose(courseId, "games");
                bool counted = false;
                for (const auto &p : shelf.players())
                    counted = counted ||
                              (p.toMap()["name"] == "Chapitre 1 : les finales" && p.toMap()["games"] == 2);
                check(counted, "Marked as played games, the collection's names count again");
                // Relu sans usage donné, le recueil garde le sien ; importé de nouveau avec un
                // usage explicite, il prend celui-là.
                shelf.importPgnFile(course, [](int) { return true; },
                                    {{"id", "import:cours"}, {"title", "Cours"}, {"kind", "import"}});
                check(shelf.sources().first().toMap()["purpose"] == "games",
                      "Re-reading keeps the chosen purpose");
                shelf.importPgnFile(
                    course, [](int) { return true; },
                    {{"id", "import:cours"}, {"title", "Cours"}, {"kind", "import"}, {"purpose", "study"}});
                check(shelf.sources().first().toMap()["purpose"] == "study",
                      "An explicit purpose on import wins");
                shelf.setSourcePurpose(courseId, "games");
                check(shelf.removeSource(courseId) == 2 && shelf.sources().isEmpty() &&
                          shelf.stats()["games"].toInt() == 1,
                      "Removing a collection trashes its own games and forgets it");
            }
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
        {
            t64::Database pages(dir.path() + "/pages.sqlite");
            for (int i = 0; i < 47; ++i) {
                t64::Game g;
                g.tags["Event"] = "Page fixture " + std::to_string(i);
                g.tags["White"] = i % 3 ? "Alice" : "Bob";
                g.tags["Black"] = i % 4 ? "Rival" : "Opponent";
                g.tags["Date"] = i % 2 ? "2026.10.10" : "2025.01.01";
                if (i % 5 == 0)
                    for (const auto *name : {"White", "Black", "Date"})
                        g.tags.erase(name);
                g.tags["WhiteElo"] = std::to_string(1800 + 100 * (i % 4));
                g.tags["BlackElo"] = "2000";
                pages.save(g);
            }
            {
                auto writer = QSqlDatabase::addDatabase("QSQLITE", "nullable-sort-fixture");
                writer.setDatabaseName(pages.path());
                check(writer.open(), "Open nullable metadata fixture");
                {
                    QSqlQuery q(writer);
                    check(q.exec("UPDATE games SET white_elo=NULL,black_elo=NULL,plies=NULL WHERE id%11=0"),
                          "Legacy null rating and length fixture");
                    check(q.exec("UPDATE games SET updated='2025-01-01T00:00:00Z' WHERE id<=25"),
                          "Cursor pages cross groups of equal timestamps");
                }
                writer.close();
            }
            QSqlDatabase::removeDatabase("nullable-sort-fixture");
            for (const auto &sort : QStringList{"updated", "date", "players", "elo", "length", "white",
                                                "black", "whiteElo", "blackElo", "result", "eco", "event"})
                for (const bool reverse : {false, true}) {
                    const auto expected =
                        pages.search({}, {}, false, {}, false, {{"sort", sort}, {"reverse", reverse}});
                    QVariantList actual;
                    QVariantMap filters{{"sort", sort}, {"reverse", reverse}, {"limit", 3}};
                    for (int page = 0; page < 30; ++page) {
                        auto rows = pages.search({}, {}, false, {}, false, filters);
                        if (rows.empty())
                            break;
                        actual += rows;
                        filters["cursor"] = rows.last().toMap();
                    }
                    check(actual == expected && actual.size() == 47,
                          "Cursor pages preserve every row, ties and nulls for every sort, both ways");
                }
            {
                const auto byWhite = pages.search({}, {}, false, {}, false, {{"sort", "white"}});
                const auto backwards =
                    pages.search({}, {}, false, {}, false, {{"sort", "white"}, {"reverse", true}});
                QVariantList flipped(backwards.crbegin(), backwards.crend());
                check(byWhite.first().toMap()["white"].toString() <=
                              byWhite.last().toMap()["white"].toString() &&
                          flipped == byWhite,
                      "Sorting by White goes A to Z, and the second click is its exact mirror");
            }
            rejects([&] { pages.search({}, {}, false, {}, false, {{"cursor", QVariantMap{{"id", 1}}}}); });
            auto canceled = std::make_shared<std::atomic_bool>(true);
            rejects([&] { pages.search({}, {}, false, {}, false, {}, canceled); });
            check(pages.search().size() == 47, "Canceled search leaves connection usable");
        }
        {
            // Virtual repetition makes the query expensive without storing millions
            // of games or relying on machine speed to hit the cancellation window.
            t64::Database slow(dir.path() + "/cancel.sqlite");
            slow.save(t64::Game());
            bool oneSqlite = false;
            for (const auto &name : QSqlDatabase::connectionNames()) {
                const auto candidate = QSqlDatabase::database(name);
                if (candidate.databaseName() == slow.path())
                    oneSqlite = t64::QueryCancellation::sameLibrary(candidate);
            }
            {
                auto writer = QSqlDatabase::addDatabase("QSQLITE", "slow-query-fixture");
                writer.setDatabaseName(slow.path());
                check(writer.open(), "Open cancellation fixture");
                {
                    QSqlQuery q(writer);
                    check(q.exec("ALTER TABLE games RENAME TO seed_games"), "Rename cancellation seed");
                    check(q.exec("CREATE VIEW games AS WITH RECURSIVE n(value) AS (SELECT 1 UNION ALL "
                                 "SELECT value+1 FROM n WHERE value<100000000) "
                                 "SELECT g.* FROM n CROSS JOIN seed_games g"),
                          "Create virtual slow search");
                }
                writer.close();
            }
            QSqlDatabase::removeDatabase("slow-query-fixture");
            // L'annulation repose sur le gestionnaire de progression de SQLite, que l'on
            // ne pose pas quand le pilote Qt embarque sa propre copie de la bibliothèque.
            // Là où elle est indisponible, il n'y a rien à mesurer : la requête de ce
            // montage est volontairement très lente, et la mener au bout coûterait une
            // demi-minute pour ne vérifier qu'une fonction absente par construction.
            if (!oneSqlite) {
                std::cout << "Skipping search-cancellation checks: the Qt driver carries its own SQLite\n";
            } else {
                auto canceled = std::make_shared<std::atomic_bool>(false);
                QElapsedTimer timer;
                timer.start();
                std::thread canceler([canceled] {
                    std::this_thread::sleep_for(std::chrono::milliseconds(20));
                    canceled->store(true);
                });
                // std::jthread manque encore à la bibliothèque standard d'Apple : on joint
                // à la main, y compris si la vérification ci-dessous lève, pour ne pas
                // laisser un fil d'exécution vivant derrière soi.
                const auto join = qScopeGuard([&] { canceler.join(); });
                rejects([&] { slow.search({}, {}, false, {}, false, {}, canceled); });
                check(canceled->load() && timer.elapsed() < 3000,
                      "SQLite stops an executing search promptly");
            }
            check(slow.stats()["games"].toInt() == 1, "Progress callback removed after interruption");
        }
        {
            t64::Database prep(dir.path() + "/preparation.sqlite");
            auto games = t64::parsePgn("[White \"Rival\"] [Black \"Partner\"] [Date \"2020.01.01\"] "
                                       "[WhiteElo \"2000\"] [BlackElo \"2200\"]\n"
                                       "1.Nf3 Nf6 2.Ng1 Ng8 3.Nf3 1-0\n"
                                       "[White \"Alice\"] [Black \"Rival\"] [Date \"2024.01.01\"] [WhiteElo "
                                       "\"2300\"] [BlackElo \"2500\"]\n1.d4 0-1\n"
                                       "[White \"Rival\"] [Date \"????.??.??\"]\n1.e4 *\n"
                                       "[White \"Rival\"] [Black \"Other\"] [Date \"2024.02.01\"] [WhiteElo "
                                       "\"1900\"] [BlackElo \"2100\"]\n1.d4 0-1");
            prep.importGames(games);
            const auto start = QString::fromStdString(chess::constants::STARTPOS);
            auto all = prep.positionStatistics(start);
            check(all.size() == 3 && all.first().toMap()["uci"] == "d2d4" &&
                      all.first().toMap()["count"].toInt() == 2 &&
                      all.first().toMap()["averageElo"].toDouble() == 2200,
                  "Position statistics group by actual move with known average ratings");
            auto white = prep.positionStatistics(
                start, {{"player", "Rival"}, {"colour", "white"}, {"fromYear", 2023}, {"toYear", 2025}});
            check(white.size() == 1 && white.first().toMap()["averageElo"].toDouble() == 2000,
                  "Player, colour and period filters combine and exclude unknown years");
            check(prep.positionStatistics(
                      start,
                      {{"player", "rIVAL"}, {"colour", "white"}, {"fromYear", 2023}, {"toYear", 2025}}) ==
                      white,
                  "An online username matches whatever its capitals");
            auto rated = prep.positionStatistics(start, {{"minElo", 2200}, {"maxElo", 2500}});
            check(rated.size() == 1 && rated.first().toMap()["count"].toInt() == 1 &&
                      rated.first().toMap()["averageElo"].toDouble() == 2400,
                  "Rating filter excludes missing ratings without estimating them");
            auto repeat = prep.positionStatistics(start, {{"toYear", 2020}});
            check(repeat.size() == 1 && repeat.first().toMap()["count"].toInt() == 1 &&
                      repeat.first().toMap()["rated"].toInt() == 1,
                  "Repeated positions do not inflate game counts or rating weights");
            const auto file = dir.path() + "/prep-source.pgn";
            QFile f(file);
            check(f.open(QIODevice::WriteOnly), "Write position source fixture");
            f.write(QByteArray::fromStdString(games[1].pgn()));
            f.close();
            prep.importPgnFile(file, {}, {{"id", "prep"}, {"title", "Preparation"}});
            auto source = prep.sources().first().toMap()["id"];
            check(prep.positionStatistics(start, {{"sourceId", source}}).first().toMap()["count"].toInt() ==
                      1,
                  "Position statistics restrict source membership without duplicate games");
            check(prep.positionStatistics(start, {{"player", "Riv%"}}).empty(), "Player name match is exact");
            rejects([&] { prep.positionStatistics(start, {{"minElo", 2500}, {"maxElo", 2000}}); });
            rejects([&] { prep.positionStatistics(start, {{"fromYear", 2025}, {"toYear", 2020}}); });
            auto stop = std::make_shared<std::atomic_bool>(true);
            rejects([&] { prep.positionStatistics(start, {}, stop); });
            check(prep.positionStatistics(start).size() == 3,
                  "Canceled statistics do not leave a progress handler behind");
        }
        {
            // L'ordre des coups légaux de Shakmaty, sur lequel repose la lecture des
            // bases d'En Croissant. Le témoin vient de Shakmaty lui-même : 3 637
            // positions tirées de 750 000, toutes celles où l'ordre est délicat — prise
            // en passant, roque, promotion, échec. Un rang de travers ne ferait pas une
            // erreur mais un autre coup légal : seule la comparaison le voit.
            const auto fixture = [](const char *name) {
                QFile file(QString(T64_TEST_DATA) + "/" + name);
                check(file.open(QIODevice::ReadOnly), "Open a test fixture");
                return qUncompress(file.readAll());
            };
            int positions = 0;
            for (const auto &line : fixture("shakmaty_order.tsv.qz").split('\n')) {
                if (line.isEmpty())
                    continue;
                const auto parts = line.split('\t');
                const chess::Board board(parts[0].toStdString() + " 0 1");
                QByteArray order;
                for (const auto move : t64::movesInShakmatyOrder(board)) {
                    if (!order.isEmpty())
                        order += ' ';
                    order += QByteArray::fromStdString(chess::uci::moveToUci(move));
                }
                if (order != parts[1])
                    throw std::runtime_error("Move order differs from Shakmaty on " + parts[0].toStdString());
                ++positions;
            }
            check(positions == 3637, "Every reference position keeps Shakmaty's legal-move order");
            // Une base entière au format d'En Croissant, écrite avec Shakmaty : 300
            // parties avec variantes, commentaires et annotations.
            const auto base = dir.filePath("essai.db3");
            {
                QFile out(base);
                check(out.open(QIODevice::WriteOnly) && out.write(fixture("croissant_legacy.db3.qz")) > 0,
                      "Write the En Croissant fixture");
            }
            check(t64::croissant::isCroissantBase(base) && t64::croissant::games(base) == 300 &&
                      t64::croissant::title(base) == "Base d'essai",
                  "An En Croissant base is recognised, counted and named");
            check(!t64::croissant::isCroissantBase(dir.filePath("dedup.sqlite")) &&
                      !t64::croissant::isCroissantBase(dir.filePath("absent.db3")),
                  "Another SQLite file, or none, is not an En Croissant base");
            QBuffer pgn;
            pgn.open(QIODevice::WriteOnly);
            int reported = 0;
            const auto outcome =
                t64::croissant::exportPgn(base, pgn, [&](int done, int) { reported = done; });
            check(outcome.written == 300 && outcome.skipped == 0 && reported == 300,
                  "Every game of the base is read and reported");
            // La ligne principale de chaque partie, comparée à ce que Shakmaty a joué.
            QHash<int, QByteArray> expected;
            for (const auto &line : fixture("croissant_legacy.txt.qz").split('\n'))
                if (line.startsWith("GAME ")) {
                    const auto rest = line.mid(5);
                    const int space = rest.indexOf(' ');
                    expected.insert(rest.left(space).toInt(), rest.mid(space + 1));
                }
            static const QRegularExpression comment(R"x(\{[^}]*\})x"), variation(R"x(\([^()]*\))x"),
                number(R"x(\d+\.+)x"), nag(R"x(\$\d+)x"), spaces(R"x(\s+)x"),
                roundTag(R"x(\[Round "(\d+)"\])x"), resultTag(R"x(\[Result "([^"]+)"\])x");
            int compared = 0;
            const auto blocks = QString::fromUtf8(pgn.data()).split("\n\n", Qt::SkipEmptyParts);
            for (int i = 0; i + 1 < blocks.size(); i += 2) {
                const auto round = roundTag.match(blocks[i]).captured(1);
                const auto result = resultTag.match(blocks[i]).captured(1);
                auto text = blocks[i + 1];
                text.remove(comment).remove(variation).remove(number).remove(nag);
                text = text.replace(spaces, " ").trimmed();
                // Le témoin note « résultat coups », le PGN « coups résultat ».
                const auto reference = QString::fromUtf8(expected[round.toInt()]);
                const auto wanted =
                    (reference.section(' ', 1) + " " + reference.section(' ', 0, 0)).trimmed();
                if (text != wanted || result != reference.section(' ', 0, 0))
                    throw std::runtime_error("Decoded game differs from Shakmaty: " + round.toStdString());
                ++compared;
            }
            check(compared == 300, "The 300 main lines are the ones Shakmaty played");
            rejects([] {
                t64::croissant::movetext(QByteArray(1, char(250)), std::string(chess::constants::STARTPOS));
            });
            rejects([] {
                t64::croissant::movetext(QByteArray::fromHex("fd0500"),
                                         std::string(chess::constants::STARTPOS));
            });
            rejects([] {
                t64::croissant::movetext(QByteArray(1, char(254)), std::string(chess::constants::STARTPOS));
            });
            check(t64::croissant::movetext({}, std::string(chess::constants::STARTPOS)).empty(),
                  "An empty game reads as no move, and broken ones are refused");
        }
        {
            // Tableau croisé d'un toutes-rondes à quatre, calculé à la main :
            // A 2,5 · C 2 · B 1 · D ½ ; SB de A = 1 (B) + ½ × 2 (C) + ½ (D) = 2,5.
            QTemporaryDir place;
            t64::Database club(place.filePath("club.sqlite"));
            const auto play = [&](const char *white, const char *black, const char *result, int round,
                                  const char *date = "2026.01.10") {
                t64::Game g;
                g.tags["Event"] = "Open du club";
                g.tags["Date"] = date;
                g.tags["Round"] = std::to_string(round);
                g.tags["White"] = white;
                g.tags["Black"] = black;
                g.tags["Result"] = result;
                const std::map<std::string, int> elo{{"A", 2500}, {"B", 2400}, {"C", 2450}, {"D", 2300}};
                g.tags["WhiteElo"] = std::to_string(elo.at(white));
                g.tags["BlackElo"] = std::to_string(elo.at(black));
                // Des coups différents, sinon les doublons se fondent.
                g.play(0, round == 1 ? "e2e4" : round == 2 ? "d2d4" : "c2c4");
                g.nodes[1].comment = std::string(white) + black + std::to_string(round);
                return club.save(g);
            };
            const auto first = play("A", "B", "1-0", 1);
            play("C", "D", "1/2-1/2", 1);
            play("A", "C", "1/2-1/2", 2);
            play("B", "D", "1-0", 2);
            play("D", "A", "0-1", 3);
            play("B", "C", "0-1", 3);
            play("A", "D", "0-1", 1, "2025.01.10"); // l'édition précédente : à part
            const auto table = club.tournament(first);
            const auto rows = table["players"].toList();
            QStringList order;
            for (const auto &row : rows)
                order << row.toMap()["name"].toString();
            const auto a = rows.value(0).toMap();
            check(order == QStringList{"A", "C", "B", "D"} && a["score"].toDouble() == 2.5 &&
                      a["sonneborn"].toDouble() == 2.5 && a["games"].toInt() == 3 &&
                      a["performance"].toInt() == 2650,
                  "Crosstable ranks by points, Sonneborn-Berger and performance");
            check(table["matrix"].toBool() && table["rounds"].toInt() == 3 &&
                      a["cells"].toList() == QVariantList{"×", "½", "1", "1"} &&
                      a["rounds"].toList() == QVariantList{"3○1", "2○½", "4●1"} &&
                      a["cellGames"].toList().value(2).toList() == QVariantList{first} &&
                      table["event"] == "Open du club" && table["year"] == "2026",
                  "Crosstable cells, rounds and games, the same event in another year left out");
            t64::Game nameless;
            nameless.play(0, "g1f3");
            const auto lone = club.save(nameless);
            nameless.tags["Event"] = "?";
            nameless.play(0, "b1c3");
            const auto unknown = club.save(nameless);
            rejects([&] { club.tournament(unknown); });
            rejects([&] { club.tournament(lone); });
            // Les tournois de la bibliothèque : l'édition 2026, celle de 2025 à part.
            {
                const auto all = club.tournaments();
                QVariantMap edition;
                for (const auto &row : all)
                    if (row.toMap()["year"] == "2026")
                        edition = row.toMap();
                check(all.size() == 2 && all.first().toMap()["year"] == "2026" &&
                          edition["games"].toInt() == 6 && edition["players"].toInt() == 4 &&
                          edition["elo"].toInt() == 2412 &&
                          club.tournament(edition["gameId"].toLongLong())["players"].toList().size() == 4,
                      "Tournaments are listed by edition with games, players and average Elo, and open their "
                      "table");
                check(club.tournaments("CLUB").size() == 2 && club.tournaments("zzz").isEmpty(),
                      "Tournament names are searched without case");
            }
            // Recherche dans les commentaires et par symbole. Les parties du tournoi portent
            // un commentaire (« AB1 »…) ; on en annote deux de plus.
            t64::Game brilliant;
            brilliant.tags["White"] = "E";
            brilliant.play(brilliant.play(0, "e2e4"), "e7e5");
            brilliant.nodes[1].nags = {3};
            brilliant.nodes[2].comment = "Une attaque du roi, gagnée à 50% des parties";
            club.save(brilliant);
            t64::Game dubious;
            dubious.tags["White"] = "F";
            dubious.play(0, "f2f3");
            dubious.nodes[1].nags = {6, 30};
            club.save(dubious);
            const auto count = [&club](const QVariantMap &filters) {
                return club.search({}, {}, false, {}, false, filters).size();
            };
            check(count({{"comment", "attaque du ROI"}}) == 1 && count({{"comment", "50%"}}) == 1 &&
                      count({{"comment", "%"}}) == 1 && count({{"comment", "_"}}) == 0,
                  "Comment search ignores case and treats % and _ literally");
            check(count({{"nag", 3}}) == 1 && count({{"nag", 6}}) == 1 && count({{"nag", 30}}) == 1 &&
                      count({{"nag", 2}}) == 0 && count({{"annotated", true}}) == 8,
                  "Symbol search reads the NAG exactly ($3 is not $30), annotated games have a comment");
            // Correction des noms.
            check(t64::nameKey("Carlsen, Magnus", true) == "carlsen|m" &&
                      t64::nameKey("Carlsen,M.", true) == "carlsen|m" &&
                      t64::nameKey("Magnus  Carlsen", true) == "carlsen|m" &&
                      t64::nameKey("Firouzja, Alireza", true) == t64::nameKey("FIROUZJA, Alirézâ", true) &&
                      t64::nameKey("Carlsen, Magnus", true) != t64::nameKey("Carlsen, Henrik", true) &&
                      t64::nameKey("Tata Steel-Masters", false) ==
                          t64::nameKey("tata steel masters", false) &&
                      t64::nameKey("  ", true).isEmpty(),
                  "Name keys ignore accents, case, punctuation and order, and keep the first initial");
            for (const auto *spelling : {"Carlsen, Magnus", "Carlsen,M.", "Magnus Carlsen"}) {
                t64::Game g;
                g.tags["White"] = spelling;
                g.tags["Black"] = "Doe, \"Jo\"";
                g.tags["Event"] = "Open du club";
                g.play(0, "a2a3");
                g.nodes[1].comment = spelling;
                club.save(g);
            }
            const auto groups = club.nameVariants("player");
            QVariantMap carlsen;
            for (const auto &group : groups)
                if (group.toMap()["names"].toList().size() == 3)
                    carlsen = group.toMap();
            check(carlsen["games"].toLongLong() == 3 && club.nameVariants("event").isEmpty(),
                  "The three spellings of one player are grouped, a single event is not");
            check(club.renameName("player", "Carlsen,M.", "Carlsen, Magnus") == 1 &&
                      club.renameName("player", "Magnus Carlsen", "Carlsen, Magnus") == 1 &&
                      club.renameName("player", "Carlsen, Magnus", "Carlsen, Magnus") == 0,
                  "Renaming counts the games it rewrites");
            const auto renamed = club.search("Carlsen, Magnus");
            bool headers = renamed.size() == 3;
            for (const auto &row : renamed)
                headers = headers &&
                          club.load(row.toMap()["id"].toLongLong()).tags.at("White") == "Carlsen, Magnus";
            check(headers && club.search("Magnus Carlsen").isEmpty(),
                  "The PGN header follows the column, and the old spelling is gone from search");
            check(club.renameName("player", "Doe, \"Jo\"", "Doe, Jo") == 3 &&
                      club.load(renamed.first().toMap()["id"].toLongLong()).tags.at("Black") == "Doe, Jo",
                  "A name with quotes is matched in its escaped PGN form");
            // Un open : trop peu de rencontres pour un tableau carré.
            std::vector<t64::CrossGame> swiss;
            for (int i = 0; i < 12; i += 2)
                swiss.push_back({i, QString("P%1").arg(i), QString("P%1").arg(i + 1), "1-0", 0, 0, 1});
            check(!t64::crosstable(swiss)["matrix"].toBool(), "An open with few pairings is shown by rounds");
            // Statistiques avancées : un toutes-rondes à trois, deux joueurs à 2400.
            const auto small = t64::crosstable({{1, "A", "B", "1-0", 2400, 2400, 1},
                                                {2, "B", "C", "1/2-1/2", 2400, 2300, 2},
                                                {3, "C", "A", "0-1", 2300, 2400, 3}});
            const auto summary = small["summary"].toMap();
            check(summary["averageElo"].toInt() == 2366 && summary["category"].toInt() == 5 &&
                      summary["decisive"].toInt() == 66 && summary["whiteWins"].toInt() == 1,
                  "The tournament summary gives the average rating, the FIDE category and the result split");
            const auto leader = small["players"].toList().first().toMap();
            check(leader["name"] == "A" && leader["buchholz"].toDouble() == 1 &&
                      std::abs(leader["change"].toDouble() - 8.6) < 0.11,
                  "Buchholz sums the opponents' points, the rating change follows the FIDE formula");
        }
        {
            // La même base SCID aux deux formats, écrite à partir de tests/scid/essai.pgn :
            // par SCID 4.7 au format 4, par le code de SCID 5 au format 5 (README) :
            // variantes imbriquées, commentaires, NAG, sous-promotion depuis une FEN,
            // prise en passant, grands roques et dame qui part en diagonale.
            const QString folder = QString(T64_TEST_DATA) + "/scid";
            for (const QString format : {"4", "5"}) {
                const auto label = [&](const char *text) {
                    return (text + (" (SCID " + format + ")")).toStdString();
                };
                const auto base = folder + "/essai.si" + format;
                check(t64::scid::indexPath(folder + "/essai.sg" + format) == base &&
                          t64::scid::indexPath(base + ".txt").isEmpty(),
                      label("Any of the three SCID files leads to the index").c_str());
                check(t64::scid::isScidBase(folder + "/essai.sn" + format) && t64::scid::games(base) == 4,
                      label("A SCID base is recognised and counted").c_str());
                QTemporaryDir lone;
                QFile::copy(base, lone.filePath("seul.si" + format));
                check(!t64::scid::isScidBase(lone.filePath("seul.si" + format)),
                      label("An index without its names and games is refused").c_str());
                QBuffer pgn;
                pgn.open(QIODevice::WriteOnly);
                const auto outcome = t64::scid::exportPgn(base, pgn);
                check(outcome.written == 4 && outcome.skipped == 0, label("Every SCID game is read").c_str());
                QFile source(folder + "/essai.pgn");
                check(source.open(QIODevice::ReadOnly), "Open the SCID source PGN");
                const auto mine = t64::parsePgn(pgn.data().toStdString());
                const auto wanted = t64::parsePgn(source.readAll().toStdString());
                check(mine.size() == wanted.size(), label("As many games as in the source").c_str());
                for (size_t i = 0; i < mine.size() && i < wanted.size(); ++i) {
                    const auto &a = mine[i], &b = wanted[i];
                    bool same = a.tags == b.tags && a.nodes.size() == b.nodes.size();
                    for (size_t n = 0; same && n < a.nodes.size(); ++n)
                        same = a.nodes[n].uci == b.nodes[n].uci && a.nodes[n].comment == b.nodes[n].comment &&
                               a.nodes[n].nags == b.nodes[n].nags &&
                               a.nodes[n].children == b.nodes[n].children;
                    check(same, label(("SCID game " + std::to_string(i + 1) +
                                       " matches its source, variations included")
                                          .c_str())
                                    .c_str());
                }
            }
        }
        {
            // Une base ChessBase écrite par l'encodeur de cbvault (tests/chessbase/README.md) :
            // modes 0, 4 et 5, coups sur deux octets, positions de départ, troisième cavalier,
            // variantes, commentaires, NAG, cases et flèches, une partie effacée et un texte.
            const QString folder = QString(T64_TEST_DATA) + "/chessbase";
            const auto base = folder + "/essai.cbh";
            check(t64::chessbase::indexPath(folder + "/essai.cbg") == base &&
                      t64::chessbase::indexPath(base + ".txt").isEmpty(),
                  "Any file of a ChessBase base leads to its index");
            check(t64::chessbase::isChessBase(folder + "/essai.cbp") && t64::chessbase::records(base) == 10,
                  "A ChessBase base is recognised and its records counted");
            QTemporaryDir lone;
            QFile::copy(base, lone.filePath("seul.cbh"));
            check(!t64::chessbase::isChessBase(lone.filePath("seul.cbh")),
                  "An index without its moves and names is refused");
            QBuffer pgn;
            pgn.open(QIODevice::WriteOnly);
            const auto outcome = t64::chessbase::exportPgn(base, pgn);
            check(outcome.written == 8 && outcome.skipped == 0,
                  "Every ChessBase game is read, the deleted one and the text left out");
            QFile source(folder + "/essai.pgn");
            check(source.open(QIODevice::ReadOnly), "Open the ChessBase reference PGN");
            const auto mine = t64::parsePgn(pgn.data().toStdString());
            const auto wanted = t64::parsePgn(source.readAll().toStdString());
            check(mine.size() == wanted.size(), "As many ChessBase games as in the reference");
            for (size_t i = 0; i < std::min(mine.size(), wanted.size()); ++i) {
                const auto &a = mine[i], &b = wanted[i];
                bool same = a.tags == b.tags && a.nodes.size() == b.nodes.size();
                for (size_t n = 0; same && n < a.nodes.size(); ++n)
                    same = a.nodes[n].uci == b.nodes[n].uci && a.nodes[n].comment == b.nodes[n].comment &&
                           a.nodes[n].nags == b.nodes[n].nags && a.nodes[n].children == b.nodes[n].children;
                check(same, ("ChessBase game " + std::to_string(i + 1) + " matches the reference").c_str());
            }
            // Des octets abîmés au hasard ne doivent jamais faire lire hors des fichiers : une
            // partie illisible est comptée et passée.
            std::mt19937 noise(64);
            for (int round = 0; round < 300; ++round) {
                QTemporaryDir damaged;
                for (const auto *suffix : {".cbh", ".cbg", ".cba", ".cbp", ".cbt", ".cbc", ".cbs"}) {
                    QFile in(folder + "/essai" + suffix);
                    in.open(QIODevice::ReadOnly);
                    auto bytes = in.readAll();
                    const bool hit = noise() % 3 == 0;
                    for (int n = 0; hit && n < 4; ++n)
                        bytes[noise() % bytes.size()] = char(noise());
                    QFile out(damaged.filePath(QString("essai") + suffix));
                    out.open(QIODevice::WriteOnly);
                    out.write(bytes);
                }
                QBuffer sink;
                sink.open(QIODevice::WriteOnly);
                try {
                    t64::chessbase::exportPgn(damaged.filePath("essai.cbh"), sink);
                } catch (const std::runtime_error &) {
                }
            }
            check(true, "Damaged ChessBase files are read without crashing");
        }
        std::cout << checks << " storage checks passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
