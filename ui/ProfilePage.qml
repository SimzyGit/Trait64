// SPDX-License-Identifier: AGPL-3.0-or-later
// La page des profils : rassembler ses comptes sous un seul nom, lancer le bilan des
// parties qui n'en ont pas encore, et lire le résultat sans changer d'écran. C'est
// l'analyse qui alimente le tableau de bord : sans elle il n'a rien à mesurer.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: root
    required property var backend
    property int profileIndex: 0
    readonly property var preferences: backend.preferences
    spacing: 10

    RowLayout {
        Layout.fillWidth: true
        spacing: 8

        Label { text: qsTr("Profil"); color: root.preferences.muted }
        TComboBox {
            objectName: "profilePageSelector"
            Layout.preferredWidth: 220
            enabled: root.backend.profiles.length > 0
            model: root.backend.profiles.map(function (p) { return p.name })
            currentIndex: root.profileIndex
            onActivated: function (chosen) { root.profileIndex = chosen }
        }

        Item { Layout.fillWidth: true }

        Label { text: qsTr("Profondeur"); color: root.preferences.muted }
        TComboBox {
            id: depth
            objectName: "profileReviewDepth"
            Layout.preferredWidth: 170
            model: [qsTr("Court · 12"), qsTr("Moyen · 18"), qsTr("Approfondi · 24")]
            currentIndex: 1
        }
        TButton {
            objectName: "reviewProfileButton"
            text: qsTr("Analyser le profil")
            accent: true
            enabled: root.backend.profiles.length > 0 && !root.backend.state.training
            onClicked: root.backend.reviewProfile(root.profileIndex, [12, 18, 24][depth.currentIndex])
            ToolTip.visible: hovered || visualFocus
            ToolTip.text: qsTr("Passe en revue les parties synchronisées de ce profil qui n’ont pas encore de bilan à cette profondeur. Celles déjà vues aussi loin sont laissées telles quelles.")
        }
    }

    SplitView {
        Layout.fillWidth: true
        Layout.fillHeight: true
        orientation: Qt.Horizontal
        handle: Rectangle {
            implicitWidth: 5
            color: SplitHandle.hovered || SplitHandle.pressed ? root.preferences.line : "transparent"
            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                width: 1; height: parent.height; color: root.preferences.line
            }
        }

        ScrollView {
            id: editorView
            SplitView.preferredWidth: 420
            SplitView.minimumWidth: 300
            contentWidth: availableWidth
            clip: true
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
            Profiles { width: editorView.availableWidth; backend: root.backend }
        }

        SkillReport {
            objectName: "profilePageReport"
            SplitView.fillWidth: true
            SplitView.minimumWidth: 320
            backend: root.backend
            profileIndex: root.profileIndex
        }
    }
}
