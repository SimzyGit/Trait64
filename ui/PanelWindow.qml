// SPDX-License-Identifier: AGPL-3.0-or-later
// Un panneau de l'analyse dans sa propre fenêtre, pour un second écran : l'échiquier, la
// notation ou la courbe du bilan. Il suit l'étude ouverte comme le panneau d'origine,
// qui reste à sa place.
import QtQuick
import QtQuick.Controls
import QtQuick.Window

Window {
    id: root
    required property var backend
    required property string panel
    readonly property var prefs: backend.preferences
    readonly property real textScale: prefs.fontScale * prefs.uiScale
    width: panel === "graph" ? 720 : 560
    height: panel === "graph" ? 260 : panel === "board" ? 600 : 640
    color: prefs.background
    title: (panel === "board" ? qsTr("Échiquier") : panel === "graph" ? qsTr("Courbe du bilan") : qsTr("Notation"))
           + " — Trait64"
    onClosing: destroy()
    Loader {
        anchors.fill: parent; anchors.margins: 10
        sourceComponent: root.panel === "board" ? board : root.panel === "graph" ? graph : notation
    }
    Component {
        id: board
        Item {
            Board {
                objectName: "detachedBoard"
                backend: root.backend; coordinates: root.prefs.coordinates
                width: Math.min(parent.width, parent.height); height: width; anchors.centerIn: parent
            }
        }
    }
    Component {
        id: notation
        MoveNotebook {
            backend: root.backend; textScale: root.textScale
            foreground: root.prefs.foreground; muted: root.prefs.muted; accent: root.prefs.accentColor
            raised: root.prefs.raised; lineColor: root.prefs.line
        }
    }
    Component { id: graph; EvalGraph { backend: root.backend } }
}
