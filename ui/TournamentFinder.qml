// SPDX-License-Identifier: AGPL-3.0-or-later
// Retrouver un tournoi de la bibliothèque, comme le chercheur de tournois de Scid vs. PC :
// événement et année, parties, joueurs, Elo moyen. Un clic ouvre son tableau croisé.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
TDialog {
    id:root
    property var backend
    property var crosstable
    readonly property var rows:backend.tournamentList.rows||[]
    readonly property real ui:backend.preferences.uiScale
    title:qsTr("Tournois de la bibliothèque")
    anchors.centerIn:parent;modal:true;standardButtons:Dialog.Close
    width:Math.min(820,parent.width-32);height:Math.min(700,parent.height-32)
    onOpened:{filter.text="";backend.findTournaments("");filter.forceActiveFocus()}
    ColumnLayout {
        anchors.fill:parent;spacing:8
        RowLayout {Layout.fillWidth:true
            TextField {id:filter;objectName:"tournamentFilter";Layout.fillWidth:true;placeholderText:qsTr("Nom du tournoi…");selectByMouse:true
                onTextChanged:typing.restart()}
            Timer {id:typing;interval:250;onTriggered:root.backend.findTournaments(filter.text)}
            Label {opacity:.7;text:root.backend.tournamentList.running?qsTr("Recherche…"):qsTr("%1 tournoi(s)").arg(root.rows.length)}
        }
        // En-tête puis une ligne par tournoi.
        RowLayout {Layout.fillWidth:true;spacing:8
            Label {text:qsTr("Tournoi");Layout.fillWidth:true;opacity:.7}
            Label {text:qsTr("Année");Layout.preferredWidth:56*root.ui;opacity:.7}
            Label {text:qsTr("Parties");Layout.preferredWidth:64*root.ui;horizontalAlignment:Text.AlignRight;opacity:.7}
            Label {text:qsTr("Joueurs");Layout.preferredWidth:64*root.ui;horizontalAlignment:Text.AlignRight;opacity:.7}
            Label {text:qsTr("Elo moyen");Layout.preferredWidth:76*root.ui;horizontalAlignment:Text.AlignRight;opacity:.7;rightPadding:12}
        }
        ListView {
            id:list;objectName:"tournamentRows";Layout.fillWidth:true;Layout.fillHeight:true;clip:true;model:root.rows
            ScrollBar.vertical:ScrollBar {}
            delegate:ItemDelegate {
                required property var modelData;required property int index
                width:list.width;height:28*root.ui
                background:Rectangle {color:parent.hovered?root.palette.alternateBase:index%2?"transparent":Qt.rgba(root.palette.highlight.r,root.palette.highlight.g,root.palette.highlight.b,.04)}
                contentItem:RowLayout {spacing:8
                    Label {text:modelData.event;Layout.fillWidth:true;elide:Text.ElideRight}
                    Label {text:modelData.year||"—";Layout.preferredWidth:56*root.ui;opacity:.75}
                    Label {text:modelData.games;Layout.preferredWidth:64*root.ui;horizontalAlignment:Text.AlignRight}
                    Label {text:modelData.players;Layout.preferredWidth:64*root.ui;horizontalAlignment:Text.AlignRight}
                    Label {text:modelData.elo>0?modelData.elo:"";Layout.preferredWidth:76*root.ui;horizontalAlignment:Text.AlignRight;opacity:.75;rightPadding:12}
                }
                onClicked:root.crosstable.show(modelData.gameId)
            }
        }
    }
}
