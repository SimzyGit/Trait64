// SPDX-License-Identifier: AGPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs as Native

Dialog {
    id: root
    required property var backend
    property var task: backend.importProgress
    property double relocateId: 0
    title: "Imports PGN reprenables"
    width: Math.min(900, parent.width - 32)
    height: Math.min(700, parent.height - 32)
    anchors.centerIn: parent
    modal: true
    standardButtons: Dialog.Close
    function statusText(status) {
        return ({paused:"En pause", running:"Interrompu ou actif dans une autre instance", failed:"À reprendre après correction", completed:"Terminé"})[status] || status
    }
    function size(bytes) { return (bytes / 1048576).toFixed(1) + " Mio" }
    ColumnLayout {
        anchors.fill: parent; spacing: 12
        Label {
            Layout.fillWidth: true; wrapMode: Text.WordWrap
            text: "Les parties sont conservées par lots de 250. Après une pause ou un plantage, la reprise repart du dernier lot validé. Le fichier entier est vérifié par SHA-256 avant chaque reprise."
        }
        RowLayout {
            TButton {objectName:"newResumableImport"; text:"Choisir un PGN"; enabled:!root.backend.busy; onClicked:{root.relocateId=0; choose.open()} }
            Label {text:"Lecteurs parallèles"}
            SpinBox {from:1;to:8;value:root.backend.preferences.importWorkers||2;enabled:!root.backend.busy;onValueModified:root.backend.setPreference("importWorkers",value)}
            Item {Layout.fillWidth: true}
            TButton {objectName:"pauseImport"; text:"Mettre en pause"; visible:root.backend.resumableImport; onClicked:root.backend.cancelTask()}
        }
        ColumnLayout {
            visible: root.backend.resumableImport; Layout.fillWidth:true
            Label {text:root.task.verifying ? "Vérification du fichier…" : "Import : " + root.task.processed + " parties lues · " + root.task.committed + " validées"; Layout.fillWidth:true; wrapMode:Text.WordWrap}
            ProgressBar {Layout.fillWidth:true; from:0; to:Math.max(1, root.task.total || 0); value:root.task.bytes || 0}
            Label {
                Layout.fillWidth:true; wrapMode:Text.WordWrap; font.pixelSize:11
                text:root.size(root.task.bytes || 0) + " / " + root.size(root.task.total || 0) +
                     (!root.task.verifying && root.task.rate > 0 ? " · " + root.size(root.task.rate) + "/s · environ " + Math.ceil(root.task.remaining / 60) + " min restantes" : "")
            }
        }
        ListView {
            id: jobs; objectName:"importJobsList"
            Layout.fillWidth:true; Layout.fillHeight:true; clip:true; spacing:10
            model:root.backend.importJobs
            ScrollBar.vertical:ScrollBar {}
            delegate:Rectangle {
                required property var modelData
                width:jobs.width; height:body.implicitHeight+24; color:root.palette.alternateBase
                ColumnLayout {
                    id:body; anchors.left:parent.left; anchors.right:parent.right; anchors.top:parent.top; anchors.margins:12; spacing:6
                    Label {text:modelData.title || modelData.file; font.bold:true; Layout.fillWidth:true; elide:Text.ElideMiddle}
                    Label {text:modelData.file; Layout.fillWidth:true; elide:Text.ElideMiddle; font.pixelSize:11}
                    Label {text:root.statusText(modelData.status)+" · "+modelData.processed+" parties validées · "+modelData.added+" nouvelles"; Layout.fillWidth:true; wrapMode:Text.WordWrap}
                    Label {text:modelData.error; visible:text.length>0; Layout.fillWidth:true; wrapMode:Text.WordWrap}
                    RowLayout {
                        TButton {text:"Reprendre"; visible:modelData.status!=="completed"; enabled:!root.backend.busy; onClicked:root.backend.resumeImport(modelData.id)}
                        TButton {text:"Retrouver le PGN"; visible:modelData.status!=="completed"; enabled:!root.backend.busy; onClicked:{root.relocateId=modelData.id; choose.open()}}
                        TButton {text:"Retirer de cette liste"; enabled:!root.backend.busy; onClicked:root.backend.forgetImport(modelData.id)}
                    }
                }
            }
            Label {anchors.centerIn:parent; visible:jobs.count===0; text:"Aucun import enregistré."}
        }
        Label {text:"Retirer un import de cette liste conserve toutes ses parties dans la bibliothèque. L’import classique reste disponible pour une annulation intégrale."; Layout.fillWidth:true; wrapMode:Text.WordWrap; font.pixelSize:11}
    }
    Native.FileDialog {
        id:choose; title:root.relocateId ? "Retrouver le PGN original" : "Importer un PGN par lots"
        nameFilters:["PGN (*.pgn *.PGN)"]
        onAccepted:root.relocateId ? root.backend.resumeImport(root.relocateId, selectedFile.toString()) : root.backend.importResumable(selectedFile.toString())
    }
}
