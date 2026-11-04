// SPDX-License-Identifier: AGPL-3.0-or-later
// Profils de joueur : rassembler sous un seul nom ses comptes en ligne et son profil
// FIDE. Retirer un profil ne retire ni les comptes ni les parties ; ils redeviennent
// seulement non rattachés.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: root
    required property var backend
    readonly property real scale: controller.preferences.fontScale * controller.preferences.uiScale
    spacing: 10

    function ratingLine(profile) {
        const parts = []
        if (profile.fideStandard) parts.push(qsTr("classique %1").arg(profile.fideStandard))
        if (profile.fideRapid) parts.push(qsTr("rapide %1").arg(profile.fideRapid))
        if (profile.fideBlitz) parts.push(qsTr("blitz %1").arg(profile.fideBlitz))
        return parts.length ? parts.join(" · ") : qsTr("aucun classement saisi")
    }

    Label {
        Layout.fillWidth: true; wrapMode: Text.WordWrap
        text: qsTr("Un profil rassemble vos comptes Lichess et Chess.com et votre profil FIDE sous un seul nom, pour que vos statistiques portent sur l’ensemble de vos parties.")
    }

    Repeater {
        model: root.backend.profiles
        delegate: Rectangle {
            id: card
            required property var modelData
            required property int index
            Layout.fillWidth: true
            implicitHeight: body.implicitHeight + 16
            color: palette.base; border.color: palette.mid; radius: 3

            ColumnLayout {
                id: body
                anchors.fill: parent; anchors.margins: 8; spacing: 6

                RowLayout {
                    Layout.fillWidth: true; spacing: 8
                    Label {
                        text: card.modelData.name; font.bold: true; elide: Text.ElideRight
                        Layout.fillWidth: true
                        font.pixelSize: Math.round(15 * root.scale)
                    }
                    TButton {
                        text: qsTr("Renommer")
                        onClicked: { rename.index = card.index; rename.proposed = card.modelData.name; rename.open() }
                    }
                    TButton {
                        text: qsTr("Profil FIDE")
                        onClicked: { fide.index = card.index; fide.load(card.modelData); fide.open() }
                    }
                    TButton {
                        text: "×"; Accessible.name: qsTr("Retirer ce profil")
                        onClicked: root.backend.removeProfile(card.index)
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("Retire le profil ; les comptes et les parties restent.")
                    }
                }
                Label {
                    Layout.fillWidth: true; color: palette.placeholderText; wrapMode: Text.WordWrap
                    font.pixelSize: Math.round(11 * root.scale)
                    text: (card.modelData.fideId ? qsTr("FIDE %1 — %2").arg(card.modelData.fideId).arg(root.ratingLine(card.modelData))
                                                 : qsTr("Aucun profil FIDE rattaché"))
                }
                Label {
                    Layout.fillWidth: true; wrapMode: Text.WordWrap
                    font.pixelSize: Math.round(11 * root.scale)
                    text: card.modelData.accounts.length === 0
                          ? qsTr("Aucun compte rattaché : utilisez « Rattacher » sur un compte ci-dessous.")
                          : qsTr("Comptes : ") + card.modelData.accounts.map(
                                a => (a.site === "lichess" ? "Lichess" : "Chess.com") + " / " + a.user).join(" · ")
                }
            }
        }
    }

    RowLayout {
        Layout.fillWidth: true; spacing: 8
        TextField {
            id: created; objectName: "profileName"; Layout.fillWidth: true; selectByMouse: true
            placeholderText: qsTr("Nom du profil, par exemple votre prénom")
            onAccepted: if (addProfile.enabled) addProfile.clicked()
        }
        TButton {
            id: addProfile; objectName: "addProfile"; text: qsTr("Créer le profil"); accent: true
            enabled: created.text.trim().length > 0
            onClicked: { root.backend.addProfile(created.text); created.text = "" }
        }
    }

    TDialog {
        id: rename; parent: Overlay.overlay; modal: true; anchors.centerIn: parent
        property int index: -1
        property string proposed: ""
        title: qsTr("Renommer le profil")
        standardButtons: Dialog.Ok | Dialog.Cancel
        onAccepted: root.backend.renameProfile(index, field.text)
        onOpened: { field.text = proposed; field.selectAll(); field.forceActiveFocus() }
        ColumnLayout {
            anchors.fill: parent
            TextField { id: field; Layout.fillWidth: true; selectByMouse: true }
        }
    }

    TDialog {
        id: fide; parent: Overlay.overlay; modal: true; anchors.centerIn: parent
        property int index: -1
        title: qsTr("Profil FIDE")
        width: Math.min(460, parent ? parent.width - 32 : 460)
        standardButtons: Dialog.Save | Dialog.Cancel
        function load(profile) {
            identifier.text = profile.fideId || ""
            fullName.text = profile.fideName || ""
            standard.value = profile.fideStandard || 0
            rapid.value = profile.fideRapid || 0
            blitz.value = profile.fideBlitz || 0
        }
        onAccepted: root.backend.setFideDetails(index, identifier.text, fullName.text,
                                                standard.value, rapid.value, blitz.value)
        ColumnLayout {
            anchors.fill: parent; spacing: 8
            Label {
                Layout.fillWidth: true; wrapMode: Text.WordWrap
                font.pixelSize: Math.round(11 * root.scale)
                text: qsTr("Votre identifiant figure sur votre fiche, à l’adresse ratings.fide.com. Laissez une cadence à zéro si vous n’y êtes pas classé.")
            }
            GridLayout {
                Layout.fillWidth: true; columns: 2; columnSpacing: 10; rowSpacing: 6
                Label { text: qsTr("Identifiant") }
                TextField {
                    id: identifier; objectName: "fideId"; Layout.fillWidth: true; selectByMouse: true
                    placeholderText: qsTr("chiffres seulement"); inputMethodHints: Qt.ImhDigitsOnly
                }
                Label { text: qsTr("Nom") }
                TextField { id: fullName; Layout.fillWidth: true; selectByMouse: true }
                Label { text: qsTr("Classique") }
                SpinBox { id: standard; from: 0; to: 3000; editable: true }
                Label { text: qsTr("Rapide") }
                SpinBox { id: rapid; from: 0; to: 3000; editable: true }
                Label { text: qsTr("Blitz") }
                SpinBox { id: blitz; from: 0; to: 3000; editable: true }
            }
        }
    }
}
