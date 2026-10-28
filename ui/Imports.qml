// SPDX-License-Identifier: AGPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs as Native

TDialog {
    id: root
    required property var backend
    property var task: backend.importProgress
    property double relocateId: 0
    title: qsTr("Imports PGN reprenables")
    width: Math.min(900, parent.width - 32)
    height: Math.min(700, parent.height - 32)
    anchors.centerIn: parent
    modal: true
    standardButtons: Dialog.Close
    function statusText(status) {
        return ({paused:qsTr("En pause"), running:qsTr("Interrompu ou actif dans une autre instance"), failed:qsTr("À reprendre après correction"), completed:qsTr("Terminé")})[status] || status
    }
    function size(bytes) { return (bytes / 1048576).toFixed(1) + " Mio" }
    ScrollView {id:importsViewport;anchors.fill:parent;contentWidth:availableWidth;clip:true;ScrollBar.horizontal.policy:ScrollBar.AlwaysOff
ColumnLayout {width:importsViewport.availableWidth;height:Math.max(importsViewport.availableHeight,implicitHeight);
         spacing: 12
        Label {
            Layout.fillWidth: true; wrapMode: Text.WordWrap
            text: qsTr("Les parties sont conservées par lots de 250. Après une pause ou un plantage, la reprise repart du dernier lot validé. Le fichier entier est vérifié par SHA-256 avant chaque reprise.")
        }
        Flow {Layout.fillWidth:true;spacing:8
TButton {objectName:"newResumableImport"; text:qsTr("Choisir un PGN"); enabled:!root.backend.busy; onClicked:{root.relocateId=0; choose.open()} }
            Label {text:qsTr("Lecteurs parallèles")}
            SpinBox {from:1;to:8;value:root.backend.preferences.importWorkers||2;enabled:!root.backend.busy;onValueModified:root.backend.setPreference("importWorkers",value)}
            Item {Layout.fillWidth: true}
            TButton {objectName:"pauseImport"; text:qsTr("Mettre en pause"); visible:root.backend.resumableImport; onClicked:root.backend.cancelTask()}
        }
        ColumnLayout {
            visible: root.backend.resumableImport; Layout.fillWidth:true
            Label {text:root.task.verifying ? qsTr("Vérification du fichier…") : qsTr("Import : ") + root.task.processed + qsTr(" parties lues · ") + root.task.committed + qsTr(" validées"); Layout.fillWidth:true; wrapMode:Text.WordWrap}
            ProgressBar {Layout.fillWidth:true; from:0; to:Math.max(1, root.task.total || 0); value:root.task.bytes || 0}
            Label {
                Layout.fillWidth:true; wrapMode:Text.WordWrap; font.pixelSize:Math.round(11*controller.preferences.fontScale*controller.preferences.uiScale)
                text:root.size(root.task.bytes || 0) + " / " + root.size(root.task.total || 0) +
                     (!root.task.verifying && root.task.rate > 0 ? " · " + root.size(root.task.rate) + qsTr("/s · environ ") + Math.ceil(root.task.remaining / 60) + qsTr(" min restantes") : "")
            }
        }
        ListView {
            id: jobs; objectName:"importJobsList"
            Layout.fillWidth:true; Layout.fillHeight:true; Layout.minimumHeight:180; clip:true; spacing:10
            model:root.backend.importJobs
            ScrollBar.vertical:ScrollBar {}
            delegate:Rectangle {
                required property var modelData
                width:jobs.width; height:body.implicitHeight+24; color:root.palette.alternateBase
                ColumnLayout {
                    id:body; anchors.left:parent.left; anchors.right:parent.right; anchors.top:parent.top; anchors.margins:12; spacing:6
                    Label {text:modelData.title || modelData.file; font.bold:true; Layout.fillWidth:true; elide:Text.ElideMiddle}
                    Label {text:modelData.file; Layout.fillWidth:true; elide:Text.ElideMiddle; font.pixelSize:Math.round(11*controller.preferences.fontScale*controller.preferences.uiScale)}
                    Label {text:root.statusText(modelData.status)+" · "+modelData.processed+qsTr(" parties validées · ")+modelData.added+qsTr(" nouvelles"); Layout.fillWidth:true; wrapMode:Text.WordWrap}
                    Label {text:modelData.error; visible:text.length>0; Layout.fillWidth:true; wrapMode:Text.WordWrap}
                    Flow {Layout.fillWidth:true;spacing:8
TButton {text:qsTr("Reprendre"); visible:modelData.status!=="completed"; enabled:!root.backend.busy; onClicked:root.backend.resumeImport(modelData.id)}
                        TButton {text:qsTr("Retrouver le PGN"); visible:modelData.status!=="completed"; enabled:!root.backend.busy; onClicked:{root.relocateId=modelData.id; choose.open()}}
                        TButton {text:qsTr("Retirer de cette liste"); enabled:!root.backend.busy; onClicked:root.backend.forgetImport(modelData.id)}
                    }
                }
            }
            Label {anchors.centerIn:parent; visible:jobs.count===0; text:qsTr("Aucun import enregistré.")}
        }
        Label {text:qsTr("Retirer un import de cette liste conserve toutes ses parties dans la bibliothèque. L’import classique reste disponible pour une annulation intégrale."); Layout.fillWidth:true; wrapMode:Text.WordWrap; font.pixelSize:Math.round(11*controller.preferences.fontScale*controller.preferences.uiScale)}
    }
}
    Native.FileDialog {
        id:choose; title:root.relocateId ? qsTr("Retrouver le PGN original") : qsTr("Importer un PGN par lots")
        nameFilters:["PGN (*.pgn *.PGN)"]
        onAccepted:root.relocateId ? root.backend.resumeImport(root.relocateId, selectedFile.toString()) : root.backend.importResumable(selectedFile.toString())
    }
}
