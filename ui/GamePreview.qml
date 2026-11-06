// SPDX-License-Identifier: AGPL-3.0-or-later
// Un aperçu de partie, à poser dans la bibliothèque ou dans un recueil : on y fait
// défiler les coups sans changer d'onglet et, surtout, sans charger la partie — ouvrir
// pour simplement regarder ferait perdre l'étude en cours d'analyse.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: root
    required property var backend
    readonly property var p: backend.preferences
    // La partie à montrer. 0 : rien de choisi.
    property double gameId: 0
    property var line: []
    property int step: 0
    property int boardSide: 260

    spacing: 6

    onGameIdChanged: {
        line = gameId > 0 ? backend.gameLine(gameId) : []
        step = 0
    }

    function go(to) {
        if (line.length === 0)
            return
        step = Math.max(0, Math.min(line.length - 1, to))
    }

    Board {
        id: preview
        objectName: "previewBoard"
        Layout.alignment: Qt.AlignHCenter
        Layout.preferredWidth: root.boardSide
        Layout.preferredHeight: root.boardSide
        backend: root.backend
        interactive: false
        coordinates: false
        arrows: []
        // Toujours vue des Blancs : l'aperçu ne suit pas l'orientation de l'analyse, qui
        // appartient à l'étude ouverte.
        flipped: false
        squares: root.line.length > 0
                 ? root.backend.squaresForFen(root.line[root.step].fen, root.line[root.step].uci)
                 : []
        visible: root.line.length > 0
    }

    Label {
        Layout.fillWidth: true
        Layout.preferredHeight: root.boardSide
        visible: root.line.length === 0
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        wrapMode: Text.WordWrap
        color: root.p.muted
        text: root.gameId > 0 ? qsTr("Partie illisible.")
                              : qsTr("Choisissez une partie pour la parcourir ici.")
    }

    RowLayout {
        Layout.fillWidth: true
        visible: root.line.length > 0
        spacing: 4
        TButton {
            text: "|◀"; flat: true; enabled: root.step > 0
            onClicked: root.go(0)
            ToolTip.visible: hovered; ToolTip.text: qsTr("Position initiale")
        }
        TButton {
            objectName: "previewBack"
            text: "◀"; flat: true; enabled: root.step > 0
            onClicked: root.go(root.step - 1)
            ToolTip.visible: hovered; ToolTip.text: qsTr("Coup précédent")
        }
        Label {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            elide: Text.ElideRight
            color: root.p.foreground
            // Le numéro du coup et sa notation : c'est ce qu'on lit en défilant.
            text: root.step === 0
                  ? qsTr("Début")
                  : Math.floor((root.step - 1) / 2 + 1) + (root.step % 2 === 1 ? ". " : "… ")
                    + (root.line[root.step].san || "")
        }
        TButton {
            objectName: "previewForward"
            text: "▶"; flat: true; enabled: root.step < root.line.length - 1
            onClicked: root.go(root.step + 1)
            ToolTip.visible: hovered; ToolTip.text: qsTr("Coup suivant")
        }
        TButton {
            text: "▶|"; flat: true; enabled: root.step < root.line.length - 1
            onClicked: root.go(root.line.length - 1)
            ToolTip.visible: hovered; ToolTip.text: qsTr("Dernière position")
        }
    }

    TButton {
        Layout.fillWidth: true
        visible: root.line.length > 0
        text: qsTr("Ouvrir dans Analyse")
        onClicked: root.backend.loadGame(root.gameId)
    }

    // La molette fait défiler les coups : c'est le geste qu'on a déjà en main.
    WheelHandler {
        target: preview
        onWheel: event => root.go(root.step + (event.angleDelta.y < 0 ? 1 : -1))
    }
    // Et les flèches, quand l'aperçu a le focus.
    Keys.onLeftPressed: root.go(root.step - 1)
    Keys.onRightPressed: root.go(root.step + 1)
}
