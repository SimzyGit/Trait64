// SPDX-License-Identifier: AGPL-3.0-or-later
// L'accueil est un tableau de bord : ce qui attend aujourd'hui, l'étude en cours en
// vignette, les dernières parties, les comptes en ligne. Il répétait auparavant
// l'échiquier de l'analyse en grand, sans rien dire de plus.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
ScrollView {
    id:root
    required property var backend
    property var p:backend.preferences
    readonly property real gap:16*p.uiScale
    signal newStudy()
    signal importStudy()
    signal resumeStudy()
    signal showLibrary()
    signal showTraining()
    signal showRepertoires()
    signal showPreparation()
    signal showEngines()
    signal showAccounts()
    contentWidth:availableWidth;clip:true;ScrollBar.horizontal.policy:ScrollBar.AlwaysOff
    // Une carte : un titre et son contenu, sur le fond des panneaux.
    component Card: Rectangle {
        id:card
        property string heading
        default property alias content:body.data
        // Les cartes d'une même rangée prennent la même hauteur : à la hauteur de leur
        // contenu, elles laissaient des trous inégaux dans la grille.
        Layout.fillWidth:true;Layout.fillHeight:true
        implicitHeight:body.implicitHeight+2*root.gap
        radius:6;color:root.p.panel;border.color:root.p.line
        ColumnLayout {
            id:body;anchors.left:parent.left;anchors.right:parent.right;anchors.top:parent.top;anchors.margins:root.gap;spacing:10
            Label {text:card.heading;font.weight:Font.DemiBold;visible:text.length>0}
        }
    }
    function resultText(result){return result==="1/2-1/2"?"½-½":(result||"*")}
    ColumnLayout {
        width:root.availableWidth;spacing:root.gap
        RowLayout {
            Layout.fillWidth:true;spacing:12
            ColumnLayout {
                Layout.fillWidth:true;spacing:0
                Label {text:"Trait64";font.pixelSize:Math.round(22*root.p.fontScale*root.p.uiScale);font.weight:Font.DemiBold}
                Label {Layout.fillWidth:true;elide:Text.ElideRight;color:root.p.muted;font.pixelSize:Math.round(12*root.p.fontScale*root.p.uiScale)
                    text:qsTr("Vos parties, votre analyse, votre progression — sur votre machine.")}
            }
            TButton {text:qsTr("Nouvelle étude");accent:true;onClicked:root.newStudy()}
            TButton {text:qsTr("Importer un PGN");onClicked:root.importStudy()}
        }
        // L'étude en cours à gauche, les quatre cartes à droite deux par deux ; une seule
        // colonne quand la fenêtre est étroite.
        GridLayout {
            id:grid;objectName:"homeDashboard"
            Layout.fillWidth:true;columnSpacing:root.gap;rowSpacing:root.gap
            readonly property bool wide:root.availableWidth>=900*root.p.uiScale
            columns:wide?2:1
            // L'étude en cours, en vignette : on la reprend d'un clic.
            Card {
                objectName:"homeStudyCard";heading:qsTr("Étude en cours")
                Layout.fillWidth:!grid.wide;Layout.preferredWidth:grid.wide?Math.min(380*root.p.uiScale,root.availableWidth*.34):-1
                Item {
                    Layout.fillWidth:true;Layout.preferredHeight:thumbnail.height
                    Board {id:thumbnail;objectName:"homeBoard";backend:root.backend;coordinates:false;interactive:false
                        width:Math.min(parent.width,300*root.p.uiScale);height:width;anchors.horizontalCenter:parent.horizontalCenter}
                    TapHandler {onTapped:root.resumeStudy()}
                    HoverHandler {cursorShape:Qt.PointingHandCursor}
                }
                Label {text:root.backend.state.title;wrapMode:Text.WordWrap;Layout.fillWidth:true}
                Label {text:root.backend.state.dirty?qsTr("Modifications conservées dans votre session."):qsTr("Votre position vous attend.");wrapMode:Text.WordWrap;Layout.fillWidth:true;color:root.p.muted}
                TButton {text:qsTr("Reprendre l’analyse");accent:true;Layout.fillWidth:true;onClicked:root.resumeStudy()}
            }
            GridLayout {
            Layout.fillWidth:true;Layout.alignment:Qt.AlignTop;columnSpacing:root.gap;rowSpacing:root.gap
            columns:root.availableWidth>=700*root.p.uiScale?2:1
            // Ce qui attend aujourd'hui : révisions, objectif du jour, puzzles.
            Card {
                id:today;objectName:"homeTodayCard";heading:qsTr("Aujourd’hui")
                readonly property int due:root.backend.stats.due||0
                readonly property int done:root.backend.stats.todayReviews||0
                readonly property int goal:Math.max(1,root.p.dailyGoal||10)
                Label {Layout.fillWidth:true;wrapMode:Text.WordWrap
                    text:today.due>0?qsTr("%1 carte(s) à réviser.").arg(today.due):qsTr("Aucune carte à réviser : tout est à jour.")}
                ProgressBar {Layout.fillWidth:true;from:0;to:today.goal;value:Math.min(today.done,today.goal)}
                Label {Layout.fillWidth:true;color:root.p.muted;font.pixelSize:Math.round(11*root.p.fontScale*root.p.uiScale)
                    text:qsTr("Objectif du jour : %1 sur %2").arg(today.done).arg(today.goal)}
                RowLayout {Layout.fillWidth:true
                    TButton {text:qsTr("S’entraîner");accent:today.due>0;Layout.fillWidth:true;onClicked:root.showTraining()}
                    TButton {text:qsTr("Mes répertoires");Layout.fillWidth:true;onClicked:root.showRepertoires()}
                }
            }
            // Les dernières parties, avec leur résultat.
            Card {
                objectName:"homeRecentCard";heading:qsTr("Dernières parties")
                Label {visible:root.backend.games.length===0;text:qsTr("Importez vos parties pour commencer.");color:root.p.muted;wrapMode:Text.WordWrap;Layout.fillWidth:true}
                Repeater {model:Math.min(5,root.backend.games.length)
                    delegate:ItemDelegate {required property int index;property var game:root.backend.games[index]||({})
                        Layout.fillWidth:true;implicitHeight:Math.max(30*root.p.uiScale,line.implicitHeight+8)
                        Accessible.name:(game.white||"")+" — "+(game.black||"")
                        background:Rectangle {radius:4;color:parent.hovered||parent.visualFocus?root.p.raised:"transparent";border.color:parent.visualFocus?root.p.accentColor:"transparent"}
                        contentItem:RowLayout {id:line;spacing:8
                            Label {text:(game.white||"?")+" — "+(game.black||"?");elide:Text.ElideRight;Layout.fillWidth:true}
                            Label {text:root.resultText(game.result);color:root.p.muted;font.weight:Font.DemiBold}
                        }
                        onClicked:root.backend.loadGame(game.id)
                    }
                }
                TButton {text:qsTr("Afficher la bibliothèque")+"  →";flat:true;Layout.fillWidth:true;onClicked:root.showLibrary()}
            }
            // La bibliothèque en chiffres et les comptes en ligne.
            Card {
                objectName:"homeLibraryCard";heading:qsTr("Bibliothèque")
                Label {Layout.fillWidth:true;wrapMode:Text.WordWrap
                    text:((root.backend.stats.games||0)===1?qsTr("Une partie"):qsTr("%1 parties").arg(root.backend.stats.games||0))+" · "+qsTr("%1 favorite(s)").arg(root.backend.stats.favorites||0)}
                Repeater {model:root.backend.onlineAccounts
                    delegate:RowLayout {required property var modelData;Layout.fillWidth:true
                        Label {Layout.fillWidth:true;elide:Text.ElideRight
                            text:(modelData.site==="lichess"?"Lichess":"Chess.com")+" · "+modelData.user}
                        Label {color:root.p.muted;font.pixelSize:Math.round(11*root.p.fontScale*root.p.uiScale)
                            text:modelData.syncing?qsTr("synchronisation…"):modelData.lastSync>0?new Date(modelData.lastSync).toLocaleDateString(Qt.locale(controller.preferences.language==="auto"?"":controller.preferences.language),Locale.ShortFormat):qsTr("jamais")}
                    }
                }
                RowLayout {Layout.fillWidth:true
                    TButton {text:root.backend.onlineAccounts.length?qsTr("Synchroniser"):qsTr("Ajouter un compte en ligne");Layout.fillWidth:true
                        onClicked:root.backend.onlineAccounts.length?root.backend.syncAllAccounts():root.showAccounts()}
                    TButton {text:qsTr("Moteurs");Layout.fillWidth:true;onClicked:root.showEngines()}
                }
            }
            Card {
                objectName:"homePrepareCard";heading:qsTr("Préparer")
                Label {Layout.fillWidth:true;wrapMode:Text.WordWrap;color:root.p.muted
                    text:qsTr("Ce qu’un adversaire joue dans une position, d’après votre bibliothèque et les parties en ligne.")}
                TButton {text:qsTr("Préparer une position");Layout.fillWidth:true;onClicked:root.showPreparation()}
            }
            }
        }
    }
}
