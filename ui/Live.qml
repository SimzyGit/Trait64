// SPDX-License-Identifier: AGPL-3.0-or-later
// Les tournois en direct, retransmis par Lichess : à gauche les tournois, à droite les
// échiquiers de la ronde, rafraîchis toutes les vingt secondes. « Suivre » ouvre la
// partie dans un onglet de l'analyse et lui ajoute ses coups au fil de la ronde.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
TDialog {
    id:root
    property var backend
    readonly property var live:backend.live
    readonly property var list:live.list||[]
    readonly property var round:live.round||({})
    readonly property real ui:backend.preferences.uiScale
    title:qsTr("Tournois en direct")
    anchors.centerIn:parent;modal:true;standardButtons:Dialog.Close
    width:Math.min(1180,parent.width-32);height:Math.min(820,parent.height-32)
    onOpened:if(!list.length)backend.liveList()
    onClosed:backend.liveClose()
    function clock(seconds){if(seconds===undefined||seconds===null)return "";var h=Math.floor(seconds/3600),m=Math.floor(seconds%3600/60),s=seconds%60
        return (h?h+":"+(m<10?"0":""):"")+m+":"+(s<10?"0":"")+s}
    // Part des Blancs dans la barre, même courbe que la barre de l'analyse.
    function whiteShare(e){return e?1/(1+Math.exp(-0.00368208*Math.max(-1500,Math.min(1500,Number(e.score)*100)))):.5}
    function who(p){return p&&p.name?((p.title?p.title+" ":"")+p.name+(p.rating?" ("+p.rating+")":"")):"?"}
    ColumnLayout {
        anchors.fill:parent;spacing:8
        RowLayout {Layout.fillWidth:true
            Label {Layout.fillWidth:true;wrapMode:Text.WordWrap;opacity:.75
                text:root.live.error?root.backend.uiText(root.live.error)
                    :root.live.following?qsTr("Partie suivie dans l’analyse : %1. Ses coups y arrivent au fil de la ronde.").arg(root.live.following)
                    :qsTr("Retransmissions publiques de Lichess. Rien n’est demandé tant que ce panneau n’est pas ouvert.")}
            TButton {text:qsTr("Arrêter le suivi");visible:!!root.live.following;onClicked:root.backend.liveStopFollowing()}
            TButton {text:qsTr("Actualiser");onClicked:root.backend.liveList()}
        }
        SplitView {
            Layout.fillWidth:true;Layout.fillHeight:true
            // Un filet d'un pixel, comme les autres panneaux : la poignée par défaut faisait une barre grise.
            handle:Rectangle {implicitWidth:7;color:"transparent";Rectangle {anchors.horizontalCenter:parent.horizontalCenter;width:1;height:parent.height;color:SplitHandle.hovered||SplitHandle.pressed?palette.highlight:palette.mid}}
            ListView {
                id:tours;objectName:"liveTours";SplitView.preferredWidth:300*root.ui;SplitView.minimumWidth:200;clip:true;spacing:2
                model:root.list
                ScrollBar.vertical:ScrollBar {}
                delegate:ItemDelegate {
                    required property var modelData;required property int index
                    readonly property bool chosen:(root.live.entry||{}).roundId===modelData.roundId
                    width:tours.width-8
                    background:Rectangle {radius:4;color:parent.chosen?Qt.rgba(root.palette.highlight.r,root.palette.highlight.g,root.palette.highlight.b,.18):parent.hovered?root.palette.alternateBase:"transparent"
                        border.color:parent.chosen?root.palette.highlight:"transparent"}
                    contentItem:ColumnLayout {spacing:1
                        Label {text:modelData.tour;elide:Text.ElideRight;Layout.fillWidth:true;font.weight:Font.DemiBold}
                        Label {text:(modelData.ongoing?"● "+qsTr("En cours")+" · ":"")+modelData.round;elide:Text.ElideRight;Layout.fillWidth:true;opacity:.7
                               color:modelData.ongoing?"#c0392b":root.palette.text}
                    }
                    onClicked:root.backend.liveOpenRound(modelData)
                }
                Label {anchors.centerIn:parent;visible:!root.list.length;text:root.live.loading?qsTr("Chargement…"):qsTr("Aucune retransmission.");opacity:.6}
            }
            Item {
                SplitView.fillWidth:true
                Label {anchors.centerIn:parent;visible:!(root.round.games||[]).length;opacity:.6
                    text:root.live.loading?qsTr("Chargement…"):qsTr("Choisissez un tournoi à gauche.")}
                GridView {
                    id:boards;objectName:"liveBoards";anchors.fill:parent;clip:true
                    readonly property real side:Math.round(170*root.ui)
                    cellWidth:Math.max(side+24,width/Math.max(1,Math.floor(width/(side+24))));cellHeight:side+96*root.ui
                    model:root.round.games||[]
                    ScrollBar.vertical:ScrollBar {}
                    delegate:Item {
                        required property var modelData;required property int index
                        width:boards.cellWidth;height:boards.cellHeight
                        readonly property var evaluation:(root.live.evals||{})[modelData.fen]
                        ColumnLayout {
                            anchors.horizontalCenter:parent.horizontalCenter;width:boards.side;spacing:2
                            RowLayout {Layout.fillWidth:true
                                Label {text:root.who(modelData.black);elide:Text.ElideRight;Layout.fillWidth:true;font.pixelSize:Math.round(11*root.ui)}
                                Label {text:root.clock(modelData.black.clock);font.pixelSize:Math.round(11*root.ui);opacity:.7}
                            }
                            MiniBoard {Layout.preferredWidth:boards.side;Layout.preferredHeight:boards.side;fen:modelData.fen||"";lastMove:modelData.lastMove||""
                                // Barre d'évaluation du moteur local, collée à gauche de l'échiquier.
                                Rectangle {
                                    objectName:"liveEval"+index
                                    width:Math.round(7*root.ui);height:parent.height;x:-width-3;radius:2;clip:true
                                    color:"#1d2125";border.color:root.palette.mid;opacity:evaluation?1:.35
                                    Rectangle {width:parent.width;anchors.bottom:parent.bottom;color:"#f4f6f8";height:parent.height*root.whiteShare(evaluation)
                                        Behavior on height {NumberAnimation {duration:180}}}
                                }
                            }
                            RowLayout {Layout.fillWidth:true
                                Label {text:root.who(modelData.white);elide:Text.ElideRight;Layout.fillWidth:true;font.pixelSize:Math.round(11*root.ui)}
                                Label {text:root.clock(modelData.white.clock);font.pixelSize:Math.round(11*root.ui);opacity:.7}
                            }
                            RowLayout {Layout.fillWidth:true
                                Label {text:modelData.status==="*"?qsTr("En cours"):modelData.status==="1/2-1/2"?"½-½":modelData.status;font.weight:Font.DemiBold;Layout.fillWidth:true}
                                Label {text:evaluation?evaluation.eval:"";opacity:.75;font.pixelSize:Math.round(11*root.ui)}
                                TButton {objectName:"follow"+index;text:qsTr("Suivre");onClicked:{root.backend.liveFollow(index);root.close()}}
                            }
                        }
                    }
                }
            }
        }
    }
}
