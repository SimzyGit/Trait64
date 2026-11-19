// SPDX-License-Identifier: AGPL-3.0-or-later
// Les positions semblables : dans la bibliothèque, celles qui ont le même matériel et le
// même camp au trait, les plus proches d'abord. Un clic ouvre la partie sur la position.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
TDialog {
    id:root
    property var backend
    readonly property var found:backend.similar||({})
    readonly property real ui:backend.preferences.uiScale
    title:qsTr("Positions semblables")
    anchors.centerIn:parent;modal:true;standardButtons:Dialog.Close
    width:Math.min(760,parent.width-32);height:Math.min(680,parent.height-32)
    onOpened:backend.findSimilar()
    ColumnLayout {
        anchors.fill:parent;spacing:10
        Label {objectName:"similarSummary";Layout.fillWidth:true;wrapMode:Text.WordWrap;opacity:.75
            text:root.found.running?qsTr("Recherche des positions de même matériel…")
                :root.found.error?root.backend.uiText(root.found.error)
                :!(root.found.rows||[]).length?qsTr("Aucune position de la bibliothèque n’a ce matériel avec le même camp au trait.")
                :qsTr("Même matériel, même camp au trait, une position par partie. L’écart compte les cases qui diffèrent, un pion comptant double.")}
        ListView {
            id:list;objectName:"similarList";Layout.fillWidth:true;Layout.fillHeight:true;clip:true;spacing:6
            model:root.found.rows||[]
            ScrollBar.vertical:ScrollBar {}
            delegate:ItemDelegate {
                required property var modelData
                width:list.width-12;padding:6
                onClicked:{root.backend.openSimilar(modelData.id,modelData.node);root.close()}
                contentItem:RowLayout {spacing:12
                    MiniBoard {fen:modelData.fen;Layout.preferredWidth:96*root.ui;Layout.preferredHeight:96*root.ui
                        flipped:(root.found.fen||"").split(" ")[1]==="b"}
                    ColumnLayout {Layout.fillWidth:true;spacing:2
                        Label {Layout.fillWidth:true;elide:Text.ElideRight;font.weight:Font.DemiBold
                            text:modelData.white+(modelData.whiteElo>0?" ("+modelData.whiteElo+")":"")+" – "
                                 +modelData.black+(modelData.blackElo>0?" ("+modelData.blackElo+")":"")}
                        Label {opacity:.75;text:modelData.result+" · "+modelData.date}
                    }
                    Label {Layout.preferredWidth:96*root.ui;horizontalAlignment:Text.AlignRight;font.weight:Font.DemiBold
                        text:modelData.distance===0?qsTr("identique"):qsTr("écart %1").arg(modelData.distance)}
                }
            }
        }
    }
}
