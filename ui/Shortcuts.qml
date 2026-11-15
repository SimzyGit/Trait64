// SPDX-License-Identifier: AGPL-3.0-or-later
// Raccourcis clavier : on clique « Changer », on presse la combinaison voulue. Une
// combinaison déjà prise est refusée, Échap renonce, « Par défaut » rend celle d'origine.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
TDialog {
    id:root
    property var backend
    property string listening:""
    property string problem:""
    title:qsTr("Raccourcis clavier")
    anchors.centerIn:parent;modal:true;standardButtons:Dialog.Close
    width:Math.min(620,parent.width-32);height:Math.min(720,parent.height-32)
    readonly property var actions:[
        {id:"back",label:qsTr("Coup précédent")},{id:"forward",label:qsTr("Coup suivant")},
        {id:"start",label:qsTr("Début de la partie")},{id:"end",label:qsTr("Fin de la variante")},
        {id:"previousVariation",label:qsTr("Variante précédente")},{id:"nextVariation",label:qsTr("Variante suivante")},
        {id:"previousGame",label:qsTr("Partie précédente de la liste")},{id:"nextGame",label:qsTr("Partie suivante de la liste")},
        {id:"flip",label:qsTr("Retourner l’échiquier")},{id:"threat",label:qsTr("Montrer la menace")},
        {id:"autoplay",label:qsTr("Lecture automatique")},{id:"deleteMove",label:qsTr("Supprimer le coup et sa suite")},
        {id:"undo",label:qsTr("Défaire le dernier changement")},{id:"redo",label:qsTr("Rétablir")},
        {id:"newGame",label:qsTr("Nouvelle partie")},{id:"open",label:qsTr("Importer")},
        {id:"save",label:qsTr("Enregistrer")},{id:"commands",label:qsTr("Palette de commandes")}
    ]
    function assign(action,sequence){problem=backend.setShortcut(action,sequence);listening=""}
    onClosed:listening=""
    ColumnLayout {
        anchors.fill:parent;spacing:8
        Label {Layout.fillWidth:true;wrapMode:Text.WordWrap;text:qsTr("Cliquez « Changer » puis pressez la combinaison voulue. Échap renonce.")}
        Label {objectName:"shortcutProblem";Layout.fillWidth:true;wrapMode:Text.WordWrap;visible:root.problem!=="";text:root.problem;color:"#c9483a"}
        ListView {
            id:list;Layout.fillWidth:true;Layout.fillHeight:true;clip:true;spacing:4;model:root.actions
            ScrollBar.vertical:ScrollBar {}
            delegate:RowLayout {
                required property var modelData
                width:list.width;spacing:8
                Label {text:modelData.label;Layout.fillWidth:true;elide:Text.ElideRight}
                Label {
                    objectName:"shortcut_"+modelData.id;Layout.preferredWidth:150;horizontalAlignment:Text.AlignHCenter
                    text:root.listening===modelData.id?qsTr("Pressez une touche…"):(root.backend.preferences.shortcuts[modelData.id]||"")
                    font.bold:root.listening===modelData.id
                }
                TButton {text:qsTr("Changer");onClicked:{root.problem="";root.listening=modelData.id;catcher.forceActiveFocus()}}
                TButton {text:qsTr("Par défaut");onClicked:root.assign(modelData.id,"")}
            }
        }
        TButton {text:qsTr("Tout remettre par défaut");onClicked:{root.backend.resetShortcuts();root.problem=""}}
        // Capte la combinaison suivante, avant que les raccourcis de la fenêtre ne la prennent.
        Item {
            id:catcher;objectName:"shortcutCatcher";focus:root.listening!=="";Layout.preferredHeight:0
            Keys.onPressed:function(event){
                if(root.listening===""){return}
                event.accepted=true
                if(event.key===Qt.Key_Escape){root.listening="";return}
                const name=root.backend.keyName(event.key,event.modifiers)
                if(name!=="")root.assign(root.listening,name)
            }
            Keys.onShortcutOverride:function(event){event.accepted=root.listening!==""}
        }
    }
}
