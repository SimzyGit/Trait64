// SPDX-License-Identifier: AGPL-3.0-or-later
#include "comparison.h"
#include "game.h"
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
