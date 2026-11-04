// SPDX-License-Identifier: AGPL-3.0-or-later
// Précision par camp et par phase. Une note porte sur les coups réellement évalués :
// une phase que la partie n'a pas atteinte affiche un tiret, jamais un zéro, qui se
// lirait comme un mauvais résultat.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: root
    required property var backend
    readonly property var scores: backend.reviewAccuracy || ({})
    readonly property var parts: [
        {key: "opening", label: qsTr("Ouverture")},
        {key: "middlegame", label: qsTr("Milieu")},
        {key: "endgame", label: qsTr("Finale")},
        {key: "all", label: qsTr("Partie")}
    ]
    readonly property real scale: controller.preferences.fontScale * controller.preferences.uiScale
    visible: !!(scores.white && scores.white.allMoves > 0)
    spacing: 4

    function reading(side, key) {
        const row = root.scores[side]
        const value = row ? row[key] : undefined
        return value === undefined || value === null ? "—" : value.toFixed(1) + " %"
    }
    function moves(side, key) {
        const row = root.scores[side]
        return (row ? row[key + "Moves"] : 0) || 0
    }

    Label { text: qsTr("Précision"); font.bold: true }
    Rectangle {
        Layout.fillWidth: true
        implicitHeight: table.implicitHeight + 16
        color: root.backend.preferences.panel; radius: 8
        ColumnLayout {
            id: table
            anchors.fill: parent; anchors.margins: 8; spacing: 4

            RowLayout {
                Layout.fillWidth: true; spacing: 8
                Item { Layout.preferredWidth: 62 }
                Repeater {
                    model: root.parts
                    delegate: Label {
                        required property var modelData
                        Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter
                        text: modelData.label; opacity: .7; elide: Text.ElideRight
                        font.pixelSize: Math.round(11 * root.scale)
                    }
                }
            }
            Repeater {
                model: [{key: "white", label: qsTr("Blancs")}, {key: "black", label: qsTr("Noirs")}]
                delegate: RowLayout {
                    id: line
                    required property var modelData
                    readonly property string side: modelData.key
                    Layout.fillWidth: true; spacing: 8
                    Label {
                        Layout.preferredWidth: 62; text: line.modelData.label; font.bold: true
                        font.pixelSize: Math.round(12 * root.scale)
                    }
                    Repeater {
                        model: root.parts
                        delegate: Label {
                            required property var modelData
                            Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter
                            text: root.reading(line.side, modelData.key)
                            font.pixelSize: Math.round((modelData.key === "all" ? 14 : 12) * root.scale)
                            font.bold: modelData.key === "all"
                            HoverHandler { id: cell }
                            ToolTip.visible: cell.hovered && root.moves(line.side, modelData.key) > 0
                            ToolTip.text: qsTr("%1 coup(s) mesuré(s)").arg(root.moves(line.side, modelData.key))
                        }
                    }
                }
            }
        }
    }
}
