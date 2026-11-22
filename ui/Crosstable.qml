// SPDX-License-Identifier: AGPL-3.0-or-later
// Le tableau d'un tournoi, comme dans ChessBase et Scid vs. PC : classement aux points,
// départage Sonneborn-Berger, performance. Tableau carré pour un toutes-rondes, une
// colonne par ronde pour un open. Un clic sur un résultat ouvre la partie.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
TDialog {
    id:root
    property var backend
    property var table:({})
    readonly property var rows:table.players||[]
    readonly property bool matrix:!!table.matrix
    // Colonnes du milieu : une par adversaire (tableau carré) ou par ronde (open).
    readonly property int middle:matrix?rows.length:(table.rounds||0)
    readonly property real unit:Math.round((matrix?30:46)*backend.preferences.uiScale)
    readonly property real nameWidth:Math.round(190*backend.preferences.uiScale)
    readonly property real narrow:Math.round(44*backend.preferences.uiScale)
    title:table.event?(table.year?qsTr("%1 · %2").arg(table.event).arg(table.year):table.event):qsTr("Tableau du tournoi")
    anchors.centerIn:parent;modal:true;standardButtons:Dialog.Close
    width:Math.min(parent.width-32,Math.max(560,nameWidth+narrow*7.6+middle*unit+60));height:Math.min(parent.height-32,180+rows.length*26)
    function show(gameId){table=backend.tournamentTable(gameId);if(rows.length)open()}
    function number(value){return value%1?(Math.floor(value)>0?Math.floor(value):"")+"½":String(value)}
    ColumnLayout {
        anchors.fill:parent;spacing:6
        Label {Layout.fillWidth:true;wrapMode:Text.WordWrap;opacity:.75
            text:root.matrix?qsTr("%1 joueurs · départage Sonneborn-Berger, puis victoires. Un clic sur un résultat ouvre la partie.").arg(root.rows.length)
                            :qsTr("%1 joueurs, %2 rondes · par ronde : rang de l’adversaire, ○ Blancs ● Noirs, résultat.").arg(root.rows.length).arg(root.table.rounds||0)}
        // Le tournoi d'un coup d'œil : force moyenne, catégorie FIDE, partage des résultats.
        Label {objectName:"tournamentSummary";Layout.fillWidth:true;wrapMode:Text.WordWrap;opacity:.75
            readonly property var sum:root.table.summary||({})
            visible:(sum.games||0)>0
            text:(sum.averageElo>0?qsTr("Elo moyen %1").arg(sum.averageElo)+(sum.category>0?" · "+qsTr("catégorie %1").arg(sum.category):"")+" · ":"")
                 +qsTr("%1 parties : %2 gains des Blancs, %3 nulles, %4 gains des Noirs (%5 % décisives)").arg(sum.games).arg(sum.whiteWins).arg(sum.draws).arg(sum.blackWins).arg(sum.decisive)}
        Flickable {
            id:sheet;Layout.fillWidth:true;Layout.fillHeight:true;clip:true
            contentWidth:Math.max(width,body.width);contentHeight:body.height
            ScrollBar.horizontal:ScrollBar {}
            ScrollBar.vertical:ScrollBar {}
            Column {
                id:body
                // L'en-tête puis une ligne par joueur, dans l'ordre du classement.
                Repeater {
                    model:root.rows.length+1
                    Rectangle {
                        required property int index
                        readonly property bool header:index===0
                        readonly property var row:header?({}):root.rows[index-1]
                        width:line.width;height:26*root.backend.preferences.uiScale
                        color:header?root.palette.alternateBase:index%2?"transparent":Qt.rgba(root.palette.highlight.r,root.palette.highlight.g,root.palette.highlight.b,.05)
                        Row {
                            id:line;height:parent.height
                            Label {width:root.narrow*.8;height:parent.height;verticalAlignment:Text.AlignVCenter;horizontalAlignment:Text.AlignRight;rightPadding:8
                                text:header?"":row.rank;opacity:.7}
                            Label {width:root.nameWidth;height:parent.height;verticalAlignment:Text.AlignVCenter;elide:Text.ElideRight
                                text:header?qsTr("Joueur"):row.name;font.weight:header?Font.Normal:Font.DemiBold;opacity:header?.7:1}
                            Label {width:root.narrow;height:parent.height;verticalAlignment:Text.AlignVCenter;horizontalAlignment:Text.AlignRight;rightPadding:6
                                text:header?"Elo":(row.elo>0?row.elo:"");opacity:.7}
                            Repeater {
                                model:root.middle
                                Rectangle {
                                    required property int index
                                    readonly property string value:header?(root.matrix?String(index+1):qsTr("R%1").arg(index+1))
                                                                         :(root.matrix?row.cells[index]:row.rounds[index])
                                    readonly property var games:!header&&root.matrix?row.cellGames[index]:[]
                                    width:root.unit;height:parent.height
                                    color:value==="×"?root.palette.mid:cellHover.hovered&&games.length?Qt.rgba(root.palette.highlight.r,root.palette.highlight.g,root.palette.highlight.b,.2):"transparent"
                                    border.color:root.matrix&&!header?Qt.rgba(root.palette.mid.r,root.palette.mid.g,root.palette.mid.b,.4):"transparent"
                                    Label {anchors.fill:parent;horizontalAlignment:Text.AlignHCenter;verticalAlignment:Text.AlignVCenter;text:parent.value==="×"?"":parent.value;opacity:header?.7:1
                                           font.pixelSize:Math.round((root.matrix?12:11)*root.backend.preferences.fontScale*root.backend.preferences.uiScale)}
                                    HoverHandler {id:cellHover;cursorShape:parent.games.length?Qt.PointingHandCursor:Qt.ArrowCursor}
                                    TapHandler {enabled:parent.games.length>0;onTapped:{root.backend.loadGame(parent.games[0]);root.close()}}
                                }
                            }
                            Label {width:root.narrow;height:parent.height;verticalAlignment:Text.AlignVCenter;horizontalAlignment:Text.AlignRight;rightPadding:6
                                text:header?qsTr("Pts"):root.number(row.score);font.weight:Font.DemiBold;opacity:header?.7:1}
                            Label {width:root.narrow*1.3;height:parent.height;verticalAlignment:Text.AlignVCenter;horizontalAlignment:Text.AlignRight;rightPadding:6
                                text:header?"SB":row.sonneborn.toFixed(2);opacity:.7}
                            Label {width:root.narrow*1.2;height:parent.height;verticalAlignment:Text.AlignVCenter;horizontalAlignment:Text.AlignRight;rightPadding:6
                                text:header?qsTr("Perf."):(row.performance>0?row.performance:"");opacity:.7
                                ToolTip.visible:header&&perfHover.hovered;ToolTip.text:qsTr("Performance : Elo moyen des adversaires + 400 × (victoires − défaites) / parties, quand tous les adversaires sont classés.")
                                HoverHandler {id:perfHover}}
                            Label {width:root.narrow*1.2;height:parent.height;verticalAlignment:Text.AlignVCenter;horizontalAlignment:Text.AlignRight;rightPadding:6
                                text:header?"Bu":root.number(row.buchholz);opacity:.7
                                ToolTip.visible:header&&buHover.hovered;ToolTip.text:qsTr("Buchholz : la somme des points des adversaires rencontrés.")
                                HoverHandler {id:buHover}}
                            Label {width:root.narrow*1.4;height:parent.height;verticalAlignment:Text.AlignVCenter;horizontalAlignment:Text.AlignRight;rightPadding:6
                                text:header?"±Elo":(row.change===undefined||row.change===null?"":(row.change>0?"+":"")+row.change.toLocaleString(Qt.locale(root.backend.preferences.language==="auto"?"":root.backend.preferences.language),"f",1))
                                opacity:header?.7:1
                                ToolTip.visible:header&&eloHover.hovered;ToolTip.text:qsTr("Variation d’Elo à la manière de la FIDE : K × (points − points attendus), K = 10 à partir de 2400, 20 en dessous.")
                                HoverHandler {id:eloHover}}
                        }
                    }
                }
            }
        }
    }
}
