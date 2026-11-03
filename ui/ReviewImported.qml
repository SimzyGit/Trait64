// SPDX-License-Identifier: AGPL-3.0-or-later
// Proposé après une synchronisation ou un import : passer en revue le lot qui vient
// d'arriver, sans toucher au reste de la bibliothèque. Les parties déjà parcourues au
// moins aussi loin sont écartées côté base, donc relancer plus profond ne refait que
// ce qui manque.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: root
    required property var backend
    readonly property int count: backend.importedGames || 0
    // Trois paliers lisibles, plus la saisie libre. La profondeur décide de l'arrêt ;
    // le plafond de temps par position vient du contrôleur, qui en est seul juge.
    readonly property var presets: [12, 18, 24]
    readonly property int depth: preset.currentIndex < 3 ? presets[preset.currentIndex] : custom.value
    visible: count > 0
    spacing: 6

    Label {
        Layout.fillWidth: true; wrapMode: Text.WordWrap; font.bold: true
        text: qsTr("%1 nouvelle(s) partie(s) à passer en revue").arg(root.count)
    }
    Label {
        Layout.fillWidth: true; wrapMode: Text.WordWrap; color: palette.placeholderText
        text: qsTr("Le calcul se fait en arrière-plan et peut être mis en pause. Comptez une soixantaine de positions par partie : un lot important demande plusieurs heures.")
    }
    RowLayout {
        Layout.fillWidth: true; spacing: 8
        TComboBox {
            id: preset; objectName: "reviewPreset"; Layout.fillWidth: true
            model: [qsTr("Court (profondeur 12)"), qsTr("Moyen (profondeur 18)"), qsTr("Approfondi (profondeur 24)"), qsTr("Profondeur choisie")]
            currentIndex: 1
        }
        SpinBox {
            id: custom; objectName: "reviewDepth"; from: 1; to: 40; value: 20
            visible: preset.currentIndex === 3; editable: true
        }
        TButton {
            objectName: "reviewImported"; text: qsTr("Lancer le bilan"); accent: true
            enabled: !root.backend.busy && !root.backend.state.training
            onClicked: root.backend.reviewImported(root.depth)
            ToolTip.visible: hovered
            ToolTip.text: qsTr("Profondeur %1, au plus %2 ms par position. Les parties déjà vues aussi loin sont ignorées.")
                              .arg(root.depth).arg(root.backend.reviewTimeCeiling(root.depth))
        }
    }
}
