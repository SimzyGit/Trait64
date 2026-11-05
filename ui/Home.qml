// SPDX-License-Identifier: AGPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
ScrollView {
    id:root
    required property var backend
    property var p:backend.preferences
    signal newStudy()
    signal importStudy()
    signal resumeStudy()
    signal showLibrary()
    signal showTraining()
    signal showRepertoires()
    signal showPreparation()
    signal showEngines()
    contentWidth:availableWidth;clip:true;ScrollBar.horizontal.policy:ScrollBar.AlwaysOff
    ColumnLayout {
        width:root.availableWidth;spacing:16
        // Le logiciel ne se nommait nulle part dans sa propre fenêtre, sinon en petit
        // dans la barre d'état. L'accueil est l'endroit où le dire.
        RowLayout {
            Layout.fillWidth:true;spacing:12
            Image {
                source:"qrc:/assets/app/trait64-128.png"
                Layout.preferredWidth:Math.round(38*root.p.uiScale)
                Layout.preferredHeight:Layout.preferredWidth
                sourceSize.width:128;sourceSize.height:128
                fillMode:Image.PreserveAspectFit
                smooth:true
            }
            ColumnLayout {
                Layout.fillWidth:true;spacing:0
                Label {
                    text:"Trait64"
                    font.pixelSize:Math.round(22*root.p.fontScale*root.p.uiScale)
                    font.weight:Font.DemiBold
                }
                Label {
                    Layout.fillWidth:true;elide:Text.ElideRight
                    color:root.p.muted
                    font.pixelSize:Math.round(12*root.p.fontScale*root.p.uiScale)
                    text:qsTr("Vos parties, votre analyse, votre progression — sur votre machine.")
                }
            }
        }
        Flow {Layout.fillWidth:true;spacing:8
            TButton {text:qsTr("Nouvelle étude");accent:true;onClicked:root.newStudy()}
            TButton {text:qsTr("Importer un PGN");onClicked:root.importStudy()}
            TButton {text:qsTr("Ouvrir la bibliothèque");onClicked:root.showLibrary()}
        }
        Flow {
            id:workspace;readonly property bool wide:root.availableWidth>=760*root.p.uiScale
            Layout.fillWidth:true;spacing:16
            Rectangle {
                id:boardCard;objectName:"homeBoardCard"
                readonly property real boardExtent:Math.max(160,Math.min(width-32,Math.max(300,root.height-210),720))
                width:workspace.wide?(workspace.width-16)*.58:workspace.width;height:implicitHeight
                implicitHeight:boardExtent+boardCommands.implicitHeight+homeTitle.implicitHeight+56
                color:root.p.panel;border.color:root.p.line;radius:8
                ColumnLayout {
                    anchors.left:parent.left;anchors.right:parent.right;anchors.top:parent.top;anchors.margins:16;spacing:12
                    Label {id:homeTitle;text:root.backend.state.title;Layout.fillWidth:true;elide:Text.ElideRight;color:root.p.muted}
                    Board {id:homeBoard;objectName:"homeBoard";backend:root.backend;coordinates:root.p.coordinates;Layout.alignment:Qt.AlignHCenter;Layout.preferredWidth:boardCard.boardExtent;Layout.preferredHeight:Layout.preferredWidth}
                    RowLayout {id:boardCommands;objectName:"homeBoardCommands";Layout.fillWidth:true;spacing:8
                        TButton {text:"|◀";implicitWidth:40;enabled:root.backend.state.canBack&&!root.backend.state.training;Accessible.name:qsTr("Début de la partie");onClicked:root.backend.step(-2)}
                        TButton {text:"◀";implicitWidth:40;enabled:root.backend.state.canBack&&!root.backend.state.training;Accessible.name:qsTr("Coup précédent");onClicked:root.backend.step(-1)}
                        TButton {text:"▶";implicitWidth:40;enabled:root.backend.state.canForward&&!root.backend.state.training;Accessible.name:qsTr("Coup suivant");onClicked:root.backend.step(1)}
                        TButton {text:"▶|";implicitWidth:40;enabled:root.backend.state.canForward&&!root.backend.state.training;Accessible.name:qsTr("Fin de la variante");onClicked:root.backend.step(2)}
                        Item {Layout.fillWidth:true}
                        TButton {text:"⇅";implicitWidth:40;Accessible.name:qsTr("Retourner l’échiquier");onClicked:root.backend.flip()}
                    }
                }
            }
            Column {
                width:workspace.wide?(workspace.width-16)*.42:workspace.width;spacing:16
                Rectangle {
                    width:parent.width;height:implicitHeight;implicitHeight:currentStudy.implicitHeight+32;radius:8;color:root.p.panel;border.color:root.p.line
                    ColumnLayout {id:currentStudy;anchors.left:parent.left;anchors.right:parent.right;anchors.top:parent.top;anchors.margins:16;spacing:12
                        Label {text:qsTr("Étude en cours");font.weight:Font.DemiBold}
                        Label {text:root.backend.state.title;wrapMode:Text.WordWrap;Layout.fillWidth:true}
                        Label {text:root.backend.state.dirty?qsTr("Modifications conservées dans votre session."):qsTr("Votre position vous attend.");wrapMode:Text.WordWrap;Layout.fillWidth:true;color:root.p.muted}
                        TButton {text:qsTr("Analyser la position");accent:true;Layout.fillWidth:true;onClicked:root.resumeStudy()}
                    }
                }
                Rectangle {
                    width:parent.width;height:implicitHeight;implicitHeight:recent.implicitHeight+32;radius:8;color:root.p.panel;border.color:root.p.line
                    ColumnLayout {id:recent;anchors.left:parent.left;anchors.right:parent.right;anchors.top:parent.top;anchors.margins:16;spacing:10
                        Label {text:qsTr("Parties de la bibliothèque");font.weight:Font.DemiBold;wrapMode:Text.WordWrap;Layout.fillWidth:true}
                        Label {visible:root.backend.games.length===0;text:qsTr("Importez vos parties pour commencer.");color:root.p.muted;wrapMode:Text.WordWrap;Layout.fillWidth:true}
                        Repeater {model:Math.min(3,root.backend.games.length)
                            delegate:ItemDelegate {required property int index;property var game:root.backend.games[index]||({})
                                Layout.fillWidth:true;implicitHeight:Math.max(52,recentText.implicitHeight+12)
                                Accessible.name:(game.white||"")+" — "+(game.black||"")
                                background:Rectangle {radius:4;color:parent.hovered||parent.visualFocus?root.p.raised:"transparent";border.color:parent.visualFocus?root.p.accentColor:"transparent"}
                                contentItem:ColumnLayout {id:recentText;spacing:3
                                    Label {text:(game.white||"")+" — "+(game.black||"");elide:Text.ElideRight;Layout.fillWidth:true}
                                    Label {text:(game.event||qsTr("Étude personnelle"))+" · "+(game.result||"*");elide:Text.ElideRight;Layout.fillWidth:true;color:root.p.muted;font.pixelSize:Math.round(11*controller.preferences.fontScale*controller.preferences.uiScale)}
                                }
                                onClicked:root.backend.loadGame(game.id)
                            }
                        }
                        TButton {text:qsTr("Afficher la bibliothèque")+"  →";flat:true;Layout.fillWidth:true;onClicked:root.showLibrary()}
                    }
                }
                Rectangle {width:parent.width;height:implicitHeight;implicitHeight:shortcuts.implicitHeight+32;radius:8;color:root.p.panel;border.color:root.p.line
                    ColumnLayout {id:shortcuts;anchors.left:parent.left;anchors.right:parent.right;anchors.top:parent.top;anchors.margins:16;spacing:8
                        Label {text:qsTr("Accès rapides");font.weight:Font.DemiBold}
                        TButton {text:qsTr("Mes répertoires");Layout.fillWidth:true;onClicked:root.showRepertoires()}
                        TButton {text:qsTr("Préparer une position");Layout.fillWidth:true;onClicked:root.showPreparation()}
                        TButton {text:qsTr("Moteurs et profils");Layout.fillWidth:true;onClicked:root.showEngines()}
                        TButton {text:qsTr("Entraînement")+" · "+(root.backend.stats.due||0)+" "+qsTr("à réviser");Layout.fillWidth:true;onClicked:root.showTraining()}
                    }
                }
            }
        }
    }
}
