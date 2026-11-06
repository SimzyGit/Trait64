// SPDX-License-Identifier: AGPL-3.0-or-later
// Le point coloré de l'analyse, à la manière de ChessBase : un coup d'œil suffit à voir
// où la partie a tourné, sans lire un chiffre. Les six teintes viennent du bilan et non
// du moteur en direct : juger un coup demande l'évaluation d'avant et celle d'après.
import QtQuick
import QtQuick.Controls

Item {
    id: root
    required property var preferences
    // L'une des catégories du bilan : good, ok, inaccuracy, mistake, blunder, mate.
    property string quality: ""
    property int side: 8

    implicitWidth: side
    implicitHeight: side
    visible: quality.length > 0

    readonly property var palette: ({
        // Teintes fixes et non celles du thème : elles portent un sens, et le rouge d'une
        // gaffe doit rester rouge quelle que soit l'apparence choisie.
        good: "#3fa35a",
        ok: "#e8ebee",
        inaccuracy: "#e2bb3a",
        mistake: "#e08b2e",
        blunder: "#cf3d3d",
        mate: "#8b5cf6"
    })
    readonly property var wording: ({
        good: qsTr("Bon coup"),
        ok: qsTr("Coup correct"),
        inaccuracy: qsTr("Imprécision"),
        mistake: qsTr("Erreur"),
        blunder: qsTr("Gaffe"),
        mate: qsTr("Séquence de mat")
    })

    Rectangle {
        anchors.centerIn: parent
        width: root.side
        height: root.side
        radius: width / 2
        color: root.palette[root.quality] || "transparent"
        // Un liseré, sans quoi le point « correct », presque blanc, disparaît sur une
        // apparence claire et le point violet se noie sur une apparence sombre.
        border.width: 1
        border.color: Qt.rgba(0, 0, 0, root.preferences.light ? .22 : 0)
        antialiasing: true
    }

    HoverHandler { id: hover }
    ToolTip.visible: hover.hovered && root.quality.length > 0
    ToolTip.text: root.wording[root.quality] || ""
    Accessible.role: Accessible.Indicator
    Accessible.name: root.wording[root.quality] || ""
}
