// SPDX-License-Identifier: AGPL-3.0-or-later
#include "controller.h"
#include "discovery.h"
#include "sounds.h"
#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImage>
#include <QImageReader>
#include <QNetworkProxy>
#include <QProcess>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickItemGrabResult>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QScopeGuard>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QThreadPool>
#include <QXmlStreamReader>
#include <QtConcurrent>
#include <functional>
#include <future>
#include <iostream>
#include <limits>

int checks = 0;
void check(bool ok, const char *message) {
    ++checks;
    if (!ok)
        throw std::runtime_error(message);
}
// Bounded wait that reports whether the condition really came true, so a test
// can fail on the spot instead of carrying on with an assumption the machine
// never met. Loaded runners reach these states much later than idle ones.
bool waitUntil(const std::function<bool()> &ready, int timeoutMs) {
    QElapsedTimer timer;
    timer.start();
    while (!ready() && timer.elapsed() < timeoutMs)
        QTest::qWait(10);
    return ready();
}
QQuickItem *visualItem(QQuickItem *parent, const QString &name) {
    if (parent->objectName() == name)
        return parent;
    for (auto *child : parent->childItems())
        if (auto *found = visualItem(child, name))
            return found;
    return nullptr;
}
#include "explorer_probe.h"
#include "import_gui_probe.h"
#include "move_notebook_probe.h"
#include "showcase_probe.h"
#include "ui_layout_probe.h"
// Hold the global pool so the queued verification cannot finish before we
// exercise the UI and request cancellation. Release safely on assertion failure.
class PoolGate {
  public:
    PoolGate() : oldMaximum_(QThreadPool::globalInstance()->maxThreadCount()) {
        QThreadPool::globalInstance()->setMaxThreadCount(1);
        std::promise<void> ready;
        auto started = ready.get_future();
        auto release = release_.get_future();
        worker_ = QtConcurrent::run([ready = std::move(ready), release = std::move(release)]() mutable {
            ready.set_value();
            release.wait();
        });
        started.wait();
    }
    void open() {
        if (!opened_) {
            release_.set_value();
            opened_ = true;
        }
    }
    ~PoolGate() {
        open();
        worker_.waitForFinished();
        QThreadPool::globalInstance()->setMaxThreadCount(oldMaximum_);
    }

