// SPDX-License-Identifier: AGPL-3.0-or-later
#include "controller.h"
#include <QGuiApplication>
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
            for (int i = 0; i < 6; ++i) {
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
        std::cout << checks << " desktop interaction checks passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
