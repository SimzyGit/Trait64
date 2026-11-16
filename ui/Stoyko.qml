// SPDX-License-Identifier: AGPL-3.0-or-later
// L'exercice de Stoyko, vérifié : chaque variante écrite depuis la position, avec
// l'évaluation du moteur à son terme, et en tête ce que le moteur joue lui-même.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
TDialog {
    id:root
    property var backend
    readonly property var st:backend.stoyko||({})
    readonly property var evals:st.evals||({})
    readonly property var start:evals[st.start]||null
    title:qsTr("Vérifier mes variantes")
    anchors.centerIn:parent;modal:true;standardButtons:Dialog.Close
    width:Math.min(720,parent.width-32);height:Math.min(640,parent.height-32)
    onOpened:backend.startStoyko()
    onClosed:backend.stopStoyko()
    // Une évaluation vue du camp au trait au départ : positive, c'est bon pour lui.
    readonly property int side:(st.start||"").split(" ")[1]==="b"?-1:1
    function tone(e){if(!e||!root.start)return root.palette.text
        var gap=root.side*(Number(e.score)-Number(root.start.score))
        return gap>=-0.3?"#2e9e5b":gap>=-1?"#c79a1e":"#e0473b"}
    ColumnLayout {
        anchors.fill:parent;spacing:10
        Label {Layout.fillWidth:true;wrapMode:Text.WordWrap;opacity:.75
            text:qsTr("Écrivez d’abord toutes les variantes que vous calculez depuis la position, sans moteur. Le moteur juge ensuite la fin de chacune : vert si elle tient l’évaluation de départ, orange à moins d’un pion, rouge au-delà.")}
        Label {objectName:"stoykoStart";Layout.fillWidth:true;wrapMode:Text.WordWrap;font.weight:Font.DemiBold
            text:root.st.error?root.backend.uiText(root.st.error)
                :root.start?qsTr("Départ : %1 — le moteur joue %2").arg(root.start.eval).arg(root.start.best||"—")
                :qsTr("Le moteur évalue la position de départ…")}
        ListView {
            id:list;objectName:"stoykoLines";Layout.fillWidth:true;Layout.fillHeight:true;clip:true;spacing:4
            model:root.st.lines||[]
            ScrollBar.vertical:ScrollBar {}
            delegate:RowLayout {
                required property var modelData
                readonly property var e:root.evals[modelData.fen]||null
                width:list.width-12;spacing:12
                Label {text:modelData.text;Layout.fillWidth:true;wrapMode:Text.WordWrap}
                Label {text:e?e.eval:"…";color:root.tone(e);font.weight:Font.DemiBold;Layout.preferredWidth:64;horizontalAlignment:Text.AlignRight}
            }
        }
        RowLayout {Layout.fillWidth:true
            BusyIndicator {running:!!root.st.running;visible:running;Layout.preferredHeight:24;Layout.preferredWidth:24}
            Label {Layout.fillWidth:true;opacity:.7;text:root.st.running?qsTr("Analyse en cours…"):""}
            TButton {text:qsTr("Relancer");enabled:!root.st.running;onClicked:root.backend.startStoyko()}
        }
    }
}
