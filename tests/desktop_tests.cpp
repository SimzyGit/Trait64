// SPDX-License-Identifier: AGPL-3.0-or-later
#include "controller.h"
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
#include <QQuickStyle>
#include <QQuickWindow>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
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
int main(int argc, char **argv) {
    QGuiApplication app(argc, argv);
    app.setOrganizationName("Trait64Tests");
    app.setApplicationName("Desktop");
    QTemporaryDir dir;
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, dir.path());
    QQuickStyle::setStyle("Basic");
    try {
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
                c.annotateReview();
                check(c.state()["comment"].toString().contains("[%eval"),
                      "Review annotations persist to PGN");
                c.analyseGame(500);
                c.newGame();
                QTest::qWait(80);
                check(!c.reviewState()["running"].toBool() && c.review().empty(),
                      "Changing study cancels review and clears stale results");
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
            c.startLocalGame(1, 2, 0, true);
            QTest::qWait(150);
            check(c.state()["clockRunning"].toBool(), "Local clock starts");
            c.move("e2e4");
            check(c.state()["whiteClock"].toString() == "1:01", "Increment applied after completed move");
            c.stopPlaying();
            check(!c.state()["clockRunning"].toBool(), "Stopping game stops clock");
            // Exercise every new dialog including small window layout and keyboard capture.
            for (const auto &name : QStringList{"appearanceDialog", "positionEditor", "reviewDialog",
                                                "settingsDialog", "updatesDialog", "compareDialog"}) {
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
            c.move("e2e4");
            c.navigate(0);
            c.move("d2d4");
            c.saveGame();
            c.flip();
            const auto previous = c.state();
            c.createRepertoire("Débuts blancs", true);
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
            QMetaObject::invokeMethod(dialog, "close");
            c.startRepertoireTraining(id);
            check(c.state()["training"].toBool() && c.state()["repertoireTraining"].toLongLong() == id &&
                      c.repertoireState()["moves"].toList().empty(),
                  "Position exercise is active and prepared answers are hidden");
            c.undo();
            c.move("a2a3");
            check(c.state()["node"].toInt() == 0, "Wrong repertoire reply and undo cannot advance exercise");
            c.move("d2d4");
            check(c.state()["trainingComplete"].toBool() && c.repertoireState()["reviews"].toInt() == 1 &&
                      c.repertoireState()["successes"].toInt() == 0,
                  "Alternative answer accepted, earlier failure recorded once");
            c.move("e2e4");
            check(c.repertoireState()["reviews"].toInt() == 1, "Completed exercise cannot be graded twice");
            c.finishTraining();
            check(c.state()["fen"] == previous["fen"] && c.state()["flipped"] == previous["flipped"] &&
                      c.state()["savedId"] == previous["savedId"],
                  "Leaving repertoire exercise restores study, orientation and saved identity");
            c.startRepertoireTraining(id, true);
            c.revealCard();
            c.move("e2e4");
            check(c.repertoireState()["successes"].toInt() == 0,
                  "Revealed answers are not counted as recall successes");
            c.startRepertoireTraining(id, true);
            c.move("e2e4");
            check(c.repertoireState()["reviews"].toInt() == 3 &&
                      c.repertoireState()["successes"].toInt() == 1 &&
                      c.repertoireState()["due"].toInt() == 0,
                  "Successful recall updates schedule independently of legacy cards");
            c.finishTraining();
            const auto output = dir.path() + "/repertoire.pgn";
            c.exportRepertoire(id, output);
            check(QFileInfo(output).size() > 0, "Repertoire PGN exported from controller");
            c.importRepertoire(id, output);
            c.navigate(0);
            check(c.repertoireState()["moves"].toList().size() == 2,
                  "Reimport preserves alternative answers");
            c.renameRepertoire(id, "Ouvertures");
            check(c.repertoireState()["name"] == "Ouvertures", "Repertoire editor updates persisted name");
            c.removeRepertoireMove(id, c.state()["fen"].toString(), "d2d4");
            check(c.repertoireState()["moves"].toList().size() == 1, "Editor removes chosen continuation");
            c.deleteRepertoire(id);
            check(c.repertoires().empty() && c.stats()["games"].toInt() == 1,
                  "Repertoire deletion preserves study");
            check(warnings.empty(), "Repertoire and training UI have no QML errors");
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
        std::cout << checks << " desktop interaction checks passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
