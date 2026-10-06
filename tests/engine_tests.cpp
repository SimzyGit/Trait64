// SPDX-License-Identifier: AGPL-3.0-or-later
#include "game.h"
#include "uci.h"
#include <QCoreApplication>
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
        t64::Uci engine;
        QSignalSpy best(&engine, &t64::Uci::bestMove), lines(&engine, &t64::Uci::lines);
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
        engine.start(path, 1, 16, 2, 20);
        check(waitFor([&] { return engine.ready(); }), "UCI handshake failed");
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
        std::cout << "UCI handshake, MultiPV, cancellation, legal reply and shutdown passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
