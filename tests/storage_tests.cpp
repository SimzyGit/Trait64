// SPDX-License-Identifier: AGPL-3.0-or-later
#include "archive.h"
#include "cancellation.h"
#include "croissant.h"
#include "database.h"
#include "legacy_positions.h"
#include "moveorder.h"
#include "polyglot.h"
#include <QBuffer>
#include <QCoreApplication>
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
            QMap<QPair<qint64, int>, QString> expected;
            for (const auto &game : specimens) {
                const auto id = materialDb.save(game);
                for (size_t node = 0; node < game.nodes.size(); ++node) {
                    // The old persisted representation is the compatibility oracle.
                    auto key = QString::fromStdString(game.nodes[node].fen).section(' ', 0, 0);
                    key.remove(QRegularExpression("[^prnbqkPRNBQK]"));
                    std::sort(key.begin(), key.end());
                    expected.insert({id, int(node)}, key);
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
                check(query.exec("SELECT game_id,node,material FROM all_positions"),
                      "Read persisted material index");
                QMap<QPair<qint64, int>, QString> actual;
                while (query.next())
                    actual.insert({query.value(0).toLongLong(), query.value(1).toInt()},
                                  query.value(2).toString());
                check(actual == expected,
                      "Material keys preserve the legacy format for variants, special moves and colours");
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
        const auto beforeCanceledRebuild = db.exportPgn();
        rejects([&] { db.rebuildIndexes([](int count) { return count < 4; }); });
        check(db.exportPgn() == beforeCanceledRebuild && db.integrity() == "ok",
              "Cancellation at the final reindex checkpoint preserves the library");
        check(db.stats()["todayReviews"].toInt() == 3, "Review history tracks today's work");
        db.deleteCard(cardId);
        check(db.stats()["todayReviews"].toInt() == 3, "Deleting card preserves activity history");
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
                check(q.exec("PRAGMA user_version") && q.next() && q.value(0).toInt() == 11,
                      "Schema version 11");
                // La marque qui distingue un recueil d'études d'une base téléchargée.
                check(q.exec("SELECT kind FROM sources LIMIT 1"), "Sources carry their kind");
                check(q.exec("SELECT typeof(key) FROM all_positions LIMIT 1") && q.next() &&
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
            for (const auto &sort : QStringList{"updated", "date", "players", "elo", "length"}) {
                const auto expected = pages.search({}, {}, false, {}, false, {{"sort", sort}});
                QVariantList actual;
                QVariantMap filters{{"sort", sort}, {"limit", 3}};
                for (int page = 0; page < 30; ++page) {
                    auto rows = pages.search({}, {}, false, {}, false, filters);
                    if (rows.empty())
                        break;
                    actual += rows;
                    filters["cursor"] = rows.last().toMap();
                }
                check(actual == expected && actual.size() == 47,
                      "Cursor pages preserve every row, ties and nulls for every sort");
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
        std::cout << checks << " storage checks passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
