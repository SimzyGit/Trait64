// SPDX-License-Identifier: AGPL-3.0-or-later
// Finales à jouer contre le moteur : les techniques de base, puis les positions qu'il faut
// savoir gagner ou tenir. On choisit, la position s'installe, le moteur répond de toute sa
// force — ce qui ne pardonne rien et ne dit jamais « presque ».
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
TDialog {
    id:root
    property var backend
    title:qsTr("Finales à jouer")
    anchors.centerIn:parent;modal:true;standardButtons:Dialog.Close
    width:Math.min(640,parent.width-32);height:Math.min(620,parent.height-32)
    // goal : "win" pour gagner, "draw" pour annuler ; white : le camp que l'on joue.
    readonly property var positions:[
        {name:qsTr("Roi et dame contre roi"),fen:"8/8/8/4k3/8/8/8/3QK3 w - - 0 1",white:true,goal:"win"},
        {name:qsTr("Roi et tour contre roi"),fen:"8/8/8/4k3/8/8/8/R3K3 w - - 0 1",white:true,goal:"win"},
        {name:qsTr("Deux fous contre roi"),fen:"8/8/8/4k3/8/8/8/2B1KB2 w - - 0 1",white:true,goal:"win"},
        {name:qsTr("Fou et cavalier contre roi"),fen:"8/8/8/4k3/8/8/8/2B1KN2 w - - 0 1",white:true,goal:"win"},
        {name:qsTr("Pion : le roi devant, en sixième"),fen:"4k3/8/4K3/4P3/8/8/8/8 w - - 0 1",white:true,goal:"win"},
        {name:qsTr("Pion : tenir l’opposition"),fen:"4k3/8/8/4P3/4K3/8/8/8 b - - 0 1",white:false,goal:"draw"},
        {name:qsTr("Tour : la position de Lucena"),fen:"1K1k4/1P6/8/8/8/8/r7/2R5 w - - 0 1",white:true,goal:"win"},
        {name:qsTr("Tour : la défense Philidor"),fen:"4k3/8/7r/3PK3/8/8/8/R7 b - - 0 1",white:false,goal:"draw"}
    ]
    function start(index){
        const p=positions[index]
        backend.setPosition(p.fen,false)
        backend.playComputer(p.white)
        close()
    }
    ColumnLayout {
        anchors.fill:parent;spacing:10
        Label {
            Layout.fillWidth:true;wrapMode:Text.WordWrap
            text:root.backend&&root.backend.preferences.enginePath?qsTr("Choisissez une finale : la position s’installe et le moteur joue l’autre camp, sans retenue."):qsTr("Ces exercices se jouent contre un moteur : installez-en un dans les réglages.")
        }
        ListView {
            id:list;objectName:"endgameList";Layout.fillWidth:true;Layout.fillHeight:true;clip:true;spacing:6;model:root.positions
            ScrollBar.vertical:ScrollBar {}
            delegate:Rectangle {
                required property var modelData;required property int index
                width:list.width;height:58;color:root.palette.alternateBase
                RowLayout {anchors.fill:parent;anchors.margins:10
                    ColumnLayout {Layout.fillWidth:true;spacing:2
                        Label {text:modelData.name;font.bold:true;Layout.fillWidth:true;elide:Text.ElideRight}
                        Label {text:(modelData.white?qsTr("Vous avez les Blancs"):qsTr("Vous avez les Noirs"))+" · "+(modelData.goal==="win"?qsTr("à gagner"):qsTr("à tenir nulle"));opacity:.7}
                    }
                    TButton {objectName:"endgame"+index;text:qsTr("Jouer");accent:true;enabled:!!(root.backend&&root.backend.preferences.enginePath)&&!root.backend.busy;onClicked:root.start(index)}
                }
            }
        }
    }
}
