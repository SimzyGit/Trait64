// SPDX-License-Identifier: AGPL-3.0-or-later
import QtQuick
import QtQuick.Controls

// Bande verticale collée à l'échiquier : la part claire est celle des Blancs.
Item {
    id: root
    required property var backend
    property bool flipped: false
    property color lightSide: "#f4f6f8"
    property color darkSide: "#1d2125"
    // Le trait du thème, pas une teinte fixe : sur fond clair la moitié blanche se
    // confondait avec la page, et la barre semblait flotter sans bas.
    property color line: backend.preferences.line
    implicitWidth: 18

    readonly property var best: (backend.engineLines && backend.engineLines.length) ? backend.engineLines[0] : null
    readonly property bool known: !!best && !!backend.state.analysis
    // Le moteur compte toujours du point de vue des Blancs, en pions, ±100 sur un mat.
    readonly property real pawns: known ? Number(best.score) : 0
    // Même courbe que la précision : proportionner la barre aux centipions bruts
    // donnerait un balancier affolé sur des écarts qui ne décident de rien.
    readonly property real whiteShare:
        known ? 1 / (1 + Math.exp(-0.00368208 * Math.max(-1500, Math.min(1500, pawns * 100)))) : 0.5

    Rectangle {
        anchors.fill: parent
        radius: Math.min(width, height) / 2
        color: root.darkSide
        border.color: root.line
        clip: true

        Rectangle {
            id: white
            width: parent.width
            color: root.lightSide
            height: parent.height * root.whiteShare
            y: root.flipped ? 0 : parent.height - height
            Behavior on height {
                enabled: !root.backend.preferences.reduceMotion
                NumberAnimation { duration: 180; easing.type: Easing.OutCubic }
            }
        }
        // Repère de l'équilibre : sans lui, l'œil ne sait pas où est le milieu.
        Rectangle {
            width: parent.width; height: 1; y: parent.height / 2
            color: "#00000055"; visible: Math.abs(root.whiteShare - 0.5) > 0.02
        }
    }

    // Au repos la barre doit rester lisible comme un blanc et un noir : trop effacée, elle
    // vire au gris sur gris et ne se lit plus comme un échiquier.
    opacity: known ? 1 : 0.72
    Behavior on opacity {
        enabled: !root.backend.preferences.reduceMotion
        NumberAnimation { duration: 150 }
    }

    HoverHandler { id: hover }
    ToolTip.visible: hover.hovered
    ToolTip.text: root.known
                  ? qsTr("Évaluation : %1 — %2 % pour les Blancs")
                      .arg(root.best.eval).arg(Math.round(root.whiteShare * 100))
                  : qsTr("Lancez l’analyse pour voir l’évaluation.")
    Accessible.role: Accessible.ProgressBar
    Accessible.name: qsTr("Barre d’évaluation")
}
