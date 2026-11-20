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
            // Lichess : se connecter dans le navigateur comme dans En Croissant, ou coller
            // un jeton et le lier à un compte. Le jeton ouvre le livre Lichess et accélère
            // le téléchargement des parties du compte ; il reste sur cette machine.
            Rectangle {
                Layout.fillWidth: true; implicitHeight: tokenBox.implicitHeight + 20
                color: palette.base; border.color: palette.mid; radius: 6
                ColumnLayout {
                    id: tokenBox; anchors.fill: parent; anchors.margins: 10; spacing: 8
                    // Relus quand la liste des comptes change, où le jeton s'annonce aussi.
                    readonly property bool waiting: (root.backend.onlineAccounts, root.backend.lichessLoginRunning())
                    readonly property var lichessAccounts: root.backend.onlineAccounts
                        .map((account, at) => ({user: account.user, at: at, site: account.site}))
                        .filter(account => account.site === "lichess")
                    Label { text: qsTr("Connexion à Lichess"); font.bold: true }
                    Label {
                        Layout.fillWidth: true; wrapMode: Text.WordWrap; color: palette.placeholderText
                        text: (root.backend.onlineAccounts, root.backend.hasLichessToken())
                              ? qsTr("Connecté : le livre Lichess est accessible et les parties de vos comptes connectés se téléchargent plus vite.")
                              : qsTr("Le livre Lichess demande un compte. Connectez-vous dans le navigateur : Trait64 reçoit un jeton sans aucune autorisation, et l’associe à votre compte.")
                    }
                    Flow {
                        Layout.fillWidth: true; Layout.preferredWidth: 1; spacing: 8
                        TButton {
                            objectName: "connectLichess"; accent: !tokenBox.waiting
                            text: tokenBox.waiting ? qsTr("En attente du navigateur… (annuler)") : qsTr("Se connecter avec Lichess")
                            onClicked: tokenBox.waiting ? root.backend.cancelLichessLogin() : root.backend.connectLichess()
                        }
                    }
                    Label { text: qsTr("Ou collez un jeton créé sur Lichess, et choisissez le compte auquel le lier :"); wrapMode: Text.WordWrap; Layout.fillWidth: true; color: palette.placeholderText }
                    Flow {
                        Layout.fillWidth: true; Layout.preferredWidth: 1; spacing: 8
                        TComboBox {
                            id: tokenAccount; objectName: "tokenAccount"; width: 200; translateOptions: false
                            // Sans compte : le jeton ne sert qu'au livre.
                            model: [qsTr("Sans compte (livre seulement)")].concat(tokenBox.lichessAccounts.map(a => a.user))
                            currentIndex: tokenBox.lichessAccounts.length > 0 ? 1 : 0
                        }
                        TTextField {
                            id: tokenField; objectName: "lichessToken"; width: 260
                            echoMode: TextInput.Password; placeholderText: qsTr("Jeton Lichess (lip_…)")
                            onAccepted: linkToken.clicked()
                        }
                        TButton {
                            id: linkToken; text: qsTr("Lier le jeton"); enabled: tokenField.text.length > 0
                            onClicked: {
                                if (tokenAccount.currentIndex === 0)
                                    root.backend.setLichessToken(tokenField.text)
                                else
                                    root.backend.setAccountToken(tokenBox.lichessAccounts[tokenAccount.currentIndex - 1].at, tokenField.text)
                                tokenField.text = ""
                            }
                        }
                        TButton { text: qsTr("Créer un jeton sur Lichess")
                                  onClicked: Qt.openUrlExternally("https://lichess.org/account/oauth/token/create?description=Trait64") }
                    }
                }
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
                        Label {
                            visible: modelData.site === "lichess"; color: modelData.connected ? root.backend.preferences.accentColor : palette.placeholderText
                            text: modelData.connected ? qsTr("● connecté") : qsTr("non connecté")
                        }
                        TButton { visible: modelData.site === "lichess" && !!modelData.connected; text: qsTr("Déconnecter")
                                  onClicked: root.backend.setAccountToken(index, "") }
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
                TTextField {
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
                TCheckBox {
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
