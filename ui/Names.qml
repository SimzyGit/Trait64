// SPDX-License-Identifier: AGPL-3.0-or-later
// Corriger les noms, comme le fichier d'orthographe de SCID ou la correction automatique
// de ChessBase : les écritures d'un même joueur ou d'un même tournoi sont regroupées, on
// choisit celle à garder (ou on la tape), et les autres sont réécrites partout.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
TDialog {
    id:root
    property var backend
    readonly property var state:backend.nameVariants
    readonly property string field:fieldBox.currentIndex===1?"event":"player"
    title:qsTr("Corriger les noms")
    anchors.centerIn:parent;modal:true;standardButtons:Dialog.Close
    width:Math.min(720,parent.width-32);height:Math.min(680,parent.height-32)
    onOpened:backend.findNameVariants(field)
    ColumnLayout {
        anchors.fill:parent;spacing:8
        RowLayout {Layout.fillWidth:true
            TComboBox {id:fieldBox;objectName:"namesField";model:[qsTr("Joueurs"),qsTr("Tournois")];onActivated:root.backend.findNameVariants(root.field)}
            Label {Layout.fillWidth:true;wrapMode:Text.WordWrap;opacity:.75
                text:root.state.running?qsTr("Recherche des écritures voisines…")
                    :root.state.error?root.state.error
                    :(root.state.groups||[]).length?qsTr("%1 nom(s) écrits de plusieurs façons. Choisissez l’écriture à garder : les autres seront réécrites dans les parties et leur PGN.").arg(root.state.groups.length)
                    :qsTr("Aucun nom écrit de plusieurs façons.")}
        }
        ListView {
            id:list;objectName:"nameGroups";Layout.fillWidth:true;Layout.fillHeight:true;clip:true;spacing:8
            model:root.state.running?[]:(root.state.groups||[])
            ScrollBar.vertical:ScrollBar {}
            delegate:Rectangle {
                id:group
                required property var modelData;required property int index
                property string kept:modelData.names[0].name
                width:list.width-12;height:content.implicitHeight+16;radius:4;color:root.palette.alternateBase
                ColumnLayout {
                    id:content;anchors.fill:parent;anchors.margins:8;spacing:2
                    ButtonGroup {id:choice}
                    Repeater {
                        model:group.modelData.names
                        RadioButton {
                            required property var modelData;required property int index
                            ButtonGroup.group:choice
                            checked:index===0
                            text:qsTr("%1 · %2 partie(s)").arg(modelData.name).arg(modelData.games)
                            onToggled:if(checked){group.kept=modelData.name;keptField.text=modelData.name}
                        }
                    }
                    RowLayout {Layout.fillWidth:true
                        TTextField {id:keptField;Layout.fillWidth:true;text:group.kept;selectByMouse:true;Accessible.name:qsTr("Écriture à garder")}
                        TButton {objectName:"unify"+group.index;text:qsTr("Unifier");accent:true;enabled:keptField.text.trim().length>0
                            onClicked:root.backend.unifyNames(root.field,group.modelData.names.map(function(n){return n.name}).filter(function(n){return n!==keptField.text.trim()}),keptField.text)}
                    }
                }
            }
        }
    }
}
