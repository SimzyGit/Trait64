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
    const Scenario scenarios[]={{960,640,1,1,false,"fr"},{1366,768,1,1,false,"fr"},{1440,940,1,1,false,"fr"},{1920,1080,1,1,true,"en"},{2560,1600,1,1,false,"en"},{960,640,1.5,1,true,"en"},{1366,768,1.5,1.25,false,"fr"},{1440,940,1.5,1.25,true,"en"},{1440,900,1,1,true,"fr"},{960,640,1.5,1.25,true,"en"}};
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
    int index=0,dialogViews=0;
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
        // Un bouton grisé garde la surface du thème : le groupe « désactivé » de la palette
        // restait clair, et en thème sombre ces boutons se peignaient en blanc.
        if(auto *palette=window->property("palette").value<QObject *>()) {
            auto *active=palette->property("active").value<QObject *>(),*disabled=palette->property("disabled").value<QObject *>();
            check(active&&disabled&&active->property("button").value<QColor>()==disabled->property("button").value<QColor>(),"Layout: disabled buttons keep the theme's surface");
        }
        auto *catalog=window->findChild<QObject *>("contentList");
        check(catalog&&catalog->property("count").toInt()>0,"Layout: translated catalog still displays engines");
        for(int page:{9,0,1,2,3,4,5,6}) {
            // Les fenêtres s'ouvrent par leurs onglets, sous le ruban.
            auto *nav=visualItem(window->contentItem(),"pageTab"+QString::number(page));
            check(nav&&nav->isVisible(), "Layout: navigation is present");nav->forceActiveFocus(Qt::TabFocusReason);QTest::qWait(10);QTest::keyClick(window,Qt::Key_Space);QTest::qWait(60);
            check(window->property("page").toInt()==page, "Layout: keyboard navigation works");
            QJsonObject row{{"scenario",index},{"width",s.width},{"height",s.height},{"textScale",s.text},{"uiScale",s.ui},{"page",page},{"language",s.language},{"light",s.light}};
            if(page==0) {
                auto *grid=visualItem(window->contentItem(),"basesGrid");
                check(grid&&grid->isVisible()&&grid->width()>=200,"Layout: the bases window shows its icons");
            }
            if(page==9||page==2) {
                // La vignette de l'accueil n'est construite qu'à l'affichage de la page.
                const char *boardName=page==9?"homeBoard":"chessboard";
                (void)QTest::qWaitFor([&]{auto *b=visualItem(window->contentItem(),boardName);return b&&b->width()>=160&&std::abs(b->width()-b->height())<1;},2000);
                auto *board=visualItem(window->contentItem(),boardName);
                if(!(board&&board->width()>=160&&std::abs(board->width()-board->height())<1))
                    qWarning()<<"Échiquier"<<boardName<<"page"<<page<<"scénario"<<index<<(board?board->size():QSizeF());
                check(board&&board->width()>=160&&std::abs(board->width()-board->height())<1, "Layout: usable square chessboard");
                auto pos=board->mapToScene(QPointF());
                check(pos.x()>=0 && pos.x()+board->width()<=window->width()+1, "Layout: board stays within horizontal viewport");
                row["boardWidth"]=board->width();
                if(page==9) {
                    auto *card=visualItem(window->contentItem(),"homeStudyCard");
                    check(card&&visualItem(window->contentItem(),"homeTodayCard"),"Layout: home study card and dashboard exist");
                    check(board->mapToItem(card,QPointF(board->width(),board->height())).y()<=card->height()+1&&
                          board->mapToItem(card,QPointF(board->width(),0)).x()<=card->width()+1,"Layout: home thumbnail stays inside its card after resize");
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
            for(const char *name:{"appearanceDialog","settingsDialog","advancedDialog","preparationDialog","repertoiresDialog","importsDialog","downloadsDialog","positionEditor","reviewDialog","compareDialog","tournamentDialog","puzzlesDialog","accountsDialog","analysisQueueDialog","updatesDialog","commandsDialog","fenDialog","pgnDialog","cardDialog","playDialog","tagsDialog","helpDialog","collectionDialog","deleteCardDialog","deleteVariationDialog","chess960Dialog","promotionDialog","reviewExplanationDialog","backupsDialog","crosstableDialog","endgamesDialog","guideDialog","labelImportDialog","libraryChartsDialog","namesDialog","plansDialog","positionQrDialog","saveSearchDialog","shortcutsDialog","similarDialog","skillDashboardDialog","removeBaseDialog"}) {
                auto *dialog=window->findChild<QObject *>(name);check(dialog, "Layout: dialog exists");++dialogViews;
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
    // Classic desktop shell: a menu bar with every family of actions, then workspace tabs.
    auto *menus=window->findChild<QObject *>("mainMenuBar");
    check(menus&&menus->property("count").toInt()==8,"Layout: the classic menu bar keeps its eight menus");
    auto *ribbonTabs=visualItem(window->contentItem(),"ribbonTabs");
    check(ribbonTabs&&ribbonTabs->isVisible()&&ribbonTabs->property("count").toInt()==8,"Layout: the ribbon offers its eight tabs");
    // Chaque onglet du ruban s'ouvre sans avertissement QML.
    // Le style En Croissant : barre latérale et menus, sans ruban ; puis un profil
    // personnalisé, et le retour au style ChessBase.
    auto *ribbonItem=window->findChild<QQuickItem *>("ribbon");
    QMetaObject::invokeMethod(window,"applyStyle",Q_ARG(QVariant,"croissant"));QTest::qWait(60);
    check(menus->property("visible").toBool()&&visualItem(window->contentItem(),"workspaceTabs")&&
          !ribbonItem->isVisible(),"Layout: the En Croissant style brings menus and sidebar");
    c.saveInterfaceProfile({{"name","Analyse seule"},{"showRibbon",false},{"showMenuBar",false},{"showSidebar",false},
                            {"showPageTabs",false},{"startPage",2},{"lightTheme",true}});QTest::qWait(60);
    check(c.preferences()["interfaceStyle"]=="custom"&&c.preferences()["showPageTabs"].toBool()&&!ribbonItem->isVisible()&&
          visualItem(window->contentItem(),"workspaceTabs")->isVisible(),
          "Layout: a custom profile without any navigation keeps the window tabs");
    c.removeInterfaceProfile("Analyse seule");
    QMetaObject::invokeMethod(window,"applyStyle",Q_ARG(QVariant,"chessbase"));QTest::qWait(60);
    check(ribbonItem->isVisible()&&!menus->property("visible").toBool()&&c.preferences()["interfaceProfiles"].toList().isEmpty(),
          "Layout: the ChessBase style brings the ribbon back");
    for(int tab=0;tab<8;++tab){window->findChild<QObject *>("ribbon")->setProperty("current",tab);QTest::qWait(20);capture(QString("ribbon-tab%1").arg(tab));}
    window->resize(960,640);window->setProperty("page",2);QTest::qWait(60);
    capture("case8-small-analysis");
    for(const char *name:{"saveStudy","toggleEngineAnalysis","appMenuButton","workspaceTabs","ribbonTabs"}) {
        auto *item=visualItem(window->contentItem(),name);check(item,"Layout: main controls exist in the smallest window");
        const auto point=item->mapToScene(QPointF());
        check(point.x()>=0&&point.x()+item->width()<=window->width()+1,"Layout: main controls stay inside the smallest window");
    }
    QSettings saved;
    check(saved.value("fontScale").toDouble()==1.5&&saved.value("uiScale").toDouble()==1.25&&saved.value("language").toString()=="en","Layout: appearance preferences are persisted");
    check(c.state()["fen"]==original["fen"]&&c.state()["comment"]==original["comment"]&&c.state()["dirty"]==original["dirty"], "Layout: preferences preserve unsaved chess study");
    c.setPreference("fontScale",std::numeric_limits<double>::infinity());check(c.preferences()["fontScale"].toDouble()==1.5,"Layout: non-finite scale rejected");
    c.setPreference("fontFamily","Trait64MissingFont_abcdef");check(c.preferences()["fontFamily"].toString()!="Trait64MissingFont_abcdef","Layout: unavailable font falls back");
    c.setPreference("highContrast",true);check(c.preferences()["foreground"].toString()=="#000000","Layout: high contrast is applied");
    c.setPreference("reduceMotion",true);c.setPreference("sidebarCompact",true);QTest::qWait(30);
    check(warnings.isEmpty(), "Layout: no QML warnings");
    if(!output.isEmpty()){QFile file(output+"/matrix.json");check(file.open(QIODevice::WriteOnly),"Layout: report opened");file.write(QJsonDocument(QJsonObject{{"cases",report},{"checks",checks},{"qmlWarnings",warnings.count()}}).toJson());}
    std::cout<<"UI layout: "<<checks<<" checks, 9 scenarios, 72 page views, "<<dialogViews<<" dialog views, "<<warnings.count()<<" QML warnings\n";
    return 0;
}
