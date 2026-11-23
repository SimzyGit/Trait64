// SPDX-License-Identifier: AGPL-3.0-or-later
// Synchroniser entre appareils par un dossier que l'on synchronise soi-même (Syncthing,
// Nextcloud, Dropbox…) : le paquet y est chiffré par une phrase de passe.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs as Native

TDialog {
    id: root
    required property var backend
    readonly property var sync: backend.syncState
    property string folder: sync.folder || ""
    objectName: "syncDialog"
    title: qsTr("Synchroniser entre appareils")
    width: Math.min(620, parent.width - 32)
    anchors.centerIn: parent; modal: true; standardButtons: Dialog.Close
    onOpened: folder = sync.folder || ""
    ColumnLayout {
        anchors.fill: parent; spacing: 10
        Label {
            Layout.fillWidth: true; wrapMode: Text.WordWrap; color: palette.placeholderText
            text: qsTr("Choisissez un dossier que vous synchronisez déjà entre vos appareils (Syncthing, Nextcloud, Dropbox…). Trait64 y dépose un paquet chiffré : répertoires, études, profils, comptes (sans leurs jetons) et réglages. Aucun serveur Trait64 n’intervient ; sans la phrase de passe, le paquet est illisible.")
        }
        RowLayout {
            Layout.fillWidth: true; spacing: 8
            Label { Layout.fillWidth: true; elide: Text.ElideMiddle; text: root.folder || qsTr("Aucun dossier choisi") }
            TButton { text: qsTr("Choisir le dossier…"); onClicked: folderDialog.open() }
        }
        TTextField {
            id: passphrase; objectName: "syncPassphrase"; Layout.fillWidth: true
            echoMode: TextInput.Password
            placeholderText: root.sync.ready ? qsTr("Phrase de passe enregistrée (la retaper pour la changer)")
                                             : qsTr("Phrase de passe, huit caractères au moins")
        }
        TTextField {
            id: confirm; Layout.fillWidth: true; echoMode: TextInput.Password
            visible: passphrase.text.length > 0
            placeholderText: qsTr("La même, pour confirmer")
        }
        Label {
            Layout.fillWidth: true; wrapMode: Text.WordWrap; color: palette.placeholderText
            text: qsTr("La phrase de passe reste sur cet appareil ; tapez la même sur les autres. Perdue, elle ne se retrouve pas : le paquet devra être recréé.")
        }
        Flow {
            Layout.fillWidth: true; Layout.preferredWidth: 1; spacing: 8
            TCheckBox { id: startup; text: qsTr("Au démarrage"); checked: !!root.sync.atStartup }
            TButton {
                objectName: "saveSync"; text: qsTr("Enregistrer")
                enabled: root.folder.length > 0 && passphrase.text.length >= 8 && passphrase.text === confirm.text
                onClicked: { root.backend.configureSync(root.folder, passphrase.text, startup.checked); passphrase.text = ""; confirm.text = "" }
            }
            TButton {
                objectName: "syncNow"; accent: true
                text: root.sync.running ? qsTr("Synchronisation…") : qsTr("Synchroniser maintenant")
                enabled: !!root.sync.ready && !root.sync.running && !root.backend.busy
                onClicked: root.backend.syncNow()
            }
            TButton { text: qsTr("Oublier ce réglage"); flat: true; visible: !!root.sync.ready; onClicked: root.backend.forgetSync() }
        }
        Label {
            Layout.fillWidth: true; wrapMode: Text.WordWrap
            text: (root.sync.last > 0 ? qsTr("Dernière synchronisation : %1").arg(new Date(root.sync.last).toLocaleString(Qt.locale(controller.preferences.language === "auto" ? "" : controller.preferences.language), Locale.ShortFormat)) : "")
                  + (root.sync.status ? (root.sync.last > 0 ? " · " : "") + root.sync.status : "")
        }
    }
    Native.FolderDialog { id: folderDialog; title: qsTr("Dossier de synchronisation"); onAccepted: root.folder = selectedFolder.toString() }
}