  private:
    int oldMaximum_;
    bool opened_ = false;
    std::promise<void> release_;
    QFuture<void> worker_;
};
int main(int argc, char **argv) {
    QGuiApplication app(argc, argv);
    app.setOrganizationName("Trait64Tests");
    app.setApplicationName("Desktop");
    QTemporaryDir dir;
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, dir.path());
    QQuickStyle::setStyle("Basic");
    try {
        if (app.arguments().contains("--move-notebook"))
            return moveNotebookProbe(dir.path());
        if (app.arguments().contains("--ui-layout"))
            return uiLayoutProbe(dir.path());
        if (app.arguments().contains("--explorer-walk"))
            return explorerProbeMain(dir.path());
        if (app.arguments().contains("--board-walk"))
            return boardProbeMain(dir.path());
        if (const int i = int(app.arguments().indexOf("--showcase")); i > 0 && i + 1 < app.arguments().size())
            return showcaseProbe(dir.path(), app.arguments()[i + 1]);
        if (app.arguments().contains("--benchmark-import"))
            return importGuiProbe(app, dir.path());
        {
            using t64::BoardSound;
            check(t64::soundFor("e4") == BoardSound::Move && t64::soundFor("O-O") == BoardSound::Move,
                  "A quiet move sounds like a move");
            check(t64::soundFor("exd5") == BoardSound::Capture, "A capture has its own sound");
            check(t64::soundFor("Qxf7#") == BoardSound::Check && t64::soundFor("Bb5+") == BoardSound::Check,
                  "Check and mate take precedence over the capture");
            check(!t64::playSound(BoardSound::Move),
                  "Without a screen nothing is played and nothing is loaded");
            check(QFile::exists(QString(T64_TEST_DATA) + "/../assets/sounds/move.wav"),
                  "The move sound ships");
        }
        {
            // Trouver un moteur déjà installé : par son nom, dans les dossiers du système
            // et là où l'on dépose un téléchargement, sans rien lancer.
            check(t64::looksLikeEngine("stockfish") && t64::looksLikeEngine("Stockfish-17-avx2") &&
                      t64::looksLikeEngine("stockfish-windows-x86-64.exe") && t64::looksLikeEngine("lc0") &&
                      !t64::looksLikeEngine("stockfish.zip") &&
                      !t64::looksLikeEngine("stockfish-17.tar.gz") &&
                      !t64::looksLikeEngine("nn-1111cefa1111.nnue") && !t64::looksLikeEngine("README.md") &&
                      !t64::looksLikeEngine("stockfish.desktop") && !t64::looksLikeEngine("firefox"),
                  "An engine is recognised by its name, and an archive or a network file is not");
            QTemporaryDir machine;
            const auto place = [&](const QString &relative, bool executable) {
                const auto path = machine.path() + "/" + relative;
                QDir().mkpath(QFileInfo(path).absolutePath());
                QFile file(path);
                check(file.open(QIODevice::WriteOnly) && file.write("#!/bin/sh\n") > 0,
                      "Write a fake program");
                file.close();
                file.setPermissions(executable ? QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner
                                               : QFile::ReadOwner | QFile::WriteOwner);
                return path;
            };
            const auto games = place("usr/games/stockfish", true);
            const auto unpacked = place(
                "home/Downloads/stockfish-ubuntu-x86-64-avx2/stockfish/stockfish-ubuntu-x86-64-avx2", true);
            place("home/Downloads/stockfish-ubuntu-x86-64-avx2.tar", true);
            place("home/Downloads/stockfish-notes", false); // porte le nom, mais ne se lance pas
            const auto leela = place("home/.local/bin/lc0", true);
            place("home/.local/bin/firefox", true);
            const auto folders =
                t64::engineSearchFolders(machine.path() + "/home", machine.path() + "/usr/games");
            check(folders.contains(machine.path() + "/usr/games") &&
                      folders.contains(machine.path() + "/home/Downloads") && folders.contains("/usr/games"),
                  "The search covers the PATH, the package folders and where downloads land");
            QStringList mine;
            for (const auto &path : t64::findEngines(folders))
                if (path.startsWith(machine.path()))
                    mine << path;
            check(mine.size() == 3 && mine.contains(games) && mine.contains(unpacked) && mine.contains(leela),
                  "Installed and unpacked engines are found; archives, notes and other programs are not");
            check(mine.indexOf(leela) > mine.indexOf(games) && mine.indexOf(leela) > mine.indexOf(unpacked),
                  "Stockfish comes before the others");
            check(t64::findEngines({machine.path() + "/nowhere"}).isEmpty(),
                  "A missing folder yields nothing");
        }
        if (!qEnvironmentVariable("T64_STOCKFISH").isEmpty()) {
            {
                // Un moteur installé après le premier lancement est trouvé au suivant : la
                // recherche n'avait lieu qu'une fois, et un chemin vide restait vide.
                const auto path = dir.path() + "/late-engine";
                {
                    Controller first(path);
                    first.setPreference("enginePath", "");
                }
                Controller second(path);
                check(QFileInfo(second.preferences()["enginePath"].toString()).isExecutable(),
                      "An engine installed after the first launch is found at the next one");
                second.setPreference("enginePath", dir.path() + "/gone/stockfish");
                check(!second.useDetectedEngine().isEmpty() &&
                          QFileInfo(second.preferences()["enginePath"].toString()).isExecutable(),
                      "Detecting replaces a path that no longer exists");
            }
            // La barre d'évaluation retombait à l'équilibre à chaque coup : le moteur vide
            // ses lignes dès qu'il change de position, et rien ne gardait la dernière
            // évaluation reçue le temps que la suivante arrive.
            Controller bar(dir.path() + "/evalbar");
            bar.setPreference("enginePath", qEnvironmentVariable("T64_STOCKFISH"));
            bar.setPreference("hash", 16);
            bar.toggleAnalysis();
            QElapsedTimer engineWait;
            engineWait.start();
            while (bar.engineLines().isEmpty() && engineWait.elapsed() < 20000)
                QTest::qWait(20);
            check(!bar.engineLines().isEmpty(), "The engine reports a line on the starting position");
            const auto first = bar.heldEvaluation();
            check(first.contains("eval"), "The held evaluation carries what the bar displays");
            bar.move("e2e4");
            // Le moteur vide ses lignes en repartant. L'invariant qui tient la barre :
            // tant que l'analyse tourne, il reste toujours une évaluation à montrer.
            bool everEmpty = false;
            engineWait.restart();
            while (engineWait.elapsed() < 2500) {
                everEmpty = everEmpty || bar.heldEvaluation().isEmpty();
                QTest::qWait(5);
            }
            check(!everEmpty, "A played move never leaves the bar without an evaluation to show");
            check(bar.heldEvaluation() != first, "The evaluation of the new position replaces it");
            bar.toggleAnalysis();
            check(bar.heldEvaluation().isEmpty(),
                  "Stopping the analysis forgets the evaluation rather than showing a stale one");
            {
                // La profondeur : avec une limite, le moteur s'y arrête ; sans, il creuse
                // tant qu'on regarde la position. C'était 24 d'office, qu'une machine
                // d'aujourd'hui atteint en quelques secondes pour ne plus rien faire.
                Controller deep(dir.path() + "/depth");
                deep.setPreference("enginePath", qEnvironmentVariable("T64_STOCKFISH"));
                deep.setPreference("hash", 16);
                deep.setPreference("multiPV", 1);
                check(deep.preferences()["analysisDepth"].toInt() == 0,
                      "Analysis has no depth limit by default");
                const auto depthNow = [&deep] {
                    return deep.engineLines().isEmpty() ? 0
                                                        : deep.engineLines().first().toMap()["depth"].toInt();
                };
                // Une finale simple : le moteur y descend très vite.
                deep.setFen("8/8/8/4k3/8/8/4P3/4K3 w - - 0 1");
                deep.setPreference("analysisDepth", 6);
                deep.toggleAnalysis();
                engineWait.restart();
                while (depthNow() < 6 && engineWait.elapsed() < 20000)
                    QTest::qWait(20);
                QTest::qWait(600);
                check(depthNow() == 6, "A depth limit stops the engine exactly there");
                deep.toggleAnalysis();
                deep.setPreference("analysisDepth", 0);
                deep.toggleAnalysis();
                engineWait.restart();
                while (depthNow() <= 24 && engineWait.elapsed() < 30000)
                    QTest::qWait(20);
                check(depthNow() > 24, "Without a limit the engine goes past the old ceiling of 24");
                // Et il s'arrête proprement quand la position change : la nouvelle
                // analyse repart, au lieu d'attendre une fin qui ne viendra pas. On le
                // reconnaît au coup proposé — celui des Noirs, dont la seule pièce est le
                // roi en e5 — et non à la profondeur : sur une finale aussi simple un
                // moteur récent retrouve la sienne avant qu'on ait eu le temps de regarder.
                const auto firstMove = [&deep] {
                    return deep.engineLines().isEmpty()
                               ? QString()
                               : deep.engineLines().first().toMap()["uci"].toString().left(2);
                };
                check(firstMove() != "e5", "Before the move, the line is White's");
                deep.move("e2e4");
                engineWait.restart();
                while (firstMove() != "e5" && engineWait.elapsed() < 10000)
                    QTest::qWait(10);
                check(firstMove() == "e5", "An unlimited search yields to the next position at once");
                // La menace : la même position, le trait passé. Les Noirs sont au trait,
                // la ligne devient celle des Blancs (roi e1 ou pion e4).
                deep.toggleThreat();
                engineWait.restart();
                while (firstMove() != "e1" && firstMove() != "e4" && engineWait.elapsed() < 10000)
                    QTest::qWait(10);
                check(deep.state()["threat"].toBool() && (firstMove() == "e1" || firstMove() == "e4"),
                      "Showing the threat analyses the position with the move passed");
                const auto nodesBefore = deep.moveFlow().size();
                deep.addEngineLine(0);
                check(deep.moveFlow().size() == nodesBefore,
                      "A threat cannot be inserted as a line of the game");
                deep.step(-1);
                check(!deep.state()["threat"].toBool(), "Leaving the position ends the threat display");
                deep.setFen("4k3/8/8/8/8/8/4R3/4K3 b - - 0 1");
                deep.toggleThreat();
                check(!deep.state()["threat"].toBool(), "In check there is no move to pass, so no threat");
                deep.setFen("rnb1kbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
                check(deep.state()["takenByWhite"] == "♛" && deep.state()["takenByBlack"] == "" &&
                          deep.state()["materialLead"] == 9,
                      "A missing black queen shows as taken by White, nine points ahead");
                deep.setFen("4k3/8/8/8/8/8/4P3/4K3 w - - 0 1");
                check(deep.state()["takenByBlack"].toString().count("♟") == 7 &&
                          deep.state()["materialLead"] == 1,
                      "Taken pieces are listed from queen to pawn, with the lead in points");
                deep.toggleAnalysis();
            }
            {
                // Les anciens réglages par défaut, écrits dans les préférences, sont
                // relevés une fois ; un réglage choisi par l'utilisateur ne l'est pas.
                const auto path = dir.path() + "/unleash";
                {
                    Controller before(path);
                    before.setPreference("analysisDepth", 24);
                }
                QSettings().remove("engine/unleashed");
                {
                    Controller after(path);
                    check(after.preferences()["analysisDepth"].toInt() == 0,
                          "The old default depth of 24 is lifted on the first start");
                    after.setPreference("analysisDepth", 24);
                }
                {
                    Controller again(path);
                    check(again.preferences()["analysisDepth"].toInt() == 24,
                          "A depth chosen afterwards is kept: the lift happens once");
                    again.setPreference("analysisDepth", 0);
                }
            }
            Controller c(dir.path() + "/tournament");
            c.setPreference("enginePath", qEnvironmentVariable("T64_STOCKFISH"));
            c.setPreference("hash", 16);
            c.saveEngineProfile("Stockfish A");
            c.saveEngineProfile("Stockfish B");
            // Book with two short lines: every tournament opening must come from it.
            const chess::Board start;
            chess::Board e4, d4;
            e4.makeMove(chess::uci::uciToMove(e4, "e2e4"));
            d4.makeMove(chess::uci::uciToMove(d4, "d2d4"));
            QFile book(dir.path() + "/tournament-book.bin");
            check(book.open(QIODevice::WriteOnly), "Tournament book written");
            std::vector<std::tuple<quint64, int, int>> rows{
                {start.hash(), 12, 28}, {start.hash(), 11, 27}, {e4.hash(), 52, 36}, {d4.hash(), 51, 35}};
            std::sort(rows.begin(), rows.end());
            for (auto [key, from, to] : rows)
                for (quint64 part : {key, quint64(to | from << 6) << 48 | quint64(1) << 32})
                    for (int shift = 56; shift >= 0; shift -= 8)
                        book.putChar(char(part >> shift & 0xff));
            book.close();
            c.setPreference("bookPath", book.fileName());
            QSignalSpy messages(&c, &Controller::message);
            c.startTournament({0}, 2, 100, 30, true);
            check(!c.tournament()->running() && messages.size() == 1, "A tournament needs two profiles");
            c.startTournament({0, 1}, 2, 100, 30, true);
            check(c.tournament()->running(), "Tournament starts from saved profiles");
            QElapsedTimer wait;
            wait.start();
            while ((c.tournament()->running() || c.busy() || c.librarySearching()) && wait.elapsed() < 60000)
                QTest::qWait(20);
            const auto games = c.games();
            check(games.size() == 2, "Both tournament games reach the library");
            bool tagged = false;
            for (const auto &value : c.sources())
                tagged |= value.toMap()["title"].toString().startsWith("Tournoi de moteurs");
            check(tagged, "Tournament games are filed under their own source");
            for (const auto &value : games) {
                c.loadGame(value.toMap()["id"].toLongLong());
                const auto pgn = c.state()["pgn"].toString();
                const auto moves = c.moveRows();
                check(moves.size() > 2, "Tournament game has moves");
                const auto first = moves[1].toMap();
                const auto line = first["white"].toMap()["text"].toString() + " " +
                                  first["black"].toMap()["text"].toString();
                check(line == "e4 e5" || line == "d4 d5", "Tournament openings come from the Polyglot book");
            }
            // Settings are shared by every controller in this process: leave them as found.
            c.setPreference("bookPath", "");
            c.removeEngineProfile(1);
            c.removeEngineProfile(0);
            c.setPreference("hash", 128);
            c.setPreference("enginePath", "");
            check(c.preferences()["engineProfiles"].toList().isEmpty(),
                  "Tournament test leaves no profile behind");
        }
        {
            Controller c(dir.path() + "/puzzles");
            QFile csv(dir.path() + "/puzzles.csv");
            check(csv.open(QIODevice::WriteOnly), "Puzzle file written");
            // Two rooks: the stored solution is d1d8, but e1e8 mates as well.
            csv.write(
                "PuzzleId,FEN,Moves,Rating,RatingDeviation,Popularity,NbPlays,Themes,GameUrl,OpeningTags\n"
                "p1,r5k1/5ppp/8/8/8/8/5PPP/3RR1K1 b - - 0 1,a8a7 d1d8,1500,75,95,1000,backRankMate "
                "mateIn1,u,\n"
                "p2,r5k1/5ppp/8/8/8/8/5PPP/3R2K1 b - - 0 1,a8a7 d1d8,1520,75,95,1000,backRankMate "
                "mateIn1,u,\n");
            csv.close();
            c.importPuzzles(csv.fileName(), 100, 0, 0);
            QElapsedTimer wait;
            wait.start();
            while (c.busy() && wait.elapsed() < 10000)
                QTest::qWait(10);
            check(c.puzzles()["total"].toInt() == 2, "Puzzle bank imported");
            const auto study = c.state()["fen"];
            const int before = c.puzzles()["rating"].toInt();
            c.startPuzzle("mateIn1", "lichess");
            check(c.puzzles()["active"].toBool() && c.state()["training"].toBool(),
                  "Puzzle starts in training mode");
            const auto first = c.puzzles()["current"].toMap()["id"];
            const bool twoRooks = c.state()["fen"].toString().startsWith("6k1/r4ppp/8/8/8/8/5PPP/3RR1K1");
            c.move("d1d2");
            check(!c.state()["trainingComplete"].toBool(), "A wrong move does not finish the puzzle");
            c.finishTraining();
            check(c.state()["fen"] == study, "Leaving a puzzle restores the study");
            c.startPuzzle("mateIn1", "lichess");
            const bool rooks = c.state()["fen"].toString().startsWith("6k1/r4ppp/8/8/8/8/5PPP/3RR1K1");
            c.move(rooks ? "e1e8" : "d1d8");
            check(c.state()["trainingComplete"].toBool(), "Any mate solves the last move");
            check(c.puzzles()["rating"].toInt() > before && c.puzzles()["solved"].toInt() == 1,
                  "Solving raises the rating and counts the puzzle");
            {
                // Une tentative manquée puis une réussie, chacune comptée pour les deux thèmes.
                QMap<QString, QVariantMap> byTheme;
                for (const auto &row : c.puzzles()["themes"].toList())
                    byTheme[row.toMap()["tag"].toString()] = row.toMap();
                check(byTheme.size() == 2 &&
                          byTheme["mateIn1"]["attempts"] == byTheme["backRankMate"]["attempts"] &&
                          byTheme["mateIn1"]["attempts"].toInt() >= 1 &&
                          byTheme["mateIn1"]["success"].toInt() ==
                              100 / byTheme["mateIn1"]["attempts"].toInt(),
                      "Results are kept per theme, every attempt counting for each theme of its puzzle");
            }
            c.nextPuzzle();
            const auto second = c.puzzles()["current"].toMap();
            check(c.puzzles()["active"].toBool() && second["id"] != (rooks ? QVariant(1) : QVariant(2)) &&
                      !second.isEmpty(),
                  "The next puzzle is one not yet solved");
            const int beforeHint = c.puzzles()["rating"].toInt();
            c.puzzleHint();
            c.move("d1d8");
            check(c.state()["trainingComplete"].toBool() && c.puzzles()["rating"].toInt() < beforeHint,
                  "A hint makes the attempt count as a failure");
            c.nextPuzzle();
            check(c.puzzles()["active"].toBool(), "Unsolved puzzles stay available");
            c.finishTraining();
            c.importText(
                "[Event \"x\"]\n\n1. e4 e5 *\n\n[Event \"y\"]\n\n1. d4 d5 *\n\n[Event \"z\"]\n\n1. c4 e5 *");
            wait.restart();
            while ((c.busy() || c.librarySearching()) && wait.elapsed() < 10000)
                QTest::qWait(10);
            const int jobs = c.analysisQueue()->jobs().size();
            c.searchLibrary("", "", false, false, false, {});
            wait.restart();
            while (c.librarySearching() && wait.elapsed() < 5000)
                QTest::qWait(10);
            // Le moteur de la machine est maintenant trouvé d'office : le cas « sans
            // moteur » se demande explicitement.
            c.setPreference("enginePath", "");
            QSignalSpy refused(&c, &Controller::message);
            c.enqueueLibrary(100, -1, 2);
            check(refused.size() == 1 && refused.first().at(1).toBool() &&
                      c.analysisQueue()->jobs().size() == jobs,
                  "Batch analysis without an engine fails once, clearly");
            if (!qEnvironmentVariable("T64_STOCKFISH").isEmpty()) {
                c.setPreference("enginePath", qEnvironmentVariable("T64_STOCKFISH"));
                c.enqueueLibrary(100, -1, 2);
                wait.restart();
                while (c.analysisQueue()->jobs().size() < jobs + 2 && wait.elapsed() < 5000)
                    QTest::qWait(10);
                check(c.analysisQueue()->jobs().size() == jobs + 2,
                      "Library games are queued for analysis up to the limit");
                c.analysisQueue()->pauseAll();
                c.setPreference("enginePath", "");
            }
            c.newGame();
            c.move("e2e4");
            c.move("c7c5");
            c.findNovelty();
            check(c.state()["node"].toInt() == 2, "Novelty search jumps to the first unplayed move");
            for (const auto &style : {QString("cburnett"), QString("merida"), QString("unicode")}) {
                c.newGame();
                c.setPreference("pieceStyle", style);
                c.toggleAnnotation("e2", "e4", "G");
                c.toggleAnnotation("d4", "d4", "R");
                const auto svgPath = dir.path() + "/diagram-" + style + ".svg";
                c.exportDiagram(svgPath);
                QFile svg(svgPath);
                check(svg.open(QIODevice::ReadOnly), "SVG diagram written");
                const auto bytes = svg.readAll();
                QXmlStreamReader xml(bytes);
                while (!xml.atEnd())
                    xml.readNext();
                check(!xml.hasError(), "SVG diagram is well-formed XML");
                check(style == "unicode" ? bytes.count("<text") >= 32
                                         : bytes.count("<svg") == 1 && bytes.count("<g transform") == 32,
                      "Every piece is in the SVG");
                check(bytes.contains("<polygon") && bytes.contains("<circle"), "Arrows and circles are kept");
                QImageReader reader(svgPath);
                const auto image = reader.read();
                check(!image.isNull() && image.width() == 832, "SVG diagram renders");
                if (!qEnvironmentVariable("T64_DIAGRAM_OUT").isEmpty())
                    image.save(qEnvironmentVariable("T64_DIAGRAM_OUT") + "/diagram-" + style + ".png");
            }
            c.setPreference("pieceStyle", "cburnett");
            Q_UNUSED(first);
            Q_UNUSED(twoRooks);
        }
        {
            // Online import: the download step is covered by online_tests; here the result
            // goes through the real controller, library, source registry and sync bookmark.
            // Nothing here may reach lichess.org or chess.com. The accounts below carry
            // plausible names, and a real reply landing between two checks imports games the
            // test never asked for: that is what makes this test come and go. Send every
            // request to a dead local port so each fetch fails at once and the only outcomes
            // are the ones the test emits itself.
            const auto realProxy = QNetworkProxy::applicationProxy();
            QNetworkProxy::setApplicationProxy(
                QNetworkProxy(QNetworkProxy::HttpProxy, QStringLiteral("127.0.0.1"), 1));
            const auto restoreProxy = qScopeGuard([&] { QNetworkProxy::setApplicationProxy(realProxy); });
            Controller c(dir.path() + "/online-import");
            auto *online = c.findChild<t64::OnlineGames *>();
            QSignalSpy messages(&c, &Controller::message);
            c.importOnline("lichess", "a/../b", 10);
            check(messages.size() == 1 && !online->busy(),
                  "Invalid online username is refused before any request");
            c.importOnline("lichess", "Simzy", 10);
            check(c.busy() && c.onlineFetching(), "Online fetch marks the application busy");
            online->cancel();
            QElapsedTimer wait;
            wait.start();
            while (c.onlineFetching() && wait.elapsed() < 5000)
                QTest::qWait(10);
            check(!c.busy(), "Canceled fetch releases the application");
            const auto file = dir.path() + "/online-import/downloads/online-lichess-simzy.pgn";
            QFile pgn(file);
            check(pgn.open(QIODevice::WriteOnly) &&
                      pgn.write("[Event \"Rated blitz\"]\n[White \"Simzy\"]\n[Black \"X\"]\n[Result "
                                "\"1-0\"]\n\n1. e4 e5 1-0\n\n"
                                "[Event \"Rated blitz\"]\n[White \"Y\"]\n[Black \"Simzy\"]\n[Result "
                                "\"0-1\"]\n\n1. d4 d5 0-1\n") > 0,
                  "Downloaded PGN fixture written");
            pgn.close();
            emit online->finished(file, 2);
            wait.restart();
            while ((c.busy() || c.librarySearching()) && wait.elapsed() < 10000)
                QTest::qWait(10);
            QSettings settings;
            check(c.games().size() == 2 && !QFile::exists(file) &&
                      settings.value("onlineSync/lichess/simzy").toLongLong() > 0,
                  "Online games reach the library, the temporary file is removed and the next sync is "
                  "incremental");
            bool tagged = false;
            for (const auto &value : c.sources())
                tagged |= value.toMap()["title"].toString() == "Lichess — simzy";
            check(tagged, "Online games are filed under their player source");
            // Saved accounts: sync queue, up-to-date bookkeeping, failures that do not block the queue.
            auto settle = [&] {
                QElapsedTimer w;
                w.start();
                while ((c.busy() || c.librarySearching()) && w.elapsed() < 10000)
                    QTest::qWait(10);
                QTest::qWait(20);
            };
            messages.clear();
            c.addOnlineAccount("lichess", "../x");
            check(c.onlineAccounts().isEmpty() && messages.size() == 1, "Invalid account names are refused");
            c.addOnlineAccount("lichess", "Kim");
            check(c.onlineAccounts().size() == 1 && c.onlineAccounts()[0].toMap()["syncing"].toBool(),
                  "Adding an account syncs it at once");
            emit online->upToDate();
            settle();
            online->cancel();
            settle();
            const auto kim = c.onlineAccounts()[0].toMap();
            check(kim["lastSync"].toLongLong() > 0 && !kim["syncing"].toBool(),
                  "An up-to-date account records its sync");
            c.addOnlineAccount("lichess", "kim");
            check(c.onlineAccounts().size() == 1, "The same account is not added twice");
            c.addOnlineAccount("chesscom", "Kim");
            online->cancel();
            settle();
            check(c.onlineAccounts().size() == 2 &&
                      c.onlineAccounts()[1].toMap()["lastSync"].toLongLong() == 0,
                  "A failed sync does not pretend to be up to date");
            c.syncAllAccounts();
            for (int i = 0; i < 2; ++i) {
                waitUntil([&] { return c.onlineFetching(); }, 3000);
                online->cancel();
                settle();
            }
            check(!c.busy() && !c.onlineAccounts()[0].toMap()["syncing"].toBool() &&
                      !c.onlineAccounts()[1].toMap()["syncing"].toBool(),
                  "Sync all goes through every account even when one fails");
            c.removeOnlineAccount(1);
            c.removeOnlineAccount(0);
            check(c.onlineAccounts().isEmpty() && c.games().size() == 2,
                  "Removing accounts keeps their games");
            // Profils : un utilisateur rassemble ses comptes sous un seul nom. Le
            // rattachement se fait par nom, donc renommer et retirer doivent suivre.
            messages.clear();
            c.addProfile("  Sïmzy  ");
            check(c.profiles().size() == 1 && c.profiles()[0].toMap()["name"] == "Sïmzy",
                  "A profile is created with its name tidied up");
            // La comparaison ignore la casse, pas les diacritiques : « sïmzy » est le
            // même nom, « Simzy » en est un autre, et c'est voulu — un accent distingue
            // deux prénoms réels aussi sûrement qu'une lettre différente.
            c.addProfile("sïmzy");
            check(c.profiles().size() == 1 && messages.size() == 1,
                  "The same name in another case is not added twice");
            c.addProfile("Simzy");
            check(c.profiles().size() == 2, "A different spelling is a different profile");
            c.removeProfile(1);
            c.addProfile("");
            check(c.profiles().size() == 1 && messages.size() == 2, "A nameless profile is refused");
            c.addOnlineAccount("lichess", "Kim");
            online->cancel();
            settle();
            c.setAccountProfile(0, "Sïmzy");
            check(c.profiles()[0].toMap()["accounts"].toList().size() == 1 &&
                      c.onlineAccounts()[0].toMap()["profile"] == "Sïmzy",
                  "An account joins a profile");
            c.renameProfile(0, "Simon");
            check(c.profiles()[0].toMap()["accounts"].toList().size() == 1 &&
                      c.onlineAccounts()[0].toMap()["profile"] == "Simon",
                  "Renaming a profile keeps its accounts attached");
            // Analyser un profil qui n'existe pas doit le dire une fois, clairement,
            // plutôt que de parcourir la base pour ne rien trouver.
            const int before = messages.size();
            c.reviewProfile(42, 18);
            check(messages.size() == before + 1, "Reviewing an unknown profile reports exactly one error");
            c.setFideDetails(0, "1503014", "Nom, Prénom", 2100, 0, 1950);
            const auto fide = c.profiles()[0].toMap();
            // Titre et fédération se saisissent à la main : télécharger la liste mensuelle
            // pour le seul titre coûterait des dizaines de mégaoctets.
            c.setFideDetails(0, "1503014", "Nom, Prénom", 2100, 0, 1950, "gm", "fra");
            const auto titled = c.profiles()[0].toMap();
            check(titled["fideTitle"] == "GM" && titled["fideCountry"] == "FRA",
                  "A typed title and federation are kept, in upper case");
            c.setFideDetails(0, "1503014", "Nom, Prénom", 2100, 0, 1950);
            check(c.profiles()[0].toMap()["fideTitle"].toString().isEmpty(),
                  "Leaving the title empty clears it rather than keeping a stale one");
            check(fide["fideId"] == "1503014" && fide["fideStandard"].toInt() == 2100 &&
                      !fide["fideRapid"].isValid() && fide["fideBlitz"].toInt() == 1950,
                  "FIDE details are kept, and an unrated speed stays empty rather than zero");
            {
                // Le portrait est recopié réduit dans le dossier de données : le profil
                // ne doit pas dépendre d'un fichier que l'on peut déplacer ou effacer.
                QTemporaryDir pictures;
                const auto original = pictures.filePath("moi.png");
                check(QImage(900, 600, QImage::Format_RGB32).save(original, "PNG"),
                      "Write a portrait to choose");
                c.setProfilePhoto(0, QUrl::fromLocalFile(original).toString());
                const auto stored = c.profiles()[0].toMap()["photo"].toString();
                check(QFileInfo(stored).isFile() && stored != original,
                      "The portrait is copied into the data folder, not merely pointed at");
                const QImage thumbnail(stored);
                check(thumbnail.width() == 256 && thumbnail.height() > 0 &&
                          thumbnail.height() < thumbnail.width(),
                      "The copy is scaled down and keeps its proportions");
                c.setProfilePhoto(0, QUrl::fromLocalFile(original).toString());
                const auto replaced = c.profiles()[0].toMap()["photo"].toString();
                check(replaced != stored && !QFileInfo::exists(stored),
                      "Choosing again replaces the copy instead of piling them up");
                QSignalSpy refused(&c, &Controller::message);
                const auto broken = pictures.filePath("cassee.png");
                QFile junk(broken);
                check(junk.open(QIODevice::WriteOnly) && junk.write("pas une image") > 0,
                      "Write a file that is not an image");
                junk.close();
                c.setProfilePhoto(0, QUrl::fromLocalFile(broken).toString());
                check(refused.count() == 1 && refused.first().at(1).toBool() &&
                          c.profiles()[0].toMap()["photo"] == replaced,
                      "An unreadable file is refused and leaves the portrait in place");
                c.setProfilePhoto(0, "");
                check(!c.profiles()[0].toMap().contains("photo") && !QFileInfo::exists(replaced),
                      "Clearing the portrait removes both the setting and the copy");
            }
            // Une cadence cherchée sans y trouver le joueur garde son chiffre, marqué
            // douteux ; une cadence non demandée n'apprend rien et reste intacte.
            c.setFideDetails(0, "1503014", "Nom, Prénom", 2100, 1800, 1950);
            c.applyFideRatings(0, {"standard", "rapid"},
                               {{"found", true}, {"name", "Nom Officiel"}, {"standard", 2150}});
            const auto marked = c.profiles()[0].toMap();
            check(marked["fideStandard"].toInt() == 2150 && !marked["fideStandardStale"].toBool(),
                  "A confirmed rating is updated and trusted");
            check(marked["fideRapid"].toInt() == 1800 && marked["fideRapidStale"].toBool(),
                  "A rating the list no longer confirms is kept and flagged");
            check(marked["fideBlitz"].toInt() == 1950 && !marked["fideBlitzStale"].toBool(),
                  "A speed that was never asked for stays untouched");
            check(marked["fideName"] == "Nom Officiel", "The published name replaces the typed one");
            c.setFideDetails(0, "1503014", "Nom, Prénom", 2100, 1800, 1950);
            check(!c.profiles()[0].toMap()["fideRapidStale"].toBool(),
                  "Typing a rating by hand clears the doubt");
            messages.clear();
            c.setFideDetails(0, "12a", {}, 0, 0, 0);
            check(c.profiles()[0].toMap()["fideId"] == "1503014" && messages.size() == 1,
                  "A FIDE identifier that is not a number is refused");
            c.removeProfile(0);
            check(c.profiles().isEmpty() && c.onlineAccounts().size() == 1 &&
                      c.onlineAccounts()[0].toMap()["profile"].toString().isEmpty(),
                  "Removing a profile detaches its accounts without deleting them");
            c.removeOnlineAccount(0);
        }
        {
            Controller c(dir.path() + "/integrity-ui");
            c.move("e2e4");
            const auto original = c.state();
            QSignalSpy messages(&c, &Controller::message);
            {
                PoolGate gate;
                c.verifyDatabase();
                check(c.busy() && c.taskStatus().contains("Vérification"),
                      "Integrity check returns immediately with a visible verification status");
                bool eventHandled = false;
                QTimer::singleShot(0, &c, [&] { eventHandled = true; });
                QTest::qWait(20);
                check(eventHandled && c.busy(), "GUI events run while verification is pending");
                c.verifyDatabase();
                check(c.busy() && messages.last()[1].toBool(), "Overlapping maintenance is rejected");
                c.cancelTask();
                gate.open();
            }
            QElapsedTimer wait;
            wait.start();
            while (c.busy() && wait.elapsed() < 5000)
                QTest::qWait(5);
            // Le message dépend de la langue depuis qu'il passe par la traduction.
            check(!c.busy() && messages.last()[0].toString() == c.uiText("Vérification interrompue.") &&
                      !messages.last()[1].toBool(),
                  "Canceled verification finishes without claiming integrity success");
            check(c.state()["fen"] == original["fen"] && c.state()["dirty"] == original["dirty"],
                  "Read-only maintenance preserves the unsaved study");
            c.verifyDatabase();
            wait.restart();
            while (c.busy() && wait.elapsed() < 5000)
                QTest::qWait(5);
            check(!c.busy() && messages.last()[0].toString() == c.uiText("Vérification SQLite : ") + "ok" &&
                      !messages.last()[1].toBool(),
                  "A new verification succeeds after cancellation");
            messages.clear();
            bool restarted = false;
            const auto restart = QObject::connect(&c, &Controller::busyChanged, &c, [&] {
                if (!c.busy() && !restarted) {
                    restarted = true;
                    c.verifyDatabase();
                }
            });
            {
                PoolGate gate;
                c.verifyDatabase();
                c.cancelTask();
                gate.open();
            }
            wait.restart();
            while (c.busy() && wait.elapsed() < 5000)
                QTest::qWait(5);
            QObject::disconnect(restart);
            check(restarted && !c.busy() && messages.size() == 2 &&
                      messages[0][0].toString() == c.uiText("Vérification interrompue.") &&
                      messages[1][0].toString() == c.uiText("Vérification SQLite : ") + "ok",
                  "Reentrant restart retains the distinct results of both verifications");
        }
        {
            Controller closing(dir.path() + "/integrity-close");
            closing.verifyDatabase(); // Destructor cancels and joins the worker before releasing state.
        }
        QString expectedFen;
        qint64 savedId = 0;
        {
            Controller c(dir.path());
            QSignalSpy errors(&c, &Controller::message);
            QQmlApplicationEngine engine;
            engine.rootContext()->setContextProperty("controller", &c);
            QSignalSpy warnings(&engine, &QQmlEngine::warnings);
            engine.load(QUrl("qrc:/ui/Main.qml"));
            check(!engine.rootObjects().empty(), "QML failed to load");
            auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
            window->resize(1440, 940);
            window->setProperty("page", 2);
            QTest::qWait(100);
            auto *board = window->findChild<QQuickItem *>("chessboard");
            check(board && board->width() > 300, "Board geometry");
            auto center = [&](int col, int row) {
                return board
                    ->mapToScene(QPointF((col + 0.5) * board->width() / 8, (row + 0.5) * board->width() / 8))
                    .toPoint();
            };
            QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, center(4, 6));
            QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, center(4, 4));
            check(c.state()["node"].toInt() == 1 && !c.state()["whiteTurn"].toBool(), "Click e2-e4");
            QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, center(4, 1));
            QTest::mouseMove(window, center(4, 3), 30);
            QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, center(4, 3));
            check(c.state()["node"].toInt() == 2 && c.state()["whiteTurn"].toBool(), "Drag e7-e5");
            auto *note = window->findChild<QQuickItem *>("positionNote");
            check(note != nullptr, "Note editor exists");
            note->forceActiveFocus();
            note->setProperty("text", "Plan: develop the knight");
            QTest::qWait(20);
            check(c.state()["comment"].toString() == "Plan: develop the knight", "Note binding saves text");
            c.navigate(1);
            check(c.state()["comment"].toString().isEmpty(),
                  "Navigating with note focus must not overwrite next position");
            c.move("c7c5");
            c.setComment("Sicilian branch");
            c.navigate(2);
            check(c.state()["comment"].toString() == "Plan: develop the knight",
                  "Previous note remains intact");
            auto *save = window->findChild<QQuickItem *>("saveStudy");
            check(save != nullptr, "Save button exists");
            QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier,
                              save->mapToScene(QPointF(save->width() / 2, save->height() / 2)).toPoint());
            check(c.stats()["games"].toInt() == 1 && !c.state()["dirty"].toBool(),
                  "Save button persists game");
            window->resize(1366, 768); // Resize immediately after refreshing library/statistics.
            QTest::qWait(30);
            savedId = c.state()["savedId"].toLongLong();
            c.navigate(1);
            c.move("c7c5");
            expectedFen = c.state()["fen"].toString();
            c.createCard("Sicilian"); // No continuation here: rejected without altering data.
            check(c.cards().empty(), "Empty training sequence rejected");
            c.navigate(0);
            c.createCard("Opening");
            check(c.cards().size() == 1, "Create training card");
            auto cardId = c.cards()[0].toMap()["id"].toLongLong();
            c.startCard(cardId);
            check(c.state()["training"].toBool(), "Start training");
            c.move("d2d4");
            check(c.state()["node"].toInt() == 0, "Wrong training move not applied");
            c.move("e2e4");
            check(c.state()["trainingComplete"].toBool(), "Training plays opponent reply and completes");
            check(c.stats()["reviews"].toInt() == 1 && c.stats()["successes"].toInt() == 0,
                  "Training error counted");
            c.move("e2e4");
            check(c.stats()["reviews"].toInt() == 1, "Completed exercise cannot be scored twice");
            c.finishTraining();
            c.navigate(1);
            c.move("c7c5");
            // Rendering all pages and dialogs must produce no QML warnings.
            for (int i = 0; i < 7; ++i) {
                window->setProperty("page", i);
                QTest::qWait(20);
            }
            for (const auto *name : {"downloadsDialog", "advancedDialog", "settingsDialog"}) {
                auto *dialog = window->findChild<QObject *>(name);
                check(dialog != nullptr, "New feature dialog exists");
                QMetaObject::invokeMethod(dialog, "open");
                QTest::qWait(80);
                if (QString(name) == "downloadsDialog") {
                    auto *category = dialog->findChild<QObject *>("contentCategory");
                    auto *list = dialog->findChild<QObject *>("contentList");
                    check(category && list, "Catalogue category controls exist");
                    category->setProperty("currentIndex", 1);
                    QTest::qWait(30);
                    check(list->property("count").toInt() == 22, "All listed world champions are selectable");
                    category->setProperty("currentIndex", 0);
                }
                QMetaObject::invokeMethod(dialog, "close");
                QTest::qWait(10);
            }
            check(warnings.empty(), "QML warnings encountered");
        }
        {
            Controller restored(dir.path());
            check(restored.state()["fen"].toString() == expectedFen, "Session restores branch position");
            check(restored.state()["comment"].toString() == "Sicilian branch",
                  "Session restores branch comment");
            check(restored.state()["savedId"].toLongLong() == savedId, "Session preserves library identity");
            restored.newGame();
            check(restored.stats()["games"].toInt() == 1, "Switching study updates existing game");
        }
        {
            Controller c(dir.path() + "/features");
            QSignalSpy errors(&c, &Controller::message);
            QQmlApplicationEngine engine;
            engine.rootContext()->setContextProperty("controller", &c);
            QSignalSpy warnings(&engine, &QQmlEngine::warnings);
            engine.load(QUrl("qrc:/ui/Main.qml"));
            check(!engine.rootObjects().empty(), "Feature QML loaded");
            auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
            window->resize(1440, 940);
            window->setProperty("page", 2);
            QTest::qWait(50);
            for (const auto &pack : QStringList{"cburnett", "merida", "spatial", "pixel"}) {
                for (QChar color : QString("wb"))
                    for (QChar piece : QString("KQRBNP"))
                        check(!QImage(":/assets/pieces/" + pack + "/" + color + piece + ".svg").isNull(),
                              "Every piece SVG renders");
                c.setPreference("pieceStyle", pack);
                QTest::qWait(10);
            }
            c.applyBoardPreset("Océan");
            c.setPreference("backgroundColor", "#243257");
            c.setPreference("boardLight", "#badbad");
            check(c.preferences()["background"].toString() == "#243257" &&
                      c.preferences()["boardLight"].toString() == "#badbad",
                  "Custom colors applied");
            c.setPreference("boardLight", "invalid");
            check(c.preferences()["boardLight"].toString() == "#badbad",
                  "Invalid color cannot corrupt settings");
            c.move("e2e4");
            c.move("e7e5");
            check(!c.state()["opening"].toString().isEmpty(), "Offline ECO dictionary available");
            auto endFen = c.state()["fen"].toString();
            c.undo();
            check(c.state()["node"].toInt() == 1, "Undo move");
            c.redo();
            check(c.state()["fen"].toString() == endFen, "Redo move");
            c.toggleAnnotation("e2", "e4", "R");
            c.toggleAnnotation("e4", "e4", "B");
            check(c.state()["arrows"].toList().size() == 2, "Persistent arrow and circle");
            c.saveGame();
            auto studyId = c.state()["savedId"].toLongLong();
            c.newGame();
            check(c.tabs().size() == 2, "New study creates tab");
            c.move("d2d4");
            c.switchTab(0);
            check(c.state()["fen"].toString() == endFen && c.state()["arrows"].toList().size() == 2,
                  "Tab preserves position and annotations");
            c.loadGame(studyId);
            check(c.tabs().size() == 2, "Opening existing tab avoids duplicate tab");
            c.clearAnnotations();
            check(c.state()["arrows"].toList().empty(), "Clear annotations");
            c.undo();
            check(c.state()["arrows"].toList().size() == 2, "Undo restores annotations");
            c.setPreference("enginePath", qEnvironmentVariable("T64_STOCKFISH", "/usr/games/stockfish"));
            // Le nombre de fils dépend désormais de la machine : comparer à la valeur
            // observée, et non à une constante, sinon le test suit le matériel du runner.
            const int threads = c.preferences()["threads"].toInt();
            c.saveEngineProfile("Test");
            c.setPreference("threads", threads == 1 ? 2 : 1);
            c.loadEngineProfile(0);
            check(c.preferences()["threads"].toInt() == threads, "Engine profile restores settings");
            if (QFileInfo(c.preferences()["enginePath"].toString()).isExecutable()) {
                QElapsedTimer optionsTimer;
                optionsTimer.start();
                while (c.engineOptions().isEmpty() && optionsTimer.elapsed() < 5000)
                    QTest::qWait(20);
                check(!c.engineOptions().isEmpty(), "Controller exposes UCI options");
                c.setEngineOption("UCI_Elo", 1500);
                c.saveEngineProfile("Test");
                c.setEngineOption("UCI_Elo", 2000);
                c.setPreference("hash", 16);
                c.setPreference("analysisDepth", 8);
                c.saveEngineProfile("Second test");
                c.loadEngineProfile(0);
                check(c.preferences()["uciOptions"].toMap()["UCI_Elo"].toInt() == 1500,
                      "First profile preserves custom option");
                c.loadEngineProfile(1);
                check(c.preferences()["uciOptions"].toMap()["UCI_Elo"].toInt() == 2000,
                      "Second profile has independent custom option");
                c.compareEngines({0, 1});
                QElapsedTimer comparisonTimer;
                comparisonTimer.start();
                while (comparisonTimer.elapsed() < 7000) {
                    const auto rows = c.comparison()->rows();
                    if (rows.size() == 2 && !rows[0].toMap()["lines"].toList().empty() &&
                        !rows[1].toMap()["lines"].toList().empty())
                        break;
                    QTest::qWait(20);
                }
                check(c.comparison()->running() &&
                          !c.comparison()->rows().last().toMap()["lines"].toList().empty(),
                      "Controller starts independent engine profiles");
                auto *compareDialog = window->findChild<QObject *>("compareDialog");
                QMetaObject::invokeMethod(compareDialog, "open");
                QTest::qWait(80);
                QMetaObject::invokeMethod(compareDialog, "close");
                c.comparison()->stop();
                c.analyseGame(100);
                QElapsedTimer timer;
                timer.start();
                while (c.reviewState()["running"].toBool() && timer.elapsed() < 12000)
                    QTest::qWait(20);
                if (c.reviewState()["running"].toBool() || c.review().size() != 2)
                    qWarning() << "Review state" << c.reviewState() << "rows" << c.review().size()
                               << "messages" << errors;
                check(!c.reviewState()["running"].toBool() && c.review().size() == 2,
                      "Full game review completes with Stockfish");
                check(c.reviewState()["passes"] == 2 && c.reviewState()["done"] == c.reviewState()["total"] &&
                          c.reviewState()["total"].toInt() == 2 * (c.review().size() + 1),
                      "Review runs a forward pass, then a backward pass");
                bool backward = true;
                for (const auto &row : c.review())
                    backward &= row.toMap()["pass"] == 2;
                check(backward, "Final evaluations come from the end-to-start pass");
                c.analyseGame(100, false);
                timer.restart();
                while (c.reviewState()["running"].toBool() && timer.elapsed() < 12000)
                    QTest::qWait(20);
                check(c.reviewState()["passes"] == 1 && c.review().size() == 2 &&
                          c.review()[0].toMap()["pass"] == 1,
                      "A single forward pass stays available");
                // Les explications sont traduites : on les lit ici en français, la langue des
                // textes attendus, quelle que soit celle de la machine de test.
                c.setPreference("language", "fr");
                const auto reviewBeforeExplanation = c.review();
                const auto beforeExplanation = c.state();
                const auto actual = c.explainReview(0, false);
                const auto alternative = c.explainReview(0, true);
                check(actual["available"].toBool() && alternative["available"].toBool() &&
                          !actual["steps"].toList().empty() && !alternative["steps"].toList().empty() &&
                          actual["steps"].toList().first().toMap()["uci"] == c.review()[0].toMap()["played"],
                      "Real Stockfish review explains played move and pre-move alternative legally");
                check(!actual["why"].toString().isEmpty() && actual["why"] == alternative["why"],
                      "Every explained move carries a verdict drawn from both lines");
                check(!c.explainReview(-1)["available"].toBool() &&
                          !c.explainReview(999)["available"].toBool(),
                      "Unavailable review rows cannot fabricate explanations");
                {
                    auto *coordinates = window->findChild<QObject *>("coordinatesDialog");
                    check(coordinates && QMetaObject::invokeMethod(coordinates, "open") &&
                              QMetaObject::invokeMethod(coordinates, "start"),
                          "The coordinates exercise opens and starts");
                    QTest::qWait(50);
                    const auto target = coordinates->property("target").toString();
                    QVariant right, wrong, name;
                    QMetaObject::invokeMethod(coordinates, "answer", Q_RETURN_ARG(QVariant, wrong),
                                              Q_ARG(QVariant, target == "a1" ? "h8" : "a1"));
                    QMetaObject::invokeMethod(coordinates, "answer", Q_RETURN_ARG(QVariant, right),
                                              Q_ARG(QVariant, target));
                    check(target.size() == 2 && !wrong.toBool() && right.toBool() &&
                              coordinates->property("score") == 1 && coordinates->property("errors") == 1 &&
                              coordinates->property("target").toString() != target,
                          "A right square scores and asks another; a wrong one counts as a mistake");
                    QMetaObject::invokeMethod(coordinates, "stop");
                    coordinates->setProperty("blackSide", true);
                    QMetaObject::invokeMethod(coordinates, "squareName", Q_RETURN_ARG(QVariant, name),
                                              Q_ARG(QVariant, 0));
                    check(coordinates->property("best") == 1 && name == "h1" &&
                              !coordinates->property("running").toBool(),
                          "Stopping keeps the best run, and Black's view starts from h1");
                    coordinates->setProperty("blackSide", false);
                    QMetaObject::invokeMethod(coordinates, "close");
                    QTest::qWait(50);
                }
                auto *reviewDialog = window->findChild<QObject *>("reviewDialog");
                QMetaObject::invokeMethod(reviewDialog, "open");
                QMetaObject::invokeMethod(reviewDialog, "explain", Q_ARG(QVariant, 0));
                QTest::qWait(80);
                auto *explanationDialog = window->findChild<QObject *>("reviewExplanationDialog");
                check(explanationDialog && explanationDialog->property("visible").toBool() &&
                          explanationDialog->property("details").toMap()["available"].toBool(),
                      "Explanation dialog opens with real analysis facts");
                if (qEnvironmentVariableIsSet("T64_EXPLANATION_SCREENSHOT")) {
                    window->resize(960, 640);
                    QTest::qWait(100);
                    check(window->grabWindow().save(qEnvironmentVariable("T64_EXPLANATION_SCREENSHOT")),
                          "Explanation screenshot saved");
                }
                auto *lineChoice = window->findChild<QObject *>("reviewLineChoice");
                lineChoice->setProperty("currentIndex", 1);
                QTest::qWait(30);
                check(explanationDialog->property("details").toMap()["uci"] == alternative["uci"],
                      "Changing the explanation choice displays the pre-move engine line");
                lineChoice->setProperty("currentIndex", 0);
                QTest::qWait(30);
                int originalTab = 0;
                for (int i = 0; i < c.tabs().size(); ++i)
                    if (c.tabs()[i].toMap()["active"].toBool())
                        originalTab = i;
                const auto originalTabs = c.tabs().size();
                auto *openLine = window->findChild<QQuickItem *>("openReviewLineButton");
                check(openLine && openLine->isVisible() && openLine->isEnabled(),
                      "Review line button is accessible");
                QTest::mouseClick(
                    window, Qt::LeftButton, Qt::NoModifier,
                    openLine->mapToScene(QPointF(openLine->width() / 2, openLine->height() / 2)).toPoint());
                QTest::qWait(60);
                check(c.tabs().size() == originalTabs + 1 && !explanationDialog->property("visible").toBool(),
                      "Clicking the review line button opens an independent study and closes the dialog");
                c.step(2);
                check(c.state()["fen"] == actual["steps"].toList().last().toMap()["fen"],
                      "Opened study reaches the exact explained terminal position");
                c.switchTab(originalTab);
                check(c.state()["fen"] == beforeExplanation["fen"] && c.review() == reviewBeforeExplanation,
                      "Returning to original study preserves position and review results");
                c.closeTab(originalTabs);
                // The first played move is known, independent of Stockfish's chosen reply.
                const QStringList motifFens{"4k3/5q2/8/8/2N5/8/8/4K3 w - - 0 1",
                                            "4k3/4n3/8/8/8/8/8/K1R5 w - - 0 1"};
                const QStringList motifMoves{"c4d6", "c1e1"};
                const QStringList motifLabels{"Fourchette", "Clouage au roi"};
                const QStringList motifMarks{"[%cal Gd6e8,Gd6f7]", "[%csl Ye1,Ye7,Ye8]"};
                for (int fixture = 0; fixture < motifFens.size(); ++fixture) {
                    const int fixtureTab = c.tabs().size();
                    c.setPosition(motifFens[fixture], false);
                    c.move(motifMoves[fixture]);
                    c.setComment("Original tactical study");
                    const auto fixtureFen = c.state()["fen"];
                    c.analyseGame(100);
                    timer.restart();
                    while (c.reviewState()["running"].toBool() && timer.elapsed() < 12000)
                        QTest::qWait(20);
                    check(!c.reviewState()["running"].toBool() && c.review().size() == 1,
                          "Stockfish completes the tactical fixture review");
                    const auto fixtureReview = c.review();
                    const auto details = c.explainReview(0, false);
                    const auto steps = details["steps"].toList();
                    check(details["available"].toBool() && !steps.empty(), "Tactical explanation available");
                    const auto first = steps.first().toMap();
                    check(first["facts"].toString().contains(motifLabels[fixture]) &&
                              first["facts"].toString().contains("aucun gain matériel forcé") &&
                              first["marks"].toString() == motifMarks[fixture],
                          "Played tactical move has exact marks and cautious explanation");
                    window->resize(960, 640);
                    QMetaObject::invokeMethod(reviewDialog, "open");
                    QMetaObject::invokeMethod(reviewDialog, "explain", Q_ARG(QVariant, 0));
                    QTest::qWait(100);
                    if (fixture == 0 && qEnvironmentVariableIsSet("T64_TACTICS_SCREENSHOT"))
                        check(window->grabWindow().save(qEnvironmentVariable("T64_TACTICS_SCREENSHOT")),
                              "Small-screen tactical explanation screenshot saved");
                    check(openLine->isVisible() && openLine->isEnabled(),
                          "Tactical copy button accessible at 960x640");
                    const int copyTab = c.tabs().size();
                    QTest::mouseClick(
                        window, Qt::LeftButton, Qt::NoModifier,
                        openLine->mapToScene(QPointF(openLine->width() / 2, openLine->height() / 2))
                            .toPoint());
                    QTest::qWait(80);
                    check(c.tabs().size() == copyTab + 1 && !explanationDialog->property("visible").toBool(),
                          "Real click creates tactical copy");
                    c.navigate(1);
                    const auto marks = c.state()["arrows"].toList();
                    check(c.state()["comment"].toString().contains(motifLabels[fixture]) &&
                              c.state()["comment"].toString().contains(motifMarks[fixture]) &&
                              marks.size() == (fixture == 0 ? 2 : 3),
                          "Copy displays tactical text and parsed board marks");
                    for (const auto &mark : marks) {
                        const auto m = mark.toMap();
                        check(fixture == 0 ? m["color"] == "G" && m["from"] == "d6" &&
                                                 (m["to"] == "e8" || m["to"] == "f7")
                                           : m["color"] == "Y" && m["from"] == m["to"],
                              "Attacks are arrows and pin alignments are square marks");
                    }
                    QTest::qWait(50);
                    if (fixture == 0 && qEnvironmentVariableIsSet("T64_TACTICS_BOARD_SCREENSHOT"))
                        check(window->grabWindow().save(qEnvironmentVariable("T64_TACTICS_BOARD_SCREENSHOT")),
                              "Tactical board screenshot saved");
                    const auto exportPath = dir.path() + "/tactical-" + QString::number(fixture) + ".pgn";
                    c.exportFile(exportPath);
                    QFile exported(exportPath);
                    check(exported.open(QIODevice::ReadOnly), "Tactical copy exported");
                    const auto restored = t64::parsePgn(exported.readAll().toStdString());
                    check(restored.size() == 1 && restored[0].nodes[1].comment.find(
                                                      motifMarks[fixture].toStdString()) != std::string::npos,
                          "Exported tactical copy retains graphical annotations");
                    c.switchTab(fixtureTab);
                    check(c.state()["fen"] == fixtureFen && c.review() == fixtureReview &&
                              c.state()["comment"] == "Original tactical study" &&
                              c.state()["arrows"].toList().empty(),
                          "Original tactical study and its review remain untouched");
                    c.closeTab(copyTab);
                    c.switchTab(originalTab);
                    c.closeTab(fixtureTab);
                }
                {
                    // Le pourquoi : un cavalier posé là où un pion le prend.
                    const int whyTab = c.tabs().size();
                    c.setPosition("4k3/ppp5/3p4/8/8/5N2/PPP5/4K3 w - - 0 1", false);
                    c.move("f3e5");
                    c.analyseGame(100);
                    timer.restart();
                    while (c.reviewState()["running"].toBool() && timer.elapsed() < 12000)
                        QTest::qWait(20);
                    const auto details = c.explainReview(0, false);
                    const auto why = details["why"].toString();
                    check(why.contains("en prise") && why.contains("cavalier en e5") &&
                              why.contains("dxe5") && why.contains("point(s) de matériel"),
                          "The review says why: the knight was left en prise and the pawn takes it");
                    const auto first = details["steps"].toList().value(0).toMap();
                    check(first["facts"].toString().contains("Pièce en prise") &&
                              first["marks"].toString() == "[%cal Rd6e5]",
                          "The hanging piece is named in the steps and drawn in red");
                    check(!why.contains("Sur l’échiquier"),
                          "A knight move that changes no pawn says nothing of structure");
                    check(c.explainReview(0, true)["why"] == details["why"],
                          "The verdict is the same whichever line is displayed");
                    c.switchTab(originalTab);
                    c.closeTab(whyTab);
                }
                c.setPreference("language", "en");
                check(c.explainReview(0, false)["steps"].toList().value(0).toMap()["side"] == "White",
                      "Explanations follow the interface language");
                c.setPreference("language", "auto");
                window->resize(1440, 940);
                c.annotateReview();
                check(c.state()["comment"].toString().contains("[%eval"),
                      "Review annotations persist to PGN");
                c.analyseGame(500);
                c.newGame();
                QTest::qWait(80);
                check(!c.reviewState()["running"].toBool() && c.review().empty(),
                      "Changing study cancels review and clears stale results");
                c.move("e2e4");
                c.move("e7e5");
                c.navigate(1);
                c.move("c7c5");
                c.setComment("Before queue snapshot");
                c.enqueueAnalysis(true, 100);
                c.move("g1f3");
                c.setComment("Study edited while analysis runs");
                const auto editingFen = c.state()["fen"];
                timer.restart();
                while (timer.elapsed() < 10000 &&
                       (c.analysisQueue()->jobs().empty() ||
                        c.analysisQueue()->jobs().first().toMap()["status"] == "queued" ||
                        c.analysisQueue()->jobs().first().toMap()["status"] == "running"))
                    QTest::qWait(20);
                check(!c.analysisQueue()->jobs().empty() &&
                          c.analysisQueue()->jobs().first().toMap()["status"] == "completed" &&
                          c.analysisQueue()->jobs().first().toMap()["total"].toInt() == 4 &&
                          c.state()["fen"] == editingFen &&
                          c.state()["comment"] == "Study edited while analysis runs",
                      "Background branch analysis leaves the current study intact");
                auto *queueDialog = window->findChild<QObject *>("analysisQueueDialog");
                const auto previousSize = window->size();
                window->resize(960, 720);
                // L'écran lui-même avant d'ouvrir quoi que ce soit : c'est la seule façon
                // de juger sa mise en page autrement qu'en la relisant.
                const auto screen = qEnvironmentVariable("T64_BOARD_SCREENSHOT");
                if (!screen.isEmpty()) {
                    auto board = window->contentItem()->grabToImage();
                    check(bool(board), "Prepare analysis screen capture");
                    QSignalSpy drawn(board.data(), &QQuickItemGrabResult::ready);
                    check(drawn.wait(3000) && board->saveToFile(screen), "Capture the analysis screen");
                }
                QMetaObject::invokeMethod(queueDialog, "open");
                QTest::qWait(80);
                const auto screenshot = qEnvironmentVariable("T64_ANALYSIS_SCREENSHOT");
                if (!screenshot.isEmpty()) {
                    auto capture = window->contentItem()->grabToImage();
                    check(bool(capture), "Prepare populated analysis capture");
                    QSignalSpy ready(capture.data(), &QQuickItemGrabResult::ready);
                    check(ready.wait(3000) && capture->saveToFile(screenshot),
                          "Capture populated background analysis dialog");
                }
                QMetaObject::invokeMethod(queueDialog, "close");
                window->resize(previousSize);
                c.closeTab(c.state()["activeTab"].toInt());
                c.switchTab(0);
            }
            c.exportStudyPdf(dir.path() + "/study.pdf");
            QFile pdf(dir.path() + "/study.pdf");
            check(pdf.open(QIODevice::ReadOnly) && pdf.read(5) == "%PDF-", "PDF study exported");
            c.exportDiagram(dir.path() + "/diagram.png");
            QImage diagram(dir.path() + "/diagram.png");
            check(!diagram.isNull() && diagram.size() == QSize(832, 832), "Styled PNG diagram exported");
            c.identifyOpening();
            check(!c.state()["tags"].toMap()["ECO"].toString().isEmpty(), "Opening tags persisted");
            c.newChess960(518);
            check(c.state()["chess960"].toBool(), "Create Chess960 study");
            c.setPosition("k7/8/8/8/8/8/8/6KR w H - 0 1", true);
            c.clickSquare("g1");
            c.clickSquare("h1");
            check(c.state()["node"].toInt() == 1, "Chess960 castle by clicking own rook");
            c.newGame();
            c.startLocalGame(std::numeric_limits<int>::max(), std::numeric_limits<int>::max(), 0, true);
            check(c.state()["tags"].toMap()["TimeControl"] == "10800+120",
                  "Clock metadata uses bounded values without integer overflow");
            c.stopPlaying();
            c.startLocalGame(1, 2, 0, true);
            QTest::qWait(150);
            check(c.state()["clockRunning"].toBool(), "Local clock starts");
            c.move("e2e4");
            check(c.state()["whiteClock"].toString() == "1:01", "Increment applied after completed move");
            c.stopPlaying();
            check(!c.state()["clockRunning"].toBool(), "Stopping game stops clock");
            // Exercise every new dialog including small window layout and keyboard capture.
            for (const auto &name :
                 QStringList{"appearanceDialog", "positionEditor", "reviewDialog", "settingsDialog",
                             "updatesDialog", "compareDialog", "tournamentDialog", "analysisQueueDialog"}) {
                QObject *dialog = window->findChild<QObject *>(name);
                check(dialog, "Feature dialog exists");
                QMetaObject::invokeMethod(dialog, "open");
                QTest::qWait(30);
                QMetaObject::invokeMethod(dialog, "close");
            }
            window->resize(960, 640);
            for (int page = 0; page < 7; ++page) {
                auto *nav = visualItem(window->contentItem(), "sectionNav" + QString::number(page));
                check(nav && nav->mapToScene(QPointF(nav->width(), nav->height())).x() <= window->width(),
                      "Numbered navigation remains visible in a small window");
                QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier,
                                  nav->mapToScene(QPointF(nav->width() / 2, nav->height() / 2)).toPoint());
                QTest::qWait(20);
                check(window->property("page").toInt() == page, "Navigation opens the requested section");
                if (page == 2) {
                    auto *panel = window->findChild<QQuickItem *>("analysisPanel");
                    check(panel && panel->mapToScene(QPointF(panel->width(), panel->height())).x() <=
                                       window->width(),
                          "Analysis panel fits the smallest supported window");
                    auto *board = window->findChild<QQuickItem *>("chessboard");
                    check(board && board->width() >= 200 &&
                              board->mapToScene(QPointF(board->width(), board->height())).y() <
                                  window->height(),
                          "Board remains usable at minimum window height");
                    auto *actions = window->findChild<QQuickItem *>("positionActions");
                    check(actions && actions->mapToScene(QPointF(actions->width(), actions->height())).y() <=
                                         window->height() - 32,
                          "Position tools fit above the footer");
                }
            }
            const auto savedBackground = c.preferences()["backgroundColor"];
            c.setPreference("lightTheme", true);
            c.setPreference("backgroundColor", "");
            window->setProperty("page", 0);
            QTest::qWait(30);
            check(c.preferences()["accentColor"].toString() == "#2f66ad", "Light default accent is readable");
            c.setPreference("accentColor", "#315ac4");
            c.setPreference("lightTheme", false);
            check(c.preferences()["accentColor"].toString() == "#315ac4",
                  "User accent survives theme changes");
            c.setPreference("backgroundColor", savedBackground);
            check(warnings.empty(), "New pages and dialogs have no QML warnings");
            c.switchTab(0);
            c.saveGame();
            const auto backupFolder = c.updater()->beforeInstall();
            check(QFileInfo::exists(backupFolder + "/session.json") &&
                      QFileInfo::exists(backupFolder + "/preferences.json"),
                  "Pre-update backup includes session and preferences");
            {
                t64::Database snapshot(backupFolder + "/library.sqlite");
                check(snapshot.stats()["games"] == c.stats()["games"] && snapshot.integrity() == "ok",
                      "Pre-update database backup is coherent and complete");
            }
            c.automaticBackup();
            QElapsedTimer backupTimer;
            backupTimer.start();
            while (!c.preferences()["lastBackup"].toDateTime().isValid() && backupTimer.elapsed() < 5000)
                QTest::qWait(20);
            check(c.preferences()["lastBackup"].toDateTime().isValid(),
                  "Background automatic backup completes");
        }
        {
            Controller c(dir.path() + "/features");
            check(c.tabs().size() >= 3, "All study tabs recovered");
            check(c.state()["node"].toInt() == 2, "Active tab position recovered");
            check(c.preferences()["background"].toString() == "#243257", "Appearance survives restart");
            check(c.state()["arrows"].toList().size() == 2, "PGN arrows survive restart");
        }
        {
            Controller c(dir.path() + "/search");
            QSignalSpy errors(&c, &Controller::message);
            const auto pgn = dir.path() + "/search.pgn";
            QFile fixture(pgn);
            check(fixture.open(QIODevice::WriteOnly), "Open paginated PGN fixture");
            for (int i = 0; i < 405; ++i)
                fixture.write(QString("[Event \"Fixture %1\"]\n[White \"Alice\"]\n[Black \"Rival\"]\n"
                                      "[ECO \"%2\"]\n\n1.e4 %3 *\n\n")
                                  .arg(i)
                                  .arg(i % 2 ? "C00" : "B90")
                                  .arg(i % 2 ? "e6" : "c5")
                                  .toUtf8());
            fixture.close();
            emit c.content()->ready(pgn, {{"id", "local-fixture"}, {"title", "Search base"}});
            auto waitSearch = [&] {
                check(QTest::qWaitFor([&] { return !c.busy() && !c.librarySearching(); }, 5000),
                      "Background search finishes");
            };
            waitSearch();
            check(c.sources().size() == 1 && c.importHistory().size() == 1 &&
                      c.stats()["games"].toInt() == 405,
                  "Downloaded source enters real import pipeline");
            const auto sourceId = c.sources().first().toMap()["id"];
            c.searchLibrary({}, {}, false, false, false, {{"sourceId", sourceId}});
            waitSearch();
            const auto first = c.games();
            check(first.size() == 200 && c.libraryHasNext() && c.libraryPage() == 1,
                  "Library starts with one bounded page");
            c.nextLibraryPage();
            waitSearch();
            const auto second = c.games();
            check(second.size() == 200 && c.libraryHasNext() && c.libraryPage() == 2 &&
                      second.first().toMap()["id"] != first.first().toMap()["id"],
                  "Next cursor page");
            c.nextLibraryPage();
            waitSearch();
            check(c.games().size() == 5 && !c.libraryHasNext() && c.libraryPage() == 3,
                  "Last page has no phantom next page");
            c.previousLibraryPage();
            waitSearch();
            check(c.games() == second, "Previous page restores its exact cursor");
            for (int i = 0; i < 25; ++i)
                c.searchLibrary("missing " + QString::number(i), {}, false, false, false, {});
            c.searchLibrary("Alice", {}, false, false, false,
                            {{"sourceId", sourceId},
                             {"openingName", "sicilienne"},
                             {"player", "Rival"},
                             {"colour", "black"}});
            waitSearch();
            check(c.games().size() == 200 && c.libraryHasNext() && c.libraryPage() == 1,
                  "Only the latest rapid combined search is displayed");
            for (const auto &row : c.games())
                check(row.toMap()["eco"] == "B90", "French opening alias limits ECO family");
            c.nextLibraryPage();
            waitSearch();
            check(c.games().size() == 3 && !c.libraryHasNext(), "Opening filter survives pagination");
            c.searchLibrary("Alice", {}, false, false, false, {});
            c.cancelLibrarySearch();
            QTest::qWait(80);
            check(!c.librarySearching() && c.games().empty() && !c.libraryHasNext(),
                  "Canceled result cannot repopulate the library");
            c.searchLibrary("Fixture 404", {}, false, false, false, {});
            waitSearch();
            check(c.games().size() == 1, "A new search works after cancellation");
            c.searchAdvanced({{"ecoFrom", "invalid"}});
            waitSearch();
            // Le texte dépend de la langue depuis qu'il passe par la traduction : on
            // vérifie qu'il dit l'échec, pas qu'il le dit en français.
            check(c.games().empty() && c.librarySearchStatus() == c.uiText("Recherche impossible."),
                  "Worker error clears stale rows and reaches the user");
            c.searchLibrary({}, {}, false, false, false, {{"openingName", "française"}});
            waitSearch();
            check(c.games().size() == 200 && c.games().first().toMap()["eco"] == "C00",
                  "Search recovers after an invalid filter");
            // Une famille se cherche en début de nom, un fragment de variante n'importe
            // où : « Najdorf » n'apparaît qu'au milieu de « Sicilian Defense: Najdorf
            // Variation ». Les deux branches sont éprouvées, car une variable de boucle
            // masquait le drapeau qui les distingue et la seconde ne servait jamais.
            const auto variant = c.openingFilters({{"openingName", "najdorf"}});
            check(variant["openingCodes"].toStringList().contains("B90"),
                  "A variant fragment is found in the middle of an opening name");
            const auto whole = c.openingFilters({{"openingName", "sicilienne"}});
            check(whole["openingCodes"].toStringList().contains("B90") &&
                      !whole["openingCodes"].toStringList().contains("C00"),
                  "A family name matches the start of the name and nothing else");
            check(Controller::lichessStudy("https://lichess.org/study/AbCd1234") ==
                          QStringList{"AbCd1234", ""} &&
                      Controller::lichessStudy(" lichess.org/study/AbCd1234/WxYz5678#12 ") ==
                          QStringList{"AbCd1234", "WxYz5678"} &&
                      Controller::lichessStudy("https://lichess.org/study").isEmpty() &&
                      Controller::lichessStudy("https://lichess.org/AbCd1234").isEmpty() &&
                      Controller::lichessStudy("https://evil.test/lichess.org/study/AbCd1234").isEmpty(),
                  "A Lichess study link names the study and, if present, one chapter");
            check(t64::OnlineGames::studyUrl("AbCd1234", "").toString() ==
                          "https://lichess.org/api/study/"
                          "AbCd1234.pgn?comments=true&variations=true&clocks=false" &&
                      t64::OnlineGames::studyUrl("AbCd1234", "WxYz5678").path() ==
                          "/api/study/AbCd1234/WxYz5678.pgn" &&
                      !t64::OnlineGames::studyUrl("../secret", "").isValid() &&
                      !t64::OnlineGames::studyUrl("AbCd1234", "x").isValid(),
                  "A study is fetched from the PGN export, with comments and variations, and nothing else");
            check(Controller::lichessGameId("https://lichess.org/AbCd1234") == "AbCd1234" &&
                      Controller::lichessGameId(" lichess.org/AbCd1234/black#42 ") == "AbCd1234" &&
                      Controller::lichessGameId("https://lichess.org/AbCd1234wxyz") == "AbCd1234" &&
                      Controller::lichessGameId("https://lichess.org/training").isEmpty() &&
                      Controller::lichessGameId("https://lichess.org/@/someone").isEmpty() &&
                      Controller::lichessGameId("1. e4 e5 https://lichess.org/AbCd1234").isEmpty() &&
                      Controller::lichessGameId("https://evil.test/lichess.org/AbCd1234").isEmpty(),
                  "A pasted Lichess game link is recognised, and nothing else is");
            // Parcourir la liste depuis l'analyse : l'aperçu lit la partie sans la
            // charger, et la navigation la remplace dans l'onglet courant.
            c.searchLibrary("Fixture 404", {}, false, false, false, {});
            waitSearch();
            {
                const auto only = c.games().first().toMap()["id"].toLongLong();
                const auto line = c.gameLine(only);
                check(line.size() > 1 && line.first().toMap()["san"].toString().isEmpty() &&
                          !line.at(1).toMap()["fen"].toString().isEmpty(),
                      "A preview line starts at the initial position and carries each FEN");
                check(c.squaresForFen(line.at(1).toMap()["fen"].toString(),
                                      line.at(1).toMap()["uci"].toString())
                              .size() == 64,
                      "A preview position fills the sixty-four squares");
                check(c.gameLine(-1).isEmpty(), "An unknown game previews as nothing at all");
            }
            c.searchLibrary({}, {}, false, false, false, {});
            waitSearch();
            const int listed = c.games().size();
            check(listed > 2, "The library lists enough games to walk through");
            c.loadGame(c.games().first().toMap()["id"].toLongLong());
            const int tabs = c.tabs().size();
            check(c.state()["gameIndex"].toInt() == 0 && c.state()["gameCount"].toInt() == listed,
                  "Analysis says where the open game sits in the list");
            c.stepGame(1);
            check(c.state()["gameIndex"].toInt() == 1 && c.tabs().size() == tabs,
                  "The next game takes the place of the open one instead of opening a tab");
            c.stepGame(-1);
            check(c.state()["gameIndex"].toInt() == 0, "And the previous one comes back");
            {
                QSignalSpy edge(&c, &Controller::message);
                c.stepGame(-1);
                check(edge.size() == 1 && edge.first().at(1).toBool() && c.state()["gameIndex"].toInt() == 0,
                      "The first game refuses to step back, and says so");
            }
            // Destruction also cancels and joins any outstanding worker.
            c.searchLibrary("Rival", {}, false, false, false, {});
        }
        {
            const auto folder = dir.path() + "/imports";
            QDir().mkpath(folder);
            const auto pgn = folder + "/local.pgn";
            QFile source(pgn);
            check(source.open(QIODevice::WriteOnly), "Create resumable GUI fixture");
            for (int i = 0; i < 15; ++i)
                source.write(QByteArray("[Event \"Resume ") + QByteArray::number(i) + "\"]\n1.e4 e5 *\n");
            source.close();
            auto waitImport = [](Controller &c) {
                QElapsedTimer timeout;
                timeout.start();
                while (c.busy() && timeout.elapsed() < 10000)
                    QTest::qWait(10);
                check(!c.busy(), "Import worker completed");
            };
            qint64 job = 0;
            {
                Controller c(folder);
                c.importResumable(pgn);
                check(c.resumableImport(), "Resumable worker state exposed");
                c.cancelTask();
                waitImport(c);
                job = c.importJobs().first().toMap()["id"].toLongLong();
                check(c.importJobs().first().toMap()["status"] == "paused", "GUI pauses import");
            }
            Controller c(folder);
            QQmlApplicationEngine engine;
            engine.rootContext()->setContextProperty("controller", &c);
            QSignalSpy warnings(&engine, &QQmlEngine::warnings);
            engine.load(QUrl("qrc:/ui/Main.qml"));
            check(!engine.rootObjects().empty(), "Import QML loads");
            auto *dialog = engine.rootObjects().first()->findChild<QObject *>("importsDialog");
            check(dialog && QMetaObject::invokeMethod(dialog, "open"), "Import manager opens");
            c.resumeImport(job);
            waitImport(c);
            QTest::qWait(30);
            check(c.importJobs().first().toMap()["status"] == "completed" &&
                      c.stats()["games"].toInt() == 15 && c.sources().first().toMap()["games"].toInt() == 15,
                  "GUI resumes persistent job and refreshes source/game lists");
            check(c.importProgress()["job"].toLongLong() == job, "Detailed progress belongs to current job");
            check(warnings.empty(), "Import dialog has no QML binding errors");
            c.forgetImport(job);
            check(c.importJobs().empty() && c.stats()["games"].toInt() == 15,
                  "GUI removes checkpoint without deleting games");
        }
        {
            Controller c(dir.path() + "/repertoire-ui");
            auto waitRepertoire = [&] {
                QElapsedTimer timeout;
                timeout.start();
                while ((c.busy() || c.repertoireState()["loading"].toBool()) && timeout.elapsed() < 10000)
                    QTest::qWait(5);
                check(!c.busy() && !c.repertoireState()["loading"].toBool(), "Repertoire workers completed");
            };
            c.move("e2e4");
            c.navigate(0);
            c.move("d2d4");
            c.saveGame();
            c.flip();
            c.createRepertoire("Débuts blancs", true);
            check(c.busy() && c.repertoireTask()["running"].toBool(),
                  "Repertoire creation runs off GUI thread");
            waitRepertoire();
            check(c.repertoires().size() == 1 && c.repertoireState()["positions"].toInt() == 1,
                  "Create named repertoire from study alternatives");
            auto id = c.repertoireState()["id"].toLongLong();
            QQmlApplicationEngine engine;
            engine.rootContext()->setContextProperty("controller", &c);
            QSignalSpy warnings(&engine, &QQmlEngine::warnings);
            engine.load(QUrl("qrc:/ui/Main.qml"));
            check(!engine.rootObjects().empty(), "Repertoire QML loads");
            auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
            window->resize(960, 640);
            auto *dialog = window->findChild<QObject *>("repertoiresDialog");
            check(dialog && QMetaObject::invokeMethod(dialog, "open"), "Repertoire manager opens");
            QTest::qWait(30);
            auto *selector = window->findChild<QQuickItem *>("repertoireSelector");
            check(selector && selector->width() > 100, "Repertoire selector renders at minimum window size");
            check(selector->property("currentText").toString() == "Débuts blancs",
                  "Selected repertoire name remains visible when dialog opens");
            const auto capture = qEnvironmentVariable("T64_REPERTOIRE_SCREENSHOT");
            if (!capture.isEmpty()) {
                const auto grab = window->contentItem()->grabToImage();
                QSignalSpy ready(grab.data(), &QQuickItemGrabResult::ready);
                check(ready.wait(3000), "Repertoire screenshot rendered");
                check(grab->saveToFile(capture), "Repertoire screenshot saved");
            }
            QMetaObject::invokeMethod(dialog, "close");
            // L'état de référence se lit juste avant l'exercice, et non vingt lignes plus
            // haut : la création du répertoire travaille hors du fil graphique, et les
            // interactions qui suivent peuvent déplacer l'étude courante. Capturé trop
            // tôt, il décrivait parfois autre chose que ce que l'exercice allait
            // remplacer, et la vérification échouait au gré du rythme de la machine.
            const auto previous = c.state();
            c.startRepertoireTraining(id);
            check(c.state()["training"].toBool() && c.state()["repertoireTraining"].toLongLong() == id &&
                      c.repertoireState()["moves"].toList().empty(),
                  "Position exercise is active and prepared answers are hidden");
            c.openBackup(dir.path() + "/repertoire-ui/library.sqlite");
            check(c.state()["training"].toBool() && c.repertoireState()["id"].toLongLong() == id,
                  "Changing library is refused during an active repertoire exercise");
            c.undo();
            c.move("a2a3");
            check(c.state()["node"].toInt() == 0, "Wrong repertoire reply and undo cannot advance exercise");
            c.move("d2d4");
            waitRepertoire();
            check(c.state()["trainingComplete"].toBool() && c.repertoireState()["reviews"].toInt() == 1 &&
                      c.repertoireState()["successes"].toInt() == 0,
                  "Alternative answer accepted, earlier failure recorded once");
            c.move("e2e4");
            waitRepertoire();
            check(c.repertoireState()["reviews"].toInt() == 1, "Completed exercise cannot be graded twice");
            c.finishTraining();
            check(c.state()["fen"] == previous["fen"] && c.state()["flipped"] == previous["flipped"] &&
                      c.state()["savedId"] == previous["savedId"],
                  "Leaving repertoire exercise restores study, orientation and saved identity");
            c.startRepertoireTraining(id, true);
            c.revealCard();
            c.move("e2e4");
            waitRepertoire();
            check(c.repertoireState()["successes"].toInt() == 0,
                  "Revealed answers are not counted as recall successes");
            c.startRepertoireTraining(id, true);
            c.move("e2e4");
            waitRepertoire();
            check(c.repertoireState()["reviews"].toInt() == 3 &&
                      c.repertoireState()["successes"].toInt() == 1 &&
                      c.repertoireState()["due"].toInt() == 0,
                  "Successful recall updates schedule independently of legacy cards");
            c.finishTraining();
            const auto output = dir.path() + "/repertoire.pgn";
            c.exportRepertoire(id, output);
            waitRepertoire();
            check(QFileInfo(output).size() > 0, "Repertoire PGN exported from controller");
            c.importRepertoire(id, output);
            waitRepertoire();
            c.navigate(0);
            check(c.repertoireState()["moves"].toList().size() == 2,
                  "Reimport preserves alternative answers");
            c.renameRepertoire(id, "Ouvertures");
            waitRepertoire();
            check(c.repertoireState()["name"] == "Ouvertures", "Repertoire editor updates persisted name");
            {
                // Migrer depuis En Croissant ou Pawn Appétit, c'est reprendre un dossier de
                // PGN répartis en sous-dossiers, pas un fichier à la fois.
                const auto folder = dir.path() + "/etudes";
                check(QDir().mkpath(folder + "/Noirs"), "Create a nested study folder");
                auto write = [&](const QString &path, const QByteArray &pgn) {
                    QFile file(path);
                    check(file.open(QIODevice::WriteOnly) && file.write(pgn) == pgn.size(),
                          "Write study file");
                };
                write(folder + "/Italienne.pgn", "1.e4 e5 2.Nf3 Nc6 3.Bc4 (3.Bb5 a6) Bc5 *\n");
                write(folder + "/Noirs/Caro-Kann.pgn", "1.e4 c6 2.d4 d5 *\n");
                write(folder + "/casse.pgn", "1.e4 coup-invalide *\n");
                // En Croissant écrit aussi des PGN compressés et garde ses bases en .db3 :
                // le premier doit s'ouvrir, le second être ignoré sans faire échouer le tout.
                write(folder + "/notes.txt", "rien a voir\n");
                write(folder + "/bases.db3", "SQLite format 3\0");
                QProcess gzip;
                gzip.setStandardOutputFile(folder + "/Noirs/Francaise.pgn.gz");
                gzip.start("gzip", {"-c"});
                check(gzip.waitForStarted(5000), "gzip is available to build the fixture");
                gzip.write("1.e4 e6 2.d4 d5 *\n");
                gzip.closeWriteChannel();
                check(gzip.waitForFinished(5000) && gzip.exitCode() == 0, "Write a compressed study");
                QList<qint64> established;
                for (const auto &row : c.repertoires())
                    established << row.toMap()["id"].toLongLong();
                c.importRepertoireFolder(QUrl::fromLocalFile(folder).toString());
                waitRepertoire();
                QStringList names;
                for (const auto &row : c.repertoires())
                    names << row.toMap()["name"].toString();
                check(c.repertoires().size() == established.size() + 3,
                      "A folder yields one repertoire per readable PGN, subfolders included");
                check(names.contains("Noirs/Francaise"),
                      "A compressed study is unpacked rather than refused");
                check(names.contains("Italienne"), "A study keeps the name of its file");
                check(names.contains("Noirs/Caro-Kann"), "A study in a subfolder keeps its relative path");
                check(!names.contains("casse"), "An unreadable file is set aside, not created empty");
                // Le scénario continue sur le répertoire d'origine : ce bloc lui rend l'état
                // qu'il a trouvé, sélection comprise.
                for (const auto &row : c.repertoires()) {
                    const auto imported = row.toMap()["id"].toLongLong();
                    if (!established.contains(imported)) {
                        c.deleteRepertoire(imported);
                        waitRepertoire();
                    }
                }
                // Un dossier d'En Croissant ne contient parfois que ses bases .db3 : il
                // faut dire ce qu'il y a, et non « aucun fichier trouvé ».
                const auto bases = dir.path() + "/encroissant";
                check(QDir().mkpath(bases), "Create a folder of foreign databases");
                write(bases + "/parties.db3", "SQLite format 3");
                write(bases + "/index.idx", "x");
                QSignalSpy refused(&c, &Controller::message);
                c.importRepertoireFolder(QUrl::fromLocalFile(bases).toString());
                waitRepertoire();
                check(refused.count() == 1 && refused.first().at(1).toBool() &&
                          refused.first().at(0).toString().contains("db3"),
                      "A folder of foreign databases names them instead of claiming it found nothing");
                c.selectRepertoire(id);
                waitRepertoire();
            }
            c.removeRepertoireMove(id, c.state()["fen"].toString(), "d2d4");
            waitRepertoire();
            check(c.repertoireState()["moves"].toList().size() == 1, "Editor removes chosen continuation");
            c.reloadRepertoires();
            c.cancelRepertoireOverview();
            QTest::qWait(20);
            check(!c.repertoireState()["loading"].toBool() && !c.repertoireState()["gapsLoaded"].toBool() &&
                      !c.repertoireState()["error"].toString().isEmpty(),
                  "Canceled overview cannot claim empty coverage is complete");
            c.reloadRepertoires();
            waitRepertoire();
            const auto bulk = dir.path() + "/bulk-repertoire.pgn";
            {
                QFile file(bulk);
                check(file.open(QIODevice::WriteOnly), "Open bulk repertoire fixture");
                const auto game = QByteArray("[Event \"Bulk\"]\n1.d4 {") + QByteArray(24000, 'x') +
                                  "} d5 2.Nf3 Nf6 3.Ng1 Ng8 4.Nf3 Nf6 5.Ng1 Ng8 *\n";
                for (int i = 0; i < 200; ++i)
                    check(file.write(game) == game.size(), "Write bulk repertoire record");
            }
            int pulses = 0;
            QTimer responsive;
            responsive.setInterval(1);
            QObject::connect(&responsive, &QTimer::timeout, [&] { ++pulses; });
            responsive.start();
            c.importRepertoire(id, bulk);
            QElapsedTimer timeout;
            timeout.start();
            while (c.busy() && c.repertoireTask()["positions"].toLongLong() < 30 && timeout.elapsed() < 10000)
                QTest::qWait(1);
            check(c.busy() && c.repertoireTask()["positions"].toLongLong() >= 30 && pulses > 0,
                  "GUI keeps processing events during repertoire import");
            c.cancelTask();
            waitRepertoire();
            responsive.stop();
            check(c.repertoireTask()["canceled"].toBool() &&
                      c.repertoireState()["moves"].toList().size() == 1 &&
                      c.repertoireState()["reviews"].toInt() == 3,
                  "Canceled active import rolls back moves and preserves reviews");
            QFile savedExport(output);
            check(savedExport.open(QIODevice::ReadOnly), "Read previous repertoire export");
            const auto originalBytes = savedExport.readAll();
            savedExport.close();
            // Cancel before dispatch: a tiny export may commit before the next UI statement.
            const auto cancelExport = QObject::connect(&c, &Controller::repertoireTaskChanged, &c, [&] {
                if (c.repertoireTask()["running"].toBool())
                    c.cancelTask();
            });
            c.exportRepertoire(id, output);
            QObject::disconnect(cancelExport);
            waitRepertoire();
            check(c.repertoireTask()["canceled"].toBool() && savedExport.open(QIODevice::ReadOnly) &&
                      savedExport.readAll() == originalBytes,
                  "Canceled export preserves destination bytes");
            savedExport.close();
            c.exportRepertoire(id, dir.path() + "/repertoire-ui/library.sqlite");
            waitRepertoire();
            check(!c.repertoireTask()["error"].toString().isEmpty() && c.stats()["games"].toInt() == 1,
                  "Export cannot overwrite the live library");
            if (!qEnvironmentVariable("T64_STOCKFISH").isEmpty()) {
                c.setPreference("enginePath", qEnvironmentVariable("T64_STOCKFISH"));
                timeout.restart();
                while (!c.analysisQueue()->state()["ready"].toBool() && timeout.elapsed() < 5000)
                    QTest::qWait(5);
                const auto beforeAnalysis = c.state();
                c.analyseRepertoire(id, 100);
                waitRepertoire();
                timeout.restart();
                while ((c.analysisQueue()->jobs().isEmpty() ||
                        c.analysisQueue()->jobs().first().toMap()["status"] != "completed") &&
                       timeout.elapsed() < 10000)
                    QTest::qWait(10);
                check(c.analysisQueue()->jobs().size() == 1 &&
                          c.analysisQueue()->jobs().first().toMap()["status"] == "completed" &&
                          c.analysisQueue()->jobs().first().toMap()["total"].toInt() == 2,
                      "Real Stockfish completes every position in a repertoire copy");
                check(c.state()["fen"] == beforeAnalysis["fen"] &&
                          c.repertoireState()["reviews"].toInt() == 3,
                      "Repertoire analysis preserves the current study and learning history");
                auto *queueDialog = window->findChild<QObject *>("analysisQueueDialog");
                check(queueDialog && queueDialog->property("visible").toBool(),
                      "Repertoire analysis opens persistent queue");
                QMetaObject::invokeMethod(queueDialog, "close");
            }
            c.deleteRepertoire(id);
            waitRepertoire();
            check(c.repertoires().empty() && c.stats()["games"].toInt() == 1,
                  "Repertoire deletion preserves study");
            check(warnings.empty(), "Repertoire and training UI have no QML errors");
            {
                // Répétition espacée : un puzzle manqué revient le lendemain, puis 3 et 7 jours
                // après chaque réussite ; trois réussites de suite et il est acquis.
                t64::Database bank(dir.path() + "/spaced.sqlite");
                t64::Puzzle one;
                one.ref = "s1";
                one.fen = "6k1/r4ppp/8/8/8/8/5PPP/3R2K1 w - - 0 1";
                one.solution = "d1d8";
                one.rating = 1500;
                one.themes = "mateIn1";
                one.source = "lichess";
                auto two = one;
                two.ref = "s2";
                bank.addPuzzles({one, two});
                const auto now = QDateTime::currentDateTimeUtc();
                const auto in = [&](double days) { return now.addSecs(qint64(days * 86400)); };
                const auto id = bank.nextPuzzle(1500)["id"].toLongLong();
                bank.recordPuzzle(id, false, 5, 1490);
                check(bank.duePuzzle({}, {}, in(0.5)).isEmpty() && bank.duePuzzles(in(0.5)) == 0,
                      "A puzzle just missed is not asked again the same day");
                check(bank.duePuzzle({}, {}, in(1.1))["id"] == id &&
                          bank.duePuzzle({}, {}, in(1.1))["review"].toBool() && bank.duePuzzles(in(1.1)) == 1,
                      "It comes back the next day, marked as a review");
                check(bank.duePuzzle("fork", {}, in(1.1)).isEmpty() &&
                          bank.duePuzzle({}, "mine", in(1.1)).isEmpty() &&
                          bank.duePuzzle("mateIn1", "lichess", in(1.1))["id"] == id,
                      "Reviews respect the chosen theme and origin");
                bank.recordPuzzle(id, true, 5, 1500);
                check(bank.duePuzzle({}, {}, in(2)).isEmpty() && bank.duePuzzle({}, {}, in(3.1))["id"] == id,
                      "After one success it waits three days");
                bank.recordPuzzle(id, true, 5, 1510);
                check(bank.duePuzzle({}, {}, in(6)).isEmpty() && bank.duePuzzle({}, {}, in(7.1))["id"] == id,
                      "After two it waits seven");
                bank.recordPuzzle(id, true, 5, 1520);
                check(bank.duePuzzle({}, {}, in(400)).isEmpty(),
                      "Three successes in a row and it is learned");
                bank.recordPuzzle(id, false, 5, 1500);
                check(bank.duePuzzle({}, {}, in(1.1))["id"] == id,
                      "Missing it again starts the schedule over");
                const auto other = id == 1 ? 2 : 1;
                bank.recordPuzzle(other, true, 5, 1510);
                check(bank.duePuzzles(in(30)) == 1, "A puzzle solved at the first try is never a review");
            }
            {
                // Deux répertoires qui partagent le début et divergent au deuxième coup blanc.
                t64::Database both(dir.path() + "/compare.sqlite");
                const auto spanish = both.createRepertoire("Espagnole", true,
                                                           t64::parsePgn("1. e4 e5 2. Nf3 Nc6 3. Bb5 *")[0]);
                const auto italian = both.createRepertoire(
                    "Italienne", true, t64::parsePgn("1. e4 e5 2. Nf3 Nc6 3. Bc4 Bc5 *")[0]);
                const auto compared = both.compareRepertoires(spanish, italian);
                const auto rows = compared["differences"].toList();
                check(compared["shared"] == 5 && compared["onlyFirst"] == 1 && compared["onlySecond"] == 2,
                      "Comparing repertoires counts shared positions and those each has alone");
                check(rows.size() == 1 && rows[0].toMap()["first"] == "Bb5" &&
                          rows[0].toMap()["second"] == "Bc4" &&
                          rows[0].toMap()["fen"].toString().startsWith("r1bqkbnr/pppp1ppp/2n5/4p3/4P3/5N2/"),
                      "The one shared position prepared differently is listed with each side's move");
                bool refused = false;
                try {
                    both.compareRepertoires(spanish, spanish);
                } catch (const std::exception &) {
                    refused = true;
                }
                check(refused, "A repertoire is not compared with itself");
            }
            c.createRepertoire("Ancienne base", true);
            waitRepertoire();
            const auto otherPath = dir.path() + "/other-library.sqlite";
            {
                t64::Database other(otherPath);
                other.createRepertoire("Nouvelle base", false, t64::Game());
            }
            c.reloadRepertoires();
            c.openBackup(otherPath);
            waitRepertoire();
            check(c.repertoires().size() == 1 && c.repertoires().first().toMap()["name"] == "Nouvelle base" &&
                      c.repertoireState()["id"].toLongLong() == 0,
                  "Results and reused repertoire IDs from previous library cannot leak after switching");
        }
        {
            Controller c(dir.path() + "/preparation-ui");
            c.importText("[White \"Alice\"] [Black \"Rival\"] [Date \"2024.01.01\"]\n1.d4 d5 *\n"
                         "[White \"Other\"] [Black \"Someone\"]\n1.e4 e5 *");
            QElapsedTimer timeout;
            timeout.start();
            while (c.busy() && timeout.elapsed() < 10000)
                QTest::qWait(10);
            check(!c.busy(), "Preparation fixture imported");
            QQmlApplicationEngine engine;
            engine.rootContext()->setContextProperty("controller", &c);
            QSignalSpy warnings(&engine, &QQmlEngine::warnings);
            engine.load(QUrl("qrc:/ui/Main.qml"));
            check(!engine.rootObjects().empty(), "Preparation QML loads");
            auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
            window->resize(960, 640);
            auto *dialog = window->findChild<QObject *>("preparationDialog");
            check(dialog && QMetaObject::invokeMethod(dialog, "open"), "Preparation manager opens");
            auto waitPreparation = [&] {
                timeout.restart();
                while (c.preparation()["running"].toBool() && timeout.elapsed() < 10000)
                    QTest::qWait(10);
                check(!c.preparation()["running"].toBool(), "Preparation worker completed");
            };
            c.preparePosition({{"player", "Other"}});
            c.preparePosition({{"player", "Rival"}, {"colour", "black"}, {"fromYear", 2020}});
            waitPreparation();
            auto rows = c.preparation()["rows"].toList();
            if (rows.size() != 1 || rows.first().toMap()["uci"] != "d2d4")
                qWarning() << "Unexpected preparation result:" << c.preparation();
            check(rows.size() == 1 && rows.first().toMap()["uci"] == "d2d4",
                  "Latest preparation filters replace stale request");
            const auto csv = dir.path() + "/preparation.csv";
            c.exportPreparation(csv);
            QFile output(csv);
            check(output.open(QIODevice::ReadOnly) && output.readAll().contains("Rival"),
                  "CSV includes filters for reproducible interpretation");
            c.playPreparedMove("d2d4");
            const auto resultingFen = c.state()["fen"];
            c.playPreparedMove("e2e4");
            check(c.state()["node"].toInt() == 1 && c.state()["fen"] == resultingFen,
                  "Stale prepared move cannot alter a different position");
            c.preparePosition({});
            c.cancelPreparation();
            QTest::qWait(30);
            check(c.preparation()["rows"].toList().empty(), "Canceled preparation results remain hidden");
            c.preparePosition({{"minElo", 4000}, {"maxElo", 1000}});
            waitPreparation();
            check(!c.preparation()["error"].toString().isEmpty() && c.preparation()["rows"].toList().empty(),
                  "Preparation errors clear old statistics");
            check(warnings.empty(), "Preparation QML has no runtime errors");
        }
        {
            const auto recovery = dir.path() + "/corrupt";
            QDir().mkpath(recovery);
            QFile bad(recovery + "/session.json");
            check(bad.open(QIODevice::WriteOnly), "Create truncated session fixture");
            bad.write("{\"tabs\":[");
            bad.close();
            { Controller recovered(recovery); }
            auto originals = QDir(recovery).entryList({"session.json.unreadable-*"}, QDir::Files);
            check(originals.size() == 1, "Corrupt session preserved before fresh session is written");
            QFile original(recovery + "/" + originals.first());
            check(original.open(QIODevice::ReadOnly) && original.readAll() == "{\"tabs\":[",
                  "Recovery preserves exact unreadable bytes");
        }
        for (const auto &bytes : QList<QByteArray>{
                 "{}", "{\"tabs\":{}}", "{\"tabs\":[]}", R"({"tabs":[{"pgn":"1.e4 *","path":["d2d4"]}]})",
                 R"({"tabs":[{"pgn":"1.e4 *","path":"invalid"}]})", R"({"tabs":[{"pgn":"1.e4 *"}]})"}) {
            QTemporaryDir recovery;
            QFile session(recovery.path() + "/session.json");
            check(session.open(QIODevice::WriteOnly) && session.write(bytes) == bytes.size(),
                  "Create structurally invalid session fixture");
            session.close();
            { Controller recovered(recovery.path()); }
            const auto originals =
                QDir(recovery.path()).entryList({"session.json.unreadable-*"}, QDir::Files);
            check(originals.size() == 1, "Invalid session structure is preserved before replacement");
            QFile original(recovery.path() + '/' + originals.first());
            check(original.open(QIODevice::ReadOnly) && original.readAll() == bytes,
                  "Invalid session structure keeps its exact original bytes");
        }
        {
            // Ranger la bibliothèque par ouverture lit la table ECO embarquée : si elle
            // devenait illisible, la commande n'étiquetterait plus rien en silence.
            QTemporaryDir shelf;
            Controller ranger(shelf.path());
            QSignalSpy messages(&ranger, &Controller::message);
            QElapsedTimer settle;
            auto idle = [&] {
                settle.restart();
                while ((ranger.busy() || ranger.librarySearching()) && settle.elapsed() < 10000)
                    QTest::qWait(10);
            };
            ranger.importText("[Event \"a\"]\n[ECO \"B90\"]\n\n1.e4 c5 *\n\n"
                              "[Event \"b\"]\n[ECO \"C42\"]\n\n1.e4 e5 2.Nf3 Nf6 *");
            idle();
            messages.clear();
            ranger.groupLibraryByOpening();
            idle();
            check(messages.count() >= 1 && !messages.last().at(1).toBool(),
                  "Ranging the library reports what it did, as news rather than an error");
            QStringList collections;
            for (const auto &row : ranger.games())
                collections << row.toMap()["collection"].toString();
            collections.sort();
            // On vérifie le résultat plutôt que la phrase : celle-ci dépend de la langue.
            check(collections.size() == 2 && !collections[0].isEmpty() && !collections[1].isEmpty() &&
                      collections[0] != collections[1],
                  "Each game lands in the collection of its own opening family");
            check(!collections[0].contains("B90") && !collections[0].contains(QString::fromUtf8("·")),
                  "The collection carries the family name, not the ECO code");
        }
        {
            // Un fichier PGN d'études n'est pas forcément un répertoire : ses parties
            // doivent rester séparées et parcourables, au lieu d'être fondues dans un
            // seul arbre de positions.
            QTemporaryDir home, files;
            Controller shelf(home.path());
            QElapsedTimer settle;
            auto idle = [&] {
                settle.restart();
                while ((shelf.busy() || shelf.librarySearching()) && settle.elapsed() < 15000)
                    QTest::qWait(10);
            };
            const auto pgn = files.filePath("Mes parties commentées.pgn");
            QFile out(pgn);
            check(out.open(QIODevice::WriteOnly), "Write a PGN of several games");
            out.write(
                "[Event \"Une\"]\n[White \"A\"]\n[Black \"B\"]\n[Result \"1-0\"]\n\n1.e4 e5 1-0\n\n"
                "[Event \"Deux\"]\n[White \"C\"]\n[Black \"D\"]\n[Result \"0-1\"]\n\n1.d4 d5 0-1\n\n"
                "[Event \"Trois\"]\n[White \"E\"]\n[Black \"F\"]\n[Result \"1/2-1/2\"]\n\n1.c4 c5 1/2-1/2\n");
            out.close();
            shelf.importStudyCollection(QUrl::fromLocalFile(pgn).toString());
            idle();
            const auto recueils = shelf.studyCollections();
            check(recueils.size() == 1, "The file becomes one collection, not one per game");
            check(recueils.first().toMap()["title"] == "Mes parties commentées",
                  "The collection is named after the file, without its extension");
            const auto games = shelf.studyGames(recueils.first().toMap()["id"].toLongLong());
            check(games.size() == 3, "Every game of the file stays a game of its own");
            QStringList events;
            for (const auto &row : games)
                events << row.toMap()["event"].toString();
            events.sort();
            check(events == QStringList({"Deux", "Trois", "Une"}),
                  "And each one keeps its own header rather than being merged into a tree");
            // Le recueil est aussi une base de la bibliothèque : recherche et bilan
            // marchent dessus comme sur le reste.
            check(shelf.sources().size() == 1 && shelf.games().size() == 3,
                  "The collection is a library database too, so search and review reach it");
            // Un dossier : un recueil par fichier, importés l'un après l'autre.
            const auto folder = files.filePath("recueils");
            check(QDir().mkpath(folder + "/Tournois"), "Create a folder of collections");
            for (const auto &leaf : {QString("Blitz.pgn"), QString("Tournois/Open.pgn")}) {
                QFile each(folder + "/" + leaf);
                check(each.open(QIODevice::WriteOnly), "Write a collection file");
                each.write("[Event \"x\"]\n[White \"G\"]\n[Black \"H\"]\n\n1.e4 *\n");
            }
            shelf.importStudyFolder(QUrl::fromLocalFile(folder).toString());
            settle.restart();
            while (shelf.studyCollections().size() < 3 && settle.elapsed() < 20000)
                QTest::qWait(20);
            QStringList titles;
            for (const auto &row : shelf.studyCollections())
                titles << row.toMap()["title"].toString();
            titles.sort();
            check(titles == QStringList({"Blitz", "Mes parties commentées", "Open"}),
                  "A folder yields one collection per file, subfolders included");
            // L'explorateur : le dossier du disque tel qu'il est, avec la marque de ce
            // qui est déjà dans la base.
            auto idleShelf = [&] {
                settle.restart();
                while ((shelf.busy() || shelf.librarySearching()) && settle.elapsed() < 20000)
                    QTest::qWait(20);
            };
            idleShelf();
            QFile foreign(folder + "/Archive.si4");
            check(foreign.open(QIODevice::WriteOnly) && foreign.write("x") == 1, "Write a foreign base");
            foreign.close();
            const auto view = shelf.browseStudies(folder);
            check(view["path"].toString() == folder && !view["parent"].toString().isEmpty(),
                  "The explorer says where it is and where it can go up to");
            const auto entries = view["entries"].toList();
            check(entries.size() == 3 && entries.first().toMap()["folder"].toBool() &&
                      entries.first().toMap()["name"] == "Tournois",
                  "Folders come before files, as in any explorer");
            QVariantMap blitz, archive;
            for (const auto &row : entries) {
                const auto each = row.toMap();
                if (each["name"] == "Blitz.pgn")
                    blitz = each;
                if (each["name"] == "Archive.si4")
                    archive = each;
            }
            check(blitz["readable"].toBool() && blitz["sourceId"].toLongLong() > 0 &&
                      blitz["games"].toInt() == 1,
                  "An imported file carries its collection and its game count");
            check(!archive.isEmpty() && !archive["readable"].toBool(),
                  "A foreign base is shown as unreadable rather than hidden");
            {
                QSignalSpy refused(&shelf, &Controller::message);
                check(shelf.openStudyFile(folder + "/Archive.si4") == 0 && refused.size() == 1 &&
                          refused.first().at(1).toBool(),
                      "Opening a foreign base says what to do instead of failing silently");
            }
            const int before = shelf.studyCollections().size();
            check(shelf.openStudyFile(folder + "/Blitz.pgn") == blitz["sourceId"].toLongLong() &&
                      shelf.studyCollections().size() == before,
                  "Reopening a known file shows its collection instead of reading it twice");
            // Deux fichiers du même nom dans deux dossiers sont deux recueils : leurs
            // parties se versaient dans le même tant que l'identifiant ignorait le dossier.
            check(QDir().mkpath(folder + "/Rapides"), "Create a sibling folder");
            QFile twin(folder + "/Rapides/Blitz.pgn");
            check(twin.open(QIODevice::WriteOnly), "Write a file whose name is already taken");
            twin.write("[Event \"Rapide\"]\n[White \"Ida\"]\n[Black \"Jo\"]\n[Date \"2021.03.04\"]\n"
                       "\n1.d4 Nf6 2.c4 e6 *\n");
            twin.close();
            idleShelf();
            shelf.openStudyFile(folder + "/Rapides/Blitz.pgn");
            settle.restart();
            while (shelf.studyCollections().size() <= before && settle.elapsed() < 20000)
                QTest::qWait(20);
            check(shelf.studyCollections().size() == before + 1,
                  "Two files of the same name in two folders make two collections");
            {
                QSignalSpy refused(&shelf, &Controller::message);
                shelf.createStudyFolder(folder, "../evade");
                shelf.createStudyFolder(folder, "Tournois");
                check(refused.size() == 2 && !QFileInfo::exists(files.path() + "/evade"),
                      "A new folder refuses a path and an existing name");
                shelf.createStudyFolder(folder, "Classiques");
                check(QFileInfo(folder + "/Classiques").isDir(), "And creates the folder asked for");
            }
            // L'arborescence : seuls les dossiers ouverts se déroulent, la recherche
            // descend partout, et chaque ligne dit sa profondeur.
            {
                const auto names = [](const QVariantList &rows) {
                    QStringList list;
                    for (const auto &row : rows)
                        list << QString::number(row.toMap()["depth"].toInt()) + ":" +
                                    row.toMap()["name"].toString();
                    return list;
                };
                const auto closed = names(shelf.studyTree(folder, {}));
                check(closed.contains("0:Tournois") && !closed.contains("1:Open.pgn") &&
                          closed.indexOf("0:Tournois") < closed.indexOf("0:Blitz.pgn"),
                      "A closed folder hides its files, and folders come first");
                const auto opened = names(shelf.studyTree(folder, {folder + "/Tournois"}));
                check(opened.contains("1:Open.pgn"), "An open folder unrolls one level deeper");
                const auto found = names(shelf.studyTree(folder, {}, "open"));
                check(found == QStringList({"0:Tournois", "1:Open.pgn"}),
                      "A search descends everywhere and keeps only what matches, with its folders");
                check(shelf.studyTree(folder, {}, "introuvable").isEmpty(),
                      "A search with no match shows nothing");
                shelf.rememberStudyExpanded({folder + "/Tournois", folder + "/disparu"});
                check(shelf.studyExpanded() == QStringList({folder + "/Tournois"}),
                      "Remembered folders that no longer exist are dropped");
            }
            // Créer, renommer, déplacer, mettre à la corbeille — et le recueil suit son fichier.
            {
                QSignalSpy said(&shelf, &Controller::message);
                const auto made = shelf.createStudyFile(folder + "/Classiques", "Nimzo", {});
                check(made == folder + "/Classiques/Nimzo.pgn" && QFileInfo(made).size() > 40,
                      "A new study file gets its extension and a blank game, not zero bytes");
                check(shelf.createStudyFile(folder + "/Classiques", "Nimzo.pgn", {}).isEmpty() &&
                          shelf.createStudyFile(folder + "/Classiques", "../evade", {}).isEmpty() &&
                          !QFileInfo::exists(folder + "/evade.pgn"),
                      "A taken name and a path are both refused");
                const auto pasted = shelf.createStudyFile(folder + "/Classiques", "Collee",
                                                          "[White \"K\"]\n[Black \"L\"]\n\n1.c4 *");
                QFile check_(pasted);
                check(check_.open(QIODevice::ReadOnly) && check_.readAll().contains("1.c4"),
                      "A pasted PGN becomes the file's content");
                qint64 blitzId = 0;
                for (const auto &row : shelf.browseStudies(folder)["entries"].toList())
                    if (row.toMap()["name"] == "Blitz.pgn")
                        blitzId = row.toMap()["sourceId"].toLongLong();
                const auto renamed = shelf.renameStudyEntry(folder + "/Blitz.pgn", "Parties rapides");
                check(renamed == folder + "/Parties rapides.pgn" && QFileInfo::exists(renamed) && blitzId > 0,
                      "Renaming keeps the extension even when the new name drops it");
                check(shelf.openStudyFile(renamed) == blitzId,
                      "A renamed file still opens its collection instead of being read again");
                const auto moved = shelf.moveStudyEntry(renamed, folder + "/Classiques");
                check(moved == folder + "/Classiques/Parties rapides.pgn" &&
                          shelf.openStudyFile(moved) == blitzId,
                      "A moved file keeps its collection too");
                check(!shelf.renameStudyEntry(folder + "/Classiques", "Anciennes").isEmpty() &&
                          shelf.openStudyFile(folder + "/Anciennes/Parties rapides.pgn") == blitzId,
                      "Renaming a folder carries the collections of the files inside it");
                check(shelf.moveStudyEntry(folder + "/Anciennes", folder + "/Anciennes").isEmpty() &&
                          shelf.moveStudyEntry(folder + "/Tournois", folder + "/Tournois/../Tournois")
                              .isEmpty(),
                      "A folder cannot be moved into itself");
                // La corbeille n'existe pas sur toutes les machines d'essai : on vérifie
                // alors que rien n'est supprimé, ce qui est la promesse.
                const auto doomed = folder + "/Anciennes/Collee.pgn";
                const bool trashed = shelf.trashStudyEntry(doomed);
                check(trashed != QFileInfo::exists(doomed),
                      "A file goes to the trash, or stays exactly where it was");
                check(QDir().rename(folder + "/Anciennes", folder + "/Classiques"), "Put the folder back");
                QFile::remove(folder + "/Classiques/Nimzo.pgn");
                QFile::remove(folder + "/Classiques/Collee.pgn");
                QFile::remove(folder + "/Classiques/Parties rapides.pgn");
            }
            // Une base d'En Croissant posée dans le dossier se lit comme un PGN.
            {
                QFile packed(QString(T64_TEST_DATA) + "/croissant_legacy.db3.qz");
                check(packed.open(QIODevice::ReadOnly), "Open the En Croissant fixture");
                QFile base(folder + "/Classiques/Essai.db3");
                check(base.open(QIODevice::WriteOnly) && base.write(qUncompress(packed.readAll())) > 0,
                      "Write an En Croissant base into the study folder");
                base.close();
                const auto listed = shelf.browseStudies(folder + "/Classiques")["entries"].toList();
                check(listed.size() == 1 && listed.first().toMap()["readable"].toBool(),
                      "An En Croissant base is offered as readable");
                idleShelf();
                const int known = shelf.studyCollections().size();
                shelf.openStudyFile(folder + "/Classiques/Essai.db3");
                settle.restart();
                while ((shelf.studyCollections().size() <= known || shelf.busy()) && settle.elapsed() < 60000)
                    QTest::qWait(20);
                idleShelf();
                qint64 imported = 0;
                for (const auto &row : shelf.studyCollections())
                    if (row.toMap()["title"] == "Essai")
                        imported = row.toMap()["games"].toLongLong();
                check(imported == 300, "Its 300 games arrive in a collection of their own");
                check(QDir(home.path()).entryList({"croissant-*.pgn"}).isEmpty(),
                      "The intermediate PGN is removed once read");
                QFile::remove(folder + "/Classiques/Essai.db3");
            }
            check(shelf.browseStudies(folder + "/Classiques")["entries"].toList().isEmpty() &&
                      shelf.browseStudies(files.filePath("nowhere"))["path"].toString().isEmpty(),
                  "An empty folder reads empty, a missing one reads as nothing at all");
        }
        {
            // Fermer la fenêtre pendant que les tâches de fond tournent : sans attente à
            // la destruction, leur réponse toucherait des membres déjà détruits.
            QTemporaryDir home;
            auto closing = std::make_unique<Controller>(home.path());
            closing->importText("[Event \"x\"]\n\n1.e4 c5 *");
            QElapsedTimer settle;
            settle.restart();
            while (closing->busy() && settle.elapsed() < 10000)
                QTest::qWait(10);
            closing->classifyLibraryOpenings();
            closing->buildOpeningReport();
            closing.reset();
            check(true, "Destroying the window while background tasks run leaves nothing dangling");
        }
        {
            // Comparer un répertoire aux parties du joueur : le profil dit lesquelles
            // sont les siennes, et son nom compte autant que ses comptes en ligne — les
            // parties jouées sur l'échiquier portent celui-là.
            QTemporaryDir home;
            Controller mine(home.path());
            QElapsedTimer settle;
            auto idle = [&] {
                settle.restart();
                while ((mine.busy() || mine.librarySearching()) && settle.elapsed() < 10000)
                    QTest::qWait(10);
            };
            check(mine.repertoireGap(1, 0)["reason"] == "noProfile",
                  "Without a profile the comparison says what is missing, not an error");
            mine.addProfile("Simzy");
            mine.importText("[Event \"x\"]\n[White \"Simzy\"]\n[Black \"Y\"]\n[Result \"1-0\"]\n\n"
                            "1.e4 e5 2.Nf3 Nc6 3.Bc4 Bc5 1-0");
            idle();
            mine.navigate(0);
            for (const auto &uci : {"e2e4", "e7e5", "g1f3", "b8c6", "f1b5"})
                mine.move(uci);
            mine.createRepertoire("Espagnole", true);
            // La création d'un répertoire a sa propre tâche de fond.
            settle.restart();
            while (mine.repertoires().isEmpty() && settle.elapsed() < 15000)
                QTest::qWait(20);
            const auto repertoires = mine.repertoires();
            check(!repertoires.isEmpty(), "The repertoire is created from the open study");
            const auto gap = mine.repertoireGap(repertoires.first().toMap()["id"].toLongLong(), 0);
            check(gap["ready"].toBool(), "A profile with no online account still names the player");
            check(gap["left"].toLongLong() == 1 && gap["rows"].toList().size() == 1,
                  "The game that left the preparation is found through the profile name alone");
            check(gap["rows"].toList().first().toMap()["played"] == "Bc4",
                  "And the move actually played is the one reported");
        }
        {
            // Classer les ouvertures : sans cela, `eco` ne valait que la balise PGN, et un
            // PGN sans balise — le cas courant — restait invisible à la recherche par
            // ouverture. Le classement doit suivre l'import sans qu'on le demande.
            QTemporaryDir home;
            Controller shelf(home.path());
            QElapsedTimer settle;
            auto idle = [&] {
                settle.restart();
                while ((shelf.busy() || shelf.librarySearching()) && settle.elapsed() < 10000)
                    QTest::qWait(10);
            };
            shelf.importText("[Event \"sans balise\"]\n\n1.e4 c5 2.Nf3 d6 *");
            idle();
            // Le classement tourne en arrière-plan : mesuré à deux secondes sur cent mille
            // parties, il figeait la fenêtre après chaque import quand il était sur son fil.
            settle.restart();
            while (shelf.games().size() == 1 && shelf.games().first().toMap()["eco"].toString().isEmpty() &&
                   settle.elapsed() < 10000)
                QTest::qWait(20);
            auto ecoOf = [&](const QString &event) {
                for (const auto &row : shelf.games())
                    if (row.toMap()["event"] == event)
                        return row.toMap()["eco"].toString();
                return QString();
            };
            check(ecoOf("sans balise").startsWith("B"),
                  "A game imported without an ECO tag is classified from its moves");
            // Le dossier de la position se construit en arrière-plan : l'interface ne doit
            // pas se figer sur une bibliothèque où des milliers de parties y sont passées.
            shelf.navigate(0);
            shelf.move("e2e4");
            shelf.buildOpeningReport();
            check(shelf.openingReport()["running"].toBool(),
                  "The dossier announces itself as running rather than freezing the window");
            settle.restart();
            while (shelf.openingReport()["running"].toBool() && settle.elapsed() < 10000)
                QTest::qWait(10);
            const auto dossier = shelf.openingReport();
            check(dossier["total"].toLongLong() == 1 && dossier["moves"].toList().size() == 1,
                  "The dossier counts the games that reached the position");
            check(dossier["moves"].toList()[0].toMap()["san"] == "c5",
                  "The move played from the position is the one reported");
            shelf.buildOpeningReport();
            check(!shelf.openingReport()["running"].toBool() &&
                      shelf.openingReport()["total"] == dossier["total"] &&
                      shelf.openingReport()["moves"] == dossier["moves"],
                  "Asking again for a dossier already computed answers at once, with the same figures");
            check(dossier["name"].toString().contains("B"),
                  "The dossier names the opening of the position it describes");
            // Et la commande explicite doit rattraper ce qui est déjà en base.
            QSignalSpy said(&shelf, &Controller::message);
            QElapsedTimer hand;
            hand.start();
            shelf.classifyLibraryOpenings();
            // Mesuré à deux secondes sur cent mille parties : l'appel doit rendre la main
            // tout de suite, sans quoi la fenêtre se fige le temps du classement.
            check(hand.elapsed() < 200, "Classifying hands control back at once rather than blocking");
            settle.restart();
            while (said.isEmpty() && settle.elapsed() < 10000)
                QTest::qWait(20);
            check(said.count() == 1 && !said.first().at(1).toBool(),
                  "Classifying again says what it did, as news rather than an error");
        }
        {
            // Plusieurs jeux de pièces côte à côte : avant, chaque import remplaçait le
            // précédent et le joueur ne pouvait pas revenir en arrière.
            QTemporaryDir home;
            Controller dresser(home.path());
            auto writeSet = [](const QString &folder) {
                QDir().mkpath(folder);
                for (QChar side : QString("wb"))
                    for (QChar kind : QString("KQRBNP")) {
                        QFile svg(folder + "/" + side + kind + ".svg");
                        if (!svg.open(QIODevice::WriteOnly))
                            return false;
                        svg.write("<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"45\" height=\"45\">"
                                  "<rect width=\"45\" height=\"45\" fill=\"#808080\"/></svg>");
                    }
                return true;
            };
            QTemporaryDir sources;
            check(writeSet(sources.filePath("Alpha")) && writeSet(sources.filePath("Merida")),
                  "Write two piece sets to import");
            dresser.importPieceSet(QUrl::fromLocalFile(sources.filePath("Alpha")).toString());
            dresser.importPieceSet(QUrl::fromLocalFile(sources.filePath("Merida")).toString());
            auto sets = [&] { return dresser.preferences()["customPieceSets"].toList(); };
            check(sets().size() == 2 && sets()[0].toMap()["name"] == "Alpha" &&
                      sets()[1].toMap()["name"] == "Merida",
                  "Both sets are kept, named after the folder they came from");
            check(dresser.preferences()["pieceStyle"] == "custom" &&
                      dresser.preferences()["customPiecePath"] == sets()[1].toMap()["path"],
                  "The last import is the one in use");
            dresser.useCustomPieceSet(0);
            check(dresser.preferences()["customPiecePath"] == sets()[0].toMap()["path"],
                  "Switching back to the first set is one call");
            const auto dropped = sets()[0].toMap()["path"].toString();
            dresser.removeCustomPieceSet(0);
            check(sets().size() == 1 && !QFileInfo::exists(dropped),
                  "Removing a set drops it from the list and deletes its copy");
            check(dresser.preferences()["customPiecePath"] == sets()[0].toMap()["path"],
                  "Removing the set in use falls back to one that still exists");
            dresser.removeCustomPieceSet(0);
            check(sets().isEmpty() && dresser.preferences()["pieceStyle"] == "cburnett",
                  "With no custom set left, the board goes back to a bundled one");
            // Le dossier doit contenir les douze pièces : un dossier incomplet ne doit
            // pas laisser un jeu à moitié installé derrière lui.
            QSignalSpy refused(&dresser, &Controller::message);
            QDir().mkpath(sources.filePath("Vide"));
            dresser.importPieceSet(QUrl::fromLocalFile(sources.filePath("Vide")).toString());
            check(refused.count() == 1 && refused.first().at(1).toBool() && sets().isEmpty(),
                  "An incomplete folder is refused and installs nothing");
        }
        {
            // Reprise de l'ancien réglage : un seul chemin était retenu avant que
            // plusieurs jeux soient possibles.
            QTemporaryDir home;
            {
                QSettings settings;
                settings.setValue("customPiecePath", home.filePath("pieces/ancien"));
                settings.setValue("customPieceSets", QVariantList());
            }
            Controller inherited(home.path());
            const auto adopted = inherited.preferences()["customPieceSets"].toList();
            check(adopted.size() == 1 && adopted[0].toMap()["path"] == home.filePath("pieces/ancien"),
                  "A set installed before the list existed joins it at startup");
            QSettings().remove("customPiecePath");
            QSettings().remove("customPieceSets");
        }
        std::cout << checks << " desktop interaction checks passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
