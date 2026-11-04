// SPDX-License-Identifier: AGPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

TDialog {
    id: root
    required property var backend
    title: qsTr("Comptes Lichess et Chess.com")
    width: Math.min(760, parent.width - 32)
    height: Math.min(600, parent.height - 32)
    anchors.centerIn: parent
    modal: true
    standardButtons: Dialog.Close
    function since(ms) {
        if (!ms) return qsTr("jamais synchronisé")
        const minutes = Math.round((Date.now() - ms) / 60000)
        if (minutes < 1) return qsTr("synchronisé à l’instant")
        if (minutes < 60) return qsTr("synchronisé il y a %1 min").arg(minutes)
        if (minutes < 1440) return qsTr("synchronisé il y a %1 h").arg(Math.round(minutes / 60))
        return qsTr("synchronisé le %1").arg(new Date(ms).toLocaleDateString(Qt.locale(), Locale.ShortFormat))
    }
    ScrollView {
        id: viewport; anchors.fill: parent; contentWidth: availableWidth; clip: true
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
        ColumnLayout {
            width: viewport.availableWidth; spacing: 12
            Label {
                Layout.fillWidth: true; wrapMode: Text.WordWrap
                text: qsTr("Vos parties publiques arrivent dans la bibliothèque, rangées par compte. Chaque synchronisation ne récupère que les parties jouées depuis la précédente. Aucun mot de passe n’est demandé.")
            }
            ReviewImported { Layout.fillWidth: true; backend: root.backend }
            Profiles { Layout.fillWidth: true; backend: root.backend }
            Rectangle { Layout.fillWidth: true; height: 1; opacity: .4; color: palette.mid }
            Repeater {
                model: root.backend.onlineAccounts
                delegate: Rectangle {
                    required property var modelData
                    required property int index
                    Layout.fillWidth: true; implicitHeight: row.implicitHeight + 16
                    color: palette.base; border.color: palette.mid; radius: 3
                    RowLayout {
                        id: row; anchors.fill: parent; anchors.margins: 8; spacing: 10
                        Label { text: modelData.site === "lichess" ? "Lichess" : "Chess.com"; font.bold: true; Layout.preferredWidth: 80 }
                        ColumnLayout {
                            Layout.fillWidth: true; spacing: 2
                            Label { text: modelData.user; font.pixelSize: 15; elide: Text.ElideRight; Layout.fillWidth: true }
                            Label {
                                text: modelData.syncing ? qsTr("synchronisation…") : root.since(modelData.lastSync) + " · " + qsTr("%1 partie(s) importée(s)").arg(modelData.games || 0)
                                color: palette.placeholderText; Layout.fillWidth: true; elide: Text.ElideRight
                            }
                        }
                        BusyIndicator { visible: !!modelData.syncing; running: visible; implicitWidth: 24; implicitHeight: 24 }
                        TComboBox {
                            objectName: "accountProfile" + index
                            visible: root.backend.profiles.length > 0
                            Layout.preferredWidth: 150
                            // La première entrée détache le compte ; les suivantes sont les profils.
                            model: [qsTr("Sans profil")].concat(root.backend.profiles.map(p => p.name))
                            currentIndex: {
                                const names = root.backend.profiles.map(p => p.name)
                                const found = names.indexOf(modelData.profile || "")
                                return found < 0 ? 0 : found + 1
                            }
                            onActivated: function(chosen) {
                                const names = root.backend.profiles.map(p => p.name)
                                root.backend.setAccountProfile(index, chosen === 0 ? "" : names[chosen - 1])
                            }
                        }
                        TButton { objectName: "syncAccount" + index; text: qsTr("Synchroniser"); enabled: !root.backend.busy; onClicked: root.backend.syncOnlineAccount(index) }
                        TButton { text: "×"; Accessible.name: qsTr("Retirer ce compte"); enabled: !modelData.syncing; onClicked: root.backend.removeOnlineAccount(index)
                            ToolTip.visible: hovered; ToolTip.text: qsTr("Retire le compte ; les parties déjà importées restent.") }
                    }
                }
            }
            Label {
                visible: root.backend.onlineAccounts.length === 0; color: palette.placeholderText
                text: qsTr("Aucun compte pour l’instant : ajoutez le vôtre ci-dessous.")
            }
            RowLayout {
                Layout.fillWidth: true; spacing: 8
                TComboBox { id: site; objectName: "accountSite"; model: ["Lichess", "Chess.com"] }
                TextField {
                    id: user; objectName: "accountUser"; Layout.fillWidth: true; placeholderText: qsTr("Pseudo"); selectByMouse: true
                    onAccepted: if (add.enabled) add.clicked()
                }
                TButton {
                    id: add; objectName: "addAccount"; text: qsTr("Ajouter et synchroniser"); accent: true
                    enabled: user.text.trim().length > 1 && !root.backend.busy
                    onClicked: { root.backend.addOnlineAccount(site.currentIndex === 0 ? "lichess" : "chesscom", user.text); user.text = "" }
                }
            }
            RowLayout {
                Layout.fillWidth: true
                CheckBox {
                    objectName: "syncAtStartup"; text: qsTr("Synchroniser au démarrage")
                    checked: !!root.backend.preferences.syncAtStartup
                    onToggled: root.backend.setPreference("syncAtStartup", checked)
                }
                Item { Layout.fillWidth: true }
                TButton { text: qsTr("Tout synchroniser"); enabled: root.backend.onlineAccounts.length > 0 && !root.backend.busy; onClicked: root.backend.syncAllAccounts() }
            }
        }
    }
}
