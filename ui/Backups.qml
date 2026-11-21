// SPDX-License-Identifier: AGPL-3.0-or-later
// Restaurer une sauvegarde : la liste de celles que Trait64 a faites, la plus récente en
// haut, avec leur nombre de parties. La bibliothèque remplacée est gardée à son tour.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
TDialog {
    id:root
    property var backend
    property var rows:[]
    property var chosen:({})
    title:qsTr("Restaurer une sauvegarde")
    anchors.centerIn:parent;modal:true;standardButtons:Dialog.Close
    width:Math.min(680,parent.width-32);height:Math.min(620,parent.height-32)
    function refresh(){rows=backend.backups()}
    function kindText(kind){return kind==="auto"?qsTr("Automatique"):kind==="update"?qsTr("Avant une mise à jour"):kind==="restore"?qsTr("Avant une restauration"):qsTr("Avant une migration")}
    function sizeText(bytes){return bytes>=1048576?qsTr("%1 Mo").arg((bytes/1048576).toFixed(1)):qsTr("%1 Ko").arg(Math.max(1,Math.round(bytes/1024)))}
    onOpened:refresh()
    ColumnLayout {
        anchors.fill:parent;spacing:10
        Label {Layout.fillWidth:true;wrapMode:Text.WordWrap;text:qsTr("Trait64 sauvegarde la bibliothèque toutes les six heures (les quatorze dernières sont gardées), avant chaque mise à jour et avant chaque restauration. Restaurer remplace la bibliothèque actuelle, qui est d’abord sauvegardée à son tour.")}
        RowLayout {
            TButton {objectName:"backupNow";text:qsTr("Sauvegarder maintenant");enabled:!root.backend.busy;onClicked:{root.backend.automaticBackup(true);refreshLater.start()}}
            Label {Layout.fillWidth:true;color:root.palette.placeholderText;text:root.backend.preferences.lastBackup?qsTr("Dernière sauvegarde automatique : %1").arg(new Date(root.backend.preferences.lastBackup).toLocaleString(Qt.locale(controller.preferences.language==="auto"?"":controller.preferences.language),Locale.ShortFormat)):""}
        }
        Timer {id:refreshLater;interval:1500;onTriggered:root.refresh()}
        ListView {
            id:list;objectName:"backupList";Layout.fillWidth:true;Layout.fillHeight:true;clip:true;spacing:6;model:root.rows
            ScrollBar.vertical:ScrollBar {}
            delegate:Rectangle {
                required property var modelData;required property int index
                width:list.width;height:58;color:root.palette.alternateBase
                RowLayout {anchors.fill:parent;anchors.margins:10
                    ColumnLayout {Layout.fillWidth:true;spacing:2
                        Label {text:modelData.date+" · "+root.kindText(modelData.kind);font.bold:true}
                        Label {opacity:.7;text:(modelData.games>=0?qsTr("%1 partie(s)").arg(modelData.games):qsTr("illisible"))+" · "+root.sizeText(modelData.size)}
                    }
                    TButton {objectName:"restore"+index;text:qsTr("Restaurer");enabled:modelData.games>=0&&!root.backend.busy;onClicked:{root.chosen=modelData;confirm.open()}}
                }
            }
            Label {anchors.centerIn:parent;visible:list.count===0;text:qsTr("Aucune sauvegarde pour l’instant.")}
        }
    }
    TDialog {
        id:confirm;objectName:"restoreConfirm";title:qsTr("Restaurer cette sauvegarde ?");anchors.centerIn:parent;modal:true
        standardButtons:Dialog.Ok|Dialog.Cancel;width:Math.min(520,root.width-40)
        // Largeur fixée par le dialogue : un texte qui s'enroule sur sa propre largeur
        // ferait boucler le calcul de taille.
        ColumnLayout {anchors.fill:parent
        Label {Layout.fillWidth:true;wrapMode:Text.WordWrap;text:qsTr("La bibliothèque sera remplacée par celle du %1 (%2 partie(s)). La bibliothèque actuelle est sauvegardée d’abord et restera dans cette liste.").arg(root.chosen.date||"").arg(root.chosen.games||0)}
        }
        onAccepted:{root.backend.restoreBackup(root.chosen.file);root.refresh()}
    }
}
