// SPDX-License-Identifier: AGPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: root
    required property var backend
    property var manager: backend.content
    signal configureEngines()
    title: "Moteurs et bases de parties"
    width: Math.min(980, parent.width - 32)
    height: Math.min(780, parent.height - 32)
    anchors.centerIn: parent
    modal: true
    standardButtons: Dialog.Close
    property string removeId: ""
    property string removeTitle: ""
    function sizeText(bytes) { return bytes > 0 ? (bytes / 1048576).toFixed(1) + " Mio" : "Taille indiquée au téléchargement" }
    function imported(id) {
        var bases = root.backend.sources
        for (var i = 0; i < bases.length; ++i) if (bases[i].slug === id) return bases[i].games
        return -1
    }
    ColumnLayout {
        anchors.fill: parent
        spacing: 10
        RowLayout {
            Layout.fillWidth: true
            ComboBox { id: category; model: ["Moteurs", "Champions du monde", "Championnes du monde", "Histoire", "Ouvertures", "Historique des imports"]; Layout.preferredWidth: 245 }
            TextField { id: search; Layout.fillWidth: true; placeholderText: "Chercher dans le catalogue…"; visible: category.currentIndex < 5 }
            TButton {text: "Profils moteurs"; onClicked: {root.close(); root.configureEngines()} }
        }
        Label {
            Layout.fillWidth: true; wrapMode: Text.WordWrap; font.pixelSize: 11
            text: "Téléchargements à la demande. Les moteurs sont vérifiés avant installation et ne démarrent pas automatiquement. Les bases importées sont utilisables hors ligne."
        }
        ListView {
            id: list
            Layout.fillWidth: true; Layout.fillHeight: true
            clip: true; spacing: 8; visible: category.currentIndex < 5
            model: root.manager.catalog.filter(function(item) { return item.category === category.currentText && (item.title + " " + item.description).toLowerCase().indexOf(search.text.toLowerCase()) >= 0 })
            ScrollBar.vertical: ScrollBar {}
            delegate: Rectangle {
                required property var modelData
                width: list.width; height: column.implicitHeight + 24
                color: root.palette.alternateBase; radius: 2
                ColumnLayout {
                    id: column; anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top; anchors.margins: 12
                    spacing: 6
                    RowLayout {
                        Layout.fillWidth: true
                        Label { text: modelData.title + (modelData.kind === "engine" ? " " + modelData.version : ""); font.bold: true; Layout.fillWidth: true }
                        Label {text: root.sizeText(modelData.bytes || modelData.downloadedBytes); font.pixelSize: 11}
                    }
                    Label {text: modelData.description; wrapMode: Text.WordWrap; Layout.fillWidth: true; font.pixelSize: 11}
                    Label {text: modelData.license; wrapMode: Text.WordWrap; Layout.fillWidth: true; font.pixelSize: 11; opacity: .75}
                    Label {
                        Layout.fillWidth: true; wrapMode: Text.WordWrap; font.pixelSize: 11
                        text: modelData.kind === "engine" ? (modelData.compatible ? "SHA-256 épinglé au catalogue · " + (modelData.installed ? "Version installée : " + modelData.installedVersion : "Non installé") : "Binaire incompatible avec ce système ; glibc ≥ " + modelData.minGlibc + " requise.")
                              : (root.imported(modelData.id) >= 0 ? root.imported(modelData.id) + " parties dans cette base locale." : "Pas encore importée.") + " Empreinte enregistrée à réception ; le fournisseur ne publie pas de SHA-256."
                    }
                    Flow {
                        Layout.fillWidth: true; spacing: 8
                        TButton {
                            text: modelData.installed ? (modelData.update ? "Mettre à jour" : modelData.kind === "engine" ? "Installé" : "Actualiser la base") : "Télécharger"
                            enabled: !root.manager.busy && !root.backend.busy && modelData.compatible && (!modelData.installed || modelData.update || modelData.kind !== "engine")
                            onClicked: root.manager.install(modelData.id)
                        }
                        TButton {
                            text: modelData.kind === "engine" ? "Ajouter un profil" : "Importer le PGN local"
                            visible: modelData.installed
                            enabled: !root.manager.busy && !root.backend.busy
                            onClicked: root.manager.useInstalled(modelData.id)
                        }
                        TButton {text: "Source et conditions"; onClicked: Qt.openUrlExternally(modelData.source)}
                        TButton {
                            text: modelData.kind === "engine" ? "Supprimer le moteur" : "Effacer le PGN téléchargé"
                            visible: modelData.installed
                            enabled: !root.manager.busy && !root.backend.busy
                            onClicked: {root.removeId = modelData.id; root.removeTitle = modelData.title; confirmRemove.open()}
                        }
                    }
                }
            }
            Label {anchors.centerIn: parent; visible: list.count === 0; text: "Aucun contenu correspondant."}
        }
        ListView {
            id: history
            Layout.fillWidth: true; Layout.fillHeight: true; visible: category.currentIndex === 5
            clip: true; spacing: 10; model: root.backend.importHistory
            ScrollBar.vertical: ScrollBar {}
            delegate: ColumnLayout {
                required property var modelData
                width: history.width
                Label {text: (modelData.title || "Import PGN local") + " · " + modelData.date; font.bold: true}
                Label {text: modelData.processed + " parties lues, " + modelData.added + " nouvelles."; font.pixelSize: 11}
                Label {text: modelData.file; Layout.fillWidth: true; wrapMode: Text.WrapAnywhere; font.pixelSize: 10}
                Label {text: modelData.sha256 ? "SHA-256 : " + modelData.sha256 : "Empreinte non enregistrée pour cet import local."; Layout.fillWidth: true; wrapMode: Text.WrapAnywhere; font.pixelSize: 10}
            }
            Label {anchors.centerIn: parent; visible: history.count === 0; text: "Aucun import PGN enregistré depuis cette version."}
        }
        ProgressBar {Layout.fillWidth: true; visible: root.manager.busy; from: 0; to: Math.max(1,root.manager.total); value: root.manager.received; indeterminate: root.manager.total <= 0 || root.manager.received >= root.manager.total}
        RowLayout {
            Layout.fillWidth: true
            Label {text: root.backend.busy ? "Import : " + root.backend.taskProgress + " parties traitées…" : root.manager.status; Layout.fillWidth: true; wrapMode: Text.WordWrap; font.pixelSize: 11}
            TButton {text: "Annuler"; visible: root.manager.busy || root.backend.busy; onClicked: root.manager.busy ? root.manager.cancel() : root.backend.cancelTask()}
        }
    }
    Dialog {
        id: confirmRemove; anchors.centerIn: parent; title: "Supprimer " + root.removeTitle + " ?"
        width: Math.min(560, root.width-40); height: 205; modal: true; standardButtons: Dialog.Yes | Dialog.No
        contentItem: Label {wrapMode: Text.WordWrap; text: "Les fichiers téléchargés seront supprimés. Les parties importées et les réglages des profils seront conservés. Un profil dont le moteur est supprimé nécessitera un nouvel exécutable."}
        onAccepted: root.manager.remove(root.removeId)
    }
}
