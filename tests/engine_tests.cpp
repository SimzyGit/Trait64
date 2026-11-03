// SPDX-License-Identifier: AGPL-3.0-or-later
#include "comparison.h"
#include "game.h"
#include "tournament.h"
#include "uci.h"
#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTimer>
#include <iostream>
bool waitFor(std::function<bool()> predicate, int milliseconds = 7000) {
    QEventLoop loop;
    QTimer poll, limit;
    poll.setInterval(10);
    limit.setSingleShot(true);
    QObject::connect(&poll, &QTimer::timeout, &loop, [&] {
        if (predicate())
            loop.quit();
    });
    QObject::connect(&limit, &QTimer::timeout, &loop, &QEventLoop::quit);
    poll.start();
    limit.start(milliseconds);
    if (!predicate())
        loop.exec();
    return predicate();
}
void check(bool ok, const char *message) {
    if (!ok)
        throw std::runtime_error(message);
}
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    try {
        const auto spin =
            t64::Uci::parseOption("option name UCI_Elo type spin default 1320 min 1320 max 3190");
        check(spin["name"] == "UCI_Elo" && t64::Uci::optionValue(spin, 1500) == "1500", "Parse spin bounds");
        bool rejected = false;
        try {
            t64::Uci::optionValue(spin, 3200);
        } catch (...) {
            rejected = true;
        }
        check(rejected, "Reject out of range UCI value");
        check(t64::Uci::optionValue(spin, 5000, true) == "3190" &&
                  t64::Uci::optionValue(spin, 1000, true) == "1320",
              "Managed spin values honor engine-declared limits");
        const auto combo = t64::Uci::parseOption(
            "option name Style type combo default Very solid var Normal var Very solid");
        check(t64::Uci::optionValue(combo, "Very solid") == "Very solid", "UCI multi-word combo");
        rejected = false;
        try {
            t64::Uci::optionValue(combo, "Attack");
        } catch (...) {
            rejected = true;
        }
        check(rejected, "Reject undeclared combo");
        const auto string = t64::Uci::parseOption("option name Eval File type string default <empty>");
        check(t64::Uci::optionValue(string, "").isEmpty() &&
                  t64::Uci::optionValue(string, "<empty>").isEmpty(),
              "Empty UCI strings and legacy markers are sent as empty values");
        rejected = false;
        try {
            t64::Uci::optionValue(string, "net\nquit");
        } catch (...) {
            rejected = true;
        }
        check(rejected && t64::Uci::parseOption("option name Bad\nquit type button").isEmpty(),
              "Reject UCI injection");
        t64::Uci engine;
        {
            QTemporaryDir dir;
            QFile fake(dir.path() + "/bounded-engine");
            check(fake.open(QIODevice::WriteOnly), "Create isolated UCI fixture");
            fake.write("#!/bin/sh\nwhile IFS= read -r line; do\ncase \"$line\" in\n"
                       "uci) printf '%s\\n' 'id name Bounded fixture' "
                       "'option name Threads type spin default 1 min 1 max 1' "
                       "'option name Hash type spin default 16 min 1 max 32' 'uciok';;\n"
                       "'setoption name Threads value 1'|'setoption name Hash value 32') ;;\n"
                       "setoption*) exit 42;;\nisready) printf 'readyok\\n';;\nquit) exit;;\nesac\ndone\n");
            fake.close();
            check(fake.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner),
                  "Make UCI fixture executable");
            t64::Uci bounded;
            bounded.start(fake.fileName(), 32, 4096, 1);
            check(waitFor([&] { return bounded.ready(); }), "Handshake adapts to bounded engine options");
            bounded.shutdown();
        }
        {
            QTemporaryDir dir;
            const auto log = dir.path() + "/commands.log";
            auto fixture = [&](const QString &name, bool elo) {
                QFile fake(dir.path() + "/" + name);
                check(fake.open(QIODevice::WriteOnly), "Create logging UCI fixture");
                fake.write(QString("#!/bin/sh\nwhile IFS= read -r line; do\nprintf '%s\\n' \"$line\" >> '%1'\n"
                                   "case \"$line\" in\nuci) printf '%s\\n' 'id name Elo fixture' "
                                   "'option name Skill Level type spin default 20 min 0 max 20' %2 'uciok';;\n"
                                   "isready) printf 'readyok\\n';;\nquit) exit;;\nesac\ndone\n")
                               .arg(log, elo ? "'option name UCI_LimitStrength type check default false' "
                                               "'option name UCI_Elo type spin default 1320 min 1320 max 3190'"
                                             : "")
                               .toUtf8());
                fake.close();
                fake.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
                return fake.fileName();
            };
            auto commands = [&] {
                QFile file(log);
                file.open(QIODevice::ReadOnly);
                const auto text = QString::fromUtf8(file.readAll());
                QFile::remove(log);
                return text;
            };
            t64::Uci limited;
            limited.setLimitElo(900);
            limited.start(fixture("elo-engine", true), 1, 16, 1, 5, {}, {{"UCI_LimitStrength", false}});
            check(waitFor([&] { return limited.ready(); }), "Elo-capable engine starts");
            limited.shutdown();
            auto sent = commands();
            check(limited.appliedElo() == 1320 && sent.contains("setoption name UCI_LimitStrength value true") &&
                      sent.contains("setoption name UCI_Elo value 1320") &&
                      sent.contains("setoption name Skill Level value 20") && !sent.contains("UCI_LimitStrength value false"),
                  "Elo target is clamped to the engine range and overrides Skill Level and custom options");
            limited.setLimitElo(0);
            limited.start(fixture("elo-engine", true), 1, 16, 1, 5);
            check(waitFor([&] { return limited.ready(); }), "Full-strength restart");
            limited.shutdown();
            sent = commands();
            check(limited.appliedElo() == 0 && !sent.contains("UCI_Elo") && sent.contains("Skill Level value 5"),
                  "No Elo target keeps the Skill Level behaviour");
            t64::Uci plain;
            plain.setLimitElo(1500);
            plain.start(fixture("plain-engine", false), 1, 16, 1, 7);
            check(waitFor([&] { return plain.ready(); }), "Engine without Elo option starts");
            plain.shutdown();
            sent = commands();
            check(plain.appliedElo() == 0 && sent.contains("Skill Level value 7"),
                  "Engines without UCI_Elo fall back to Skill Level");
        }
        QSignalSpy best(&engine, &t64::Uci::bestMove), lines(&engine, &t64::Uci::lines),
            complete(&engine, &t64::Uci::completed);
        QString path = qEnvironmentVariable("T64_STOCKFISH");
        if (path.isEmpty())
            path = QStandardPaths::findExecutable("stockfish");
        if (path.isEmpty() && QFileInfo::exists("/usr/games/stockfish"))
            path = "/usr/games/stockfish";
        if (path.isEmpty()) {
            std::cerr << "Stockfish missing; engine integration skipped\n";
            return 77;
        }
        {
            check(t64::Tournament::eloDifference(.5) == 0 && t64::Tournament::eloDifference(.75) == 191 &&
                      t64::Tournament::eloDifference(1) == 798 && t64::Tournament::eloDifference(0) == -798,
                  "Score fractions map to the logistic Elo difference, bounded at both ends");
            QTemporaryDir dir;
            QFile fake(dir.path() + "/illegal-engine");
            check(fake.open(QIODevice::WriteOnly), "Create illegal-move fixture");
            fake.write("#!/bin/sh\nwhile IFS= read -r line; do\ncase \"$line\" in\n"
                       "uci) printf '%s\\n' 'id name Illegal' 'uciok';;\nisready) printf 'readyok\\n';;\n"
                       "go*) printf 'bestmove a1a1\\n';;\nquit) exit;;\nesac\ndone\n");
            fake.close();
            fake.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
            t64::Tournament tournament;
            QSignalSpy games(&tournament, &t64::Tournament::gameFinished), done(&tournament, &t64::Tournament::finished);
            bool rejected = false;
            try {
                tournament.start({QVariantMap{{"name", "A"}, {"enginePath", path}}, QVariantMap{{"name", "B"}, {"enginePath", path}}},
                                 {2, 100, 40, {"e2e4 e7e5 e1e3"}});
            } catch (const std::exception &) {
                rejected = true;
            }
            check(rejected && !tournament.running(), "Illegal opening lines are refused before any game");
            tournament.start({QVariantMap{{"name", "Stockfish"}, {"enginePath", path}, {"threads", 1}, {"hash", 16}},
                              QVariantMap{{"name", "Illegal"}, {"enginePath", fake.fileName()}}},
                             {2, 100, 40, {"d2d4 d7d5"}});
            check(tournament.total() == 2, "Two games per pairing, one with each colour");
            check(waitFor([&] { return !done.isEmpty(); }, 20000), "Forfeit tournament completes");
            const auto table = tournament.standings();
            check(games.size() == 2 && table[0].toMap()["name"] == "Stockfish" && table[0].toMap()["points"] == 2.0 &&
                      table[1].toMap()["losses"] == 2,
                  "An illegal move loses the game");
            const auto first = games[0].first().toString(), second = games[1].first().toString();
            check(first.contains("[White \"Stockfish\"]") && second.contains("[White \"Illegal\"]") &&
                      first.contains("coup illégal") && first.contains("1. d4 d5"),
                  "Colours alternate and the PGN records opening and termination");
            const auto parsed = t64::parsePgn((first + second).toStdString());
            check(parsed.size() == 2, "Tournament PGN is readable by Trait64");

            games.clear();
            done.clear();
            tournament.start({QVariantMap{{"name", "SF 1"}, {"enginePath", path}, {"hash", 16}},
                              QVariantMap{{"name", "SF 2"}, {"enginePath", path}, {"hash", 16}}},
                             {2, 100, 24, {}});
            check(waitFor([&] { return !done.isEmpty(); }, 60000), "Real engine match completes");
            double points = 0;
            int played = 0;
            for (const auto &row : tournament.standings()) {
                points += row.toMap()["points"].toDouble();
                played += row.toMap()["games"].toInt();
            }
            check(games.size() == 2 && points == 2.0 && played == 4 && !tournament.running(),
                  "Every game awards exactly one point");
            for (const auto &game : games) {
                const auto parsedGame = t64::parsePgn(game.first().toString().toStdString());
                check(parsedGame.size() == 1 && parsedGame.front().nodes.size() > 10, "Engine games replay legally");
            }
            tournament.start({QVariantMap{{"name", "SF 1"}, {"enginePath", path}}, QVariantMap{{"name", "SF 2"}, {"enginePath", path}}},
                             {4, 100, 300, {}});
            done.clear();
            QTimer::singleShot(300, &tournament, [&] { tournament.stop(); });
            check(waitFor([&] { return !done.isEmpty(); }, 5000) && !tournament.running(), "A tournament can be stopped");
        }
        auto start = QString::fromStdString(chess::constants::STARTPOS);
        {
            QTemporaryDir dir;
            const auto isolatedEngine = dir.path() + "/stockfish";
            check(QFile::copy(path, isolatedEngine), "Copy actual Stockfish into isolated test directory");
            check(QDir(dir.path()).mkdir("<empty>"), "Block the unintended literal debug filename");
            for (const auto &empty : QStringList{QString(), QStringLiteral("<empty>")}) {
                t64::Uci logging;
                QSignalSpy replies(&logging, &t64::Uci::bestMove);
                logging.start(isolatedEngine, 1, 16, 1, 20, {}, {{"Debug Log File", empty}});
                check(waitFor([&] { return logging.ready(); }),
                      "Disabling debug output cannot open a literal <empty> file or stop the engine");
                logging.analyse(start, "position startpos", true, 100);
                check(waitFor([&] { return !replies.empty(); }), "Engine still analyzes with debug logging disabled");
                logging.shutdown();
                check(QFileInfo(dir.path() + "/<empty>").isDir(), "Debug filename guard remains untouched");
            }
        }
        for (const auto &bad : QStringList{"score cp abc", "score cp 2147483648", "score bananas 10",
                                           "score mate -2147483648", "depth 5", "score cp 1 multipv abc"})
            check(t64::Uci::parseInfo("info " + bad + " pv e2e4", start).isEmpty(),
                  "Malformed score must never be accepted as an exact evaluation");
        check(t64::Uci::parseInfo("info string score cp 0 pv e2e4", start).isEmpty(),
              "Engine diagnostic text is not a score");
        const auto parsed = t64::Uci::parseInfo(
            "info depth 7 score cp 123 wdl 200 700 100 nodes 123456 time 42 pv e2e4 e7e5 e2e5", start);
        check(parsed["cp"].toInt() == 123 && parsed["scoreType"] == "cp" && parsed["uci"] == "e2e4 e7e5" &&
                  parsed["nodes"].toULongLong() == 123456 && parsed["time"].toInt() == 42 &&
                  parsed["wdl"].toList() == QVariantList{200, 700, 100},
              "Typed score, WDL, counters and only legal PV prefix");
        auto blackFen = start;
        blackFen.replace(" w ", " b ");
        const auto blackScore = t64::Uci::parseInfo("info score cp 25 wdl 200 700 100 pv e7e5", blackFen);
        check(blackScore["cp"].toInt() == -25 && blackScore["wdl"].toList() == QVariantList{100, 700, 200},
              "Scores and WDL consistently use White's point of view");
        const auto mateScore = t64::Uci::parseInfo("info score mate -2 pv e7e5", blackFen);
        check(mateScore["scoreType"] == "mate" && mateScore["mate"].toInt() == 2 &&
                  !mateScore["cp"].isValid(),
              "Mate is not a synthetic centipawn score");
        check(!t64::Uci::parseInfo("info score cp 0 wdl 0 0 0 pv e2e4", start).contains("wdl"),
              "Invalid WDL is omitted, not invented");
        QSignalSpy initialized(&engine, &t64::Uci::initialized);
        engine.start(path, 1, 16, 2, 20, {}, {{"UCI_LimitStrength", false}});
        check(waitFor([&] { return engine.ready(); }), "UCI handshake failed");
        check(initialized.size() == 1, "Initialization announced after readyok");
        check(!engine.options().isEmpty(), "Engine publishes option descriptors");
        check(engine.pressOption("Clear Hash"), "Declared button is actionable");
        check(waitFor([&] { return engine.ready(); }), "Ready after button");
        check(!engine.pressOption("Clear Hash\nquit"), "Reject unknown button");
        engine.analyse(start, "position fen " + start);
        check(waitFor([&] {
                  for (const auto &args : lines)
                      if (!args[0].toList().empty())
                          return true;
                  return false;
              }),
              "No analysis lines");
        t64::Game game;
        game.play(0, "e2e4");
        auto fen = QString::fromStdString(game.nodes[1].fen);
        // Rapid position switch must drain old bestmove before starting new request.
        engine.analyse(start, "position fen " + start, true, 300);
        engine.analyse(fen, "position startpos moves e2e4", true, 300);
        check(waitFor([&] { return !best.empty(); }), "No playing bestmove");
        check(best.last()[1].toString() == fen, "Stale engine move leaked");
        game.play(1, best.last()[0].toString().toStdString());
        engine.stop();
        engine.shutdown();
        check(best.size() == 1, "Old bestmove incorrectly emitted");
        check(complete.size() == 1 && !complete.last()[1].toList().empty(),
              "Completed search includes exact PV");
        check(!complete.last()[1].toList().first().toMap()["bound"].toBool(),
              "Bound score cannot replace exact completed score");
        engine.start(path, 1, 16, 1, 20);
        auto frc = QString::fromStdString(t64::chess960Fen(37));
        engine.analyse(frc, "position fen " + frc, true, 100, 24, true);
        check(waitFor([&] { return best.size() == 2; }), "Chess960 UCI reply");
        t64::Game frcGame(frc.toStdString(), true);
        frcGame.play(0, best.last()[0].toString().toStdString());
        engine.shutdown();
        {
            t64::Uci chained;
            QObject::connect(&chained, &t64::Uci::completed, &app,
                             [&](const QString &completedFen, const QVariantList &) {
                if (completedFen == start)
                    chained.analyse(fen, "position startpos moves e2e4", true, 100);
            });
            QSignalSpy searches(&chained, &t64::Uci::completed), replies(&chained, &t64::Uci::bestMove);
            chained.start(path, 1, 16, 1);
            chained.analyse(start, "position startpos", true, 100);
            check(waitFor([&] { return searches.size() >= 2; }), "Chained searches finish");
            check(searches.first()[0].toString() == start && searches.last()[0].toString() == fen,
                  "Completion signal keeps its original FEN when a receiver starts another search");
            check(replies.size() == 1 && replies.first()[1].toString() == fen,
                  "A superseded completion cannot deliver its old move as the new position's move");
            chained.shutdown();
        }
        {
            t64::Uci stopped;
            QObject::connect(&stopped, &t64::Uci::completed, &app,
                             [&](const QString &, const QVariantList &) { stopped.stop(); });
            QSignalSpy searches(&stopped, &t64::Uci::completed), replies(&stopped, &t64::Uci::bestMove);
            stopped.start(path, 1, 16, 1);
            stopped.analyse(start, "position startpos", true, 100);
            check(waitFor([&] { return !searches.empty(); }), "Stop at completion received");
            check(replies.empty(), "Stopping in a completion callback cancels move delivery");
            stopped.shutdown();
        }
        t64::Comparison comparison;
        QVariantMap profile{{"name", "Stockfish A"},
                            {"enginePath", path},
                            {"hash", 16},
                            {"multiPV", 2},
                            {"analysisDepth", 8}};
        comparison.start({profile, profile}, start, "position fen " + start, false);
        auto hasBoth = [&] {
            const auto rows = comparison.rows();
            return rows.size() == 2 && !rows[0].toMap()["lines"].toList().empty() &&
                   !rows[1].toMap()["lines"].toList().empty();
        };
        check(waitFor(hasBoth), "Both UCI processes return independent analysis");
        comparison.setPosition(fen, "position startpos moves e2e4", false);
        check(comparison.rows()[0].toMap()["lines"].toList().empty(),
              "Navigation clears old comparison immediately");
        comparison.setPosition(start, "position startpos", false);
        comparison.setPosition(fen, "position startpos moves e2e4", false);
        check(waitFor(hasBoth), "Comparison follows rapid position changes");
        for (const auto &row : comparison.rows())
            for (const auto &line : row.toMap()["lines"].toList())
                check(line.toMap()["fen"].toString() == fen, "No stale comparison PV");
        comparison.stop();
        check(!comparison.running(), "All comparison processes stopped");
        std::cout << "UCI handshake, MultiPV, cancellation, legal reply and shutdown passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
