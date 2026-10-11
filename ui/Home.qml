// SPDX-License-Identifier: AGPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ScrollView {
    id: root
    required property var backend
    property var p: backend.preferences
    signal newStudy()
    signal importStudy()
    signal resumeStudy()
    signal showLibrary()
    signal showTraining()
    clip: true
    contentWidth: availableWidth

    ColumnLayout {
        width: root.availableWidth
        spacing: 24
        RowLayout {
            Layout.fillWidth: true; spacing: 36; Layout.topMargin: 12
            ColumnLayout {
                Layout.fillWidth: true; spacing: 16
                Label {text: "VOTRE CARNET / 01"; color: root.p.accentColor; font.family: "DejaVu Sans Mono"; font.pixelSize: 10; font.letterSpacing: 1.5}
                Label {text: "À vous\nle trait."; font.family: "DejaVu Serif"; font.pixelSize: root.width > 1150 ? 76 : 60; lineHeight: 0.95; font.letterSpacing: -2}
                Label {text: "Une position. Des idées. Votre chemin."; color: root.p.muted; font.pixelSize: 14; Layout.topMargin: 2}
                RowLayout {
                    spacing: 8; Layout.topMargin: 8
                    TButton {text: "Nouvelle étude  +"; accent: true; onClicked: root.newStudy()}
                    TButton {text: "Importer un PGN"; flat: true; onClicked: root.importStudy()}
                }
            }
            ColumnLayout {
                visible: root.width >= 1050
                Layout.preferredWidth: 330; Layout.rightMargin: 40; spacing: 10
                Item {
                    Layout.preferredWidth: 308; Layout.preferredHeight: 308; Layout.alignment: Qt.AlignHCenter
                    Rectangle {anchors.fill: parent; anchors.margins: -7; color: "transparent"; border.color: root.p.line}
                    Board {anchors.fill: parent; backend: root.backend; enabled: false; coordinates: false}
                    Button {
                        anchors.fill: parent; hoverEnabled: true
                        Accessible.name: "Reprendre l’étude sur l’échiquier"
                        background: Rectangle {color: "transparent"; border.width: parent.visualFocus ? 3 : 0; border.color: root.p.accentColor}
                        onClicked: root.resumeStudy()
                        ToolTip.visible: hovered; ToolTip.text: "Reprendre cette position"
                    }
                }
                Label {text: root.backend.state.title; color: root.p.muted; elide: Text.ElideRight; Layout.fillWidth: true; font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter}
            }
            ColumnLayout {
                Layout.preferredWidth: root.width >= 1050 ? 195 : 275; spacing: 16
                Rectangle {width: 38; height: 3; color: root.p.accentColor}
                Label {text: "L’étude\nen cours"; font.family: "DejaVu Serif"; font.pixelSize: 26}
                Label {text: root.backend.state.title; color: root.p.muted; wrapMode: Text.WordWrap; Layout.fillWidth: true}
                Label {text: root.backend.state.dirty ? "Modifications conservées\ndans votre session." : "Votre position vous attend."; color: root.p.muted; wrapMode: Text.WordWrap; Layout.fillWidth: true; font.pixelSize: 11}
                TButton {text: "Reprendre  →"; onClicked: root.resumeStudy()}
            }
        }

        Rectangle {Layout.fillWidth: true; height: 1; color: root.p.line; Layout.topMargin: 6}
        RowLayout {
            Layout.fillWidth: true; spacing: 0
            Repeater {
                model: [{name: "Parties", key: "games", destination: 1}, {name: "Favoris", key: "favorites", destination: 1}, {name: "À réviser", key: "due", destination: 4}]
                delegate: ItemDelegate {
                    required property var modelData
                    Layout.fillWidth: true; Layout.preferredWidth: 1; implicitHeight: 64
                    Accessible.name: modelData.name + " : " + (root.backend.stats[modelData.key] || 0)
                    background: Rectangle {color: parent.hovered || parent.visualFocus ? root.p.panel : "transparent"}
                    contentItem: RowLayout {
                        spacing: 18
                        Label {text: root.backend.stats[modelData.key] || 0; font.family: "DejaVu Serif"; font.pixelSize: 36; color: root.p.accentColor}
                        Label {text: modelData.name; font.pixelSize: 12; Layout.fillWidth: true}
                        Label {text: "↗"; font.pixelSize: 18; color: root.p.muted; Layout.rightMargin: 26}
                    }
                    onClicked: modelData.destination === 4 ? root.showTraining() : root.showLibrary()
                }
            }
        }
        Rectangle {Layout.fillWidth: true; height: 1; color: root.p.line}

        RowLayout {
            Layout.fillWidth: true; spacing: 38; Layout.bottomMargin: 20
            ColumnLayout {
                Layout.fillWidth: true; spacing: 4
                RowLayout {
                    Layout.fillWidth: true; Layout.bottomMargin: 14
                    Label {text: "Les pages à rouvrir"; font.family: "DejaVu Serif"; font.pixelSize: 23}
                    Item {Layout.fillWidth: true}
                    TButton {text: "Bibliothèque  →"; flat: true; onClicked: root.showLibrary()}
                }
                Label {visible: root.backend.games.length === 0; text: "Importez vos parties pour écrire la suite."; color: root.p.muted}
                TButton {visible: root.backend.games.length === 0; text: "Découvrir les parties d’exemple"; onClicked: root.backend.loadExamples()}
                Repeater {
                    model: Math.min(3, root.backend.games.length)
                    delegate: ItemDelegate {
                        required property int index
                        property var game: root.backend.games[index] || ({})
                        Layout.fillWidth: true; implicitHeight: 68
                        Accessible.name: (game.white || "") + " contre " + (game.black || "")
                        background: Rectangle {
                            color: parent.hovered || parent.visualFocus ? root.p.panel : "transparent"
                            Rectangle {anchors.bottom: parent.bottom; width: parent.width; height: 1; color: root.p.line}
                        }
                        contentItem: RowLayout {
                            spacing: 16
                            Label {text: "0" + (index + 1); color: root.p.accentColor; font.family: "DejaVu Sans Mono"; font.pixelSize: 11}
                            ColumnLayout {
                                Layout.fillWidth: true; spacing: 5
                                Label {text: (game.white || "") + " — " + (game.black || ""); font.weight: Font.DemiBold; elide: Text.ElideRight; Layout.fillWidth: true}
                                Label {text: game.event || "Étude personnelle"; color: root.p.muted; font.pixelSize: 11; elide: Text.ElideRight; Layout.fillWidth: true}
                            }
                            Label {text: game.result || "*"; color: root.p.muted; font.family: "DejaVu Sans Mono"; font.pixelSize: 11}
                            Label {text: "→"; color: root.p.accentColor; font.pixelSize: 20}
                        }
                        onClicked: root.backend.loadGame(game.id)
                    }
                }
                Item {Layout.fillHeight: true}
            }
            Rectangle {Layout.preferredWidth: 1; Layout.fillHeight: true; color: root.p.line}
            ColumnLayout {
                Layout.preferredWidth: 275; Layout.alignment: Qt.AlignTop; spacing: 15
                Label {text: "DE L’IDÉE AU RÉFLEXE"; color: root.p.accentColor; font.pixelSize: 9; font.letterSpacing: 1.5; Layout.topMargin: 12}
                Label {text: "Ce qui s’apprend\nse retrouve."; font.family: "DejaVu Serif"; font.pixelSize: 25}
                Label {text: (root.backend.stats.due || 0) + " cartes à revoir · " + (root.backend.stats.todayReviews || 0) + " révisions aujourd’hui"; color: root.p.muted; font.pixelSize: 11; wrapMode: Text.WordWrap; Layout.fillWidth: true}
                ProgressBar {id: progress; Layout.fillWidth: true; from: 0; to: root.p.dailyGoal; value: root.backend.stats.todayReviews || 0
                    background: Rectangle {implicitHeight: 3; color: root.p.line}
                    contentItem: Item {implicitHeight: 3; Rectangle {width: parent.width * progress.visualPosition; height: parent.height; color: root.p.accentColor}}
                }
                TButton {text: "Ouvrir mon entraînement  →"; flat: true; onClicked: root.showTraining()}
            }
        }
    }
}
