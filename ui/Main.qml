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
    property var nav: ["Accueil", "Bibliothèque", "Analyse", "Ouvertures", "Entraînement", "Outils", "Joueurs"]
    palette.window: bg; palette.windowText: fg; palette.base: panel; palette.text: fg
    palette.button: panel; palette.buttonText: fg; palette.mid: line
    palette.alternateBase: raised; palette.highlight: accent; palette.highlightedText: contrast(accent)
    palette.placeholderText: muted
    color: bg
    font.family: "DejaVu Sans"
    font.pixelSize: 13
    function contrast(c){function linear(v){return v<=.04045?v/12.92:Math.pow((v+.055)/1.055,2.4)}return .2126*linear(c.r)+.7152*linear(c.g)+.0722*linear(c.b)>.179?"#171b21":"#ffffff"}
    property double selectedBaseId:0
    property var advanced:({})
    function applyAdvanced(){var f=Object.assign({},advanced);f.sourceId=selectedBaseId;controller.searchLibrary(searchField.text,resultBox.currentIndex===0?"":resultBox.currentText,favoriteCheck.checked,positionCheck.checked,trashCheck.checked,f)}
    function filterLibrary() {applyAdvanced()}
    Connections {target:controller;function onLibraryOpened(){win.selectedBaseId=0;win.advanced=({});baseBox.currentIndex=0;win.filterLibrary()}}
    function openExport(all) {exportAll=all;exportDialog.open()}

    header: Rectangle {
        height: 116; color: win.bg
        ColumnLayout {
            anchors.fill: parent; anchors.leftMargin: 32; anchors.rightMargin: 32; spacing: 0
            RowLayout {
                Layout.fillWidth: true; Layout.preferredHeight: 62; spacing: 18
                Label {text: "Trait"; font.family: "DejaVu Serif"; font.pixelSize: 30; font.letterSpacing: -1.2}
                Rectangle {width: 2; height: 29; rotation: 24; color: win.accent; Layout.leftMargin: -9; Layout.rightMargin: -9}
                Label {text: "64"; font.family: "DejaVu Sans Mono"; font.pixelSize: 24; color: win.accent}
                Label {text: "L’ATELIER DES ÉCHECS"; font.pixelSize: 9; font.letterSpacing: 2; color: win.muted; Layout.leftMargin: 18; visible: win.width >= 1100}
                Item {Layout.fillWidth: true}
                TButton {text: "Apparence"; flat: true; onClicked: appearanceDialog.open()}
                TButton {text: "Commandes  /  Ctrl K"; onClicked: commands.open()}
                TButton {id: appMenuButton; objectName: "appMenuButton"; text: "Menu  ≡"; flat: true; onClicked: appMenu.open()
                    Menu {id: appMenu; y: appMenuButton.height
                        MenuItem {text: "Préparation par position"; onTriggered: preparationDialog.open()}
                        MenuItem {text: "Mes répertoires"; onTriggered: repertoiresDialog.open()}
                        MenuItem {text: "Imports PGN reprenables"; onTriggered: importsDialog.open()}
                        MenuItem {text: "Moteurs et bases à télécharger"; onTriggered: downloadsDialog.open()}
                        MenuItem {text: "Paramètres"; onTriggered: settingsDialog.open()}
                        MenuItem {text: "Pièces, échiquier et couleurs"; onTriggered: appearanceDialog.open()}
                        MenuItem {text: "Mises à jour"; onTriggered: updatesDialog.open()}
                        MenuSeparator {}
                        MenuItem {text: "Aide & raccourcis"; onTriggered: helpDialog.open()}
                    }
                }
            }
            RowLayout {
                Layout.fillWidth: true; Layout.fillHeight: true; spacing: 0
                Repeater {
                    model: win.nav
                    delegate: Button {
                        id: sectionNav
                        required property string modelData
                        required property int index
                        objectName: "sectionNav" + index
                        Layout.fillWidth: true; Layout.preferredWidth: 1; Layout.fillHeight: true
                        hoverEnabled: true; text: modelData
                        Accessible.name: modelData; Accessible.role: Accessible.PageTab
                        Accessible.selected: win.page === index
                        contentItem: RowLayout {
                            spacing: 9
                            Label {text: "0" + (index + 1); font.family: "DejaVu Sans Mono"; font.pixelSize: 10; color: win.page === index ? win.accent : win.muted}
                            Label {text: modelData; font.pixelSize: win.width < 1100 ? 12 : 13; font.weight: win.page === index ? Font.DemiBold : Font.Normal; color: win.page === index ? win.fg : win.muted; Layout.fillWidth: true; elide: Text.ElideRight}
                        }
                        background: Rectangle {
                            color: sectionNav.hovered || sectionNav.visualFocus ? win.panel : "transparent"
                            Rectangle {anchors.bottom: parent.bottom; width: parent.width; height: win.page === index ? 3 : 1; color: win.page === index ? win.accent : win.line}
                        }
                        onClicked: win.page = index
                    }
                }
            }
        }
    }
    Rectangle {anchors.fill: parent; color: win.bg}
    RowLayout {
        anchors.fill: parent; spacing: 0
        StackLayout {
            id: pages
            Layout.fillWidth: true; Layout.fillHeight: true; currentIndex: win.page
            Layout.leftMargin: 32; Layout.rightMargin: 32; Layout.topMargin: win.height < 820 ? 14 : 24; Layout.bottomMargin: 16

            Home {
                backend: controller
                onNewStudy: controller.newGame()
                onImportStudy: importDialog.open()
                onResumeStudy: win.page = 2
                onShowLibrary: win.page = 1
                onShowTraining: win.page = 4
            }

            // LIBRARY
            ColumnLayout {
                Layout.margins:0;spacing:16
                RowLayout {Layout.fillWidth:true
                    ColumnLayout {spacing:6;Label {text:"Votre bibliothèque";font.family:"DejaVu Serif";font.pixelSize:30;font.weight:Font.Normal} Label {text:(controller.stats.games||0)+" parties · PGN, études et collections";color:win.muted}}
                    Item {Layout.fillWidth:true}
                    TButton {text:"Téléchargements";onClicked:downloadsDialog.open()}
                    TButton {text:"Exporter la bibliothèque";onClicked:win.openExport(true)}
                    TButton {text:"+  Importer";accent:true;enabled:!controller.busy;onClicked:importDialog.open()}
                }
                RowLayout {Layout.fillWidth:true;spacing:10
                    ComboBox {id:baseBox;objectName:"baseSelector";Layout.preferredWidth:260;textRole:"title";valueRole:"id";model:[{id:0,title:"Toutes les bases"}].concat(controller.sources);onActivated:{win.selectedBaseId=currentValue;win.filterLibrary()} onModelChanged:{currentIndex=Math.max(0,indexOfValue(win.selectedBaseId))}}
                    TButton {text:"Imports reprenables";objectName:"showImports";onClicked:importsDialog.open()}
                    TButton {text:"Exporter cette base";enabled:baseBox.currentIndex>0;onClicked:sourceExportDialog.open()}
                    Item {Layout.fillWidth:true}
                }
                RowLayout {Layout.fillWidth:true;spacing:10
                    TextField {id:searchField;objectName:"librarySearch";placeholderText:"Rechercher un joueur, un code ECO, un tournoi…";Layout.fillWidth:true;onTextChanged:searchTimer.restart();onAccepted:{searchTimer.stop();win.filterLibrary()}}
                    ComboBox {id:resultBox;model:["Tous résultats","1-0","0-1","1/2-1/2","*"];onActivated:win.filterLibrary();Layout.preferredWidth:155}
                }
                RowLayout {Layout.fillWidth:true;spacing:14
                    CheckBox {id:favoriteCheck;text:"Favoris";onToggled:win.filterLibrary()}
                    CheckBox {id:positionCheck;text:"Position de l’échiquier";onToggled:win.filterLibrary()}
                    CheckBox {id:trashCheck;text:"Corbeille";onToggled:win.filterLibrary()}
                    Item {Layout.fillWidth:true}
                    TButton {text:"Annuler la recherche";visible:controller.librarySearching;onClicked:controller.cancelLibrarySearch()}
                    Label {text:controller.busy?"Import en cours…":controller.librarySearchStatus;color:win.muted;font.pixelSize:11;Layout.fillWidth:true;Layout.minimumWidth:70;elide:Text.ElideRight;horizontalAlignment:Text.AlignRight}
                }
                Rectangle {Layout.fillWidth:true;height:37;color:win.raised;radius:6
                    RowLayout {anchors.fill:parent;anchors.leftMargin:18;anchors.rightMargin:18
                        Label {text:"JOUEURS";Layout.fillWidth:true;font.pixelSize:10;color:win.muted;font.letterSpacing:1}
                        Label {text:"ÉVÉNEMENT / COLLECTION";Layout.preferredWidth:235;font.pixelSize:10;color:win.muted}
                        Label {text:"DATE";Layout.preferredWidth:96;font.pixelSize:10;color:win.muted}
                        Label {text:"RÉSULTAT";Layout.preferredWidth:67;font.pixelSize:10;color:win.muted}
                        Item {Layout.preferredWidth:118}
                    }
                }
                ListView {
                    id:libraryList;objectName:"libraryResults";Layout.fillWidth:true;Layout.fillHeight:true;clip:true;spacing:5;model:controller.games;enabled:!controller.librarySearching;opacity:enabled?1:.5
                    ScrollBar.vertical:ScrollBar {}
                    delegate:Rectangle {
                        required property var modelData
                        width:libraryList.width;height:72;radius:2;color:win.panel
                        RowLayout {anchors.fill:parent;anchors.margins:12;spacing:12
                            ColumnLayout {Layout.fillWidth:true;spacing:5
                                Label {text:modelData.white+" — "+modelData.black;elide:Text.ElideRight;Layout.fillWidth:true;font.weight:Font.DemiBold}
                                Label {text:modelData.eco?"ECO "+modelData.eco:"Étude personnelle";font.pixelSize:10;color:win.muted}
                            }
                            ColumnLayout {Layout.preferredWidth:235;spacing:5
                                Label {text:modelData.event;elide:Text.ElideRight;Layout.fillWidth:true;font.pixelSize:12}
                                Label {text:modelData.collection||"Sans collection";font.pixelSize:10;color:win.muted;elide:Text.ElideRight;Layout.fillWidth:true}
                            }
                            Label {text:modelData.date;Layout.preferredWidth:96;color:win.muted;font.pixelSize:11}
                            Label {text:modelData.result;Layout.preferredWidth:67;font.weight:Font.DemiBold}
                            TButton {text:modelData.favorite?"★":"☆";implicitWidth:32;onClicked:controller.favorite(modelData.id,!modelData.favorite);ToolTip.visible:hovered;ToolTip.text:"Favori"}
                            TButton {text:"Ouvrir";onClicked:controller.loadGame(modelData.id)}
                            TButton {text:"⋮";implicitWidth:28;onClicked:{win.selectedGame=modelData.id;collectionField.text=modelData.collection||"";gameMenu.open()}}
                        }
                    }
                    Label {anchors.centerIn:parent;visible:controller.games.length===0;text:controller.librarySearching?"Recherche…":controller.librarySearchStatus==="Recherche annulée."?controller.librarySearchStatus:controller.librarySearchStatus==="Recherche impossible."?controller.librarySearchStatus:"Aucune partie ne correspond aux filtres.";color:win.muted}
                }
                RowLayout {Layout.fillWidth:true
                    TButton {text:"Recherche avancée";onClicked:advancedDialog.open()}
                    TButton {text:"Doublons de coups";onClicked:controller.findDuplicates()}
                    Item {Layout.fillWidth:true}
                    TButton {text:"Page précédente";objectName:"previousLibraryPage";enabled:!controller.librarySearching&&controller.libraryPage>1;onClicked:controller.previousLibraryPage()}
                    TButton {text:"Page suivante";objectName:"nextLibraryPage";enabled:!controller.librarySearching&&controller.libraryHasNext;onClicked:controller.nextLibraryPage()}
                }
            }

            // ANALYSIS
            ColumnLayout {
                Layout.margins:0;spacing:win.height<820?8:14
                RowLayout {Layout.fillWidth:true
                    ColumnLayout {spacing:3;Layout.fillWidth:true
                        Label {text:controller.state.training?(controller.state.repertoireTraining>0?"Retrouver le coup":"Retrouver la ligne"):"Sur l’échiquier.";font.family:"DejaVu Serif";font.pixelSize:win.height<820?24:30}
                        Label {visible:win.height>=820||controller.state.training;text:controller.state.training?"Jouez les coups de votre répertoire · "+controller.state.trainingProgress:controller.state.title;color:win.muted;font.pixelSize:12;elide:Text.ElideRight;Layout.fillWidth:true}
                    }
                    Item {Layout.fillWidth:true}
                    TButton {text:"Position suivante";visible:controller.state.repertoireTraining>0;enabled:controller.state.trainingComplete && controller.state.repertoireDue>0;onClicked:controller.startRepertoireTraining(controller.state.repertoireTraining)}
                    TButton {text:controller.state.training?"Quitter l’exercice":"Nouvelle étude";onClicked:controller.state.training?controller.finishTraining():controller.newGame()}
                    TButton {objectName:"saveStudy";text:controller.state.training?"Voir la solution":"Enregistrer";accent:true;onClicked:controller.state.training?controller.revealCard():controller.saveGame()}
                    TButton {text:"⋮";visible:!controller.state.training;onClicked:analysisMenu.open()}
                }
                RowLayout {Layout.fillWidth:true
                    ListView {Layout.fillWidth:true;Layout.preferredHeight:36;orientation:ListView.Horizontal;model:controller.tabs;spacing:5;clip:true
                        delegate:RowLayout {required property var modelData;required property int index;spacing:0
                            TButton {text:(modelData.dirty?"• ":"")+modelData.title;accent:modelData.active;flat:!modelData.active;implicitHeight:32;implicitWidth:Math.min(260,Math.max(180,text.length*8+30));onClicked:controller.switchTab(index)}
                            TButton {text:"×";implicitWidth:27;implicitHeight:32;onClicked:controller.closeTab(index)}
                        }
                    }
                    TButton {text:"↶";enabled:controller.state.canUndo;onClicked:controller.undo();ToolTip.visible:hovered;ToolTip.text:"Annuler"}
                    TButton {text:"↷";enabled:controller.state.canRedo;onClicked:controller.redo();ToolTip.visible:hovered;ToolTip.text:"Rétablir"}
                    TButton {text:"Bilan";enabled:!controller.state.training;onClicked:reviewDialog.open()}
                }
                SplitView {
                    id:analysisSplit;objectName:"analysisSplit"
                    Layout.fillWidth:true;Layout.fillHeight:true;orientation:Qt.Horizontal
                    handle:Rectangle {implicitWidth:12;color:"transparent";Rectangle {anchors.centerIn:parent;width:2;height:36;radius:1;color:win.line}}
                    ColumnLayout {
                        SplitView.preferredWidth:Math.min(700,Math.max(380,analysisSplit.width*.56));SplitView.minimumWidth:360;spacing:win.height<820?5:10
                        RowLayout {Layout.fillWidth:true;Layout.leftMargin:Math.max(0,(parent.width-chessboard.width)/2);Layout.rightMargin:Layout.leftMargin
                            Label {Layout.fillWidth:true;elide:Text.ElideRight;text:controller.state.flipped?(controller.state.tags.White||"Blancs"):(controller.state.tags.Black||"Noirs");font.weight:Font.DemiBold}
                            Item {Layout.fillWidth:true}
                            Label {text:controller.state.clockRunning?(controller.state.flipped?controller.state.whiteClock:controller.state.blackClock):(controller.state.flipped?"BLANCS":"NOIRS");color:win.muted;font.pixelSize:9;font.letterSpacing:1.5}
                        }
                        Item {
                            Layout.fillWidth:true;Layout.fillHeight:true;Layout.minimumHeight:200
                            Rectangle {width:chessboard.width+12;height:width;anchors.centerIn:parent;color:"transparent";border.color:win.line}
                            Board {id:chessboard;objectName:"chessboard";backend:controller;coordinates:controller.preferences.coordinates;width:Math.min(parent.width-12,parent.height-12);height:width;anchors.centerIn:parent}
                        }
                        RowLayout {Layout.fillWidth:true;Layout.leftMargin:Math.max(0,(parent.width-chessboard.width)/2);Layout.rightMargin:Layout.leftMargin
                            Label {Layout.fillWidth:true;elide:Text.ElideRight;text:controller.state.flipped?(controller.state.tags.Black||"Noirs"):(controller.state.tags.White||"Blancs");font.weight:Font.DemiBold}
                            Item {Layout.fillWidth:true}
                            Label {text:controller.state.clockRunning?(controller.state.flipped?controller.state.blackClock:controller.state.whiteClock):(controller.state.flipped?"NOIRS":"BLANCS");color:win.muted;font.pixelSize:9;font.letterSpacing:1.5}
                        }
                        RowLayout {Layout.fillWidth:true;spacing:6;Layout.leftMargin:Math.max(0,(parent.width-Math.max(chessboard.width,430))/2);Layout.rightMargin:Layout.leftMargin
                            TButton {text:"|◀";implicitWidth:38;padding:6;enabled:controller.state.canBack&&!controller.state.training;onClicked:controller.step(-2);Accessible.name:"Début de la partie";ToolTip.visible:hovered;ToolTip.text:"Début"}
                            TButton {text:"◀";implicitWidth:38;padding:6;enabled:controller.state.canBack&&!controller.state.training;onClicked:controller.step(-1);Accessible.name:"Coup précédent";ToolTip.visible:hovered;ToolTip.text:"Coup précédent"}
                            TButton {text:"▶";implicitWidth:38;padding:6;enabled:controller.state.canForward&&!controller.state.training;onClicked:controller.step(1);Accessible.name:"Coup suivant";ToolTip.visible:hovered;ToolTip.text:"Coup suivant"}
                            TButton {text:"▶|";implicitWidth:38;padding:6;enabled:controller.state.canForward&&!controller.state.training;onClicked:controller.step(2);Accessible.name:"Fin de la variante";ToolTip.visible:hovered;ToolTip.text:"Fin de la variante"}
                            TButton {text:"⇅";implicitWidth:38;onClicked:controller.flip();Accessible.name:"Retourner l’échiquier";ToolTip.visible:hovered;ToolTip.text:"Retourner l’échiquier · F"}
                            Item {Layout.fillWidth:true}
                            TButton {id:positionActions;objectName:"positionActions";text:analysisSplit.width>1000?"Position  ⋯":"⋯";implicitWidth:analysisSplit.width>1000?110:38;visible:!controller.state.training;onClicked:positionMenu.open();Accessible.name:"Outils de la position"
                                Menu {id:positionMenu;y:-height
                                    MenuItem {text:"Éditer la position";onTriggered:positionEditor.open()}
                                    MenuItem {text:"Copier la FEN";onTriggered:controller.copyText("fen")}
                                    MenuItem {text:"Charger une FEN";onTriggered:fenDialog.open()}
                                    MenuItem {text:"Exporter le diagramme";onTriggered:diagramDialog.open()}
                                    MenuSeparator {}
                                    MenuItem {text:controller.state.playing?"Arrêter la partie":"Jouer une partie";onTriggered:controller.state.playing?controller.stopPlaying():playDialog.open()}
                                }
                            }
                            TButton {text:controller.state.playing?"Arrêter":"Jouer";visible:!controller.state.training&&analysisSplit.width>1000;onClicked:controller.state.playing?controller.stopPlaying():playDialog.open()}
                        }
                        Label {text:controller.state.clockResult||controller.state.status;color:win.accent;Layout.alignment:Qt.AlignHCenter;font.pixelSize:11;Layout.topMargin:2}
                    }
                    ColumnLayout {
                        objectName:"analysisPanel"
                        SplitView.fillWidth:true;SplitView.minimumWidth:310;spacing:12
                        Rectangle {
                            Layout.fillWidth:true;implicitHeight:controller.state.analysis?(win.height<760?152:204):88;color:win.panel
                            Rectangle {width:3;height:parent.height;color:win.accent}
                            ColumnLayout {anchors.fill:parent;anchors.margins:14;spacing:8
                                RowLayout {Layout.fillWidth:true
                                    Label {text:"01  /  ANALYSE MOTEUR";font.pixelSize:10;font.letterSpacing:1.1;color:win.muted}
                                    Item {Layout.fillWidth:true}
                                    TButton {text:controller.state.analysis?"Arrêter":"Analyser";accent:!controller.state.analysis;enabled:!controller.state.training;implicitHeight:30;onClicked:controller.toggleAnalysis()}
                                }
                                Label {text:controller.engineStatus+(controller.state.opening?" · "+controller.state.opening:"");elide:Text.ElideRight;Layout.fillWidth:true;color:win.muted;font.pixelSize:10}
                                ListView {
                                    Layout.fillWidth:true;Layout.fillHeight:true;visible:controller.state.analysis;clip:true;model:controller.engineLines;spacing:5
                                    delegate:RowLayout {
                                        required property var modelData
                                        required property int index
                                        width:ListView.view.width;spacing:8
                                        Label {text:modelData.eval;Layout.preferredWidth:57;font.weight:Font.DemiBold;color:win.accent}
                                        ColumnLayout {Layout.fillWidth:true;spacing:2
                                            Label {text:modelData.san;elide:Text.ElideRight;Layout.fillWidth:true;font.pixelSize:11}
                                            Label {text:"Profondeur "+modelData.depth+" · "+Math.round(Number(modelData.nps)/1000)+" kN/s";font.pixelSize:9;color:win.muted}
                                        }
                                        TButton {text:"+";implicitWidth:28;implicitHeight:30;onClicked:controller.addEngineLine(index);ToolTip.visible:hovered;ToolTip.text:"Ajouter cette variante"}
                                    }
                                }
                            }
                        }
                        Rectangle {
                            Layout.fillWidth:true;Layout.fillHeight:true;color:win.panel
                            Rectangle {width:parent.width;height:1;color:win.line}
                            ColumnLayout {anchors.fill:parent;anchors.margins:12;spacing:7
                                TabBar {id:notationTabs;Layout.fillWidth:true
                                    TTabButton {text:"02  /  Carnet de coups";font.pixelSize:11}
                                    TTabButton {text:"Explorateur";font.pixelSize:11}
                                }
                                StackLayout {
                                    Layout.fillWidth:true;Layout.fillHeight:true;currentIndex:notationTabs.currentIndex
                                    ListView {id:moveList;model:controller.notation;clip:true;spacing:2;ScrollBar.vertical:ScrollBar {}
                                        delegate:ItemDelegate {
                                            required property var modelData
                                            width:moveList.width;implicitHeight:modelData.comment?52:31
                                            background:Rectangle {color:modelData.active?win.raised:(parent.hovered?win.bg:"transparent");Rectangle {width:2;height:parent.height;color:win.accent;visible:modelData.active}}
                                            contentItem:ColumnLayout {spacing:1
                                                Label {text:(modelData.depth>0?"↳  ":"")+modelData.text;Layout.leftMargin:Math.min(modelData.depth*16,80);font.family:"DejaVu Sans Mono";font.weight:modelData.active?Font.Bold:Font.Normal;color:modelData.depth>0?win.muted:win.fg;font.pixelSize:12}
                                                Label {visible:!!modelData.comment;text:modelData.comment;elide:Text.ElideRight;Layout.fillWidth:true;Layout.leftMargin:Math.min(modelData.depth*16,80);font.pixelSize:10;color:win.muted}
                                            }
                                            onClicked:controller.navigate(modelData.id)
                                        }
                                    }
                                    ListView {id:miniExplorer;model:controller.openings;clip:true;spacing:4
                                        delegate:ItemDelegate {
                                            required property var modelData
                                            width:miniExplorer.width;text:modelData.san+"     "+modelData.count+" partie(s)";onClicked:controller.move(modelData.uci)
                                        }
                                        Label {anchors.centerIn:parent;visible:controller.openings.length===0;text:"Aucune suite dans votre base.";color:win.muted;font.pixelSize:11}
                                    }
                                }
                                RowLayout {Layout.fillWidth:true;visible:!controller.state.training
                                    ComboBox {id:nagBox;model:["Annotation","! Bon coup","? Erreur","!! Excellent","?? Gaffe","!? Intéressant","?! Douteux"];currentIndex:Math.min(controller.state.nag,6);onActivated:controller.setNag(currentIndex);Layout.fillWidth:true;font.pixelSize:11}
                                    TButton {text:"↑";enabled:controller.state.node>0;onClicked:controller.promote();ToolTip.visible:hovered;ToolTip.text:"Promouvoir en ligne principale"}
                                }
                                Label {text:"03  /  NOTES DE POSITION";font.pixelSize:9;font.letterSpacing:1;color:win.muted;Layout.topMargin:6}
                                ScrollView {Layout.fillWidth:true;Layout.preferredHeight:win.height<760?52:85
                                    TextArea {id:note;objectName:"positionNote";text:controller.state.comment;readOnly:controller.state.training;placeholderText:"Une idée, un plan, une question…";wrapMode:TextEdit.Wrap;font.pixelSize:12;selectByMouse:true
                                        onTextChanged:if(activeFocus&&text!==controller.state.comment)controller.setComment(text)
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // OPENINGS
            ColumnLayout {
                Layout.margins:0;spacing:20
                Label {text:"Explorer vos ouvertures";font.family:"DejaVu Serif";font.pixelSize:30;font.weight:Font.Normal}
                Label {text:"Les suites jouées dans votre bibliothèque, depuis la position actuelle de l’échiquier.";color:win.muted;wrapMode:Text.WordWrap;Layout.fillWidth:true}
                RowLayout {TButton {text:"←  Reculer";enabled:controller.state.canBack;onClicked:controller.step(-1)} TButton {text:"Position initiale";onClicked:controller.step(-2)} TButton {text:"Voir l’échiquier";onClicked:win.page=2} Item {Layout.fillWidth:true} TButton {text:"Préparer";onClicked:preparationDialog.open()} TButton {text:"Mes répertoires";objectName:"showRepertoires";onClicked:repertoiresDialog.open()} TButton {text:"Créer une carte depuis ici";accent:true;onClicked:cardDialog.open()}}
                Label {text:controller.state.fen;color:win.muted;font.family:"monospace";font.pixelSize:11;elide:Text.ElideRight;Layout.fillWidth:true}
                Rectangle {Layout.fillWidth:true;height:1;color:win.line}
                RowLayout {Layout.fillWidth:true;Label {text:"COUP";Layout.preferredWidth:95;color:win.muted;font.pixelSize:10} Label {text:"PARTIES";Layout.preferredWidth:90;color:win.muted;font.pixelSize:10} Label {text:"BLANCS / NULLES / NOIRS (RÉSULTATS TERMINÉS)";color:win.muted;font.pixelSize:10;Layout.fillWidth:true}}
                ListView {id:openingList;Layout.fillWidth:true;Layout.fillHeight:true;clip:true;spacing:10;model:controller.openings
                    delegate:Rectangle {
                        required property var modelData
                        width:openingList.width;height:62;radius:2;color:win.panel
                        RowLayout {anchors.fill:parent;anchors.margins:12;spacing:18
                            TButton {text:modelData.san;Layout.preferredWidth:76;onClicked:controller.move(modelData.uci)}
                            Label {text:modelData.count;Layout.preferredWidth:66;font.weight:Font.DemiBold}
                            Row {Layout.fillWidth:true;Layout.preferredHeight:25
                                property real total:Math.max(1,modelData.white+modelData.draw+modelData.black)
                                Rectangle {width:parent.width*modelData.white/parent.total;height:25;color:"#e6ecdf";Text {anchors.centerIn:parent;text:modelData.white;color:"#263329";font.pixelSize:11;visible:parent.width>25}}
                                Rectangle {width:parent.width*modelData.draw/parent.total;height:25;color:"#80937f";Text {anchors.centerIn:parent;text:modelData.draw;color:"#ffffff";font.pixelSize:11;visible:parent.width>25}}
                                Rectangle {width:parent.width*modelData.black/parent.total;height:25;color:"#3b4840";Text {anchors.centerIn:parent;text:modelData.black;color:"#ffffff";font.pixelSize:11;visible:parent.width>25}}
                            }
                            TButton {text:"Jouer  →";onClicked:controller.move(modelData.uci)}
                        }
                    }
                    ColumnLayout {anchors.centerIn:parent;visible:controller.openings.length===0;spacing:15
                        Label {text:"Aucune partie ne continue depuis cette position.";color:win.muted}
                        TButton {text:"Importer un PGN";Layout.alignment:Qt.AlignHCenter;onClicked:importDialog.open()}
                    }
                }
                Label {text:"Chaque partie compte une fois par coup. Les parties inachevées sont comptées mais exclues de la barre de résultats.";wrapMode:Text.WordWrap;Layout.fillWidth:true;font.pixelSize:11;color:win.muted}
            }

            // TRAINING
            ColumnLayout {
                Layout.margins:0;spacing:20
                RowLayout {Layout.fillWidth:true
                    ColumnLayout {spacing:6;Label {text:"Apprendre pour retrouver.";font.family:"DejaVu Serif";font.pixelSize:30;font.weight:Font.Normal} Label {text:"Vos propres lignes, avec des révisions espacées dans le temps.";color:win.muted}}
                    Item {Layout.fillWidth:true}
                    TButton {text:"+  Créer une carte";accent:true;onClicked:cardDialog.open()}
                }
                RowLayout {Layout.fillWidth:true;spacing:16
                    Repeater {model:[{title:"À réviser",key:"due"},{title:"Révisions effectuées",key:"reviews"},{title:"Réussites",key:"successes"}]
                        delegate:Rectangle {required property var modelData;Layout.fillWidth:true;height:102;radius:2;color:win.panel;border.color:win.line
                            ColumnLayout {anchors.fill:parent;anchors.margins:20;Label {text:modelData.title;color:win.muted;font.pixelSize:11} Label {text:controller.stats[modelData.key]||0;font.family:"DejaVu Serif";font.pixelSize:30;font.weight:Font.Normal}}
                        }
                    }
                }
                RowLayout {Layout.fillWidth:true
                    TButton {text:"Réviser un répertoire";onClicked:repertoiresDialog.open()}
                    TButton {text:"Réviser la prochaine carte";accent:true;enabled:(controller.stats.due||0)>0;onClicked:controller.nextDueCard()}
                    Label {text:"Aujourd’hui : "+(controller.stats.todayReviews||0)+" / "+controller.preferences.dailyGoal+" · Cette semaine : "+(controller.stats.weekReviews||0);color:win.muted;Layout.fillWidth:true}
                    Label {text:"Objectif"} SpinBox {from:1;to:1000;value:controller.preferences.dailyGoal;onValueModified:controller.setPreference("dailyGoal",value)}
                }
                ProgressBar {Layout.fillWidth:true;from:0;to:controller.preferences.dailyGoal;value:controller.stats.todayReviews||0}
                Label {text:"MES CARTES";font.pixelSize:10;color:win.muted;font.letterSpacing:1.5;Layout.topMargin:8}
                ListView {id:trainingList;Layout.fillWidth:true;Layout.fillHeight:true;clip:true;spacing:8;model:controller.cards;ScrollBar.vertical:ScrollBar {}
                    delegate:Rectangle {required property var modelData;width:trainingList.width;height:80;radius:2;color:win.panel
                        RowLayout {anchors.fill:parent;anchors.margins:16;spacing:16
                            Label {text:"◎";font.pixelSize:28;color:win.accent}
                            ColumnLayout {Layout.fillWidth:true;spacing:6
                                Label {text:modelData.title;font.weight:Font.DemiBold;elide:Text.ElideRight;Layout.fillWidth:true}
                                Label {text:"Révision : "+modelData.due+" · "+modelData.reviews+" passage(s) · intervalle "+modelData.interval+" j";color:win.muted;font.pixelSize:11}
                            }
                            TButton {text:"Réviser";accent:true;onClicked:controller.startCard(modelData.id)}
                            TButton {text:"×";onClicked:{deleteCardDialog.cardId=modelData.id;deleteCardDialog.open()}}
                        }
                    }
                    ColumnLayout {anchors.centerIn:parent;visible:controller.cards.length===0;spacing:16
                        Label {text:"Vos variantes deviennent votre entraînement.";font.pixelSize:19;Layout.alignment:Qt.AlignHCenter}
                        Label {text:"Dans une étude, placez-vous avant les coups à apprendre,\npuis cliquez sur « Créer une carte ».";horizontalAlignment:Text.AlignHCenter;color:win.muted;Layout.alignment:Qt.AlignHCenter}
                        TButton {text:"Ouvrir l’atelier";Layout.alignment:Qt.AlignHCenter;onClicked:win.page=2}
                    }
                }
            }

            // TOOLS
            ScrollView {contentWidth:availableWidth;clip:true
                ColumnLayout {width:pages.width;spacing:20
                    Label {text:"La boîte à outils";font.family:"DejaVu Serif";font.pixelSize:30;font.weight:Font.Normal;Layout.topMargin:0}
                    Label {text:"Importer, sauvegarder, vérifier. Gardez la main sur vos données.";color:win.muted;wrapMode:Text.WordWrap;Layout.fillWidth:true}
                    GridLayout {columns:2;columnSpacing:18;rowSpacing:18;Layout.fillWidth:true
                        Rectangle {Layout.fillWidth:true;implicitHeight:192;radius:2;color:win.panel;border.color:win.line
                            ColumnLayout {anchors.fill:parent;anchors.margins:22;spacing:12
                                Label {text:"PGN & positions";font.pixelSize:18;font.weight:Font.DemiBold}
                                Label {text:"Ouvrir un fichier ou coller une partie et ses variantes.";wrapMode:Text.WordWrap;Layout.fillWidth:true;color:win.muted}
                                Item {Layout.fillHeight:true}
                                RowLayout {TButton {text:"Coller un PGN";onClicked:pgnDialog.open()} TButton {text:"Charger une FEN";onClicked:fenDialog.open()}}
                            }
                        }
                        Rectangle {Layout.fillWidth:true;implicitHeight:192;radius:2;color:win.panel;border.color:win.line
                            ColumnLayout {anchors.fill:parent;anchors.margins:22;spacing:12
                                Label {text:"Sauvegarde complète";font.pixelSize:18;font.weight:Font.DemiBold}
                                Label {text:"Parties, favoris, collections et cartes dans un fichier SQLite.";wrapMode:Text.WordWrap;Layout.fillWidth:true;color:win.muted}
                                Item {Layout.fillHeight:true}
                                RowLayout {TButton {text:"Sauvegarder";onClicked:backupDialog.open()} TButton {text:"Ouvrir une base";onClicked:openDatabaseDialog.open()}}
                            }
                        }
                        Rectangle {Layout.fillWidth:true;implicitHeight:192;radius:2;color:win.panel;border.color:win.line
                            ColumnLayout {anchors.fill:parent;anchors.margins:22;spacing:12
                                Label {text:"Intégrité & diagnostic";font.pixelSize:18;font.weight:Font.DemiBold}
                                Label {text:"Vérifiez la bibliothèque et comptez les positions légales depuis l’échiquier.";wrapMode:Text.WordWrap;Layout.fillWidth:true;color:win.muted}
                                Item {Layout.fillHeight:true}
                                RowLayout {TButton {text:"Vérifier la base";onClicked:controller.verifyDatabase()} TButton {text:"Réindexer";enabled:!controller.busy;onClicked:controller.rebuildIndexes()}}
                            }
                        }
                        Rectangle {Layout.fillWidth:true;implicitHeight:192;radius:2;color:win.panel;border.color:win.line
                            ColumnLayout {anchors.fill:parent;anchors.margins:22;spacing:12
                                Label {text:"Fichiers & exemples";font.pixelSize:18;font.weight:Font.DemiBold}
                                Label {text:"Accédez aux fichiers locaux ou chargez les études de découverte.";wrapMode:Text.WordWrap;Layout.fillWidth:true;color:win.muted}
                                Item {Layout.fillHeight:true}
                                RowLayout {TButton {text:"Dossier local";onClicked:controller.openDataFolder()} TButton {text:"Exemples";onClicked:controller.loadExamples()}}
                            }
                        }
                    }
                    Rectangle {Layout.fillWidth:true;implicitHeight:120;radius:2;color:win.raised
                        ColumnLayout {anchors.fill:parent;anchors.margins:22;spacing:10
                            Label {text:"Trait64 est un logiciel libre.";font.pixelSize:17;font.weight:Font.DemiBold}
                            Label {text:"C++23 · Qt6 · chess-library · moteurs UCI externes. Code sous AGPL-3.0-or-later.\nCette version alpha propose les outils locaux essentiels ; le catalogue complet reste en développement.";color:win.muted;wrapMode:Text.WordWrap;Layout.fillWidth:true;font.pixelSize:12}
                        }
                    }
                    Item {height:20}
                }
            }
            Players {backend:controller;Layout.margins:0;onPreparePlayer:function(name){preparationDialog.usePlayer(name)}}
        }
    }
    footer:Rectangle {
        height:32;color:win.bg
        Rectangle {width:parent.width;height:1;color:win.line}
        RowLayout {anchors.fill:parent;anchors.leftMargin:22;anchors.rightMargin:22
            Label {text:controller.busy?"●  Traitement en cours · "+controller.taskProgress+" parties lues":controller.state.dirty?"●  Modifications · récupération de session automatique":"●  Prêt";font.pixelSize:10;color:win.muted}
            Item {Layout.fillWidth:true}
            TButton {visible:controller.busy;text:controller.resumableImport?"Mettre en pause":"Interrompre";implicitHeight:24;onClicked:controller.cancelTask()}
            Label {text:controller.state.training?"Entraînement local":"Ctrl S  Enregistrer   ·   F  Retourner   ·   ← →  Parcourir";font.pixelSize:10;color:win.muted;visible:win.width>=1100}
            TButton {text:"Trait64 / "+controller.updater.state.current;flat:true;implicitHeight:26;font.pixelSize:10;onClicked:updatesDialog.open();ToolTip.visible:hovered;ToolTip.text:"Versions et mises à jour"}
        }
    }

    Timer {id:searchTimer;interval:220;onTriggered:win.filterLibrary()}
    Connections {target:controller
        function onMessage(text,error){toastLabel.text=text;toast.error=error;toast.open();toastTimer.restart()}
        function onPromotionNeeded(prefix){win.promotionPrefix=prefix;promotionDialog.open()}
        function onShowAnalysis(){win.page=2}
    }
    Popup {id:toast;property bool error:false;x:(win.width-width)/2;y:win.height-height-64;width:Math.min(760,win.width-280);padding:18;closePolicy:Popup.CloseOnEscape|Popup.CloseOnPressOutside
        background:Rectangle {color:toast.error?"#633c37":win.raised;border.color:toast.error?"#a86559":win.line;radius:2}
        contentItem:Label {id:toastLabel;wrapMode:Text.WordWrap;color:toast.error?"#ffffff":win.fg}
    }
    Timer {id:toastTimer;interval:6500;onTriggered:toast.close()}
    Preparation {id:preparationDialog;objectName:"preparationDialog";backend:controller}
    Repertoires {id:repertoiresDialog;objectName:"repertoiresDialog";backend:controller}
    Imports {id:importsDialog;objectName:"importsDialog";backend:controller}
    Downloads {id:downloadsDialog;objectName:"downloadsDialog";backend:controller;onConfigureEngines:settingsDialog.open()}
    Native.FileDialog {id:sourceExportDialog;title:"Exporter cette base en PGN";fileMode:Native.FileDialog.SaveFile;defaultSuffix:"pgn";nameFilters:["PGN (*.pgn)"];onAccepted:controller.exportSource(selectedFile.toString(),baseBox.currentValue)}
    Native.FileDialog {id:importDialog;title:"Importer des parties";nameFilters:["Parties et positions (*.pgn *.t64 *.epd *.fen)","Tous les fichiers (*)"];onAccepted:controller.importFile(selectedFile.toString())}
    Native.FileDialog {id:exportDialog;title:"Exporter en PGN ou archive T64 expérimentale";fileMode:Native.FileDialog.SaveFile;defaultSuffix:"pgn";nameFilters:["PGN (*.pgn)","Position EPD (*.epd)","Archive T64 expérimentale (*.t64)"];onAccepted:controller.exportFile(selectedFile.toString(),win.exportAll)}
    Native.FileDialog {id:backupDialog;title:"Créer une sauvegarde complète";fileMode:Native.FileDialog.SaveFile;defaultSuffix:"sqlite";nameFilters:["Bibliothèque Trait64 (*.sqlite)"];onAccepted:controller.backup(selectedFile.toString())}
    Native.FileDialog {id:openDatabaseDialog;title:"Ouvrir une bibliothèque Trait64";nameFilters:["Bibliothèque Trait64 (*.sqlite)"];onAccepted:controller.openBackup(selectedFile.toString())}
    Native.FileDialog {id:engineFileDialog;title:"Choisir l’exécutable Stockfish ou UCI";onAccepted:{controller.setPreference("enginePath",selectedFile.toString());enginePathField.text=controller.preferences.enginePath}}

    Dialog {id:fenDialog;title:"Charger une position FEN";anchors.centerIn:parent;width:650;modal:true;standardButtons:Dialog.Ok|Dialog.Cancel
        ColumnLayout {anchors.fill:parent;spacing:15
            Label {text:"Collez une FEN complète. L’étude en cours sera conservée dans votre bibliothèque.";wrapMode:Text.WordWrap;Layout.fillWidth:true;color:win.muted}
            TextArea {id:fenField;Layout.fillWidth:true;Layout.preferredHeight:95;wrapMode:TextEdit.Wrap;selectByMouse:true;placeholderText:"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"}
        }
        onAccepted:controller.setFen(fenField.text)
    }
    Dialog {id:pgnDialog;title:"Importer un texte PGN";anchors.centerIn:parent;width:700;height:460;modal:true;standardButtons:Dialog.Ok|Dialog.Cancel
        ScrollView {anchors.fill:parent;TextArea {id:pgnField;placeholderText:"[Event \"Mon étude\"]\n\n1. e4 e5 2. Nf3 *";wrapMode:TextEdit.Wrap;selectByMouse:true}}
        onAccepted:controller.importText(pgnField.text)
    }
    Dialog {id:promotionDialog;title:"Choisir la promotion";anchors.centerIn:parent;modal:true
        RowLayout {spacing:12;Repeater {model:[{label:"♛ Dame",piece:"q"},{label:"♜ Tour",piece:"r"},{label:"♝ Fou",piece:"b"},{label:"♞ Cavalier",piece:"n"}]
            delegate:TButton {required property var modelData;text:modelData.label;onClicked:{controller.move(win.promotionPrefix+modelData.piece);promotionDialog.close()}}
        }}
    }
    Dialog {id:cardDialog;title:"Créer une carte d’entraînement";anchors.centerIn:parent;width:520;modal:true;standardButtons:Dialog.Ok|Dialog.Cancel
        ColumnLayout {anchors.fill:parent;spacing:14
            Label {text:"La suite principale depuis la position actuelle sera à retrouver (40 demi-coups maximum). Placez-vous avant le premier coup à apprendre.";color:win.muted;wrapMode:Text.WordWrap;Layout.fillWidth:true}
            TextField {id:cardTitle;placeholderText:"Ex. Italienne — mon plan avec les Blancs";Layout.fillWidth:true}
        }
        onAccepted:controller.createCard(cardTitle.text)
    }
    Dialog {id:deleteCardDialog;property int cardId:0;title:"Supprimer cette carte ?";anchors.centerIn:parent;modal:true;standardButtons:Dialog.Yes|Dialog.No
        Label {text:"L’activité déjà enregistrée restera dans votre progression."}
        onAccepted:controller.deleteCard(cardId)
    }
    Dialog {id:playDialog;title:"Partie locale";anchors.centerIn:parent;width:470;modal:true
        ColumnLayout {anchors.fill:parent;spacing:16
            ComboBox {id:opponent;model:["Deux joueurs sur cet ordinateur","Contre le moteur UCI"];currentIndex:1;Layout.fillWidth:true}
            RowLayout {Label {text:"Minutes"} SpinBox {id:clockMinutes;from:1;to:180;value:10;editable:true} Label {text:"Incrément (s)"} SpinBox {id:clockIncrement;from:0;to:120;value:5;editable:true}}
            ComboBox {id:playSide;model:["Je joue les Blancs","Je joue les Noirs"];Layout.fillWidth:true}
            TButton {text:"Démarrer avec pendule";accent:true;Layout.fillWidth:true;onClicked:{controller.startLocalGame(clockMinutes.value,clockIncrement.value,opponent.currentIndex,playSide.currentIndex===0);playDialog.close()}}
            TButton {text:"Moteur sans pendule";visible:opponent.currentIndex===1;Layout.fillWidth:true;onClicked:{controller.playComputer(playSide.currentIndex===0);playDialog.close()}}
            Label {text:"La partie démarre depuis la position de l’échiquier. Parcourir les coups arrête la pendule.";wrapMode:Text.WordWrap;Layout.fillWidth:true;color:win.muted;font.pixelSize:11}
        }
    }
    Dialog {id:tagsDialog;title:"Informations sur la partie";anchors.centerIn:parent;width:540;modal:true;standardButtons:Dialog.Close
        GridLayout {anchors.fill:parent;columns:2;columnSpacing:16;rowSpacing:10
            Repeater {model:["White","Black","Event","Site","Date","Round","Result","ECO","WhiteElo","BlackElo"]
                delegate:ColumnLayout {required property string modelData;Layout.fillWidth:true;spacing:4
                    Label {text:({White:"Blancs",Black:"Noirs",Event:"Événement",Site:"Lieu",Date:"Date (AAAA.MM.JJ)",Round:"Ronde",Result:"Résultat : 1-0 / 0-1 / 1/2-1/2 / *",ECO:"Code ECO",WhiteElo:"Elo Blancs",BlackElo:"Elo Noirs"})[modelData];font.pixelSize:10;color:win.muted;wrapMode:Text.WordWrap;Layout.fillWidth:true}
                    TextField {text:controller.state.tags[modelData]||"";Layout.fillWidth:true;onEditingFinished:{var t={};t[modelData]=text;controller.setTags(t)}}
                }
            }
        }
    }
    Menu {id:analysisMenu;x:win.width-width-32;y:172
        MenuItem {text:"Comparer plusieurs moteurs";onTriggered:compareDialog.open()}
        MenuItem {text:"Éditer la position";onTriggered:positionEditor.open()}
        MenuItem {text:"Copier la position FEN";onTriggered:controller.copyText("fen")}
        MenuItem {text:controller.state.playing?"Arrêter la partie":"Jouer une partie";onTriggered:controller.state.playing?controller.stopPlaying():playDialog.open()}
        MenuItem {text:"Nouvelle partie Chess960";onTriggered:chess960Dialog.open()}
        MenuItem {text:"Dupliquer l’étude";onTriggered:controller.duplicateStudy()}
        MenuItem {text:"Identifier et renseigner l’ouverture";onTriggered:controller.identifyOpening()}
        MenuItem {text:"Exporter le diagramme en PNG";onTriggered:diagramDialog.open()}
        MenuItem {text:"Exporter l’étude en PDF";onTriggered:pdfDialog.open()}
        MenuItem {text:"Effacer les flèches et cercles";onTriggered:controller.clearAnnotations()}
        MenuItem {text:"Abandonner la partie";visible:controller.state.playing;onTriggered:controller.resignGame()}
        MenuItem {text:"Réclamer la nulle";visible:controller.state.playing;onTriggered:controller.claimDraw()}
        MenuItem {text:"Informations de la partie";onTriggered:tagsDialog.open()}
        MenuItem {text:"Exporter cette étude";onTriggered:win.openExport(false)}
        MenuItem {text:"Copier le PGN";onTriggered:controller.copyText("pgn")}
        MenuItem {text:"Créer une carte depuis ici";onTriggered:cardDialog.open()}
        MenuSeparator {}
        MenuItem {text:"Promouvoir cette variante";enabled:controller.state.node>0;onTriggered:controller.promote()}
        MenuItem {text:"Supprimer ce coup et sa suite…";enabled:controller.state.node>0;onTriggered:deleteVariationDialog.open()}
    }
    Dialog {id:deleteVariationDialog;title:"Supprimer cette variante ?";anchors.centerIn:parent;modal:true;standardButtons:Dialog.Yes|Dialog.No
        Label {text:"Le coup sélectionné et ses continuations seront retirés."}
        onAccepted:controller.deleteVariation()
    }
    Menu {id:gameMenu;x:win.width-width-40;y:220
        MenuItem {text:"Fusionner dans l’étude courante";onTriggered:controller.mergeGame(win.selectedGame)}
        MenuItem {text:"Modifier la collection";onTriggered:collectionDialog.open()}
        MenuItem {text:trashCheck.checked?"Restaurer la partie":"Mettre à la corbeille";onTriggered:controller.trash(win.selectedGame,!trashCheck.checked)}
    }
    Dialog {id:collectionDialog;title:"Collection de la partie";anchors.centerIn:parent;modal:true;width:440;standardButtons:Dialog.Ok|Dialog.Cancel
        TextField {id:collectionField;anchors.fill:parent;placeholderText:"Ex. Préparation tournoi, Répertoire Blancs…"}
        onAccepted:controller.setCollection(win.selectedGame,collectionField.text)
    }
    PositionEditor {id:positionEditor;objectName:"positionEditor";backend:controller}
    Review {id:reviewDialog;objectName:"reviewDialog";backend:controller}
    CompareEngines {id:compareDialog;objectName:"compareDialog";backend:controller}
    Native.FileDialog {id:diagramDialog;title:"Exporter le diagramme";fileMode:Native.FileDialog.SaveFile;defaultSuffix:"png";nameFilters:["Image PNG (*.png)"];onAccepted:controller.exportDiagram(selectedFile.toString())}
    Native.FileDialog {id:pdfDialog;title:"Exporter l’étude en PDF";fileMode:Native.FileDialog.SaveFile;defaultSuffix:"pdf";nameFilters:["PDF (*.pdf)"];onAccepted:controller.exportStudyPdf(selectedFile.toString())}
    Dialog {id:chess960Dialog;title:"Nouvelle étude Chess960";anchors.centerIn:parent;modal:true;standardButtons:Dialog.Ok|Dialog.Cancel
        ColumnLayout {Label {text:"Numéro de la position (518 = position classique)"} SpinBox {id:frcNumber;from:0;to:959;value:518;editable:true}}
        onAccepted:controller.newChess960(frcNumber.value)
    }
    Appearance {id:appearanceDialog;objectName:"appearanceDialog";backend:controller}
    Dialog {id:settingsDialog;objectName:"settingsDialog";title:"Paramètres";anchors.centerIn:parent;width:700;height:Math.min(690,win.height-32);modal:true;standardButtons:Dialog.Close
        ScrollView {anchors.fill:parent;contentWidth:availableWidth;clip:true
        ColumnLayout {width:parent.width;spacing:18
            Label {text:"MOTEUR UCI LOCAL";font.pixelSize:10;font.letterSpacing:1.5;color:win.muted}
            RowLayout {Layout.fillWidth:true;TextField {id:enginePathField;Layout.fillWidth:true;text:controller.preferences.enginePath;placeholderText:"/usr/games/stockfish";onEditingFinished:controller.setPreference("enginePath",text)} TButton {text:"Parcourir";onClicked:engineFileDialog.open()}}
            CheckBox {text:"Sauvegardes automatiques (14 instantanés)";checked:controller.preferences.autoBackup;onToggled:controller.setPreference("autoBackup",checked)}
            TButton {text:"Créer une sauvegarde automatique maintenant";onClicked:controller.automaticBackup()}
            GridLayout {columns:4;columnSpacing:14;rowSpacing:10;Layout.fillWidth:true
                Label {text:"Threads";color:win.muted} SpinBox {from:1;to:32;value:controller.preferences.threads;onValueModified:controller.setPreference("threads",value)}
                Label {text:"Mémoire (Mio)";color:win.muted} SpinBox {from:16;to:4096;stepSize:16;value:controller.preferences.hash;onValueModified:controller.setPreference("hash",value)}
                Label {text:"Variantes";color:win.muted} SpinBox {from:1;to:5;value:controller.preferences.multiPV;onValueModified:controller.setPreference("multiPV",value)}
                Label {text:"Niveau de jeu";color:win.muted} SpinBox {from:0;to:20;value:controller.preferences.skill;onValueModified:controller.setPreference("skill",value)}
                Label {text:"Réflexion (ms)";color:win.muted} SpinBox {from:100;to:10000;stepSize:100;value:controller.preferences.moveTime;onValueModified:controller.setPreference("moveTime",value)}
            }
            Label {text:"Le niveau concerne le jeu contre Stockfish. L’analyse utilise sa pleine force.\nLes réglages sont appliqués au démarrage du moteur.";color:win.muted;font.pixelSize:11;wrapMode:Text.WordWrap;Layout.fillWidth:true}
            RowLayout {Label {text:"Profondeur d’analyse";color:win.muted} SpinBox {from:1;to:99;value:controller.preferences.analysisDepth;onValueModified:controller.setPreference("analysisDepth",value)}}
            TextField {Layout.fillWidth:true;text:controller.preferences.syzygyPath;placeholderText:"Dossier des tablebases Syzygy locales (optionnel)";onEditingFinished:controller.setPreference("syzygyPath",text);selectByMouse:true}
            RowLayout {Layout.fillWidth:true
                TextField {id:profileName;Layout.fillWidth:true;placeholderText:"Nom du profil moteur"}
                TButton {text:"Mémoriser";onClicked:controller.saveEngineProfile(profileName.text)}
            }
            RowLayout {Layout.fillWidth:true
                ComboBox {id:profileBox;Layout.fillWidth:true;model:controller.preferences.engineProfiles;textRole:"name"}
                TButton {text:"Charger";enabled:profileBox.currentIndex>=0;onClicked:controller.loadEngineProfile(profileBox.currentIndex)}
                TButton {text:"×";enabled:profileBox.currentIndex>=0;onClicked:controller.removeEngineProfile(profileBox.currentIndex)}
            }
            RowLayout {TButton {text:"Démarrer / appliquer";accent:true;onClicked:controller.startEngine()} Label {text:controller.engineStatus;elide:Text.ElideRight;Layout.fillWidth:true;font.pixelSize:11;color:win.muted}}
            Rectangle {Layout.fillWidth:true;height:1;color:win.line}
            RowLayout {CheckBox {text:"Thème clair";checked:controller.preferences.lightTheme;onToggled:controller.setPreference("lightTheme",checked)} CheckBox {text:"Coordonnées de l’échiquier";checked:controller.preferences.coordinates;onToggled:controller.setPreference("coordinates",checked)}}
            TButton {text:"Télécharger d’autres moteurs";onClicked:{settingsDialog.close();downloadsDialog.open()}}
            EngineOptions {backend:controller}
            Label {text:"Vos données : "+controller.preferences.dataPath;wrapMode:Text.WordWrap;Layout.fillWidth:true;color:win.muted;font.pixelSize:10}
        }
    }
    }
    Dialog {id:helpDialog;title:"Bienvenue dans Trait64";anchors.centerIn:parent;width:620;modal:true;standardButtons:Dialog.Close
        ColumnLayout {anchors.fill:parent;spacing:15
            Label {text:"Un atelier personnel pour vos échecs.";font.pixelSize:21;font.weight:Font.DemiBold}
            Label {text:"• Cliquez une pièce puis sa case, ou glissez-la.\n• Revenez dans une partie et jouez un autre coup pour créer une variante.\n• Clic droit glissé : flèche ; clic droit sur une case : cercle.\n• Enregistrez une étude pour l’ajouter à votre bibliothèque.\n• Importez un PGN pour alimenter l’explorateur d’ouvertures.\n• Créez des cartes à partir des lignes que vous souhaitez retenir.";wrapMode:Text.WordWrap;Layout.fillWidth:true;lineHeight:1.5}
            Label {text:"Ctrl N : nouvelle étude    ·    Ctrl O : importer\nCtrl S : enregistrer    ·    Ctrl K : commandes\n← / → : précédent / suivant    ·    F : retourner\nDébut / Fin : début / fin de la ligne";color:win.muted;lineHeight:1.6}
            Label {text:"Version de développement 0.3.0. Les sauvegardes PGN et SQLite sont recommandées pour vos études importantes. Le format .t64 est expérimental.";wrapMode:Text.WordWrap;Layout.fillWidth:true;font.pixelSize:11;color:win.muted}
        }
    }
    Dialog {id:advancedDialog;objectName:"advancedDialog";title:"Recherche avancée";anchors.centerIn:parent;width:Math.min(690,win.width-32);height:Math.min(760,win.height-32);modal:true;standardButtons:Dialog.Ok|Dialog.Cancel
        property var draft:({})
        onOpened:{draft=Object.assign({},win.advanced);minElo.value=draft.minElo||0;maxElo.value=draft.maxElo||0;maxPlies.value=draft.maxPlies||0;sortBy.currentIndex=Math.max(0,["updated","date","players","elo","length"].indexOf(draft.sort||"updated"));colourFilter.currentIndex=Math.max(0,["","white","black"].indexOf(draft.colour||""))}
        ScrollView {anchors.fill:parent;contentWidth:availableWidth;clip:true
        GridLayout {width:parent.width;columns:2;columnSpacing:12;rowSpacing:10
            Repeater {model:[{key:"player",label:"Joueur (selon la couleur ci-dessous)"},{key:"openingName",label:"Ouverture : Française, Najdorf… → ECO"},{key:"white",label:"Blancs"},{key:"black",label:"Noirs"},{key:"event",label:"Tournoi / événement"},{key:"eco",label:"Code ECO ou fragment"},{key:"ecoFrom",label:"ECO de (ex. B20)"},{key:"ecoTo",label:"ECO à (ex. B99)"},{key:"collection",label:"Collection personnelle"},{key:"material",label:"Matériel exact (ex. KRkrp)"},{key:"dateFrom",label:"À partir de (AAAA.MM.JJ)"},{key:"dateTo",label:"Jusqu’au (AAAA.MM.JJ)"}]
                delegate:ColumnLayout {required property var modelData;Layout.fillWidth:true
                    Label {text:modelData.label;font.pixelSize:11}
                    TextField {Layout.fillWidth:true;text:advancedDialog.draft[modelData.key]||"";onTextEdited:advancedDialog.draft[modelData.key]=text}
                }
            }
            Label {text:"Couleur du joueur"} ComboBox {id:colourFilter;model:["Les deux couleurs","Blancs","Noirs"]}
            Label {text:"Elo minimal des deux joueurs"} SpinBox {id:minElo;from:0;to:3500;stepSize:100;editable:true}
            Label {text:"Elo maximal (0 = sans limite)"} SpinBox {id:maxElo;from:0;to:3500;stepSize:100;editable:true}
            Label {text:"Demi-coups maximum (0 = tous)"} SpinBox {id:maxPlies;from:0;to:2000;editable:true}
            CheckBox {text:"Correspondance exacte des champs texte";checked:advancedDialog.draft.exactNames||false;Layout.columnSpan:2;onToggled:advancedDialog.draft.exactNames=checked}
            Label {text:"Trier par"} ComboBox {id:sortBy;model:["Modification","Date","Joueurs","Elo","Longueur"]}
            Label {text:"Le nom d’ouverture sélectionne les codes ECO du catalogue Lichess, pas une variante exacte. Pour celle-ci, utilisez la position de l’échiquier. Les filtres Elo excluent les classements inconnus. Le matériel est recherché dans toutes les variantes.";wrapMode:Text.WordWrap;Layout.fillWidth:true;Layout.columnSpan:2;font.pixelSize:10;color:win.muted}
            TButton {text:"Effacer les filtres";onClicked:{win.advanced={};win.applyAdvanced();advancedDialog.close()}}
        }}
        onAccepted:{draft.minElo=minElo.value;draft.maxElo=maxElo.value;draft.maxPlies=maxPlies.value;draft.colour=["","white","black"][colourFilter.currentIndex];draft.sort=["updated","date","players","elo","length"][sortBy.currentIndex];win.advanced=Object.assign({},draft);win.applyAdvanced()}
    }
    Dialog {id:commands;title:"Commandes rapides";anchors.centerIn:parent;width:510;modal:true
        ColumnLayout {anchors.fill:parent;spacing:8
            Repeater {model:["Nouvelle étude","Importer des parties","Enregistrer l’étude","Charger une FEN","Créer une carte","Paramètres","Aide"]
                delegate:TButton {required property int index;required property string modelData;text:modelData;Layout.fillWidth:true;onClicked:{commands.close();switch(index){case 0:controller.newGame();break;case 1:importDialog.open();break;case 2:controller.saveGame();break;case 3:fenDialog.open();break;case 4:cardDialog.open();break;case 5:settingsDialog.open();break;case 6:helpDialog.open();break}}}
            }
        }
    }
    Shortcut {sequence:"Ctrl+N";onActivated:controller.newGame()}
    Shortcut {sequence:"Ctrl+O";onActivated:importDialog.open()}
    Shortcut {sequence:"Ctrl+S";onActivated:controller.saveGame()}
    Shortcut {sequence:"Ctrl+K";onActivated:commands.open()}
    function navigationKeys(){var f=win.activeFocusItem;return win.page===2&&!(f && (f.cursorPosition!==undefined || f.editable===true))&&!downloadsDialog.visible&&!appearanceDialog.visible&&!positionEditor.visible&&!settingsDialog.visible&&!fenDialog.visible&&!tagsDialog.visible&&!cardDialog.visible&&!commands.visible&&!playDialog.visible&&!reviewDialog.visible&&!compareDialog.visible&&!updatesDialog.visible&&!appMenu.visible&&!analysisMenu.visible&&!positionMenu.visible}
    Shortcut {sequence:"Left";enabled:win.navigationKeys();onActivated:controller.step(-1)}
    Shortcut {sequence:"Right";enabled:win.navigationKeys();onActivated:controller.step(1)}
    Shortcut {sequence:"Home";enabled:win.navigationKeys();onActivated:controller.step(-2)}
    Shortcut {sequence:"End";enabled:win.navigationKeys();onActivated:controller.step(2)}
    Shortcut {sequence:"F";enabled:win.navigationKeys();onActivated:controller.flip()}
    Shortcut {sequence:"Ctrl+Z";enabled:win.navigationKeys()&&controller.state.canUndo;onActivated:controller.undo()}
    Shortcut {sequence:"Ctrl+Shift+Z";enabled:win.navigationKeys()&&controller.state.canRedo;onActivated:controller.redo()}
}
