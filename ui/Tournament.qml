// SPDX-License-Identifier: AGPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

TDialog {
    id: root
    required property var backend
    property var selected: []
    readonly property var tournament: backend.tournament
    readonly property int pairs: selected.length * (selected.length - 1) / 2
    title: qsTr("Tournoi de moteurs")
    width: Math.min(900, parent.width - 32)
    height: Math.min(720, parent.height - 32)
    anchors.centerIn: parent
    modal: true
    standardButtons: Dialog.Close
    ScrollView {
        id: viewport; anchors.fill: parent; contentWidth: availableWidth; clip: true
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
        ColumnLayout {
            width: viewport.availableWidth; spacing: 12
            Label {
                Layout.fillWidth: true; wrapMode: Text.WordWrap
                text: qsTr("Toutes rondes entre 2 et 8 profils enregistrés. Chaque paire joue les deux couleurs depuis la même ouverture ; les parties sont ajoutées à la bibliothèque.")
            }
            Flow {
                Layout.fillWidth: true; spacing: 8
                Repeater {
                    model: root.backend.preferences.engineProfiles
                    CheckBox {
                        required property int index
                        required property var modelData
                        objectName: "tournamentProfile_" + index
                        text: modelData.name; enabled: !root.tournament.running
                        checked: root.selected.indexOf(index) >= 0
                        onToggled: {
                            let a = root.selected.slice(), n = a.indexOf(index)
                            if (checked && n < 0) a.push(index); else if (!checked && n >= 0) a.splice(n, 1)
                            root.selected = a
                        }
                    }
                }
            }
            Label {
                visible: root.backend.preferences.engineProfiles.length < 2; Layout.fillWidth: true; wrapMode: Text.WordWrap
                text: qsTr("Enregistrez au moins deux profils UCI dans les paramètres pour organiser un tournoi.")
            }
            GridLayout {
                columns: viewport.availableWidth > 640 ? 4 : 2; columnSpacing: 14; rowSpacing: 8; enabled: !root.tournament.running
                Label { text: qsTr("Parties par paire") }
                SpinBox { id: games; objectName: "tournamentGames"; from: 2; to: 20; stepSize: 2; value: 2; editable: true }
                Label { text: qsTr("Temps par coup (ms)") }
                SpinBox { id: moveTime; objectName: "tournamentMoveTime"; from: 100; to: 5000; stepSize: 100; value: 300; editable: true }
                Label { text: qsTr("Nulle adjugée après (demi-coups)") }
                SpinBox { id: maxPlies; from: 40; to: 600; stepSize: 20; value: 300; editable: true }
                CheckBox {
                    id: useBook; Layout.columnSpan: 2
                    text: qsTr("Ouvertures tirées du livre Polyglot")
                    enabled: !!root.backend.preferences.bookPath; checked: false
                }
            }
            Label {
                color: palette.placeholderText; Layout.fillWidth: true; wrapMode: Text.WordWrap
                text: root.pairs > 0 ? qsTr("%1 partie(s), environ %2 min au maximum.").arg(root.pairs * games.value)
                                          .arg(Math.ceil(root.pairs * games.value * maxPlies.value * moveTime.value / 60000))
                                     : qsTr("Sélectionnez au moins deux profils.")
            }
            RowLayout {
                TButton {
                    objectName: "startTournament"; text: qsTr("Lancer le tournoi"); accent: true
                    enabled: !root.tournament.running && root.selected.length >= 2 && root.selected.length <= 8
                    onClicked: root.backend.startTournament(root.selected, games.value, moveTime.value, maxPlies.value, useBook.checked)
                }
                TButton { text: qsTr("Arrêter"); enabled: root.tournament.running; onClicked: root.tournament.stop() }
            }
            Label { objectName: "tournamentStatus"; text: root.tournament.status; Layout.fillWidth: true; wrapMode: Text.WordWrap }
            ProgressBar {
                Layout.fillWidth: true; visible: root.tournament.total > 0
                from: 0; to: Math.max(1, root.tournament.total); value: root.tournament.played
            }
            GridLayout {
                Layout.fillWidth: true; columns: 7; columnSpacing: 12; rowSpacing: 6
                visible: root.tournament.standings.length > 0
                Repeater {
                    model: [qsTr("Rang"), qsTr("Moteur"), qsTr("Points"), "+", "=", "−", qsTr("Elo ±")]
                    Label { required property string modelData; text: modelData; font.bold: true }
                }
                Repeater {
                    model: root.tournament.standings
                    delegate: Repeater {
                        required property int index
                        required property var modelData
                        model: [index + 1, modelData.name, modelData.points + " / " + modelData.games, modelData.wins,
                                modelData.draws, modelData.losses, (modelData.elo > 0 ? "+" : "") + modelData.elo]
                        Label { required property int index; required property var modelData; text: String(modelData); Layout.fillWidth: index === 1 }
                    }
                }
            }
            Label {
                visible: root.tournament.standings.length > 0; color: palette.placeholderText
                Layout.fillWidth: true; wrapMode: Text.WordWrap
                text: qsTr("« Elo ± » est l'écart estimé face au reste du plateau. Sur peu de parties, la marge d'erreur dépasse souvent 100 Elo.")
            }
        }
    }
}
