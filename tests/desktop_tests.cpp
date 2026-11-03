// SPDX-License-Identifier: AGPL-3.0-or-later
#include "controller.h"
#include <limits>
#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImage>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickItemGrabResult>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QXmlStreamReader>
#include <QImageReader>
#include <QTest>
#include <QThreadPool>
#include <QtConcurrent>
#include <future>
#include <iostream>

int checks = 0;
void check(bool ok, const char *message) {
    ++checks;
    if (!ok)
        throw std::runtime_error(message);
}
QQuickItem *visualItem(QQuickItem *parent, const QString &name) {
    if (parent->objectName() == name)
        return parent;
    for (auto *child : parent->childItems())
        if (auto *found = visualItem(child, name))
            return found;
    return nullptr;
}
#include "import_gui_probe.h"
#include "ui_layout_probe.h"
#include "move_notebook_probe.h"
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
        if (app.arguments().contains("--move-notebook")) return moveNotebookProbe(dir.path());
        if (app.arguments().contains("--ui-layout")) return uiLayoutProbe(dir.path());
        if (app.arguments().contains("--benchmark-import"))
            return importGuiProbe(app, dir.path());
        if (!qEnvironmentVariable("T64_STOCKFISH").isEmpty()) {
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
            std::vector<std::tuple<quint64, int, int>> rows{{start.hash(), 12, 28}, {start.hash(), 11, 27},
                                                            {e4.hash(), 52, 36}, {d4.hash(), 51, 35}};
            std::sort(rows.begin(), rows.end());
            for (auto [key, from, to] : rows)
                for (quint64 part : {key, quint64(to | from << 6) << 48 | quint64(1) << 32})
                    for (int shift = 56; shift >= 0; shift -= 8) book.putChar(char(part >> shift & 0xff));
            book.close();
            c.setPreference("bookPath", book.fileName());
            QSignalSpy messages(&c, &Controller::message);
            c.startTournament({0}, 2, 100, 30, true);
            check(!c.tournament()->running() && messages.size() == 1, "A tournament needs two profiles");
            c.startTournament({0, 1}, 2, 100, 30, true);
            check(c.tournament()->running(), "Tournament starts from saved profiles");
            QElapsedTimer wait;
            wait.start();
            while ((c.tournament()->running() || c.busy() || c.librarySearching()) && wait.elapsed() < 60000) QTest::qWait(20);
            const auto games = c.games();
            check(games.size() == 2, "Both tournament games reach the library");
            bool tagged = false;
            for (const auto &value : c.sources()) tagged |= value.toMap()["title"].toString().startsWith("Tournoi de moteurs");
            check(tagged, "Tournament games are filed under their own source");
            for (const auto &value : games) {
                c.loadGame(value.toMap()["id"].toLongLong());
                const auto pgn = c.state()["pgn"].toString();
                const auto moves = c.moveRows();
                check(moves.size() > 2, "Tournament game has moves");
                const auto first = moves[1].toMap();
                const auto line = first["white"].toMap()["text"].toString() + " " + first["black"].toMap()["text"].toString();
                check(line == "e4 e5" || line == "d4 d5", "Tournament openings come from the Polyglot book");
            }
            // Settings are shared by every controller in this process: leave them as found.
            c.setPreference("bookPath", "");
            c.removeEngineProfile(1);
            c.removeEngineProfile(0);
            c.setPreference("hash", 128);
            c.setPreference("enginePath", "");
            check(c.preferences()["engineProfiles"].toList().isEmpty(), "Tournament test leaves no profile behind");
        }
        {
            Controller c(dir.path() + "/puzzles");
            QFile csv(dir.path() + "/puzzles.csv");
            check(csv.open(QIODevice::WriteOnly), "Puzzle file written");
            // Two rooks: the stored solution is d1d8, but e1e8 mates as well.
            csv.write("PuzzleId,FEN,Moves,Rating,RatingDeviation,Popularity,NbPlays,Themes,GameUrl,OpeningTags\n"
                      "p1,r5k1/5ppp/8/8/8/8/5PPP/3RR1K1 b - - 0 1,a8a7 d1d8,1500,75,95,1000,backRankMate mateIn1,u,\n"
                      "p2,r5k1/5ppp/8/8/8/8/5PPP/3R2K1 b - - 0 1,a8a7 d1d8,1520,75,95,1000,backRankMate mateIn1,u,\n");
            csv.close();
            c.importPuzzles(csv.fileName(), 100, 0, 0);
            QElapsedTimer wait;
            wait.start();
            while (c.busy() && wait.elapsed() < 10000) QTest::qWait(10);
            check(c.puzzles()["total"].toInt() == 2, "Puzzle bank imported");
            const auto study = c.state()["fen"];
            const int before = c.puzzles()["rating"].toInt();
            c.startPuzzle("mateIn1", "lichess");
            check(c.puzzles()["active"].toBool() && c.state()["training"].toBool(), "Puzzle starts in training mode");
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
            c.importText("[Event \"x\"]\n\n1. e4 e5 *\n\n[Event \"y\"]\n\n1. d4 d5 *\n\n[Event \"z\"]\n\n1. c4 e5 *");
            wait.restart();
            while ((c.busy() || c.librarySearching()) && wait.elapsed() < 10000) QTest::qWait(10);
            const int jobs = c.analysisQueue()->jobs().size();
            c.searchLibrary("", "", false, false, false, {});
            wait.restart();
            while (c.librarySearching() && wait.elapsed() < 5000) QTest::qWait(10);
            QSignalSpy refused(&c, &Controller::message);
            c.enqueueLibrary(100, -1, 2);
            check(refused.size() == 1 && refused.first().at(1).toBool() && c.analysisQueue()->jobs().size() == jobs,
                  "Batch analysis without an engine fails once, clearly");
            if (!qEnvironmentVariable("T64_STOCKFISH").isEmpty()) {
                c.setPreference("enginePath", qEnvironmentVariable("T64_STOCKFISH"));
                c.enqueueLibrary(100, -1, 2);
                wait.restart();
                while (c.analysisQueue()->jobs().size() < jobs + 2 && wait.elapsed() < 5000) QTest::qWait(10);
                check(c.analysisQueue()->jobs().size() == jobs + 2, "Library games are queued for analysis up to the limit");
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
                while (!xml.atEnd()) xml.readNext();
                check(!xml.hasError(), "SVG diagram is well-formed XML");
                check(style == "unicode" ? bytes.count("<text") >= 32 : bytes.count("<svg") == 1 && bytes.count("<g transform") == 32, "Every piece is in the SVG");
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
            Controller c(dir.path() + "/online-import");
            auto *online = c.findChild<t64::OnlineGames *>();
            QSignalSpy messages(&c, &Controller::message);
            c.importOnline("lichess", "a/../b", 10);
            check(messages.size() == 1 && !online->busy(), "Invalid online username is refused before any request");
            c.importOnline("lichess", "Simzy", 10);
            check(c.busy() && c.onlineFetching(), "Online fetch marks the application busy");
            online->cancel();
            QElapsedTimer wait;
            wait.start();
            while (c.onlineFetching() && wait.elapsed() < 5000) QTest::qWait(10);
            check(!c.busy(), "Canceled fetch releases the application");
            const auto file = dir.path() + "/online-import/downloads/online-lichess-simzy.pgn";
            QFile pgn(file);
            check(pgn.open(QIODevice::WriteOnly) &&
                      pgn.write("[Event \"Rated blitz\"]\n[White \"Simzy\"]\n[Black \"X\"]\n[Result \"1-0\"]\n\n1. e4 e5 1-0\n\n"
                                "[Event \"Rated blitz\"]\n[White \"Y\"]\n[Black \"Simzy\"]\n[Result \"0-1\"]\n\n1. d4 d5 0-1\n") > 0,
                  "Downloaded PGN fixture written");
            pgn.close();
            emit online->finished(file, 2);
            wait.restart();
            while ((c.busy() || c.librarySearching()) && wait.elapsed() < 10000) QTest::qWait(10);
            QSettings settings;
            check(c.games().size() == 2 && !QFile::exists(file) &&
                      settings.value("onlineSync/lichess/simzy").toLongLong() > 0,
                  "Online games reach the library, the temporary file is removed and the next sync is incremental");
            bool tagged = false;
            for (const auto &value : c.sources()) tagged |= value.toMap()["title"].toString() == "Lichess — simzy";
            check(tagged, "Online games are filed under their player source");
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
            check(!c.busy() && messages.last()[0].toString().contains("interrompue") &&
                      !messages.last()[1].toBool(),
                  "Canceled verification finishes without claiming integrity success");
            check(c.state()["fen"] == original["fen"] && c.state()["dirty"] == original["dirty"],
                  "Read-only maintenance preserves the unsaved study");
            c.verifyDatabase();
            wait.restart();
            while (c.busy() && wait.elapsed() < 5000)
                QTest::qWait(5);
            check(!c.busy() && messages.last()[0].toString() == "Vérification SQLite : ok" &&
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
                      messages[0][0].toString().contains("interrompue") &&
                      messages[1][0].toString() == "Vérification SQLite : ok",
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
            c.saveEngineProfile("Test");
            c.setPreference("threads", 2);
            c.loadEngineProfile(0);
            check(c.preferences()["threads"].toInt() == 1, "Engine profile restores settings");
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
                const auto reviewBeforeExplanation = c.review();
                const auto beforeExplanation = c.state();
                const auto actual = c.explainReview(0, false);
                const auto alternative = c.explainReview(0, true);
                check(actual["available"].toBool() && alternative["available"].toBool() &&
                          !actual["steps"].toList().empty() && !alternative["steps"].toList().empty() &&
                          actual["steps"].toList().first().toMap()["uci"] == c.review()[0].toMap()["played"],
                      "Real Stockfish review explains played move and pre-move alternative legally");
                check(!c.explainReview(-1)["available"].toBool() && !c.explainReview(999)["available"].toBool(),
                      "Unavailable review rows cannot fabricate explanations");
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
                    if (c.tabs()[i].toMap()["active"].toBool()) originalTab = i;
                const auto originalTabs = c.tabs().size();
                auto *openLine = window->findChild<QQuickItem *>("openReviewLineButton");
                check(openLine && openLine->isVisible() && openLine->isEnabled(),
                      "Review line button is accessible");
                QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier,
                                  openLine->mapToScene(QPointF(openLine->width()/2, openLine->height()/2)).toPoint());
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
                const QStringList motifFens{
                    "4k3/5q2/8/8/2N5/8/8/4K3 w - - 0 1",
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
                    check(openLine->isVisible() && openLine->isEnabled(), "Tactical copy button accessible at 960x640");
                    const int copyTab = c.tabs().size();
                    QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier,
                                      openLine->mapToScene(QPointF(openLine->width()/2, openLine->height()/2)).toPoint());
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
                    check(restored.size() == 1 && restored[0].nodes[1].comment.find(motifMarks[fixture].toStdString()) != std::string::npos,
                          "Exported tactical copy retains graphical annotations");
                    c.switchTab(fixtureTab);
                    check(c.state()["fen"] == fixtureFen && c.review() == fixtureReview &&
                              c.state()["comment"] == "Original tactical study" && c.state()["arrows"].toList().empty(),
                          "Original tactical study and its review remain untouched");
                    c.closeTab(copyTab);
                    c.switchTab(originalTab);
                    c.closeTab(fixtureTab);
                }
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
            check(c.preferences()["accentColor"].toString() == "#a1442b", "Light default accent is readable");
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
            check(c.games().empty() && c.librarySearchStatus() == "Recherche impossible.",
                  "Worker error clears stale rows and reaches the user");
            c.searchLibrary({}, {}, false, false, false, {{"openingName", "française"}});
            waitSearch();
            check(c.games().size() == 200 && c.games().first().toMap()["eco"] == "C00",
                  "Search recovers after an invalid filter");
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
            const auto previous = c.state();
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
            {
                Controller recovered(recovery);
            }
            auto originals = QDir(recovery).entryList({"session.json.unreadable-*"}, QDir::Files);
            check(originals.size() == 1, "Corrupt session preserved before fresh session is written");
            QFile original(recovery + "/" + originals.first());
            check(original.open(QIODevice::ReadOnly) && original.readAll() == "{\"tabs\":[",
                  "Recovery preserves exact unreadable bytes");
        }
        for (const auto &bytes : QList<QByteArray>{
                 "{}", "{\"tabs\":{}}", "{\"tabs\":[]}",
                 R"({"tabs":[{"pgn":"1.e4 *","path":["d2d4"]}]})",
                 R"({"tabs":[{"pgn":"1.e4 *","path":"invalid"}]})",
                 R"({"tabs":[{"pgn":"1.e4 *"}]})"}) {
            QTemporaryDir recovery;
            QFile session(recovery.path() + "/session.json");
            check(session.open(QIODevice::WriteOnly) && session.write(bytes) == bytes.size(),
                  "Create structurally invalid session fixture");
            session.close();
            { Controller recovered(recovery.path()); }
            const auto originals = QDir(recovery.path()).entryList({"session.json.unreadable-*"}, QDir::Files);
            check(originals.size() == 1, "Invalid session structure is preserved before replacement");
            QFile original(recovery.path() + '/' + originals.first());
            check(original.open(QIODevice::ReadOnly) && original.readAll() == bytes,
                  "Invalid session structure keeps its exact original bytes");
        }
        std::cout << checks << " desktop interaction checks passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
