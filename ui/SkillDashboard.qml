// SPDX-License-Identifier: AGPL-3.0-or-later
// L'habillage du bilan de compétences en boîte de dialogue. Le contenu vit dans
// SkillReport, que la page des profils affiche directement : la boîte reste pour qui
// l'ouvre depuis le menu ou la barre d'outils.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

TDialog {
    id: root
    required property var backend
    property int profileIndex: 0
    title: qsTr("Bilan de vos compétences")
    width: Math.min(880, parent ? parent.width - 24 : 880)
    height: Math.min(720, parent ? parent.height - 24 : 720)
    anchors.centerIn: parent
    modal: true
    standardButtons: Dialog.Close

    SkillReport {
        id: content
        objectName: "skillReport"
        anchors.fill: parent
        backend: root.backend
        profileIndex: root.profileIndex
    }
    onOpened: content.refresh()
    onProfileIndexChanged: content.profileIndex = profileIndex
}
