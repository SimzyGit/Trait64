// SPDX-License-Identifier: AGPL-3.0-or-later
import QtQuick
import QtQuick.Controls
// Champ de saisie à la hauteur des boutons (28 px à 100 %), bordé d'un filet qui passe à
// l'accent quand il a le focus : le champ blanc sans bord du thème Basic, plus haut que
// les boutons voisins, cassait chaque rangée d'outils.
TextField {
    id: control
    readonly property real unit: controller.preferences.uiScale
    implicitHeight: Math.max(28 * unit, contentHeight + topPadding + bottomPadding)
    leftPadding: 8 * unit
    rightPadding: 8 * unit
    topPadding: 4 * unit
    bottomPadding: 4 * unit
    verticalAlignment: TextInput.AlignVCenter
    font.pixelSize: Math.round(12 * controller.preferences.fontScale * unit)
    color: palette.text
    placeholderTextColor: palette.placeholderText
    selectionColor: Qt.rgba(palette.highlight.r, palette.highlight.g, palette.highlight.b, .35)
    selectedTextColor: palette.text
    background: Rectangle {
        implicitWidth: 200  // fixe : multiplié par l’échelle, il bouclait avec la largeur des dialogues
        radius: 3
        color: control.palette.base
        border.width: control.activeFocus ? 2 : 1
        border.color: control.activeFocus ? control.palette.highlight
                    : control.hovered ? Qt.darker(control.palette.mid, 1.15) : control.palette.mid
        opacity: control.enabled ? 1 : .55
    }
}
