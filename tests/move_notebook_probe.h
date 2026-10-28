// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include "move_notation.h"
#include <QSet>

int moveNotebookProbe(const QString &directory) {
    const QString fixture = "[Event \"Notebook\"]\n[Result \"*\"]\n\n"
        "{Initial note} 1. e4! {White comment} (1. d4 d5 (1... Nf6) 2. c4) "
        "e5 {Black comment} (1... c5 2. Nf3 (2. d4 cxd4) d6) 2. Nf3 Nc6 3. Bb5 *";
    const auto game = t64::parsePgn(fixture.toStdString()).front();
    const auto rows = t64::moveRows(game, 2, true);
    QSet<int> seen;
    int selected = 0;
    for (const auto &value : rows) {
        const auto row = value.toMap();
        if (row["initial"].toBool()) continue;
        const auto white = row["white"].toMap(), black = row["black"].toMap();
        if (!white.isEmpty() && !black.isEmpty())
            check(black["parent"] == white["id"], "Notebook pairs direct parent/child in one branch");
        for (const auto &colour : {QString("white"), QString("black")}) {
            auto cell = row[colour].toMap();
            if (cell.isEmpty()) continue;
            int id = cell["id"].toInt();
            check(!seen.contains(id), "Notebook never repeats a node across branches");
            seen.insert(id);
            const auto parent = game.boardAt(game.nodes[id].parent);
            check((parent.sideToMove() == chess::Color::WHITE) == (colour == "white"), "Notebook uses actual side to move");
            check(row["number"].toInt() == parent.fullMoveNumber(), "Notebook uses actual FEN move counter");
            check(cell["comment"].toString().toStdString() == game.nodes[id].comment, "Notebook retains node comments");
            selected += cell["active"].toBool();
        }
    }
    check(seen.size() == int(game.nodes.size()) - 1 && selected == 1, "All nested moves appear once with one selection");
    check(rows.last().toMap()["black"].toMap().isEmpty(), "Final white-only row has empty black cell");
    check(rows[1].toMap()["white"].toMap()["text"].toString().contains('!'), "NAG belongs to its half-move");
    check(t64::displaySan("Nf3",true)=="Cf3" && t64::displaySan("exd8=Q+",true)=="exd8=D+" &&
          t64::displaySan("O-O",true)=="O-O" && t64::displaySan("Nf3",false)=="Nf3", "SAN translation is presentation only");
    const auto blackStart = t64::parsePgn("[SetUp \"1\"]\n[FEN \"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR b KQkq - 0 23\"]\n\n23... e5 24. Nf3 Nc6 *").front();
    const auto blackRows=t64::moveRows(blackStart,1,true);
    check(blackRows[1].toMap()["number"]==23 && blackRows[1].toMap()["white"].toMap().isEmpty() &&
          blackRows[1].toMap()["black"].toMap()["id"]==1 && blackRows[2].toMap()["number"]==24,
          "Black FEN start keeps empty white cell and fullmove counter");

    Controller c(directory+"/notebook");
    c.importText(fixture);
    QElapsedTimer timer; timer.start();
    while(c.busy() && timer.elapsed()<10000) QTest::qWait(10);
    check(!c.busy() && !c.games().isEmpty(),"Notebook PGN imported");
    c.loadGame(c.games().first().toMap()["id"].toLongLong());
    c.setPreference("language","fr");
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("controller",&c);
    QSignalSpy warnings(&engine,&QQmlEngine::warnings);
    engine.load(QUrl("qrc:/ui/Main.qml"));
    check(!engine.rootObjects().isEmpty(),"Notebook QML loads");
    auto *window=qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    window->resize(1440,940); window->setProperty("page",2);QTest::qWait(100);
    auto *list=visualItem(window->contentItem(),"moveNotebook");
    check(list && list->height()>50,"Notebook viewport is usable");
    for(int id:seen) {
        c.navigate(id);QTest::qWait(30);
        auto *cell=visualItem(window->contentItem(),"notebookMove_"+QString::number(id));
        check(cell && cell->isVisible(),"Every branch cell can be materialized");
        // Change selection without changing the row being tested, then click it.
        c.step(-1);QTest::qWait(20);
        QMetaObject::invokeMethod(list,"positionViewAtIndex",Q_ARG(int,list->property("currentIndex").toInt()+1),Q_ARG(int,0));
        c.navigate(id);QTest::qWait(20);
        cell=visualItem(window->contentItem(),"notebookMove_"+QString::number(id));
        auto p=cell->mapToScene(QPointF(cell->width()/2,cell->height()/2));
        check(p.y()>=list->mapToScene(QPointF()).y() && p.y()<=list->mapToScene(QPointF(0,list->height())).y(),"Selected half-move stays visible");
        QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,p.toPoint());QTest::qWait(10);
        check(c.state()["node"].toInt()==id,"Half-move click maps to exact node");
    }
    c.navigate(1);QTest::qWait(20);
    auto *black=visualItem(window->contentItem(),"notebookMove_"+QString::number(game.nodes[1].children[0]));
    check(black,"Black principal cell exists beside white");
    QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,black->mapToScene(QPointF(black->width()/2,black->height()/2)).toPoint());
    check(c.state()["node"].toInt()==game.nodes[1].children[0],"Click black column changes selected position");
    QTest::keyClick(window,Qt::Key_Left);QTest::qWait(20);
    check(c.state()["node"]==1,"Left shortcut returns to white half-move");
    QTest::keyClick(window,Qt::Key_Right);QTest::qWait(20);
    check(c.state()["node"].toInt()==game.nodes[1].children[0],"Right shortcut returns to black half-move");

    c.navigate(1);c.setComment(QString("Long <plain text> comment & annotation. ").repeated(50));c.setNag(3);
    const auto original=c.state();
    const auto screenshotDir=qEnvironmentVariable("T64_NOTEBOOK_SCREENSHOTS");
    if(!screenshotDir.isEmpty())QDir().mkpath(screenshotDir);
    struct Case {int width,height;double text,ui;bool light,contrast;const char *language;};
    const Case cases[]={{960,640,1,1,false,false,"fr"},{1440,940,1,1,false,false,"fr"},
        {2560,1600,1,1,true,false,"en"},{960,640,1.5,1.25,true,true,"fr"},
        {960,640,1.5,1.25,false,true,"en"},{1366,768,1.5,1.25,false,false,"fr"}};
    int index=0;
    for(const auto &scenario:cases) {
        c.setPreference("fontScale",scenario.text);c.setPreference("uiScale",scenario.ui);
        c.setPreference("lightTheme",scenario.light);c.setPreference("highContrast",scenario.contrast);
        c.setPreference("language",scenario.language);window->resize(scenario.width,scenario.height);QTest::qWait(100);
        auto *cell=visualItem(window->contentItem(),"notebookMove_1");
        check(cell && cell->width()>35 && cell->height()>20,"Move cell survives smallest viewport with maximum scales");
        check(cell->mapToScene(QPointF(cell->width(),0)).x()<=window->width(),"Move cell stays inside window");
        check(c.state()["fen"]==original["fen"] && c.state()["comment"]==original["comment"] && c.state()["dirty"]==original["dirty"],"Appearance preserves unsaved study and comments");
        if(!screenshotDir.isEmpty()) {
            auto grab=window->contentItem()->grabToImage();QSignalSpy ready(grab.data(),&QQuickItemGrabResult::ready);
            check(ready.wait(3000) && grab->saveToFile(screenshotDir+"/notebook-"+QString::number(index)+".png"),"Notebook screenshot saved");
        }
        ++index;
    }
    c.saveGame(); const auto gameId=c.state()["id"].toLongLong();
    c.loadGame(gameId);c.navigate(1);
    check(c.state()["comment"]==original["comment"] && c.state()["nag"]==3,"Notebook comments and NAG survive save/reopen");
    c.exportFile(directory+"/notebook.pgn");QFile output(directory+"/notebook.pgn");
    check(output.open(QIODevice::ReadOnly),"Notebook PGN export exists");
    const auto exported=output.readAll();
    check(exported.contains("Nf3") && !exported.contains("Cf3") && t64::parsePgn(exported.toStdString()).front().nodes.size()==game.nodes.size(),"Standard SAN and all branches survive PGN round trip");
    check(warnings.isEmpty(),"Notebook has no QML warnings");
    std::cout<<checks<<" notebook checks passed\n";
    return 0;
}
