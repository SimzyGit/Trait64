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
            check(row["number"].toUInt() == parent.fullMoveNumber(), "Notebook uses actual FEN move counter");
            check(cell["comment"].toString().toStdString() == game.nodes[id].comment, "Notebook retains node comments");
            selected += cell["active"].toBool();
        }
    }
    check(seen.size() == int(game.nodes.size()) - 1 && selected == 1, "All nested moves appear once with one selection");
    check(rows.last().toMap()["black"].toMap().isEmpty(), "Final white-only row has empty black cell");
    check(rows[1].toMap()["white"].toMap()["text"].toString().contains('!'), "NAG belongs to its half-move");
    check(t64::displaySan("Nf3",true)=="Cf3" && t64::displaySan("exd8=Q+",true)=="exd8=D+" &&
          t64::displaySan("O-O",true)=="O-O" && t64::displaySan("Nf3",false)=="Nf3", "SAN translation is presentation only");
    check(t64::displaySan("Nf3",t64::Figurines)=="♘f3" && t64::displaySan("exd8=Q+",t64::Figurines)=="exd8=♕+" &&
          t64::displaySan("Kxe2",t64::Figurines)=="♔xe2" && t64::displaySan("O-O-O",t64::Figurines)=="O-O-O" &&
          t64::displaySan("e4",t64::Figurines)=="e4", "Figurine notation replaces piece letters only");
    {
        const auto parts=t64::splitComment("[%clk 0:02:59.4] [%eval 0.25] Nice move [%cal Ge2e4,Rd1d8]");
        check(parts.text=="Nice move" && parts.clock=="2:59" && parts.eval=="+0.25","PGN commands are shown apart from the comment");
        check(t64::splitComment("[%eval #-3]").eval=="#-3" && t64::splitComment("[%clk 1:05:00]").clock=="1:05:00" &&
              t64::splitComment("[%eval -1.5]").eval=="-1.50" && t64::splitComment("[%csl Gd4]").text.isEmpty(),
              "Mates, long clocks, negative scores and pure arrows are handled");
        const auto lichess=t64::parsePgn("1. e4 { [%eval 0.2] [%clk 0:03:00] } e5 { [%clk 0:02:58] } *").front();
        const auto flowTokens=t64::moveFlow(lichess,0,false);
        int comments=0;
        for(const auto &v:flowTokens) comments+=v.toMap()["kind"]=="comment";
        check(comments==0 && flowTokens[0].toMap()["clock"]=="3:00" && flowTokens[0].toMap()["eval"]=="+0.20" &&
              flowTokens[1].toMap()["clock"]=="2:58","Lichess clocks no longer flood the notebook");
        check(lichess.pgn().find("[%clk 0:03:00]")!=std::string::npos,"Stored PGN keeps every command");
    }
    const auto blackStart = t64::parsePgn("[SetUp \"1\"]\n[FEN \"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR b KQkq - 0 23\"]\n\n23... e5 24. Nf3 Nc6 *").front();
    const auto blackRows=t64::moveRows(blackStart,1,true);
    check(blackRows[1].toMap()["number"]==23 && blackRows[1].toMap()["white"].toMap().isEmpty() &&
          blackRows[1].toMap()["black"].toMap()["id"]==1 && blackRows[2].toMap()["number"]==24,
          "Black FEN start keeps empty white cell and fullmove counter");
    auto flowText=[](const QVariantList &tokens) {
        QStringList parts; int active=0;
        for(const auto &value:tokens) {
            const auto token=value.toMap(); const auto kind=token["kind"].toString();
            if(kind=="move") { parts<<(token["number"].toString().isEmpty() ? "" : token["number"].toString()+" ")+token["text"].toString(); active+=token["active"].toBool(); }
            else if(kind=="comment") parts<<"{"+token["text"].toString()+"}";
            else parts<<(kind=="open" ? "(" : ")");
        }
        return std::pair{parts.join(' '),active};
    };
    const auto [flow,flowActive]=flowText(t64::moveFlow(game,2,true));
    check(flow=="{Initial note} 1. e4! {White comment} ( 1. d4 d5 ( 1… Cf6 ) 2. c4 ) 1… e5 {Black comment} "
                "( 1… c5 2. Cf3 ( 2. d4 cxd4 ) 2… d6 ) 2. Cf3 Cc6 3. Fb5" && flowActive==1,
          "Continuous notation follows PGN order with black numbers after breaks");
    check(flowText(t64::moveFlow(blackStart,1,false)).first=="23… e5 24. Nf3 Nc6","Continuous notation keeps a black FEN start");

    Controller c(directory+"/notebook");
    c.importText(fixture + "\n\n[Event \"Other game\"]\n\n1. d4 d5 *");
    QElapsedTimer timer; timer.start();
    while((c.busy() || c.librarySearching()) && timer.elapsed()<10000) QTest::qWait(10);
    check(!c.busy() && c.games().size()==2,"Multi-game notebook PGN imported and library search finished");
    qint64 notebookId=0;
    for(const auto &value:c.games()) if(value.toMap()["event"]=="Notebook") notebookId=value.toMap()["id"].toLongLong();
    check(notebookId>0,"Notebook game selected from multi-game import");
    c.loadGame(notebookId);
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
    auto *notebookRoot=visualItem(window->contentItem(),"notebookRoot");
    auto settled=[&] {
        QElapsedTimer wait;wait.start();
        while(notebookRoot->property("followPending").toBool() && wait.elapsed()<2000)QTest::qWait(5);
        check(!notebookRoot->property("followPending").toBool(),"Notebook finishes its scheduled layout/scroll update");
    };
    for(int id:seen) {
        c.navigate(0);settled();
        int target=-1,index=0;
        for(const auto &value:c.moveRows()) {
            const auto row=value.toMap();
            if(row["white"].toMap()["id"]==id || row["black"].toMap()["id"]==id) target=index;
            ++index;
        }
        check(target>=0 && QMetaObject::invokeMethod(list,"positionViewAtIndex",Q_ARG(int,target),Q_ARG(int,0)),"Can scroll to each branch row");
        QTest::qWait(40);
        auto *cell=visualItem(window->contentItem(),"notebookMove_"+QString::number(id));
        check(cell && cell->isVisible(),"Every branch cell can be materialized");
        auto p=cell->mapToScene(QPointF(cell->width()/2,cell->height()/2));
        check(p.y()>=list->mapToScene(QPointF()).y() && p.y()<=list->mapToScene(QPointF(0,list->height())).y(),"Half-move cell is visible before click");
        QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,p.toPoint());settled();
        cell=visualItem(window->contentItem(),"notebookMove_"+QString::number(id));
        check(c.state()["node"].toInt()==id && cell && cell->property("move").toMap()["active"].toBool(),"Half-move click selects exact node and highlights its cell");
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

    auto *flowSwitch=visualItem(window->contentItem(),"notebookLayoutFlow");
    check(flowSwitch,"Notebook offers the continuous layout");
    QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,flowSwitch->mapToScene(QPointF(flowSwitch->width()/2,flowSwitch->height()/2)).toPoint());settled();
    auto *flowView=visualItem(window->contentItem(),"moveFlow");
    check(c.preferences()["notebookLayout"]=="flow" && flowView && flowView->isVisible() && !list->isVisible(),"Continuous layout replaces the table and is remembered");
    for(int id:seen) {
        c.navigate(0);settled();
        auto *token=visualItem(window->contentItem(),"flowMove_"+QString::number(id));
        check(token && token->isVisible(),"Every move exists in continuous layout");
        const auto p=token->mapToScene(QPointF(token->width()/2,token->height()/2));
        QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,p.toPoint());settled();
        token=visualItem(window->contentItem(),"flowMove_"+QString::number(id));
        check(c.state()["node"].toInt()==id && token && token->property("active").toBool(),"Continuous move click selects exact node");
    }
    auto *row1=visualItem(window->contentItem(),"flowMove_1"),*row2=visualItem(window->contentItem(),"flowMove_"+QString::number(game.nodes[1].children[0]));
    check(row1 && row2 && std::abs(row1->mapToScene(QPointF()).y()-row2->mapToScene(QPointF()).y())<1,"White and black moves share a line");
    auto *start=visualItem(window->contentItem(),"flowInitialPosition");
    check(start,"Continuous layout has an initial position button");
    QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,start->mapToScene(QPointF(start->width()/2,start->height()/2)).toPoint());settled();
    check(c.state()["node"]==0,"Continuous layout keeps the initial position reachable");
    auto *tableSwitch=visualItem(window->contentItem(),"notebookLayoutTable");
    check(tableSwitch,"Notebook offers the table layout");
    QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,tableSwitch->mapToScene(QPointF(tableSwitch->width()/2,tableSwitch->height()/2)).toPoint());settled();
    check(c.preferences()["notebookLayout"]=="table" && list->isVisible() && !flowView->isVisible(),"Table layout can be restored");
    c.setPreference("notebookLayout","bogus");
    check(c.preferences()["notebookLayout"]=="table","Unknown notebook layout is rejected");

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
        c.setPreference("language",scenario.language);window->resize(scenario.width,scenario.height);QTest::qWait(100);settled();
        auto *cell=visualItem(window->contentItem(),"notebookMove_1");
        check(cell && cell->width()>35 && cell->height()>20,"Move cell survives smallest viewport with maximum scales");
        check(cell->mapToScene(QPointF(cell->width(),0)).x()<=window->width(),"Move cell stays inside window");
        auto *outer=visualItem(window->contentItem(),"analysisViewport");
        auto *flick=outer->property("contentItem").value<QQuickItem *>();
        qInfo()<<"Notebook case"<<index<<"cell"<<cell->mapToScene(QPointF())<<cell->height()<<"outer"<<outer->height()<<"flick"<<flick->property("contentY")<<flick->property("contentHeight")<<flick->height();
        check(cell->mapToItem(outer,QPointF()).y()>=-1 && cell->mapToItem(outer,QPointF(0,cell->height())).y()<=outer->height()+1,
              "Selected cell is visible in the real window, including outer page scrolling");
        check(c.state()["fen"]==original["fen"] && c.state()["comment"]==original["comment"] && c.state()["dirty"]==original["dirty"],"Appearance preserves unsaved study and comments");
        if(!screenshotDir.isEmpty()) {
            auto grab=window->contentItem()->grabToImage();QSignalSpy ready(grab.data(),&QQuickItemGrabResult::ready);
            check(ready.wait(3000) && grab->saveToFile(screenshotDir+"/notebook-"+QString::number(index)+".png"),"Notebook screenshot saved");
        }
        ++index;
    }
    c.step(2);QTest::qWait(100);
    const int endNode=c.state()["node"].toInt();
    auto *endCell=visualItem(window->contentItem(),"notebookMove_"+QString::number(endNode));
    check(endCell && endCell->mapToItem(list,QPointF(0,endCell->height())).y()<=list->height()+1 &&
          endCell->mapToItem(list,QPointF()).y()>=-1,"Automatic scrolling reveals final move past long comments");
    list->setProperty("contentY",100);c.flip();QTest::qWait(100);
    check(std::abs(list->property("contentY").toDouble()-100)<2,"Same-node refresh preserves manual scrolling");
    c.navigate(1);c.saveGame(); const auto gameId=c.state()["savedId"].toLongLong();
    check(gameId>0 && !c.state()["dirty"].toBool(),"Notebook study saved with a real library ID");
    c.newGame();check(c.moveRows().size()==1,"Temporary empty study replaces the saved game");
    c.loadGame(gameId);c.navigate(1);
    check(c.state()["comment"]==original["comment"] && c.state()["nag"]==3,"Notebook comments and NAG survive save/reopen");
    c.exportFile(directory+"/notebook.pgn");QFile output(directory+"/notebook.pgn");
    check(output.open(QIODevice::ReadOnly),"Notebook PGN export exists");
    const auto exported=output.readAll();
    check(exported.contains("Nf3") && !exported.contains("Cf3") && t64::parsePgn(exported.toStdString()).front().nodes.size()==game.nodes.size(),"Standard SAN and all branches survive PGN round trip");
    c.setFen(QString::fromStdString(blackStart.nodes[0].fen));c.move("e7e5");c.navigate(0);settled();
    const auto fenRows=c.moveRows();
    check(fenRows[1].toMap()["number"]==23 && fenRows[1].toMap()["white"].toMap().isEmpty(),"Black-start FEN reaches the actual UI model");
    QMetaObject::invokeMethod(list,"positionViewAtIndex",Q_ARG(int,1),Q_ARG(int,0));QTest::qWait(40);
    auto *fenBlack=visualItem(window->contentItem(),"notebookMove_1");
    check(fenBlack,"Black-start FEN has a separate black cell");
    QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,fenBlack->mapToScene(QPointF(fenBlack->width()/2,fenBlack->height()/2)).toPoint());settled();
    check(c.state()["node"]==1 && c.state()["fen"].toString().toStdString()==blackStart.nodes[1].fen,"Clicking black FEN cell shows its exact position");
    {
        const chess::Board start;
        QFile book(directory+"/book.bin");
        check(book.open(QIODevice::WriteOnly),"Book fixture written");
        for(auto [from,to,weight]:{std::tuple{11,27,1},std::tuple{12,28,3}}) {
            const quint64 key=start.hash(),move=quint64(to|from<<6)<<48|quint64(weight)<<32;
            for(quint64 part:{key,move}) for(int shift=56;shift>=0;shift-=8) book.putChar(char(part>>shift&0xff));
        }
        book.close();
        c.newGame();c.setPreference("bookPath",directory+"/book.bin");
        const auto moves=c.bookMoves();
        check(moves.size()==2 && moves[0].toMap()["uci"]=="e2e4" && moves[0].toMap()["percent"]==75 && moves[1].toMap()["san"]=="d4",
              "Book moves reach the analysis panel with their share");
        c.move("e2e4");
        check(c.bookMoves().isEmpty(),"Leaving the book empties the list");
        QSignalSpy messages(&c,&Controller::message);
        c.setPreference("bookPath",directory+"/missing.bin");
        check(c.bookMoves().isEmpty() && messages.size()==1 && messages.first().at(1).toBool(),"Unreadable book is reported, not used");
        c.setPreference("bookPath","");
    }
    {
        c.newGame();c.move("g1f3");
        c.setPreference("figurines",true);
        bool figurine=false;
        for(const auto &value:c.moveFlow()) figurine|=value.toMap()["text"].toString()=="♘f3";
        check(figurine,"Figurine preference reaches the notebook");
        c.setPreference("figurines",false);
        auto *drop=visualItem(window->contentItem(),"fileDrop");
        QVariant first;
        const QVariantList urls{QUrl("file:///tmp/notes.txt"),QUrl("file:///tmp/Games.PGN"),QUrl("file:///tmp/b.pgn")};
        check(drop && QMetaObject::invokeMethod(drop,"firstFile",Q_RETURN_ARG(QVariant,first),Q_ARG(QVariant,QVariant(urls))) &&
              first.toString()=="file:///tmp/Games.PGN","Drop area keeps the first importable file");
        QVariant none;
        QMetaObject::invokeMethod(drop,"firstFile",Q_RETURN_ARG(QVariant,none),Q_ARG(QVariant,QVariant(QVariantList{QUrl("file:///tmp/a.zip")})));
        check(none.toString().isEmpty(),"Drop area ignores other files");
    }
    check(warnings.isEmpty(),"Notebook has no QML warnings");
    std::cout<<checks<<" notebook checks passed\n";
    return 0;
}
