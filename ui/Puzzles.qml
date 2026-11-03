// SPDX-License-Identifier: AGPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs as Native

TDialog {
    id: root
    required property var backend
    readonly property var state: backend.puzzles
    // Lichess theme tags, with French labels.
    readonly property var themes: [
        {tag: "", label: qsTr("Tous les thèmes")}, {tag: "mateIn1", label: qsTr("Mat en 1")},
        {tag: "mateIn2", label: qsTr("Mat en 2")}, {tag: "mateIn3", label: qsTr("Mat en 3")},
        {tag: "fork", label: qsTr("Fourchette")}, {tag: "pin", label: qsTr("Clouage")},
        {tag: "skewer", label: qsTr("Enfilade")}, {tag: "discoveredAttack", label: qsTr("Attaque à la découverte")},
        {tag: "sacrifice", label: qsTr("Sacrifice")}, {tag: "deflection", label: qsTr("Déviation")},
        {tag: "hangingPiece", label: qsTr("Pièce en prise")}, {tag: "backRankMate", label: qsTr("Mat du couloir")},
        {tag: "promotion", label: qsTr("Promotion")}, {tag: "endgame", label: qsTr("Finale")},
        {tag: "opening", label: qsTr("Ouverture")}, {tag: "short", label: qsTr("Court")}, {tag: "long", label: qsTr("Long")}]
    title: qsTr("Puzzles tactiques")
    width: Math.min(820, parent.width - 32)
    height: Math.min(680, parent.height - 32)
    anchors.centerIn: parent
    modal: true
    standardButtons: Dialog.Close
    ScrollView {
        id: viewport; anchors.fill: parent; contentWidth: availableWidth; clip: true
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
        ColumnLayout {
            width: viewport.availableWidth; spacing: 14
            Flow {
                Layout.fillWidth: true; spacing: 24
                Repeater {
                    model: [{label: qsTr("Classement"), value: root.state.rating},
                            {label: qsTr("Puzzles disponibles"), value: root.state.total || 0},
                            {label: qsTr("Résolus"), value: root.state.solved || 0},
                            {label: qsTr("Aujourd’hui"), value: root.state.today || 0},
                            {label: qsTr("Réussite récente"), value: (root.state.recentSuccess || 0) + " %"}]
                    ColumnLayout {
                        required property var modelData
                        spacing: 2
                        Label { text: modelData.label; color: palette.placeholderText }
                        Label { text: String(modelData.value); font.pixelSize: 22; font.bold: true }
                    }
                }
            }
            GridLayout {
                columns: 2; columnSpacing: 12; rowSpacing: 8; Layout.fillWidth: true
                Label { text: qsTr("Thème") }
                TComboBox { id: theme; objectName: "puzzleTheme"; Layout.fillWidth: true; model: root.themes.map(t => t.label) }
                Label { text: qsTr("Origine") }
                TComboBox {
                    id: source; objectName: "puzzleSource"; Layout.fillWidth: true
                    model: [qsTr("Toutes"), qsTr("Base Lichess"), qsTr("Mes erreurs (%1)").arg(root.state.mine || 0)]
                }
            }
            TButton {
                objectName: "startPuzzle"; text: qsTr("Commencer"); accent: true; enabled: !root.backend.busy
                onClicked: {
                    root.backend.startPuzzle(root.themes[theme.currentIndex].tag, ["", "lichess", "mine"][source.currentIndex])
                    if (root.backend.puzzles.active) root.close()
                }
            }
            Label {
                Layout.fillWidth: true; wrapMode: Text.WordWrap; color: palette.placeholderText
                text: qsTr("Chaque puzzle est choisi près de votre classement, qui évolue comme un Elo. Au dernier coup, tout mat est accepté. Un indice ou la solution comptent comme un échec.")
            }
            Rectangle { Layout.fillWidth: true; height: 1; opacity: .4; color: palette.mid }
            Label { text: qsTr("IMPORTER LA BASE LICHESS"); font.bold: true }
            Label {
                Layout.fillWidth: true; wrapMode: Text.WordWrap
                text: qsTr("Environ 5 millions de puzzles notés, sous licence CC0. Téléchargez « lichess_db_puzzle.csv.zst » (≈ 280 Mo) depuis le site, puis importez-le ici : le fichier compressé est lu directement.")
            }
            Flow {
                Layout.fillWidth: true; spacing: 8
                TButton { text: qsTr("Ouvrir la page de téléchargement"); onClicked: Qt.openUrlExternally("https://database.lichess.org/#puzzles") }
                RowLayout {
                    Label { text: qsTr("Nombre max.") }
                    SpinBox { id: maxPuzzles; from: 1000; to: 6000000; stepSize: 50000; value: 300000; editable: true }
                }
                RowLayout {
                    Label { text: qsTr("Classement") }
                    SpinBox { id: minRating; from: 400; to: 3000; stepSize: 100; value: 600; editable: true }
                    Label { text: "–" }
                    SpinBox { id: maxRating; from: 600; to: 3500; stepSize: 100; value: 3000; editable: true }
                }
                TButton { objectName: "importPuzzles"; text: qsTr("Importer le fichier…"); enabled: !root.backend.busy; onClicked: puzzleFile.open() }
                TButton { text: qsTr("Annuler"); visible: root.backend.busy; onClicked: root.backend.cancelTask() }
            }
            Label { visible: root.backend.busy; text: root.backend.taskStatus; Layout.fillWidth: true; wrapMode: Text.WordWrap }
        }
    }
    Native.FileDialog {
        id: puzzleFile; title: qsTr("Base de puzzles Lichess")
        nameFilters: [qsTr("Puzzles Lichess (*.csv *.csv.zst *.zst)"), qsTr("Tous les fichiers (*)")]
        onAccepted: root.backend.importPuzzles(selectedFile, maxPuzzles.value, minRating.value, maxRating.value)
    }
}
