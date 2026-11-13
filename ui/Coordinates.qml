// SPDX-License-Identifier: AGPL-3.0-or-later
// Exercice de coordonnées : une case est nommée, on la clique sur un échiquier nu. Trente
// secondes, autant de cases que possible. Les repères du bord sont masqués pendant la
// série : c'est justement eux qu'on apprend à ne plus chercher.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
TDialog {
    id:root
    property bool blackSide:false
    property bool running:false
    property string target:""
    property int score:0
    property int errors:0
    property int best:0
    property int remaining:30
    property string missed:""
    title:qsTr("Coordonnées")
    anchors.centerIn:parent;modal:true;standardButtons:Dialog.Close
    width:Math.min(560,parent.width-32);height:Math.min(680,parent.height-32)
    function squareName(index){var d=blackSide?63-index:index;return "abcdefgh"[d%8]+(8-Math.floor(d/8))}
    function next(){var name;do{name="abcdefgh"[Math.floor(Math.random()*8)]+(1+Math.floor(Math.random()*8))}while(name===target);target=name}
    function start(){score=0;errors=0;remaining=30;missed="";running=true;next()}
    function stop(){running=false;best=Math.max(best,score)}
    // Vrai si la case cliquée était la bonne.
    function answer(name){
        if(!running)return false
        if(name===target){score++;missed="";next();return true}
        errors++;missed=name;return false
    }
    onClosed:if(running)stop()
    Timer {interval:1000;repeat:true;running:root.running;onTriggered:{if(--root.remaining<=0)root.stop()}}
    ColumnLayout {
        anchors.fill:parent;spacing:10
        RowLayout {
            Layout.fillWidth:true;spacing:12
            Label {
                objectName:"coordinateTarget";Layout.fillWidth:true
                text:root.running?root.target:(root.score+root.errors>0?qsTr("%1 case(s) trouvée(s), %2 erreur(s)").arg(root.score).arg(root.errors):qsTr("Cliquez la case demandée, le plus vite possible."))
                font.pixelSize:root.running?34:15;font.weight:root.running?Font.Bold:Font.Normal;wrapMode:Text.WordWrap
            }
            Label {visible:root.running;text:root.remaining+" s · "+root.score;font.pixelSize:18;color:root.appearance.muted}
        }
        Item {
            Layout.fillWidth:true;Layout.fillHeight:true
            Grid {
                id:grid;anchors.centerIn:parent;columns:8
                readonly property real cell:Math.floor(Math.min(parent.width,parent.height)/8)
                Repeater {
                    model:64
                    Rectangle {
                        required property int index
                        readonly property string name:root.squareName(index)
                        width:grid.cell;height:grid.cell
                        color:root.missed===name?"#c9483a":((Math.floor(index/8)+index%8)%2===0?root.appearance.boardLight:root.appearance.boardDark)
                        Accessible.role:Accessible.Button;Accessible.name:root.running?qsTr("Case de l’échiquier"):name
                        // Hors série, les noms sont là pour se repérer avant de commencer.
                        Label {anchors.centerIn:parent;visible:!root.running;text:parent.name;opacity:.55;color:"#1d2125";font.pixelSize:Math.max(9,grid.cell*.22)}
                        MouseArea {anchors.fill:parent;onClicked:root.answer(parent.name)}
                    }
                }
            }
        }
        RowLayout {
            Layout.fillWidth:true;spacing:8
            TButton {objectName:"coordinateStart";text:root.running?qsTr("Arrêter"):qsTr("Commencer (30 s)");accent:!root.running;onClicked:root.running?root.stop():root.start()}
            TButton {text:root.blackSide?qsTr("Vu des Noirs"):qsTr("Vu des Blancs");enabled:!root.running;onClicked:root.blackSide=!root.blackSide}
            Item {Layout.fillWidth:true}
            Label {visible:root.best>0;text:qsTr("Meilleure série : %1").arg(root.best);color:root.appearance.muted}
        }
    }
}
