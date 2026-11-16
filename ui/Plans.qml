// SPDX-License-Identifier: AGPL-3.0-or-later
// L'explorateur de plans : pour la structure de pions de l'échiquier, ce que chaque camp a
// fait de ses pièces dans les parties de la bibliothèque qui l'ont eue, et les ruptures qui
// l'ont changée. Un clic ouvre la partie la mieux classée où la manœuvre a été jouée.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
TDialog {
    id:root
    property var backend
    readonly property var plans:backend.plans||({})
    readonly property real ui:backend.preferences.uiScale
    title:qsTr("Plans de cette structure")
    anchors.centerIn:parent;modal:true;standardButtons:Dialog.Close
    width:Math.min(820,parent.width-32);height:Math.min(640,parent.height-32)
    onOpened:backend.explorePlans()
    component Side: ColumnLayout {
        id:side
        property string heading
        property var rows:[]
        Layout.fillWidth:true;Layout.fillHeight:true;Layout.preferredWidth:1;spacing:4
        Label {text:side.heading;font.weight:Font.DemiBold}
        ListView {
            id:list;Layout.fillWidth:true;Layout.fillHeight:true;clip:true;spacing:2
            model:side.rows
            ScrollBar.vertical:ScrollBar {}
            delegate:ItemDelegate {
                required property var modelData
                width:list.width-10;padding:4
                ToolTip.visible:hovered;ToolTip.delay:500
                ToolTip.text:qsTr("Ouvrir la partie la mieux classée où ce coup a été joué")
                contentItem:RowLayout {spacing:8
                    Label {text:modelData.text;font.weight:Font.DemiBold;Layout.preferredWidth:64*root.ui}
                    Label {text:modelData.rupture?qsTr("rupture"):"";color:"#c0392b";Layout.preferredWidth:64*root.ui;font.pixelSize:Math.round(11*root.ui)}
                    Item {Layout.fillWidth:true;Layout.preferredHeight:10*root.ui
                        Rectangle {height:parent.height;radius:2;color:root.backend.preferences.accentColor;opacity:.75
                            width:parent.width*modelData.share/100}
                    }
                    Label {text:qsTr("%1 % · score %2 %").arg(modelData.share).arg(modelData.score);opacity:.75;Layout.preferredWidth:120*root.ui;horizontalAlignment:Text.AlignRight}
                }
                onClicked:{root.backend.loadGame(modelData.example);root.close()}
            }
        }
    }
    ColumnLayout {
        anchors.fill:parent;spacing:10
        Label {objectName:"plansSummary";Layout.fillWidth:true;wrapMode:Text.WordWrap;opacity:.75
            text:root.plans.running?qsTr("Recherche des parties de cette structure…")
                :root.plans.error?root.backend.uiText(root.plans.error)
                :(root.plans.games||0)===0?qsTr("Aucune partie de la bibliothèque n’a eu cette structure de pions.")
                :!(root.plans.white||[]).length&&!(root.plans.black||[]).length?qsTr("Les parties de la bibliothèque qui ont eu cette structure s’y arrêtent : aucun coup ne l’a suivie.")
                :qsTr("%1 partie(s) de la bibliothèque ont eu cette structure de pions (les mieux classées d’abord). Chaque manœuvre compte une fois par partie, tant que la structure tient ; la rupture est la poussée de pion qui la change.").arg(root.plans.games)}
        RowLayout {Layout.fillWidth:true;Layout.fillHeight:true;spacing:16
            Side {objectName:"plansWhite";heading:qsTr("Blancs");rows:root.plans.white||[]}
            Side {objectName:"plansBlack";heading:qsTr("Noirs");rows:root.plans.black||[]}
        }
    }
}
