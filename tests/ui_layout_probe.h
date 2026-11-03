// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <cmath>
// Exercise the real application with an isolated library, including live preference changes.
int uiLayoutProbe(const QString &directory) {
    QCoreApplication::setApplicationVersion(T64_VERSION);
    Controller c(directory + "/layout");
    c.move("e2e4"); c.move("c7c5"); c.saveGame();
    c.step(-2);c.createCard("Opening practice");c.step(2);
    c.createRepertoire("White repertoire",true);QElapsedTimer ready;ready.start();
    while(c.busy()&&ready.elapsed()<10000)QTest::qWait(10);
    check(!c.busy(),"Layout: repertoire fixture is ready");
    c.inspectPlayer("Blancs");c.setPreference("fontFamily","DejaVu Sans");
    c.setComment("Unsaved note — preserved during appearance changes");
    const auto original = c.state();
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("controller", &c);
    QSignalSpy warnings(&engine, &QQmlEngine::warnings);
    engine.load(QUrl("qrc:/ui/Main.qml"));
    check(!engine.rootObjects().isEmpty(), "Layout: QML loads");
    auto *window=qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    check(window, "Layout: application window exists");
    struct Scenario {int width,height;double text,ui;bool light;const char *language;};
    const Scenario scenarios[]={{960,640,1,1,false,"fr"},{1366,768,1,1,false,"fr"},{1440,940,1,1,false,"fr"},{1920,1080,1,1,true,"en"},{2560,1600,1,1,false,"en"},{960,640,1.5,1,true,"en"},{1366,768,1.5,1.25,false,"fr"},{1440,940,1.5,1.25,true,"en"},{960,640,1.5,1.25,true,"en"}};
    const auto output=qEnvironmentVariable("T64_UI_SCREENSHOTS");
    if(!output.isEmpty()) QDir().mkpath(output);
    QJsonArray report;
    auto capture=[&](const QString &name) {
        if(output.isEmpty())return;
        auto grab=window->contentItem()->grabToImage();
        check(bool(grab), "Layout: screenshot initiated");
        QSignalSpy ready(grab.data(), &QQuickItemGrabResult::ready);
        check(ready.wait(3000), "Layout: screenshot ready");
        check(grab->saveToFile(output+"/"+name+".png"), "Layout: screenshot saved");
    };
    int index=0;
    for(const auto &s:scenarios) {
        auto *language=window->findChild<QObject *>("languageSelector");
        auto *category=window->findChild<QObject *>("contentCategory");
        const int selectedCategory=category->property("currentIndex").toInt();
        c.setPreference("fontScale",s.text);c.setPreference("uiScale",s.ui);c.setPreference("lightTheme",s.light);c.setPreference("language",s.language);
        window->resize(s.width,s.height);QTest::qWait(80);
        check(language&&language->property("currentIndex").toInt()==(QString(s.language)=="en"?1:0),"Layout: language selector agrees with active language");
        auto *font=window->findChild<QObject *>("fontSelector");
        check(font&&font->property("currentText").toString()==c.preferences()["fontFamily"].toString(),"Layout: font selector agrees with selected font");
        check(category->property("currentIndex").toInt()==selectedCategory,"Layout: language changes preserve catalog selection");
        auto *catalog=window->findChild<QObject *>("contentList");
        check(catalog&&catalog->property("count").toInt()>0,"Layout: translated catalog still displays engines");
        for(int page=0;page<7;++page) {
            auto *nav=visualItem(window->contentItem(),"sectionNav"+QString::number(page));
            check(nav, "Layout: navigation is present");nav->forceActiveFocus(Qt::TabFocusReason);QTest::qWait(10);QTest::keyClick(window,Qt::Key_Space);QTest::qWait(60);
            check(window->property("page").toInt()==page, "Layout: keyboard navigation works");
            QJsonObject row{{"scenario",index},{"width",s.width},{"height",s.height},{"textScale",s.text},{"uiScale",s.ui},{"page",page},{"language",s.language},{"light",s.light}};
            if(page==0||page==2) {
                auto *board=visualItem(window->contentItem(),page==0?"homeBoard":"chessboard");
                check(board&&board->width()>=160&&std::abs(board->width()-board->height())<1, "Layout: usable square chessboard");
                auto pos=board->mapToScene(QPointF());
                check(pos.x()>=0 && pos.x()+board->width()<=window->width()+1, "Layout: board stays within horizontal viewport");
                row["boardWidth"]=board->width();
                if(page==0) {
                    auto *card=visualItem(window->contentItem(),"homeBoardCard");
                    auto *actions=visualItem(window->contentItem(),"homeBoardCommands");
                    check(card&&actions,"Layout: home board card and controls exist");
                    check(actions->mapToItem(card,QPointF(0,actions->height())).y()<=card->height()+1,"Layout: home board controls stay inside their card after resize");
                }
                if(page==2) {
                    auto *save=visualItem(window->contentItem(),"saveStudy");check(save, "Layout: save action exists");
                    auto p=save->mapToScene(QPointF());check(p.x()>=0&&p.x()+save->width()<=window->width()+1, "Layout: save action remains reachable");
                    auto *analyse=visualItem(window->contentItem(),"toggleEngineAnalysis");check(analyse,"Layout: engine analysis action exists");
                    auto a=analyse->mapToScene(QPointF());check(a.x()>=0&&a.x()+analyse->width()<=window->width()+1,"Layout: engine analysis action remains reachable");
                    check(save->property("text").toString()==(QString(s.language)=="en"?"Save":"Enregistrer"), "Layout: live language applies to save action");
                }
            }
            capture(QString("case%1-page%2").arg(index).arg(page)); report.append(row);
        }
        if(index==0||index==5||index==7||index==8) {
            for(const char *name:{"appearanceDialog","settingsDialog","advancedDialog","preparationDialog","repertoiresDialog","importsDialog","downloadsDialog","positionEditor","reviewDialog","compareDialog","tournamentDialog","analysisQueueDialog","updatesDialog","commandsDialog","fenDialog","pgnDialog","cardDialog","playDialog","tagsDialog","helpDialog","collectionDialog","deleteCardDialog","deleteVariationDialog","chess960Dialog","promotionDialog","reviewExplanationDialog"}) {
                auto *dialog=window->findChild<QObject *>(name);check(dialog, "Layout: dialog exists");
                check(QMetaObject::invokeMethod(dialog,"open"), "Layout: dialog opens");QTest::qWait(60);
                check(dialog->property("visible").toBool(), "Layout: dialog visible");
                if(QString::fromLatin1(name)=="preparationDialog") {
                    auto *prepare=window->findChild<QObject *>("calculatePreparation");
                    check(prepare&&prepare->property("accentColor").value<QColor>().name()==QColor(c.preferences()["accentColor"].toString()).name(),"Layout: visible dialog controls follow the application accent");
                }
                if(QString::fromLatin1(name)=="helpDialog") {
                    auto *title=visualItem(window->contentItem(),"helpTitle");
                    check(title&&title->property("contentWidth").toDouble()<=title->width()+1,"Layout: help title wraps inside its dialog");
                }
                check(dialog->property("width").toDouble()<=s.width&&dialog->property("height").toDouble()<=s.height, "Layout: dialog fits viewport");
                capture(QString("case%1-%2").arg(index).arg(name));QMetaObject::invokeMethod(dialog,"close");QTest::qWait(25);
            }
        }
        ++index;
    }
    auto *palette=window->findChild<QObject *>("commandsDialog");
    QMetaObject::invokeMethod(palette,"open");QTest::qWait(30);
    auto *search=window->findChild<QObject *>("commandSearch");search->setProperty("text","repertoire");QTest::qWait(30);
    check(window->findChild<QObject *>("commandResults")->property("count").toInt()==1,"Layout: command palette filters translated actions");
    QTest::keyClick(window,Qt::Key_Return);QTest::qWait(50);
    auto *repertoires=window->findChild<QObject *>("repertoiresDialog");
    check(!palette->property("visible").toBool()&&repertoires->property("visible").toBool(),"Layout: command palette executes the selected action");
    QMetaObject::invokeMethod(repertoires,"close");QTest::qWait(30);
    auto *toggle=visualItem(window->contentItem(),"sidebarToggle");
    toggle->forceActiveFocus();QTest::keyClick(window,Qt::Key_Space);QTest::qWait(40);
    check(!window->property("compactSidebar").toBool(),"Layout: navigation can be expanded in a small window");
    window->setProperty("page",2);QTest::qWait(60);
    capture("case8-expanded-sidebar");
    for(const char *name:{"saveStudy","toggleEngineAnalysis","appMenuButton"}) {
        auto *button=visualItem(window->contentItem(),name);check(button,"Layout: expanded navigation leaves controls available");
        const auto point=button->mapToScene(QPointF());
        if(point.x()<0||point.x()+button->width()>window->width()+1)qWarning()<<"Clipped expanded sidebar action"<<name<<point<<button->width()<<window->width();
        check(point.x()>=0&&point.x()+button->width()<=window->width()+1,"Layout: expanded navigation keeps controls inside the viewport");
    }
    toggle->forceActiveFocus();QTest::keyClick(window,Qt::Key_Space);QTest::qWait(30);
    QSettings saved;
    check(saved.value("fontScale").toDouble()==1.5&&saved.value("uiScale").toDouble()==1.25&&saved.value("language").toString()=="en","Layout: appearance preferences are persisted");
    check(c.state()["fen"]==original["fen"]&&c.state()["comment"]==original["comment"]&&c.state()["dirty"]==original["dirty"], "Layout: preferences preserve unsaved chess study");
    c.setPreference("fontScale",std::numeric_limits<double>::infinity());check(c.preferences()["fontScale"].toDouble()==1.5,"Layout: non-finite scale rejected");
    c.setPreference("fontFamily","Trait64MissingFont_abcdef");check(c.preferences()["fontFamily"].toString()!="Trait64MissingFont_abcdef","Layout: unavailable font falls back");
    c.setPreference("highContrast",true);check(c.preferences()["foreground"].toString()=="#000000","Layout: high contrast is applied");
    c.setPreference("reduceMotion",true);c.setPreference("sidebarCompact",true);QTest::qWait(30);
    check(warnings.isEmpty(), "Layout: no QML warnings");
    if(!output.isEmpty()){QFile file(output+"/matrix.json");check(file.open(QIODevice::WriteOnly),"Layout: report opened");file.write(QJsonDocument(QJsonObject{{"cases",report},{"checks",checks},{"qmlWarnings",warnings.count()}}).toJson());}
    std::cout<<"UI layout: "<<checks<<" checks, 9 scenarios, 63 page views, 100 dialog views, "<<warnings.count()<<" QML warnings\n";
    return 0;
}
