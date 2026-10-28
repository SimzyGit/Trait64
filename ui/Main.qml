import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs as Native

ApplicationWindow {
    id: win
    width: Math.max(minimumWidth,Math.min(1440,Screen.desktopAvailableWidth)); height: Math.max(minimumHeight,Math.min(940,Screen.desktopAvailableHeight))
    minimumWidth: 960; minimumHeight: 640
    visible: true
    title: "Trait64 — " + controller.state.title + (controller.state.dirty ? " •" : "")
    onClosing: function(close) { if(controller.updater.state.status==="installing") close.accepted=false }
    Updates {id:updatesDialog;objectName:"updatesDialog";parent:Overlay.overlay;backend:controller.updater}
    property int page: 0
    property bool light: controller.preferences.lightTheme
    property color bg: controller.preferences.background
    property color panel: controller.preferences.panel
    property color raised: controller.preferences.raised
    property color line: controller.preferences.line
    property color fg: controller.preferences.foreground
    property color muted: controller.preferences.muted
    property color accent: controller.preferences.accentColor
    property bool exportAll: false
    property string promotionPrefix: ""
    property int selectedGame: 0
    property var nav: [qsTr("Accueil"), qsTr("Bibliothèque"), qsTr("Analyse"), qsTr("Ouvertures"), qsTr("Entraînement"), qsTr("Outils"), qsTr("Joueurs")]
    palette.window: bg; palette.windowText: fg; palette.base: panel; palette.text: fg
    palette.button: panel; palette.buttonText: fg; palette.mid: line
    palette.alternateBase: raised; palette.highlight: accent; palette.highlightedText: contrast(accent)
    palette.placeholderText: muted
    palette.dark: muted;palette.light:fg;palette.midlight:line;palette.shadow:line;palette.brightText:fg
    color: bg
    font.family: controller.preferences.fontFamily
    font.pixelSize: Math.round(13*textScale)
    function contrast(c){function linear(v){return v<=.04045?v/12.92:Math.pow((v+.055)/1.055,2.4)}return .2126*linear(c.r)+.7152*linear(c.g)+.0722*linear(c.b)>.179?"#171b21":"#ffffff"}
    property double selectedBaseId:0
    property var advanced:({})
    function applyAdvanced(){var f=Object.assign({},advanced);f.sourceId=selectedBaseId;controller.searchLibrary(searchField.text,resultBox.currentIndex===0?"":resultBox.currentText,favoriteCheck.checked,positionCheck.checked,trashCheck.checked,f)}
    function filterLibrary() {applyAdvanced()}
    Connections {target:controller;function onLibraryOpened(){win.selectedBaseId=0;win.advanced=({});baseBox.currentIndex=0;win.filterLibrary()}}
    function openExport(all) {exportAll=all;exportDialog.open()}

    property real uiScale: controller.preferences.uiScale
    property real textScale: controller.preferences.fontScale * uiScale
    property string uiLanguage: controller.preferences.language
    onUiLanguageChanged: controller.retranslateUi(win)
    property bool sidebarExpanded: false
    readonly property bool compactSidebar: !sidebarExpanded && (controller.preferences.sidebarCompact || width < 1180 * uiScale)
    function systemText(text) { var language=controller.preferences.language; return controller.uiText(text) }
    function sidebarAction(action) {
        if(action === "preparationDialog") preparationDialog.open()
        else if(action === "engineSettings") settingsDialog.open()
        else appearanceDialog.open()
    }
    Rectangle {anchors.fill: parent; color: win.bg}
    RowLayout {
        anchors.fill: parent; spacing: 0
        T64Sidebar {
            objectName:"sidebar";Layout.preferredWidth:implicitWidth;Layout.fillHeight:true
            currentPage:win.page;preferences:controller.preferences;collapsed:win.compactSidebar
            onPageRequested:function(index){win.page=index}
            onActionRequested:function(action){win.sidebarAction(action)}
        }
        ColumnLayout {
            Layout.fillWidth:true;Layout.fillHeight:true;spacing:0
            Rectangle {
                Layout.fillWidth:true;Layout.preferredHeight:Math.max(68*win.uiScale,topbar.implicitHeight+16);color:win.bg
                Rectangle {anchors.bottom:parent.bottom;width:parent.width;height:1;color:win.line}
                RowLayout {
                    id:topbar;anchors.fill:parent;anchors.margins:12;spacing:8
                    TButton {objectName:"sidebarToggle";text:"☰";implicitWidth:40;Accessible.name:qsTr("Réduire ou développer la navigation");onClicked:{var expand=win.compactSidebar;win.sidebarExpanded=expand;controller.setPreference("sidebarCompact",!expand)}}
                    ColumnLayout {Layout.fillWidth:true;Layout.minimumWidth:0;spacing:2
                        Label {text:win.nav[win.page];Layout.fillWidth:true;elide:Text.ElideRight;font.weight:Font.DemiBold}
                        Label {text:controller.state.title;Layout.fillWidth:true;elide:Text.ElideRight;color:win.muted;font.pixelSize:Math.round(11*controller.preferences.fontScale*controller.preferences.uiScale)}
                    }
                    TButton {objectName:"commandPaletteButton";text:win.width>1250*win.textScale?qsTr("Rechercher une commande…")+"   Ctrl K":"Ctrl K";onClicked:commands.open()}
                    TButton {objectName:"themeToggle";text:win.light?"☾":"☀";implicitWidth:40;Accessible.name:qsTr("Changer le thème");onClicked:{controller.setPreference("lightTheme",!win.light);controller.setPreference("backgroundColor","")}}
                    TButton {objectName:"appearanceButton";text:"⚙";implicitWidth:40;Accessible.name:qsTr("Apparence et langue");onClicked:appearanceDialog.open()}
                    TButton {id:appMenuButton;objectName:"appMenuButton";text:qsTr("Menu");onClicked:appMenu.open()
                    Menu {id: appMenu; y: appMenuButton.height
                        MenuItem {text: qsTr("Préparation par position"); onTriggered: preparationDialog.open()}
                        MenuItem {text: qsTr("Mes répertoires"); onTriggered: repertoiresDialog.open()}
                        MenuItem {text: qsTr("Imports PGN reprenables"); onTriggered: importsDialog.open()}
                        MenuItem {text: qsTr("Moteurs et bases à télécharger"); onTriggered: downloadsDialog.open()}
                        MenuItem {text: qsTr("Paramètres"); onTriggered: settingsDialog.open()}
                        MenuItem {text: qsTr("Pièces, échiquier et couleurs"); onTriggered: appearanceDialog.open()}
                        MenuItem {text: qsTr("Mises à jour"); onTriggered: updatesDialog.open()}
                        MenuSeparator {}
                        MenuItem {text: qsTr("Aide & raccourcis"); onTriggered: helpDialog.open()}
                    }
                    }
                }
            }
        StackLayout {
            id: pages
            Layout.fillWidth: true; Layout.fillHeight: true; currentIndex: win.page
            Layout.leftMargin:16;Layout.rightMargin:16;Layout.topMargin:12;Layout.bottomMargin:12

            Home {
                backend: controller
                onNewStudy: controller.newGame()
                onImportStudy: importDialog.open()
                onResumeStudy: win.page = 2
                onShowLibrary: win.page = 1
                onShowTraining: win.page = 4
                onShowRepertoires: repertoiresDialog.open()
                onShowPreparation: preparationDialog.open()
                onShowEngines: settingsDialog.open()
            }

            // LIBRARY
            ScrollView {id:libraryViewport;contentWidth:availableWidth;clip:true;ScrollBar.horizontal.policy:ScrollBar.AlwaysOff
            ColumnLayout {width:libraryViewport.availableWidth;height:Math.max(libraryViewport.availableHeight,implicitHeight);spacing:12
                Label {text:qsTr("Votre bibliothèque");font.pixelSize:Math.round(26*win.textScale);font.weight:Font.DemiBold;Layout.fillWidth:true;wrapMode:Text.WordWrap}
                Label {text:(controller.stats.games||0)+qsTr(" parties · PGN, études et collections");color:win.muted;Layout.fillWidth:true;wrapMode:Text.WordWrap}
                Flow {Layout.fillWidth:true;spacing:8
                    TButton {text:qsTr("Téléchargements");onClicked:downloadsDialog.open()}
                    TButton {text:qsTr("Exporter la bibliothèque");onClicked:win.openExport(true)}
                    TButton {text:qsTr("+  Importer");accent:true;enabled:!controller.busy;onClicked:importDialog.open()}
                }
                Flow {Layout.fillWidth:true;spacing:8
                    TComboBox {id:baseBox;onlyFirstOption:true;objectName:"baseSelector";width:Math.min(parent.width,260*win.textScale);textRole:"title";valueRole:"id";model:[{id:0,title:"Toutes les bases"}].concat(controller.sources);onActivated:{win.selectedBaseId=currentValue;win.filterLibrary()} onModelChanged:currentIndex=Math.max(0,indexOfValue(win.selectedBaseId))}
                    TButton {text:qsTr("Imports reprenables");objectName:"showImports";onClicked:importsDialog.open()}
                    TButton {text:qsTr("Exporter cette base");enabled:baseBox.currentIndex>0;onClicked:sourceExportDialog.open()}
                }
                RowLayout {Layout.fillWidth:true;spacing:10
                    TextField {id:searchField;objectName:"librarySearch";placeholderText:qsTr("Rechercher un joueur, un code ECO, un tournoi…");Layout.fillWidth:true;Layout.minimumWidth:0;onTextChanged:searchTimer.restart();onAccepted:{searchTimer.stop();win.filterLibrary()}}
                    TComboBox {id:resultBox;model:["Tous résultats","1-0","0-1","1/2-1/2","*"];onActivated:win.filterLibrary();Layout.preferredWidth:155*win.textScale}
                }
                Flow {Layout.fillWidth:true;spacing:8
                    CheckBox {id:favoriteCheck;text:qsTr("Favoris");onToggled:win.filterLibrary()}
                    CheckBox {id:positionCheck;text:qsTr("Position de l’échiquier");onToggled:win.filterLibrary()}
                    CheckBox {id:trashCheck;text:qsTr("Corbeille");onToggled:win.filterLibrary()}
                    TButton {text:qsTr("Annuler la recherche");visible:controller.librarySearching;onClicked:controller.cancelLibrarySearch()}
                }
                Label {text:controller.busy?qsTr("Traitement en cours…"):controller.librarySearchStatus;color:win.muted;font.pixelSize:Math.round(11*win.textScale);Layout.fillWidth:true;wrapMode:Text.WordWrap}
                Rectangle {Layout.fillWidth:true;height:37*win.textScale;color:win.raised;radius:6
                    RowLayout {anchors.fill:parent;anchors.margins:12;spacing:12
                        Label {text:qsTr("JOUEURS");Layout.fillWidth:true;Layout.minimumWidth:0;color:win.muted}
                        Label {text:qsTr("ÉVÉNEMENT / COLLECTION");visible:libraryList.detailed;Layout.preferredWidth:libraryList.eventWidth;Layout.maximumWidth:libraryList.eventWidth;font.pixelSize:Math.round(10*win.textScale);color:win.muted}
                        Label {text:qsTr("DATE");visible:libraryList.detailed;Layout.preferredWidth:libraryList.dateWidth;Layout.maximumWidth:libraryList.dateWidth;color:win.muted}
                        Label {text:qsTr("RÉSULTAT");Layout.preferredWidth:libraryList.resultWidth;Layout.maximumWidth:libraryList.resultWidth;font.pixelSize:Math.round(10*win.textScale);color:win.muted}
                        Item {Layout.preferredWidth:libraryList.actionsWidth;Layout.maximumWidth:libraryList.actionsWidth}
                    }
                }
                ListView {id:libraryList;objectName:"libraryResults";Layout.fillWidth:true;Layout.fillHeight:true;Layout.minimumHeight:180;clip:true;spacing:5;model:controller.games;enabled:!controller.librarySearching;opacity:enabled?1:.5
                    readonly property bool detailed:width>1000*win.textScale
                    readonly property real eventWidth:210*win.textScale
                    readonly property real dateWidth:90*win.textScale
                    readonly property real resultWidth:68*win.textScale
                    readonly property real actionsWidth:150*win.textScale
                    ScrollBar.vertical:ScrollBar {}
                    delegate:Rectangle {required property var modelData;width:libraryList.width;height:(libraryList.detailed?76:92)*win.textScale;radius:6;color:win.panel
                        RowLayout {anchors.fill:parent;anchors.margins:12;spacing:12
                            ColumnLayout {Layout.fillWidth:true;Layout.minimumWidth:0;spacing:4
                                Label {text:modelData.white+" — "+modelData.black;elide:Text.ElideRight;Layout.fillWidth:true;font.weight:Font.DemiBold}
                                Label {text:modelData.eco?"ECO "+modelData.eco:qsTr("Étude personnelle");font.pixelSize:Math.round(10*win.textScale);color:win.muted;elide:Text.ElideRight;Layout.fillWidth:true}
                                Label {visible:!libraryList.detailed;text:[modelData.event,modelData.collection,modelData.date].filter(function(s){return !!s}).join(" · ");elide:Text.ElideRight;Layout.fillWidth:true;font.pixelSize:Math.round(10*win.textScale);color:win.muted}
                            }
                            ColumnLayout {visible:libraryList.detailed;Layout.preferredWidth:libraryList.eventWidth;Layout.maximumWidth:libraryList.eventWidth;spacing:5
                                Label {text:modelData.event;elide:Text.ElideRight;Layout.fillWidth:true}
                                Label {text:modelData.collection||qsTr("Sans collection");font.pixelSize:Math.round(10*win.textScale);color:win.muted;elide:Text.ElideRight;Layout.fillWidth:true}
                            }
                            Label {visible:libraryList.detailed;text:modelData.date;Layout.preferredWidth:libraryList.dateWidth;Layout.maximumWidth:libraryList.dateWidth;color:win.muted}
                            Label {text:modelData.result;Layout.preferredWidth:libraryList.resultWidth;Layout.maximumWidth:libraryList.resultWidth;font.weight:Font.DemiBold}
                            RowLayout {Layout.preferredWidth:libraryList.actionsWidth;Layout.maximumWidth:libraryList.actionsWidth;spacing:6
                                TButton {text:modelData.favorite?"★":"☆";Layout.preferredWidth:32*win.textScale;Accessible.name:qsTr("Favori");onClicked:controller.favorite(modelData.id,!modelData.favorite)}
                                TButton {text:qsTr("Ouvrir");Layout.fillWidth:true;onClicked:controller.loadGame(modelData.id)}
                                TButton {text:"⋮";Layout.preferredWidth:28*win.textScale;Accessible.name:qsTr("Actions de la partie");onClicked:{win.selectedGame=modelData.id;collectionField.text=modelData.collection||"";gameMenu.open()}}
                            }
                        }
                    }
                    Label {anchors.centerIn:parent;width:parent.width-24;wrapMode:Text.WordWrap;horizontalAlignment:Text.AlignHCenter;visible:controller.games.length===0;text:controller.librarySearching?qsTr("Recherche…"):controller.librarySearchStatus==="Recherche annulée."||controller.librarySearchStatus==="Recherche impossible."?controller.librarySearchStatus:qsTr("Aucune partie ne correspond aux filtres.");color:win.muted}
                }
                Flow {Layout.fillWidth:true;spacing:8
                    TButton {text:qsTr("Recherche avancée");onClicked:advancedDialog.open()}
                    TButton {text:qsTr("Doublons de coups");onClicked:controller.findDuplicates()}
                    TButton {text:qsTr("Page précédente");objectName:"previousLibraryPage";enabled:!controller.librarySearching&&controller.libraryPage>1;onClicked:controller.previousLibraryPage()}
                    TButton {text:qsTr("Page suivante");objectName:"nextLibraryPage";enabled:!controller.librarySearching&&controller.libraryHasNext;onClicked:controller.nextLibraryPage()}
                }
            }}

            // ANALYSIS
            ScrollView {id:analysisViewport;objectName:"analysisViewport";contentWidth:availableWidth;clip:true;ScrollBar.horizontal.policy:ScrollBar.AlwaysOff
ColumnLayout {width:analysisViewport.availableWidth;height:Math.max(analysisViewport.availableHeight,implicitHeight);
                Layout.margins:0;spacing:win.height<820?8:14
                RowLayout {Layout.fillWidth:true
                    ColumnLayout {spacing:3;Layout.fillWidth:true;Layout.minimumWidth:0
                        Label {Layout.fillWidth:true;Layout.minimumWidth:0;elide:Text.ElideRight;text:controller.state.training?(controller.state.repertoireTraining>0?qsTr("Retrouver le coup"):qsTr("Retrouver la ligne")):qsTr("Sur l’échiquier.");font.family:controller.preferences.fontFamily;font.pixelSize:Math.round(24*win.textScale)}
                        Label {visible:win.height>=820||controller.state.training;text:controller.state.training?qsTr("Jouez les coups de votre répertoire · ")+controller.state.trainingProgress:controller.state.title;color:win.muted;font.pixelSize:Math.round(12*controller.preferences.fontScale*controller.preferences.uiScale);elide:Text.ElideRight;Layout.fillWidth:true}
                    }
                    Item {Layout.fillWidth:true}
                    TButton {text:qsTr("Position suivante");visible:controller.state.repertoireTraining>0;enabled:controller.state.trainingComplete && controller.state.repertoireDue>0;onClicked:controller.startRepertoireTraining(controller.state.repertoireTraining)}
                    TButton {text:controller.state.training?qsTr("Quitter l’exercice"):qsTr("Nouvelle étude");onClicked:controller.state.training?controller.finishTraining():controller.newGame()}
                    TButton {objectName:"saveStudy";text:controller.state.training?qsTr("Voir la solution"):qsTr("Enregistrer");accent:true;onClicked:controller.state.training?controller.revealCard():controller.saveGame()}
                    TButton {text:"⋮";Accessible.name:qsTr("Actions de l’étude");visible:!controller.state.training;onClicked:analysisMenu.open()}
                }
                RowLayout {Layout.fillWidth:true
                    ListView {Layout.fillWidth:true;Layout.preferredHeight:38*win.textScale;orientation:ListView.Horizontal;model:controller.tabs;spacing:5;clip:true
                        delegate:RowLayout {required property var modelData;required property int index;spacing:0
                            TButton {text:(modelData.dirty?"• ":"")+modelData.title;accent:modelData.active;flat:!modelData.active;implicitHeight:32*win.textScale;implicitWidth:Math.min(260,Math.max(180,text.length*8+30));onClicked:controller.switchTab(index)}
                            TButton {text:"×";implicitWidth:27*win.textScale;implicitHeight:32*win.textScale;Accessible.name:qsTr("Fermer l’étude");onClicked:controller.closeTab(index)}
                        }
                    }
                    TButton {text:"↶";Accessible.name:qsTr("Annuler le coup");enabled:controller.state.canUndo;onClicked:controller.undo();ToolTip.visible:hovered||visualFocus;ToolTip.text:qsTr("Annuler le coup")}
                    TButton {text:"↷";Accessible.name:qsTr("Rétablir");enabled:controller.state.canRedo;onClicked:controller.redo();ToolTip.visible:hovered||visualFocus;ToolTip.text:qsTr("Rétablir")}
                    TButton {text:qsTr("Bilan");enabled:!controller.state.training;onClicked:reviewDialog.open()}
                }
                SplitView {
                    id:analysisSplit;Layout.minimumHeight:365*win.textScale;
                    property real savedRatio:controller.preferences.analysisPanelRatio
                    readonly property real fittedBoardWidth:Math.max(290,Math.min(width*savedRatio,width-312))
                    onFittedBoardWidthChanged:if(boardPanel&&!resizing)boardPanel.SplitView.preferredWidth=fittedBoardWidth
objectName:"analysisSplit"
                    Layout.fillWidth:true;Layout.fillHeight:true;orientation:Qt.Horizontal
                    onResizingChanged:if(!resizing&&width>0)controller.setPreference("analysisPanelRatio",boardPanel.width/width)
                    handle:Rectangle {implicitWidth:12;color:"transparent";Rectangle {anchors.centerIn:parent;width:2;height:36;radius:1;color:win.line}}
                    ColumnLayout {
                        id:boardPanel;SplitView.preferredWidth:analysisSplit.fittedBoardWidth;SplitView.minimumWidth:290;spacing:win.height<820?5:10
                        RowLayout {Layout.fillWidth:true;Layout.leftMargin:Math.max(0,(parent.width-chessboard.width)/2);Layout.rightMargin:Layout.leftMargin
                            Label {Layout.fillWidth:true;elide:Text.ElideRight;text:controller.state.flipped?(controller.state.tags.White||qsTr("Blancs")):(controller.state.tags.Black||qsTr("Noirs"));font.weight:Font.DemiBold}
                            Item {Layout.fillWidth:true}
                            Label {text:controller.state.clockRunning?(controller.state.flipped?controller.state.whiteClock:controller.state.blackClock):(controller.state.flipped?qsTr("BLANCS"):qsTr("NOIRS"));color:win.muted;font.pixelSize:Math.round(9*controller.preferences.fontScale*controller.preferences.uiScale);font.letterSpacing:1.5}
                        }
                        Item {
                            Layout.fillWidth:true;Layout.fillHeight:true;Layout.minimumHeight:200
                            Rectangle {width:chessboard.width+12;height:width;anchors.centerIn:parent;color:"transparent";border.color:win.line}
                            Board {id:chessboard;objectName:"chessboard";backend:controller;coordinates:controller.preferences.coordinates;width:Math.min(parent.width-12,parent.height-12);height:width;anchors.centerIn:parent}
                        }
                        RowLayout {Layout.fillWidth:true;Layout.leftMargin:Math.max(0,(parent.width-chessboard.width)/2);Layout.rightMargin:Layout.leftMargin
                            Label {Layout.fillWidth:true;elide:Text.ElideRight;text:controller.state.flipped?(controller.state.tags.Black||qsTr("Noirs")):(controller.state.tags.White||qsTr("Blancs"));font.weight:Font.DemiBold}
                            Item {Layout.fillWidth:true}
                            Label {text:controller.state.clockRunning?(controller.state.flipped?controller.state.blackClock:controller.state.whiteClock):(controller.state.flipped?qsTr("NOIRS"):qsTr("BLANCS"));color:win.muted;font.pixelSize:Math.round(9*controller.preferences.fontScale*controller.preferences.uiScale);font.letterSpacing:1.5}
                        }
                        RowLayout {Layout.fillWidth:true;spacing:6;Layout.leftMargin:Math.max(0,(parent.width-Math.max(chessboard.width,430))/2);Layout.rightMargin:Layout.leftMargin
                            TButton {text:"|◀";implicitWidth:38;padding:6;enabled:controller.state.canBack&&!controller.state.training;onClicked:controller.step(-2);Accessible.name:qsTr("Début de la partie");ToolTip.visible:hovered||visualFocus;ToolTip.text:qsTr("Début")}
                            TButton {text:"◀";implicitWidth:38;padding:6;enabled:controller.state.canBack&&!controller.state.training;onClicked:controller.step(-1);Accessible.name:qsTr("Coup précédent");ToolTip.visible:hovered||visualFocus;ToolTip.text:qsTr("Coup précédent")}
                            TButton {text:"▶";implicitWidth:38;padding:6;enabled:controller.state.canForward&&!controller.state.training;onClicked:controller.step(1);Accessible.name:qsTr("Coup suivant");ToolTip.visible:hovered||visualFocus;ToolTip.text:qsTr("Coup suivant")}
                            TButton {text:"▶|";implicitWidth:38;padding:6;enabled:controller.state.canForward&&!controller.state.training;onClicked:controller.step(2);Accessible.name:qsTr("Fin de la variante");ToolTip.visible:hovered||visualFocus;ToolTip.text:qsTr("Fin de la variante")}
                            TButton {text:"⇅";implicitWidth:38;onClicked:controller.flip();Accessible.name:qsTr("Retourner l’échiquier");ToolTip.visible:hovered||visualFocus;ToolTip.text:qsTr("Retourner l’échiquier · F")}
                            Item {Layout.fillWidth:true}
                            TButton {id:positionActions;objectName:"positionActions";text:analysisSplit.width>1000?qsTr("Position  ⋯"):"⋯";implicitWidth:analysisSplit.width>1000?110:38;visible:!controller.state.training;onClicked:positionMenu.open();Accessible.name:qsTr("Outils de la position")
                                Menu {id:positionMenu;y:-height
                                    MenuItem {text:qsTr("Éditer la position");onTriggered:positionEditor.open()}
                                    MenuItem {text:qsTr("Copier la FEN");onTriggered:controller.copyText("fen")}
                                    MenuItem {text:qsTr("Charger une FEN");onTriggered:fenDialog.open()}
                                    MenuItem {text:qsTr("Exporter le diagramme");onTriggered:diagramDialog.open()}
                                    MenuSeparator {}
                                    MenuItem {text:controller.state.playing?qsTr("Arrêter la partie"):qsTr("Jouer une partie");onTriggered:controller.state.playing?controller.stopPlaying():playDialog.open()}
                                }
                            }
                            TButton {text:controller.state.playing?qsTr("Arrêter"):qsTr("Jouer");visible:!controller.state.training&&analysisSplit.width>1000;onClicked:controller.state.playing?controller.stopPlaying():playDialog.open()}
                        }
                        Label {text:win.systemText(controller.state.clockResult||controller.state.status);color:win.accent;Layout.alignment:Qt.AlignHCenter;font.pixelSize:Math.round(11*controller.preferences.fontScale*controller.preferences.uiScale);Layout.topMargin:2}
                    }
                    ColumnLayout {
                        objectName:"analysisPanel"
                        SplitView.fillWidth:true;SplitView.minimumWidth:300;spacing:12
                        Rectangle {
                            Layout.fillWidth:true;implicitHeight:(controller.state.analysis?(win.height<760?152:204):88)*win.textScale;color:win.panel
                            Rectangle {width:3;height:parent.height;color:win.accent}
                            ColumnLayout {anchors.fill:parent;anchors.margins:14;spacing:8
                                RowLayout {Layout.fillWidth:true
                                    Label {Layout.fillWidth:true;Layout.minimumWidth:0;elide:Text.ElideRight;text:qsTr("01  /  ANALYSE MOTEUR");font.pixelSize:Math.round(10*controller.preferences.fontScale*controller.preferences.uiScale);font.letterSpacing:1.1;color:win.muted}
                                    TButton {objectName:"toggleEngineAnalysis";text:controller.state.analysis?qsTr("Arrêter"):qsTr("Analyser");accent:!controller.state.analysis;enabled:!controller.state.training;implicitHeight:Math.max(30*win.uiScale,36*win.textScale);onClicked:controller.toggleAnalysis()}
                                }
                                Label {text:win.systemText(controller.engineStatus)+(controller.state.opening?" · "+controller.state.opening:"");elide:Text.ElideRight;Layout.fillWidth:true;color:win.muted;font.pixelSize:Math.round(10*controller.preferences.fontScale*controller.preferences.uiScale)}
                                ListView {
                                    Layout.fillWidth:true;Layout.fillHeight:true;visible:controller.state.analysis;clip:true;model:controller.engineLines;spacing:5
                                    delegate:RowLayout {
                                        required property var modelData
                                        required property int index
                                        width:ListView.view.width;spacing:8
                                        Label {text:modelData.eval;Layout.preferredWidth:57;font.weight:Font.DemiBold;color:win.accent}
                                        ColumnLayout {Layout.fillWidth:true;spacing:2
                                            Label {text:modelData.san;elide:Text.ElideRight;Layout.fillWidth:true;font.pixelSize:Math.round(11*controller.preferences.fontScale*controller.preferences.uiScale)}
                                            Label {text:qsTr("Profondeur ")+modelData.depth+" · "+Math.round(Number(modelData.nps)/1000)+" kN/s";font.pixelSize:Math.round(9*controller.preferences.fontScale*controller.preferences.uiScale);color:win.muted}
                                        }
                                        TButton {text:"+";implicitWidth:28;implicitHeight:Math.max(30*win.uiScale,36*win.textScale);onClicked:controller.addEngineLine(index);ToolTip.visible:hovered||visualFocus;ToolTip.text:qsTr("Ajouter cette variante")}
                                    }
                                }
                            }
                        }
                        Rectangle {
                            Layout.fillWidth:true;Layout.fillHeight:true;color:win.panel
                            Rectangle {width:parent.width;height:1;color:win.line}
                            ColumnLayout {anchors.fill:parent;anchors.margins:12;spacing:7
                                TabBar {id:notationTabs;Layout.fillWidth:true
                                    TTabButton {text:qsTr("02  /  Carnet de coups");font.pixelSize:Math.round(11*controller.preferences.fontScale*controller.preferences.uiScale)}
                                    TTabButton {text:qsTr("Explorateur");font.pixelSize:Math.round(11*controller.preferences.fontScale*controller.preferences.uiScale)}
                                }
                                StackLayout {
                                    Layout.fillWidth:true;Layout.fillHeight:true;currentIndex:notationTabs.currentIndex
                                    MoveNotebook {
                                        backend: controller; textScale: win.textScale; outerViewport: analysisViewport
                                        foreground: win.fg; muted: win.muted; accent: win.accent
                                        raised: win.raised; lineColor: win.line
                                    }
                                    ListView {id:miniExplorer;model:controller.openings;clip:true;spacing:4
                                        delegate:ItemDelegate {
                                            required property var modelData
                                            width:miniExplorer.width;text:modelData.san+"     "+modelData.count+qsTr(" partie(s)");onClicked:controller.move(modelData.uci)
                                        }
                                        Label {anchors.centerIn:parent;visible:controller.openings.length===0;text:qsTr("Aucune suite dans votre base.");color:win.muted;font.pixelSize:Math.round(11*controller.preferences.fontScale*controller.preferences.uiScale)}
                                    }
                                }
                                RowLayout {Layout.fillWidth:true;visible:!controller.state.training
                                    TComboBox {id:nagBox;model:["Annotation","! Bon coup","? Erreur","!! Excellent","?? Gaffe","!? Intéressant","?! Douteux"];currentIndex:Math.min(controller.state.nag,6);onActivated:controller.setNag(currentIndex);Layout.fillWidth:true;font.pixelSize:Math.round(11*controller.preferences.fontScale*controller.preferences.uiScale)}
                                    TButton {text:"↑";enabled:controller.state.node>0;onClicked:controller.promote();ToolTip.visible:hovered||visualFocus;ToolTip.text:qsTr("Promouvoir en ligne principale")}
                                }
                                Label {text:qsTr("03  /  NOTES DE POSITION");font.pixelSize:Math.round(9*controller.preferences.fontScale*controller.preferences.uiScale);font.letterSpacing:1;color:win.muted;Layout.topMargin:6}
                                ScrollView {Layout.fillWidth:true;Layout.preferredHeight:(win.height<760?52:85)*win.textScale
                                    TextArea {id:note;objectName:"positionNote";text:controller.state.comment;readOnly:controller.state.training;placeholderText:qsTr("Une idée, un plan, une question…");wrapMode:TextEdit.Wrap;font.pixelSize:Math.round(12*controller.preferences.fontScale*controller.preferences.uiScale);selectByMouse:true
                                        onTextChanged:if(activeFocus&&text!==controller.state.comment)controller.setComment(text)
                                    }
                                }
                            }
                        }
                    }
                }
            }
}

            // OPENINGS
            ScrollView {id:openingsViewport;contentWidth:availableWidth;clip:true;ScrollBar.horizontal.policy:ScrollBar.AlwaysOff
ColumnLayout {width:openingsViewport.availableWidth;height:Math.max(openingsViewport.availableHeight,implicitHeight);
                Layout.margins:0;spacing:20
                Label {text:qsTr("Explorer vos ouvertures");font.family:controller.preferences.fontFamily;font.pixelSize:Math.round(30*controller.preferences.fontScale*controller.preferences.uiScale);font.weight:Font.Normal}
                Label {text:qsTr("Les suites jouées dans votre bibliothèque, depuis la position actuelle de l’échiquier.");color:win.muted;wrapMode:Text.WordWrap;Layout.fillWidth:true}
                Flow {Layout.fillWidth:true;spacing:8;TButton {text:qsTr("←  Reculer");enabled:controller.state.canBack;onClicked:controller.step(-1)} TButton {text:qsTr("Position initiale");onClicked:controller.step(-2)} TButton {text:qsTr("Voir l’échiquier");onClicked:win.page=2} Item {Layout.fillWidth:true} TButton {text:qsTr("Préparer");onClicked:preparationDialog.open()} TButton {text:qsTr("Mes répertoires");objectName:"showRepertoires";onClicked:repertoiresDialog.open()} TButton {text:qsTr("Créer une carte depuis ici");accent:true;onClicked:cardDialog.open()}}
                Label {text:controller.state.fen;color:win.muted;font.family:"monospace";font.pixelSize:Math.round(11*controller.preferences.fontScale*controller.preferences.uiScale);elide:Text.ElideRight;Layout.fillWidth:true}
                Rectangle {Layout.fillWidth:true;height:1;color:win.line}
                RowLayout {Layout.fillWidth:true;Label {text:qsTr("COUP");Layout.preferredWidth:95;color:win.muted;font.pixelSize:Math.round(10*controller.preferences.fontScale*controller.preferences.uiScale)} Label {text:qsTr("PARTIES");Layout.preferredWidth:90;color:win.muted;font.pixelSize:Math.round(10*controller.preferences.fontScale*controller.preferences.uiScale)} Label {text:qsTr("BLANCS / NULLES / NOIRS (RÉSULTATS TERMINÉS)");color:win.muted;font.pixelSize:Math.round(10*controller.preferences.fontScale*controller.preferences.uiScale);Layout.fillWidth:true}}
                ListView {id:openingList;Layout.minimumHeight:160;Layout.fillWidth:true;Layout.fillHeight:true;clip:true;spacing:10;model:controller.openings
                    delegate:Rectangle {
                        required property var modelData
                        width:openingList.width;height:62;radius:2;color:win.panel
                        RowLayout {anchors.fill:parent;anchors.margins:12;spacing:18
                            TButton {text:modelData.san;Layout.preferredWidth:76;onClicked:controller.move(modelData.uci)}
                            Label {text:modelData.count;Layout.preferredWidth:66;font.weight:Font.DemiBold}
                            Row {Layout.fillWidth:true;Layout.preferredHeight:25
                                property real total:Math.max(1,modelData.white+modelData.draw+modelData.black)
                                Rectangle {width:parent.width*modelData.white/parent.total;height:25;color:"#e6ecdf";Text {anchors.centerIn:parent;text:modelData.white;color:"#263329";font.pixelSize:Math.round(11*controller.preferences.fontScale*controller.preferences.uiScale);visible:parent.width>25}}
                                Rectangle {width:parent.width*modelData.draw/parent.total;height:25;color:"#80937f";Text {anchors.centerIn:parent;text:modelData.draw;color:"#ffffff";font.pixelSize:Math.round(11*controller.preferences.fontScale*controller.preferences.uiScale);visible:parent.width>25}}
                                Rectangle {width:parent.width*modelData.black/parent.total;height:25;color:"#3b4840";Text {anchors.centerIn:parent;text:modelData.black;color:"#ffffff";font.pixelSize:Math.round(11*controller.preferences.fontScale*controller.preferences.uiScale);visible:parent.width>25}}
                            }
                            TButton {text:qsTr("Jouer  →");onClicked:controller.move(modelData.uci)}
                        }
                    }
                    ColumnLayout {anchors.centerIn:parent;visible:controller.openings.length===0;spacing:15
                        Label {text:qsTr("Aucune partie ne continue depuis cette position.");color:win.muted}
                        TButton {text:qsTr("Importer un PGN");Layout.alignment:Qt.AlignHCenter;onClicked:importDialog.open()}
                    }
                }
                Label {text:qsTr("Chaque partie compte une fois par coup. Les parties inachevées sont comptées mais exclues de la barre de résultats.");wrapMode:Text.WordWrap;Layout.fillWidth:true;font.pixelSize:Math.round(11*controller.preferences.fontScale*controller.preferences.uiScale);color:win.muted}
            }
}

            // TRAINING
            ScrollView {id:trainingViewport;contentWidth:availableWidth;clip:true;ScrollBar.horizontal.policy:ScrollBar.AlwaysOff
ColumnLayout {width:trainingViewport.availableWidth;height:Math.max(trainingViewport.availableHeight,implicitHeight);
                Layout.margins:0;spacing:20
                ColumnLayout {Layout.fillWidth:true
                    ColumnLayout {Layout.fillWidth:true;spacing:6;Label {text:qsTr("Apprendre pour retrouver.");font.family:controller.preferences.fontFamily;font.pixelSize:Math.round(30*controller.preferences.fontScale*controller.preferences.uiScale);font.weight:Font.Normal} Label {text:qsTr("Vos propres lignes, avec des révisions espacées dans le temps.");Layout.fillWidth:true;wrapMode:Text.WordWrap;color:win.muted}}
                    Item {Layout.fillWidth:true}
                    TButton {text:qsTr("+  Créer une carte");accent:true;onClicked:cardDialog.open()}
                }
                Row {Layout.fillWidth:true;spacing:16
                    Repeater {model:[{title:"À réviser",key:"due"},{title:"Révisions effectuées",key:"reviews"},{title:"Réussites",key:"successes"}]
                        delegate:Rectangle {required property var modelData;width:(parent.width-32)/3;height:102*win.textScale;radius:2;color:win.panel;border.color:win.line
                            ColumnLayout {anchors.fill:parent;anchors.margins:20;Label {text:win.systemText(modelData.title);Layout.fillWidth:true;wrapMode:Text.WordWrap;color:win.muted;font.pixelSize:Math.round(11*controller.preferences.fontScale*controller.preferences.uiScale)} Label {text:controller.stats[modelData.key]||0;font.family:controller.preferences.fontFamily;font.pixelSize:Math.round(30*controller.preferences.fontScale*controller.preferences.uiScale);font.weight:Font.Normal}}
                        }
                    }
                }
                Flow {Layout.fillWidth:true;spacing:8
                    TButton {text:qsTr("Réviser un répertoire");onClicked:repertoiresDialog.open()}
                    TButton {text:qsTr("Réviser la prochaine carte");accent:true;enabled:(controller.stats.due||0)>0;onClicked:controller.nextDueCard()}
                    Label {text:qsTr("Aujourd’hui : ")+(controller.stats.todayReviews||0)+" / "+controller.preferences.dailyGoal+qsTr(" · Cette semaine : ")+(controller.stats.weekReviews||0);color:win.muted;Layout.fillWidth:true}
                    Label {text:qsTr("Objectif")} SpinBox {from:1;to:1000;value:controller.preferences.dailyGoal;onValueModified:controller.setPreference("dailyGoal",value)}
                }
                ProgressBar {Layout.fillWidth:true;from:0;to:controller.preferences.dailyGoal;value:controller.stats.todayReviews||0}
                Label {text:qsTr("MES CARTES");font.pixelSize:Math.round(10*controller.preferences.fontScale*controller.preferences.uiScale);color:win.muted;font.letterSpacing:1.5;Layout.topMargin:8}
                ListView {id:trainingList;Layout.minimumHeight:180;Layout.fillWidth:true;Layout.fillHeight:true;clip:true;spacing:8;model:controller.cards;ScrollBar.vertical:ScrollBar {}
                    delegate:Rectangle {required property var modelData;width:trainingList.width;height:80*win.textScale;radius:2;color:win.panel
                        RowLayout {anchors.fill:parent;anchors.margins:16;spacing:16
                            Label {text:"◎";font.pixelSize:Math.round(28*controller.preferences.fontScale*controller.preferences.uiScale);color:win.accent}
                            ColumnLayout {Layout.fillWidth:true;spacing:6
                                Label {text:modelData.title;font.weight:Font.DemiBold;elide:Text.ElideRight;Layout.fillWidth:true}
                                Label {text:qsTr("Révision : ")+modelData.due+" · "+modelData.reviews+qsTr(" passage(s) · intervalle ")+modelData.interval+qsTr(" j");color:win.muted;font.pixelSize:Math.round(11*controller.preferences.fontScale*controller.preferences.uiScale)}
                            }
                            TButton {text:qsTr("Réviser");accent:true;onClicked:controller.startCard(modelData.id)}
                            TButton {text:"×";Accessible.name:qsTr("Supprimer la carte");onClicked:{deleteCardDialog.cardId=modelData.id;deleteCardDialog.open()}}
                        }
                    }
                    ColumnLayout {anchors.centerIn:parent;width:parent.width-24;visible:controller.cards.length===0;spacing:16
                        Label {text:qsTr("Vos variantes deviennent votre entraînement.");font.pixelSize:Math.round(19*controller.preferences.fontScale*controller.preferences.uiScale);Layout.alignment:Qt.AlignHCenter;Layout.fillWidth:true;wrapMode:Text.WordWrap}
                        Label {text:qsTr("Dans une étude, placez-vous avant les coups à apprendre,\npuis cliquez sur « Créer une carte ».");horizontalAlignment:Text.AlignHCenter;color:win.muted;Layout.alignment:Qt.AlignHCenter;Layout.fillWidth:true;wrapMode:Text.WordWrap}
                        TButton {text:qsTr("Ouvrir l’atelier");Layout.alignment:Qt.AlignHCenter;onClicked:win.page=2}
                    }
                }
            }
}

            // TOOLS
            ScrollView {id:toolsViewport;contentWidth:availableWidth;clip:true
                ColumnLayout {width:toolsViewport.availableWidth;spacing:20
                    Label {text:qsTr("La boîte à outils");font.family:controller.preferences.fontFamily;font.pixelSize:Math.round(30*controller.preferences.fontScale*controller.preferences.uiScale);font.weight:Font.Normal;Layout.topMargin:0}
                    Label {text:qsTr("Importer, sauvegarder, vérifier. Gardez la main sur vos données.");color:win.muted;wrapMode:Text.WordWrap;Layout.fillWidth:true}
                    GridLayout {columns:toolsViewport.availableWidth>1000*win.textScale?2:1;columnSpacing:18;rowSpacing:18;Layout.fillWidth:true
                        Rectangle {Layout.fillWidth:true;implicitHeight:Math.max(192,children[0].implicitHeight+44);radius:8;color:win.panel;border.color:win.line
                            ColumnLayout {anchors.fill:parent;anchors.margins:22;spacing:12
                                Label {text:qsTr("PGN & positions");font.pixelSize:Math.round(18*controller.preferences.fontScale*controller.preferences.uiScale);font.weight:Font.DemiBold}
                                Label {text:qsTr("Ouvrir un fichier ou coller une partie et ses variantes.");wrapMode:Text.WordWrap;Layout.fillWidth:true;color:win.muted}
                                Item {Layout.fillHeight:true}
                                Flow {Layout.fillWidth:true;spacing:8;TButton {text:qsTr("Coller un PGN");onClicked:pgnDialog.open()} TButton {text:qsTr("Charger une FEN");onClicked:fenDialog.open()}}
                            }
                        }
                        Rectangle {Layout.fillWidth:true;implicitHeight:Math.max(192,children[0].implicitHeight+44);radius:8;color:win.panel;border.color:win.line
                            ColumnLayout {anchors.fill:parent;anchors.margins:22;spacing:12
                                Label {text:qsTr("Sauvegarde complète");font.pixelSize:Math.round(18*controller.preferences.fontScale*controller.preferences.uiScale);font.weight:Font.DemiBold}
                                Label {text:qsTr("Parties, favoris, collections et cartes dans un fichier SQLite.");wrapMode:Text.WordWrap;Layout.fillWidth:true;color:win.muted}
                                Item {Layout.fillHeight:true}
                                Flow {Layout.fillWidth:true;spacing:8;TButton {text:qsTr("Sauvegarder");onClicked:backupDialog.open()} TButton {text:qsTr("Ouvrir une base");onClicked:openDatabaseDialog.open()}}
                            }
                        }
                        Rectangle {Layout.fillWidth:true;implicitHeight:Math.max(192,children[0].implicitHeight+44);radius:8;color:win.panel;border.color:win.line
                            ColumnLayout {anchors.fill:parent;anchors.margins:22;spacing:12
                                Label {text:qsTr("Intégrité & diagnostic");font.pixelSize:Math.round(18*controller.preferences.fontScale*controller.preferences.uiScale);font.weight:Font.DemiBold}
                                Label {text:qsTr("Vérifiez la bibliothèque et comptez les positions légales depuis l’échiquier.");wrapMode:Text.WordWrap;Layout.fillWidth:true;color:win.muted}
                                Item {Layout.fillHeight:true}
                                Flow {Layout.fillWidth:true;spacing:8;TButton {text:qsTr("Vérifier la base");enabled:!controller.busy;onClicked:controller.verifyDatabase()} TButton {text:qsTr("Réindexer");enabled:!controller.busy;onClicked:controller.rebuildIndexes()}}
                            }
                        }
                        Rectangle {Layout.fillWidth:true;implicitHeight:Math.max(192,children[0].implicitHeight+44);radius:8;color:win.panel;border.color:win.line
                            ColumnLayout {anchors.fill:parent;anchors.margins:22;spacing:12
                                Label {text:qsTr("Fichiers & exemples");font.pixelSize:Math.round(18*controller.preferences.fontScale*controller.preferences.uiScale);font.weight:Font.DemiBold}
                                Label {text:qsTr("Accédez aux fichiers locaux ou chargez les études de découverte.");wrapMode:Text.WordWrap;Layout.fillWidth:true;color:win.muted}
                                Item {Layout.fillHeight:true}
                                Flow {Layout.fillWidth:true;spacing:8;TButton {text:qsTr("Dossier local");onClicked:controller.openDataFolder()} TButton {text:qsTr("Exemples");onClicked:controller.loadExamples()}}
                            }
                        }
                    }
                    Rectangle {Layout.fillWidth:true;implicitHeight:children[0].implicitHeight+44;radius:8;color:win.raised
                        ColumnLayout {anchors.fill:parent;anchors.margins:22;spacing:10
                            Label {text:qsTr("Trait64 est un logiciel libre.");font.pixelSize:Math.round(17*controller.preferences.fontScale*controller.preferences.uiScale);font.weight:Font.DemiBold}
                            Label {text:qsTr("C++23 · Qt6 · chess-library · moteurs UCI externes. Code sous AGPL-3.0-or-later.\nCette version alpha propose les outils locaux essentiels ; le catalogue complet reste en développement.");color:win.muted;wrapMode:Text.WordWrap;Layout.fillWidth:true;font.pixelSize:Math.round(12*controller.preferences.fontScale*controller.preferences.uiScale)}
                        }
                    }
                    Item {height:20}
                }
            }
            Players {backend:controller;Layout.margins:0;onPreparePlayer:function(name){preparationDialog.usePlayer(name)}}
        }
        }
    }
    footer:Rectangle {
        height:Math.max(32,32*win.textScale);color:win.bg
        Rectangle {width:parent.width;height:1;color:win.line}
        RowLayout {anchors.fill:parent;anchors.leftMargin:22;anchors.rightMargin:22
            Label {text:controller.repertoireTask.running?controller.repertoireTask.status+" · "+controller.repertoireTask.positions+" positions":controller.busy?"●  "+controller.taskStatus:controller.state.dirty?qsTr("●  Modifications · récupération de session automatique"):qsTr("●  Prêt");font.pixelSize:Math.round(10*controller.preferences.fontScale*controller.preferences.uiScale);color:win.muted;Layout.fillWidth:true;Layout.minimumWidth:0;elide:Text.ElideRight}
            TButton {visible:controller.busy;text:controller.resumableImport?qsTr("Mettre en pause"):qsTr("Interrompre");implicitHeight:Math.max(24,24*win.textScale);onClicked:controller.cancelTask()}
            Label {text:controller.state.training?qsTr("Entraînement local"):qsTr("Ctrl S  Enregistrer   ·   F  Retourner   ·   ← →  Parcourir");font.pixelSize:Math.round(10*controller.preferences.fontScale*controller.preferences.uiScale);color:win.muted;visible:win.width>=1350*win.textScale}
            TButton {text:"Trait64 / "+controller.updater.state.current;flat:true;implicitHeight:Math.max(26,26*win.textScale);font.pixelSize:Math.round(10*controller.preferences.fontScale*controller.preferences.uiScale);onClicked:updatesDialog.open();ToolTip.visible:hovered||visualFocus;ToolTip.text:qsTr("Versions et mises à jour")}
        }
    }

    Timer {id:searchTimer;interval:220;onTriggered:win.filterLibrary()}
    Connections {target:controller
        function onMessage(text,error){toastLabel.text=text;toast.error=error;toast.open();toastTimer.restart()}
        function onPromotionNeeded(prefix){win.promotionPrefix=prefix;promotionDialog.open()}
        function onShowAnalysis(){win.page=2}
        function onShowAnalysisQueue(){analysisQueueDialog.open()}
    }
    Popup {id:toast;parent:Overlay.overlay;property bool error:false;x:(parent.width-width)/2;y:Math.max(8,parent.height-height-64);width:Math.min(760,win.width-280);height:toastLabel.implicitHeight+topPadding+bottomPadding;padding:18;closePolicy:Popup.CloseOnEscape|Popup.CloseOnPressOutside
        background:Rectangle {color:toast.error?"#633c37":win.raised;border.color:toast.error?"#a86559":win.line;radius:2}
        contentItem:Label {id:toastLabel;wrapMode:Text.WordWrap;color:toast.error?"#ffffff":win.fg}
    }
    Timer {id:toastTimer;interval:6500;onTriggered:toast.close()}
    Preparation {id:preparationDialog;objectName:"preparationDialog";backend:controller}
    Repertoires {id:repertoiresDialog;objectName:"repertoiresDialog";backend:controller}
    Imports {id:importsDialog;objectName:"importsDialog";backend:controller}
    Downloads {id:downloadsDialog;objectName:"downloadsDialog";backend:controller;onConfigureEngines:settingsDialog.open()}
    Native.FileDialog {id:sourceExportDialog;title:qsTr("Exporter cette base en PGN");fileMode:Native.FileDialog.SaveFile;defaultSuffix:"pgn";nameFilters:["PGN (*.pgn)"];onAccepted:controller.exportSource(selectedFile.toString(),baseBox.currentValue)}
    Native.FileDialog {id:importDialog;title:qsTr("Importer des parties");nameFilters:[qsTr("Parties et positions (*.pgn *.t64 *.epd *.fen)"),qsTr("Tous les fichiers (*)")];onAccepted:controller.importFile(selectedFile.toString())}
    Native.FileDialog {id:exportDialog;title:qsTr("Exporter en PGN ou archive T64 expérimentale");fileMode:Native.FileDialog.SaveFile;defaultSuffix:"pgn";nameFilters:["PGN (*.pgn)",qsTr("Position EPD (*.epd)"),qsTr("Archive T64 expérimentale (*.t64)")];onAccepted:controller.exportFile(selectedFile.toString(),win.exportAll)}
    Native.FileDialog {id:backupDialog;title:qsTr("Créer une sauvegarde complète");fileMode:Native.FileDialog.SaveFile;defaultSuffix:"sqlite";nameFilters:[qsTr("Bibliothèque Trait64 (*.sqlite)")];onAccepted:controller.backup(selectedFile.toString())}
    Native.FileDialog {id:openDatabaseDialog;title:qsTr("Ouvrir une bibliothèque Trait64");nameFilters:[qsTr("Bibliothèque Trait64 (*.sqlite)")];onAccepted:controller.openBackup(selectedFile.toString())}
    Native.FileDialog {id:engineFileDialog;title:qsTr("Choisir l’exécutable Stockfish ou UCI");onAccepted:{controller.setPreference("enginePath",selectedFile.toString());enginePathField.text=controller.preferences.enginePath}}

    TDialog {id:fenDialog;objectName:"fenDialog";title:qsTr("Charger une position FEN");anchors.centerIn:parent;width:Math.min(650,win.width-32);height:Math.min(340,win.height-32);modal:true;standardButtons:Dialog.Ok|Dialog.Cancel
        ScrollView {id:fenDialogViewport;anchors.fill:parent;contentWidth:availableWidth;clip:true;ScrollBar.horizontal.policy:ScrollBar.AlwaysOff
        ColumnLayout {width:fenDialogViewport.availableWidth;spacing:15
            Label {text:qsTr("Collez une FEN complète. L’étude en cours sera conservée dans votre bibliothèque.");wrapMode:Text.WordWrap;Layout.fillWidth:true;color:win.muted}
            TextArea {id:fenField;Layout.fillWidth:true;Layout.preferredHeight:95;wrapMode:TextEdit.Wrap;selectByMouse:true;placeholderText:"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"}
        }
        }
        onAccepted:controller.setFen(fenField.text)
    }
    TDialog {id:pgnDialog;objectName:"pgnDialog";title:qsTr("Importer un texte PGN");anchors.centerIn:parent;width:Math.min(700,win.width-32);height:Math.min(550,win.height-32);modal:true;standardButtons:Dialog.Ok|Dialog.Cancel
        ScrollView {anchors.fill:parent;TextArea {id:pgnField;placeholderText:"[Event \"Mon étude\"]\n\n1. e4 e5 2. Nf3 *";wrapMode:TextEdit.Wrap;selectByMouse:true}}
        onAccepted:controller.importText(pgnField.text)
    }
    TDialog {id:promotionDialog;objectName:"promotionDialog";width:Math.min(650,win.width-32);title:qsTr("Choisir la promotion");anchors.centerIn:parent;modal:true
        Flow {width:parent.width;spacing:12;Repeater {model:[{label:qsTr("♛ Dame"),piece:"q"},{label:qsTr("♜ Tour"),piece:"r"},{label:qsTr("♝ Fou"),piece:"b"},{label:qsTr("♞ Cavalier"),piece:"n"}]
            delegate:TButton {required property var modelData;text:modelData.label;onClicked:{controller.move(win.promotionPrefix+modelData.piece);promotionDialog.close()}}
        }}
    }
    TDialog {id:cardDialog;objectName:"cardDialog";title:qsTr("Créer une carte d’entraînement");anchors.centerIn:parent;width:Math.min(620,win.width-32);height:Math.min(360,win.height-32);modal:true;standardButtons:Dialog.Ok|Dialog.Cancel
        ScrollView {id:cardDialogViewport;anchors.fill:parent;contentWidth:availableWidth;clip:true;ScrollBar.horizontal.policy:ScrollBar.AlwaysOff
        ColumnLayout {width:cardDialogViewport.availableWidth;spacing:14
            Label {text:qsTr("La suite principale depuis la position actuelle sera à retrouver (40 demi-coups maximum). Placez-vous avant le premier coup à apprendre.");color:win.muted;wrapMode:Text.WordWrap;Layout.fillWidth:true}
            TextField {id:cardTitle;placeholderText:qsTr("Ex. Italienne — mon plan avec les Blancs");Layout.fillWidth:true}
        }
        }
        onAccepted:controller.createCard(cardTitle.text)
    }
    TDialog {id:deleteCardDialog;objectName:"deleteCardDialog";width:Math.min(580,win.width-32);property int cardId:0;title:qsTr("Supprimer cette carte ?");anchors.centerIn:parent;modal:true;standardButtons:Dialog.Yes|Dialog.No
        Label {text:qsTr("L’activité déjà enregistrée restera dans votre progression.");width:parent.width;wrapMode:Text.WordWrap}
        onAccepted:controller.deleteCard(cardId)
    }
    TDialog {id:playDialog;objectName:"playDialog";title:qsTr("Partie locale");anchors.centerIn:parent;width:Math.min(620,win.width-32);height:Math.min(600,win.height-32);modal:true
        ScrollView {id:playDialogViewport;anchors.fill:parent;contentWidth:availableWidth;clip:true;ScrollBar.horizontal.policy:ScrollBar.AlwaysOff
        ColumnLayout {width:playDialogViewport.availableWidth;spacing:16
            TComboBox {id:opponent;model:["Deux joueurs sur cet ordinateur","Contre le moteur UCI"];currentIndex:1;Layout.fillWidth:true}
            GridLayout {columns:2;Layout.fillWidth:true;Label {text:qsTr("Minutes")} SpinBox {id:clockMinutes;from:1;to:180;value:10;editable:true} Label {text:qsTr("Incrément (s)")} SpinBox {id:clockIncrement;from:0;to:120;value:5;editable:true}}
            TComboBox {id:playSide;model:["Je joue les Blancs","Je joue les Noirs"];Layout.fillWidth:true}
            TButton {text:qsTr("Démarrer avec pendule");accent:true;Layout.fillWidth:true;onClicked:{controller.startLocalGame(clockMinutes.value,clockIncrement.value,opponent.currentIndex,playSide.currentIndex===0);playDialog.close()}}
            TButton {text:qsTr("Moteur sans pendule");visible:opponent.currentIndex===1;Layout.fillWidth:true;onClicked:{controller.playComputer(playSide.currentIndex===0);playDialog.close()}}
            Label {text:qsTr("La partie démarre depuis la position de l’échiquier. Parcourir les coups arrête la pendule.");wrapMode:Text.WordWrap;Layout.fillWidth:true;color:win.muted;font.pixelSize:Math.round(11*controller.preferences.fontScale*controller.preferences.uiScale)}
        }
        }
    }
    TDialog {id:tagsDialog;objectName:"tagsDialog";title:qsTr("Informations sur la partie");anchors.centerIn:parent;width:Math.min(700,win.width-32);height:Math.min(740,win.height-32);modal:true;standardButtons:Dialog.Close
        ScrollView {id:tagsDialogViewport;anchors.fill:parent;contentWidth:availableWidth;clip:true;ScrollBar.horizontal.policy:ScrollBar.AlwaysOff
        GridLayout {width:tagsDialogViewport.availableWidth;columns:2;columnSpacing:16;rowSpacing:10
            Repeater {model:["White","Black","Event","Site","Date","Round","Result","ECO","WhiteElo","BlackElo"]
                delegate:ColumnLayout {required property string modelData;Layout.fillWidth:true;spacing:4
                    Label {text:({White:qsTr("Blancs"),Black:qsTr("Noirs"),Event:qsTr("Événement"),Site:qsTr("Lieu"),Date:qsTr("Date (AAAA.MM.JJ)"),Round:qsTr("Ronde"),Result:qsTr("Résultat : 1-0 / 0-1 / 1/2-1/2 / *"),ECO:qsTr("Code ECO"),WhiteElo:qsTr("Elo Blancs"),BlackElo:qsTr("Elo Noirs")})[modelData];font.pixelSize:Math.round(10*controller.preferences.fontScale*controller.preferences.uiScale);color:win.muted;wrapMode:Text.WordWrap;Layout.fillWidth:true}
                    TextField {text:controller.state.tags[modelData]||"";Layout.fillWidth:true;onEditingFinished:{var t={};t[modelData]=text;controller.setTags(t)}}
                }
            }
        }
        }
    }
    Menu {id:analysisMenu;x:win.width-width-32;y:172
        MenuItem {text:qsTr("Analyses en arrière-plan");onTriggered:analysisQueueDialog.open()}
        MenuItem {text:qsTr("Comparer plusieurs moteurs");onTriggered:compareDialog.open()}
        MenuItem {text:qsTr("Éditer la position");onTriggered:positionEditor.open()}
        MenuItem {text:qsTr("Copier la position FEN");onTriggered:controller.copyText("fen")}
        MenuItem {text:controller.state.playing?qsTr("Arrêter la partie"):qsTr("Jouer une partie");onTriggered:controller.state.playing?controller.stopPlaying():playDialog.open()}
        MenuItem {text:qsTr("Nouvelle partie Chess960");onTriggered:chess960Dialog.open()}
        MenuItem {text:qsTr("Dupliquer l’étude");onTriggered:controller.duplicateStudy()}
        MenuItem {text:qsTr("Identifier et renseigner l’ouverture");onTriggered:controller.identifyOpening()}
        MenuItem {text:qsTr("Exporter le diagramme en PNG");onTriggered:diagramDialog.open()}
        MenuItem {text:qsTr("Exporter l’étude en PDF");onTriggered:pdfDialog.open()}
        MenuItem {text:qsTr("Effacer les flèches et cercles");onTriggered:controller.clearAnnotations()}
        MenuItem {text:qsTr("Abandonner la partie");visible:controller.state.playing;onTriggered:controller.resignGame()}
        MenuItem {text:qsTr("Réclamer la nulle");visible:controller.state.playing;onTriggered:controller.claimDraw()}
        MenuItem {text:qsTr("Informations de la partie");onTriggered:tagsDialog.open()}
        MenuItem {text:qsTr("Exporter cette étude");onTriggered:win.openExport(false)}
        MenuItem {text:qsTr("Copier le PGN");onTriggered:controller.copyText("pgn")}
        MenuItem {text:qsTr("Créer une carte depuis ici");onTriggered:cardDialog.open()}
        MenuSeparator {}
        MenuItem {text:qsTr("Promouvoir cette variante");enabled:controller.state.node>0;onTriggered:controller.promote()}
        MenuItem {text:qsTr("Supprimer ce coup et sa suite…");enabled:controller.state.node>0;onTriggered:deleteVariationDialog.open()}
    }
    TDialog {id:deleteVariationDialog;objectName:"deleteVariationDialog";width:Math.min(580,win.width-32);title:qsTr("Supprimer cette variante ?");anchors.centerIn:parent;modal:true;standardButtons:Dialog.Yes|Dialog.No
        Label {text:qsTr("Le coup sélectionné et ses continuations seront retirés.");width:parent.width;wrapMode:Text.WordWrap}
        onAccepted:controller.deleteVariation()
    }
    Menu {id:gameMenu;x:win.width-width-40;y:220
        MenuItem {text:qsTr("Fusionner dans l’étude courante");onTriggered:controller.mergeGame(win.selectedGame)}
        MenuItem {text:qsTr("Modifier la collection");onTriggered:collectionDialog.open()}
        MenuItem {text:trashCheck.checked?qsTr("Restaurer la partie"):qsTr("Mettre à la corbeille");onTriggered:controller.trash(win.selectedGame,!trashCheck.checked)}
    }
    TDialog {id:collectionDialog;objectName:"collectionDialog";title:qsTr("Collection de la partie");anchors.centerIn:parent;modal:true;width:440;standardButtons:Dialog.Ok|Dialog.Cancel
        TextField {id:collectionField;anchors.fill:parent;placeholderText:qsTr("Ex. Préparation tournoi, Répertoire Blancs…")}
        onAccepted:controller.setCollection(win.selectedGame,collectionField.text)
    }
    PositionEditor {id:positionEditor;objectName:"positionEditor";backend:controller}
    AnalysisQueue {id:analysisQueueDialog;objectName:"analysisQueueDialog";backend:controller}
    Review {id:reviewDialog;objectName:"reviewDialog";backend:controller}
    CompareEngines {id:compareDialog;objectName:"compareDialog";backend:controller}
    Native.FileDialog {id:diagramDialog;title:qsTr("Exporter le diagramme");fileMode:Native.FileDialog.SaveFile;defaultSuffix:"png";nameFilters:["Image PNG (*.png)"];onAccepted:controller.exportDiagram(selectedFile.toString())}
    Native.FileDialog {id:pdfDialog;title:qsTr("Exporter l’étude en PDF");fileMode:Native.FileDialog.SaveFile;defaultSuffix:"pdf";nameFilters:["PDF (*.pdf)"];onAccepted:controller.exportStudyPdf(selectedFile.toString())}
    TDialog {id:chess960Dialog;objectName:"chess960Dialog";title:qsTr("Nouvelle étude Chess960");anchors.centerIn:parent;modal:true;standardButtons:Dialog.Ok|Dialog.Cancel
        ColumnLayout {Label {text:qsTr("Numéro de la position (518 = position classique)")} SpinBox {id:frcNumber;from:0;to:959;value:518;editable:true}}
        onAccepted:controller.newChess960(frcNumber.value)
    }
    Appearance {id:appearanceDialog;objectName:"appearanceDialog";backend:controller}
    TDialog {id:settingsDialog;objectName:"settingsDialog";title:qsTr("Paramètres");anchors.centerIn:parent;width:700;height:Math.min(690,win.height-32);modal:true;standardButtons:Dialog.Close
        ScrollView {id:settingsScroll;anchors.fill:parent;contentWidth:availableWidth;clip:true
        ColumnLayout {width:settingsScroll.availableWidth;spacing:18
            Label {text:qsTr("MOTEUR UCI LOCAL");font.pixelSize:Math.round(10*controller.preferences.fontScale*controller.preferences.uiScale);font.letterSpacing:1.5;color:win.muted}
            RowLayout {Layout.fillWidth:true;TextField {id:enginePathField;Layout.fillWidth:true;text:controller.preferences.enginePath;placeholderText:"/usr/games/stockfish";onEditingFinished:controller.setPreference("enginePath",text)} TButton {text:qsTr("Parcourir");onClicked:engineFileDialog.open()}}
            CheckBox {text:qsTr("Sauvegardes automatiques (14 instantanés)");checked:controller.preferences.autoBackup;onToggled:controller.setPreference("autoBackup",checked)}
            TButton {text:qsTr("Créer une sauvegarde automatique maintenant");onClicked:controller.automaticBackup()}
            GridLayout {columns:settingsScroll.availableWidth>700*win.textScale?4:2;columnSpacing:14;rowSpacing:10;Layout.fillWidth:true
                Label {text:"Threads";color:win.muted} SpinBox {from:1;to:32;value:controller.preferences.threads;onValueModified:controller.setPreference("threads",value)}
                Label {text:qsTr("Mémoire (Mio)");color:win.muted} SpinBox {from:16;to:4096;stepSize:16;value:controller.preferences.hash;onValueModified:controller.setPreference("hash",value)}
                Label {text:qsTr("Variantes");color:win.muted} SpinBox {from:1;to:5;value:controller.preferences.multiPV;onValueModified:controller.setPreference("multiPV",value)}
                Label {text:qsTr("Niveau de jeu");color:win.muted} SpinBox {from:0;to:20;value:controller.preferences.skill;onValueModified:controller.setPreference("skill",value)}
                Label {text:qsTr("Réflexion (ms)");color:win.muted} SpinBox {from:100;to:10000;stepSize:100;value:controller.preferences.moveTime;onValueModified:controller.setPreference("moveTime",value)}
            }
            Label {text:qsTr("Le niveau concerne le jeu contre Stockfish. L’analyse utilise sa pleine force.\nLes réglages sont appliqués au démarrage du moteur.");color:win.muted;font.pixelSize:Math.round(11*controller.preferences.fontScale*controller.preferences.uiScale);wrapMode:Text.WordWrap;Layout.fillWidth:true}
            RowLayout {Label {text:qsTr("Profondeur d’analyse");color:win.muted} SpinBox {from:1;to:99;value:controller.preferences.analysisDepth;onValueModified:controller.setPreference("analysisDepth",value)}}
            TextField {Layout.fillWidth:true;text:controller.preferences.syzygyPath;placeholderText:qsTr("Dossier des tablebases Syzygy locales (optionnel)");onEditingFinished:controller.setPreference("syzygyPath",text);selectByMouse:true}
            RowLayout {Layout.fillWidth:true
                TextField {id:profileName;Layout.fillWidth:true;placeholderText:qsTr("Nom du profil moteur")}
                TButton {text:qsTr("Mémoriser");onClicked:controller.saveEngineProfile(profileName.text)}
            }
            RowLayout {Layout.fillWidth:true
                ComboBox {id:profileBox;Layout.fillWidth:true;model:controller.preferences.engineProfiles;textRole:"name"}
                TButton {text:qsTr("Charger");enabled:profileBox.currentIndex>=0;onClicked:controller.loadEngineProfile(profileBox.currentIndex)}
                TButton {text:"×";enabled:profileBox.currentIndex>=0;onClicked:controller.removeEngineProfile(profileBox.currentIndex)}
            }
            RowLayout {TButton {text:qsTr("Démarrer / appliquer");accent:true;onClicked:controller.startEngine()} Label {text:win.systemText(controller.engineStatus);elide:Text.ElideRight;Layout.fillWidth:true;font.pixelSize:Math.round(11*controller.preferences.fontScale*controller.preferences.uiScale);color:win.muted}}
            Rectangle {Layout.fillWidth:true;height:1;color:win.line}
            RowLayout {CheckBox {text:qsTr("Thème clair");checked:controller.preferences.lightTheme;onToggled:controller.setPreference("lightTheme",checked)} CheckBox {text:qsTr("Coordonnées de l’échiquier");checked:controller.preferences.coordinates;onToggled:controller.setPreference("coordinates",checked)}}
            TButton {text:qsTr("Télécharger d’autres moteurs");onClicked:{settingsDialog.close();downloadsDialog.open()}}
            EngineOptions {backend:controller}
            Label {text:qsTr("Vos données : ")+controller.preferences.dataPath;wrapMode:Text.WordWrap;Layout.fillWidth:true;color:win.muted;font.pixelSize:Math.round(10*controller.preferences.fontScale*controller.preferences.uiScale)}
        }
    }
    }
    TDialog {id:helpDialog;objectName:"helpDialog";title:qsTr("Bienvenue dans Trait64");anchors.centerIn:parent;width:Math.min(760,win.width-32);height:Math.min(700,win.height-32);modal:true;standardButtons:Dialog.Close
        ScrollView {id:helpScroll;anchors.fill:parent;contentWidth:availableWidth;clip:true
        ColumnLayout {width:helpScroll.availableWidth;spacing:15
            Label {objectName:"helpTitle";Layout.fillWidth:true;Layout.minimumWidth:0;wrapMode:Text.WordWrap;text:qsTr("Un atelier personnel pour vos échecs.");font.pixelSize:Math.round(21*controller.preferences.fontScale*controller.preferences.uiScale);font.weight:Font.DemiBold}
            Label {text:qsTr("• Cliquez une pièce puis sa case, ou glissez-la.\n• Revenez dans une partie et jouez un autre coup pour créer une variante.\n• Clic droit glissé : flèche ; clic droit sur une case : cercle.\n• Enregistrez une étude pour l’ajouter à votre bibliothèque.\n• Importez un PGN pour alimenter l’explorateur d’ouvertures.\n• Créez des cartes à partir des lignes que vous souhaitez retenir.");wrapMode:Text.WordWrap;Layout.fillWidth:true;lineHeight:1.5}
            Label {text:qsTr("Ctrl N : nouvelle étude    ·    Ctrl O : importer\nCtrl S : enregistrer    ·    Ctrl K : commandes\n← / → : précédent / suivant    ·    F : retourner\nDébut / Fin : début / fin de la ligne");Layout.fillWidth:true;wrapMode:Text.WordWrap;color:win.muted;lineHeight:1.6}
            Label {text:qsTr("Version de développement 0.3.0. Les sauvegardes PGN et SQLite sont recommandées pour vos études importantes. Le format .t64 est expérimental.");wrapMode:Text.WordWrap;Layout.fillWidth:true;font.pixelSize:Math.round(11*controller.preferences.fontScale*controller.preferences.uiScale);color:win.muted}
        }}
    }
    TDialog {id:advancedDialog;objectName:"advancedDialog";title:qsTr("Recherche avancée");anchors.centerIn:parent;width:Math.min(690,win.width-32);height:Math.min(760,win.height-32);modal:true;standardButtons:Dialog.Ok|Dialog.Cancel
        property var draft:({})
        onOpened:{draft=Object.assign({},win.advanced);minElo.value=draft.minElo||0;maxElo.value=draft.maxElo||0;maxPlies.value=draft.maxPlies||0;sortBy.currentIndex=Math.max(0,["updated","date","players","elo","length"].indexOf(draft.sort||"updated"));colourFilter.currentIndex=Math.max(0,["","white","black"].indexOf(draft.colour||""))}
        ScrollView {id:advancedScroll;anchors.fill:parent;contentWidth:availableWidth;clip:true
        GridLayout {width:advancedScroll.availableWidth;columns:2;columnSpacing:12;rowSpacing:10
            Repeater {model:[{key:"player",label:qsTr("Joueur (selon la couleur ci-dessous)")},{key:"openingName",label:qsTr("Ouverture : Française, Najdorf… → ECO")},{key:"white",label:qsTr("Blancs")},{key:"black",label:qsTr("Noirs")},{key:"event",label:qsTr("Tournoi / événement")},{key:"eco",label:qsTr("Code ECO ou fragment")},{key:"ecoFrom",label:qsTr("ECO de (ex. B20)")},{key:"ecoTo",label:qsTr("ECO à (ex. B99)")},{key:"collection",label:qsTr("Collection personnelle")},{key:"material",label:qsTr("Matériel exact (ex. KRkrp)")},{key:"dateFrom",label:qsTr("À partir de (AAAA.MM.JJ)")},{key:"dateTo",label:qsTr("Jusqu’au (AAAA.MM.JJ)")}]
                delegate:ColumnLayout {required property var modelData;Layout.fillWidth:true
                    Label {text:modelData.label;wrapMode:Text.WordWrap;Layout.fillWidth:true;font.pixelSize:Math.round(11*controller.preferences.fontScale*controller.preferences.uiScale)}
                    TextField {Layout.fillWidth:true;text:advancedDialog.draft[modelData.key]||"";onTextEdited:advancedDialog.draft[modelData.key]=text}
                }
            }
            Label {text:qsTr("Couleur du joueur")} TComboBox {id:colourFilter;model:["Les deux couleurs","Blancs","Noirs"]}
            Label {text:qsTr("Elo minimal des deux joueurs");wrapMode:Text.WordWrap;Layout.fillWidth:true} SpinBox {id:minElo;from:0;to:3500;stepSize:100;editable:true}
            Label {text:qsTr("Elo maximal (0 = sans limite)");wrapMode:Text.WordWrap;Layout.fillWidth:true} SpinBox {id:maxElo;from:0;to:3500;stepSize:100;editable:true}
            Label {text:qsTr("Demi-coups maximum (0 = tous)");wrapMode:Text.WordWrap;Layout.fillWidth:true} SpinBox {id:maxPlies;from:0;to:2000;editable:true}
            CheckBox {text:qsTr("Correspondance exacte des champs texte");font.pixelSize:Math.round(12*win.textScale);checked:advancedDialog.draft.exactNames||false;Layout.columnSpan:2;onToggled:advancedDialog.draft.exactNames=checked}
            Label {text:qsTr("Trier par")} TComboBox {id:sortBy;model:["Modification","Date","Joueurs","Elo","Longueur"]}
            Label {text:qsTr("Le nom d’ouverture sélectionne les codes ECO du catalogue Lichess, pas une variante exacte. Pour celle-ci, utilisez la position de l’échiquier. Les filtres Elo excluent les classements inconnus. Le matériel est recherché dans toutes les variantes.");wrapMode:Text.WordWrap;Layout.fillWidth:true;Layout.columnSpan:2;font.pixelSize:Math.round(10*controller.preferences.fontScale*controller.preferences.uiScale);color:win.muted}
            TButton {text:qsTr("Effacer les filtres");onClicked:{win.advanced={};win.applyAdvanced();advancedDialog.close()}}
        }}
        onAccepted:{draft.minElo=minElo.value;draft.maxElo=maxElo.value;draft.maxPlies=maxPlies.value;draft.colour=["","white","black"][colourFilter.currentIndex];draft.sort=["updated","date","players","elo","length"][sortBy.currentIndex];win.advanced=Object.assign({},draft);win.applyAdvanced()}
    }
    TDialog {id:commands;objectName:"commandsDialog";title:qsTr("Commandes rapides");anchors.centerIn:parent;width:Math.min(680,win.width-32);height:Math.min(620,win.height-32);modal:true;standardButtons:Dialog.Close
        readonly property var entries:[
            {title:qsTr("Nouvelle étude"),run:function(){controller.newGame()}},
            {title:qsTr("Enregistrer"),run:function(){controller.saveGame()}},
            {title:qsTr("Importer un PGN"),run:function(){importDialog.open()}},
            {title:qsTr("Accueil"),run:function(){win.page=0}},
            {title:qsTr("Bibliothèque"),run:function(){win.page=1}},
            {title:qsTr("Analyse"),run:function(){win.page=2}},
            {title:qsTr("Ouvertures"),run:function(){win.page=3}},
            {title:qsTr("Entraînement"),run:function(){win.page=4}},
            {title:qsTr("Outils"),run:function(){win.page=5}},
            {title:qsTr("Joueurs"),run:function(){win.page=6}},
            {title:qsTr("Mes répertoires"),run:function(){repertoiresDialog.open()}},
            {title:qsTr("Préparation par position"),run:function(){preparationDialog.open()}},
            {title:qsTr("Moteurs et profils"),run:function(){settingsDialog.open()}},
            {title:qsTr("Comparer les moteurs"),run:function(){compareDialog.open()}},
            {title:qsTr("File d’analyse"),run:function(){analysisQueueDialog.open()}},
            {title:qsTr("Imports PGN reprenables"),run:function(){importsDialog.open()}},
            {title:qsTr("Téléchargements"),run:function(){downloadsDialog.open()}},
            {title:qsTr("Apparence et langue"),run:function(){appearanceDialog.open()}},
            {title:qsTr("Recherche avancée"),run:function(){advancedDialog.open()}},
            {title:qsTr("Éditer la position"),run:function(){positionEditor.open()}},
            {title:qsTr("Charger une FEN"),run:function(){fenDialog.open()}},
            {title:qsTr("Coller un PGN"),run:function(){pgnDialog.open()}},
            {title:qsTr("Créer une carte d’entraînement"),run:function(){cardDialog.open()}},
            {title:qsTr("Bilan de l’étude"),run:function(){reviewDialog.open()}},
            {title:qsTr("Jouer une partie"),run:function(){playDialog.open()}},
            {title:qsTr("Ouvrir une base"),run:function(){openDatabaseDialog.open()}},
            {title:qsTr("Sauvegarde complète"),run:function(){backupDialog.open()}},
            {title:qsTr("Exporter cette étude"),run:function(){win.openExport(false)}},
            {title:qsTr("Exporter le diagramme en PNG"),run:function(){diagramDialog.open()}},
            {title:qsTr("Exporter l’étude en PDF"),run:function(){pdfDialog.open()}},
            {title:qsTr("Mises à jour"),run:function(){updatesDialog.open()}},
            {title:qsTr("Aide & raccourcis"),run:function(){helpDialog.open()}}
        ]
        function choose(index){if(index>=0&&index<commandResults.count){var action=commandResults.model[index].run;close();action()}}
        onOpened:{commandSearch.clear();commandSearch.forceActiveFocus()}
        ColumnLayout {anchors.fill:parent;spacing:10
            TextField {id:commandSearch;objectName:"commandSearch";Layout.fillWidth:true;placeholderText:qsTr("Rechercher une commande…");selectByMouse:true;onAccepted:commands.choose(0);Keys.onDownPressed:{commandResults.currentIndex=0;commandResults.forceActiveFocus()}}
            ListView {id:commandResults;objectName:"commandResults";Layout.fillWidth:true;Layout.fillHeight:true;clip:true;spacing:4;ScrollBar.vertical:ScrollBar {}
                model:commands.entries.filter(function(entry){return entry.title.toLocaleLowerCase().indexOf(commandSearch.text.toLocaleLowerCase())>=0})
                delegate:ItemDelegate {required property var modelData;required property int index;width:commandResults.width;text:modelData.title;highlighted:ListView.isCurrentItem;onClicked:commands.choose(index)}
                Keys.onReturnPressed:commands.choose(currentIndex)
                Keys.onEnterPressed:commands.choose(currentIndex)
                Label {anchors.centerIn:parent;text:qsTr("Aucune commande trouvée.");visible:commandResults.count===0}
            }
        }
    }

    Shortcut {sequence:"Ctrl+N";onActivated:controller.newGame()}
    Shortcut {sequence:"Ctrl+O";onActivated:importDialog.open()}
    Shortcut {sequence:"Ctrl+S";onActivated:controller.saveGame()}
    Shortcut {sequence:"Ctrl+K";onActivated:commands.open()}
    function navigationKeys(){var f=win.activeFocusItem;return win.page===2&&!(f && (f.cursorPosition!==undefined || f.editable===true))&&!downloadsDialog.visible&&!appearanceDialog.visible&&!positionEditor.visible&&!settingsDialog.visible&&!fenDialog.visible&&!tagsDialog.visible&&!cardDialog.visible&&!commands.visible&&!playDialog.visible&&!analysisQueueDialog.visible&&!reviewDialog.visible&&!compareDialog.visible&&!updatesDialog.visible&&!preparationDialog.visible&&!repertoiresDialog.visible&&!importsDialog.visible&&!advancedDialog.visible&&!helpDialog.visible&&!pgnDialog.visible&&!collectionDialog.visible&&!promotionDialog.visible&&!chess960Dialog.visible&&!deleteCardDialog.visible&&!deleteVariationDialog.visible&&!appMenu.visible&&!analysisMenu.visible&&!positionMenu.visible}
    Shortcut {sequence:"Left";enabled:win.navigationKeys();onActivated:controller.step(-1)}
    Shortcut {sequence:"Right";enabled:win.navigationKeys();onActivated:controller.step(1)}
    Shortcut {sequence:"Home";enabled:win.navigationKeys();onActivated:controller.step(-2)}
    Shortcut {sequence:"End";enabled:win.navigationKeys();onActivated:controller.step(2)}
    Shortcut {sequence:"F";enabled:win.navigationKeys();onActivated:controller.flip()}
    Shortcut {sequence:"Ctrl+Z";enabled:win.navigationKeys()&&controller.state.canUndo;onActivated:controller.undo()}
    Shortcut {sequence:"Ctrl+Shift+Z";enabled:win.navigationKeys()&&controller.state.canRedo;onActivated:controller.redo()}
}
