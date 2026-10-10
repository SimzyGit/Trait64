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
                c.setPreference("hash", 16);
                c.setPreference("analysisDepth", 8);
                c.saveEngineProfile("Second test");
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
            window->resize(960, 720);
            for (int page = 0; page < 7; ++page) {
                window->setProperty("page", page);
                QTest::qWait(20);
                if (page == 2) {
                    auto *panel = window->findChild<QQuickItem *>("analysisPanel");
                    check(panel && panel->mapToScene(QPointF(panel->width(), panel->height())).x() <=
                                       window->width(),
                          "Analysis panel fits the smallest supported window");
                }
            }
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
