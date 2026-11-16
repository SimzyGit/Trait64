// SPDX-License-Identifier: AGPL-3.0-or-later
// La sélection de la bibliothèque en graphiques, comme les statistiques de base de
// ChessBase et de Scid vs. PC : parties par année, résultats, Elo moyen des deux joueurs.
// Les filtres sont ceux de la liste à l'écran.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
TDialog {
    id:root
    property var backend
    readonly property var summary:backend.librarySummary||({})
    readonly property var results:summary.results||({})
    readonly property real ui:backend.preferences.uiScale
    title:qsTr("Graphiques de la sélection")
    anchors.centerIn:parent;modal:true;standardButtons:Dialog.Close
    width:Math.min(760,parent.width-32);height:Math.min(760,parent.height-32)
    onOpened:backend.summarizeLibrary()
    function share(n){return summary.total?Math.round(100*n/summary.total)+" %":"—"}
    // Une série en barres horizontales : libellé, barre à l'échelle du plus grand, nombre.
    component Bars: ColumnLayout {
        id:bars
        property string heading
        property var rows:[]
        readonly property real most:Math.max.apply(null,[1].concat(rows.map(function(r){return r.count})))
        Layout.fillWidth:true;spacing:3
        Label {text:bars.heading;font.weight:Font.DemiBold;visible:bars.rows.length>0}
        Repeater {model:bars.rows.length
            delegate:RowLayout {required property int index;readonly property var row:bars.rows[index]
                Layout.fillWidth:true;spacing:8
                Label {text:row.label;Layout.preferredWidth:92*root.ui;horizontalAlignment:Text.AlignRight;opacity:.75}
                Item {Layout.fillWidth:true;Layout.preferredHeight:14*root.ui
                    Rectangle {height:parent.height;radius:2;color:root.backend.preferences.accentColor
                        width:Math.max(2,parent.width*row.count/bars.most)}
                }
                Label {text:row.count;Layout.preferredWidth:64*root.ui;opacity:.75}
            }
        }
    }
    ScrollView {
        anchors.fill:parent;contentWidth:availableWidth;clip:true
        ColumnLayout {
            width:parent.width;spacing:16
            Label {Layout.fillWidth:true;wrapMode:Text.WordWrap;opacity:.75
                text:root.summary.running?qsTr("Calcul…"):root.summary.error?root.backend.uiText(root.summary.error)
                    :qsTr("%1 partie(s) dans la sélection · Elo moyen %2").arg(root.summary.total||0)
                        .arg(root.summary.averageElo?"≈ "+root.summary.averageElo:qsTr("inconnu"))}
            // Les résultats en une barre partagée, des Blancs aux Noirs.
            ColumnLayout {Layout.fillWidth:true;spacing:4;visible:(root.summary.total||0)>0
                Label {text:qsTr("Résultats");font.weight:Font.DemiBold}
                Item {id:resultsBar;objectName:"resultsBar";Layout.fillWidth:true;Layout.preferredHeight:18*root.ui
                    Row {anchors.fill:parent
                        Repeater {model:[{key:"white",color:"#eceff1"},{key:"draws",color:"#90a4ae"},{key:"black",color:"#263238"},{key:"unfinished",color:"#d0b06a"}]
                            delegate:Rectangle {required property var modelData;height:resultsBar.height;color:modelData.color;border.color:root.backend.preferences.line
                                width:resultsBar.width*(root.results[modelData.key]||0)/Math.max(1,root.summary.total||0)}
                        }
                    }
                }
                Label {Layout.fillWidth:true;wrapMode:Text.WordWrap;opacity:.75
                    text:qsTr("Blancs %1 · nulles %2 · Noirs %3 · inachevées %4").arg(root.share(root.results.white||0))
                        .arg(root.share(root.results.draws||0)).arg(root.share(root.results.black||0)).arg(root.share(root.results.unfinished||0))}
            }
            Bars {objectName:"yearBars";heading:qsTr("Parties par année");rows:root.summary.years||[]}
            Bars {objectName:"eloBars";heading:qsTr("Elo moyen des deux joueurs");rows:root.summary.elo||[]}
            Label {visible:(root.summary.unknownElo||0)>0;opacity:.6;Layout.fillWidth:true;wrapMode:Text.WordWrap
                text:qsTr("%1 partie(s) sans Elo pour l’un des joueurs.").arg(root.summary.unknownElo||0)}
        }
    }
}
