// SPDX-License-Identifier: AGPL-3.0-or-later
// Tableau de bord d'un profil. La référence est le joueur lui-même : son mois le plus
// récent contre ses mois antérieurs. Trait64 travaille sur votre machine et n'a donc
// aucune moyenne d'autres joueurs à opposer — et comparer à des chiffres inventés
// vaudrait moins que de ne rien comparer.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

TDialog {
    id: root
    required property var backend
    property int profileIndex: 0
    // « data » est une propriété finale de QML : le nom est pris.
    readonly property var report: backend.skillDashboard(profileIndex)
    readonly property real scale: controller.preferences.fontScale * controller.preferences.uiScale
    title: qsTr("Bilan de vos compétences")
    width: Math.min(880, parent ? parent.width - 24 : 880)
    height: Math.min(720, parent ? parent.height - 24 : 720)
    anchors.centerIn: parent
    modal: true
    standardButtons: Dialog.Close

    // Recalcule à l'ouverture et quand un bilan vient de s'achever : les chiffres
    // viennent de la base, pas d'un cache que rien ne rafraîchirait.
    function refresh() { profileIndex = profileIndex }
    onOpened: refresh()
    Connections { target: root.backend; function onLibraryChanged() { root.refresh() } }

    function reading(value, suffix) {
        return value === undefined || value === null ? "—" : value.toFixed(1) + (suffix || "")
    }
    function month(period) {
        if (!period || period.length < 7) return qsTr("date inconnue")
        const names = [qsTr("janv."), qsTr("févr."), qsTr("mars"), qsTr("avr."), qsTr("mai"),
                       qsTr("juin"), qsTr("juil."), qsTr("août"), qsTr("sept."), qsTr("oct."),
                       qsTr("nov."), qsTr("déc.")]
        const index = parseInt(period.substring(5, 7), 10) - 1
        return index >= 0 && index < 12 ? names[index] + " " + period.substring(0, 4) : period
    }

    ScrollView {
        id: viewport; anchors.fill: parent; contentWidth: availableWidth; clip: true
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
        ColumnLayout {
            width: viewport.availableWidth; spacing: 12

            RowLayout {
                Layout.fillWidth: true; spacing: 8
                Label { text: qsTr("Profil"); visible: root.backend.profiles.length > 1 }
                TComboBox {
                    objectName: "dashboardProfile"
                    visible: root.backend.profiles.length > 1
                    Layout.preferredWidth: 200
                    model: root.backend.profiles.map(p => p.name)
                    currentIndex: root.profileIndex
                    onActivated: function(chosen) { root.profileIndex = chosen }
                }
                Item { Layout.fillWidth: true }
            }

            Label {
                Layout.fillWidth: true; wrapMode: Text.WordWrap
                visible: !root.report.ready
                text: root.report.reason || qsTr("Créez un profil pour suivre vos progrès.")
            }

            ColumnLayout {
                Layout.fillWidth: true; spacing: 12
                visible: !!root.report.ready

                Label {
                    Layout.fillWidth: true; wrapMode: Text.WordWrap
                    font.pixelSize: Math.round(11 * root.scale); opacity: .75
                    text: qsTr("%1 partie(s) passée(s) en revue, %2 coup(s) mesuré(s), sous %3.")
                              .arg(root.report.games || 0).arg(root.report.moves || 0)
                              .arg((root.report.accounts || []).join(", "))
                }

                // Note d'ensemble, et l'écart avec les mois précédents.
                Rectangle {
                    Layout.fillWidth: true; implicitHeight: headline.implicitHeight + 20
                    color: root.backend.preferences.panel; radius: 8
                    RowLayout {
                        id: headline
                        anchors.fill: parent; anchors.margins: 10; spacing: 16
                        ColumnLayout {
                            spacing: 2
                            Label { text: qsTr("Précision"); opacity: .7
                                    font.pixelSize: Math.round(11 * root.scale) }
                            Label {
                                text: root.reading(root.report.all, " %")
                                font.bold: true; font.pixelSize: Math.round(26 * root.scale)
                            }
                        }
                        ColumnLayout {
                            spacing: 2
                            visible: root.report.trend !== undefined && root.report.trend !== null
                            Label { text: qsTr("Ce mois-ci"); opacity: .7
                                    font.pixelSize: Math.round(11 * root.scale) }
                            Label {
                                readonly property real delta: root.report.trend || 0
                                text: (delta >= 0 ? "+" : "−") + Math.abs(delta).toFixed(1) + " pt"
                                color: delta >= 0 ? "#3f9a5c" : "#c05b4d"
                                font.bold: true; font.pixelSize: Math.round(20 * root.scale)
                                ToolTip.visible: hovered.hovered
                                ToolTip.text: qsTr("Écart entre votre mois le plus récent et la moyenne de vos mois précédents.")
                                HoverHandler { id: hovered }
                            }
                        }
                        Item { Layout.fillWidth: true }
                        ColumnLayout {
                            spacing: 2
                            Label { text: qsTr("Tactique"); opacity: .7
                                    font.pixelSize: Math.round(11 * root.scale) }
                            Label {
                                text: (root.report.tactics && root.report.tactics.solved > 0)
                                      ? root.report.tactics.recentSuccess + " %" : "—"
                                font.bold: true; font.pixelSize: Math.round(20 * root.scale)
                                ToolTip.visible: puzzles.hovered
                                ToolTip.text: (root.report.tactics && root.report.tactics.solved > 0)
                                    ? qsTr("Réussite sur vos 50 derniers exercices, %1 résolu(s) en tout.")
                                          .arg(root.report.tactics.solved)
                                    : qsTr("Résolvez des exercices pour mesurer votre tactique.")
                                HoverHandler { id: puzzles }
                            }
                        }
                    }
                }

                Label { text: qsTr("Par phase de la partie"); font.bold: true }
                RowLayout {
                    Layout.fillWidth: true; spacing: 8
                    Repeater {
                        model: [{key: "opening", label: qsTr("Ouverture")},
                                {key: "middlegame", label: qsTr("Milieu de jeu")},
                                {key: "endgame", label: qsTr("Finale")}]
                        delegate: Rectangle {
                            required property var modelData
                            Layout.fillWidth: true; implicitHeight: 64
                            color: root.backend.preferences.panel; radius: 8
                            ColumnLayout {
                                anchors.centerIn: parent; spacing: 2
                                Label {
                                    text: root.reading(root.report[modelData.key], " %")
                                    font.bold: true; font.pixelSize: Math.round(20 * root.scale)
                                    Layout.alignment: Qt.AlignHCenter
                                }
                                Label {
                                    text: modelData.label; opacity: .7
                                    font.pixelSize: Math.round(11 * root.scale)
                                    Layout.alignment: Qt.AlignHCenter
                                }
                            }
                        }
                    }
                }

                Label { text: qsTr("Fautes pour cent coups"); font.bold: true }
                RowLayout {
                    Layout.fillWidth: true; spacing: 8
                    Repeater {
                        model: [{key: "blunderRate", label: qsTr("Gaffes"), total: "blunders"},
                                {key: "mistakeRate", label: qsTr("Erreurs"), total: "mistakes"},
                                {key: "inaccuracyRate", label: qsTr("Imprécisions"), total: "inaccuracies"}]
                        delegate: Rectangle {
                            required property var modelData
                            Layout.fillWidth: true; implicitHeight: 58
                            color: root.backend.preferences.panel; radius: 8
                            ColumnLayout {
                                anchors.centerIn: parent; spacing: 2
                                Label {
                                    text: root.reading(root.report[modelData.key])
                                    font.bold: true; font.pixelSize: Math.round(18 * root.scale)
                                    Layout.alignment: Qt.AlignHCenter
                                }
                                Label {
                                    text: modelData.label + " (" + (root.report[modelData.total] || 0) + ")"
                                    opacity: .7; font.pixelSize: Math.round(11 * root.scale)
                                    Layout.alignment: Qt.AlignHCenter
                                }
                            }
                        }
                    }
                }

                Label {
                    text: qsTr("Mois par mois"); font.bold: true
                    visible: (root.report.history || []).length > 0
                }
                // Une barre par mois, du plus ancien au plus récent : c'est le sens de
                // lecture d'une progression. La hauteur part de 50 %, plancher au-dessous
                // duquel une partie n'est plus vraiment jouée.
                Rectangle {
                    Layout.fillWidth: true; Layout.preferredHeight: 150
                    visible: (root.report.history || []).length > 0
                    color: root.backend.preferences.panel; radius: 8
                    RowLayout {
                        anchors.fill: parent; anchors.margins: 10; spacing: 4
                        Repeater {
                            model: (root.report.history || []).slice().reverse()
                            delegate: ColumnLayout {
                                required property var modelData
                                Layout.fillWidth: true; Layout.fillHeight: true; spacing: 3
                                Label {
                                    text: modelData.accuracy === undefined || modelData.accuracy === null
                                          ? "—" : modelData.accuracy.toFixed(0)
                                    font.pixelSize: Math.round(10 * root.scale); opacity: .8
                                    Layout.alignment: Qt.AlignHCenter
                                }
                                Item {
                                    Layout.fillWidth: true; Layout.fillHeight: true
                                    Rectangle {
                                        anchors.bottom: parent.bottom
                                        anchors.horizontalCenter: parent.horizontalCenter
                                        width: Math.max(8, parent.width - 6)
                                        height: Math.max(2, parent.height *
                                                Math.max(0, Math.min(1, ((modelData.accuracy || 50) - 50) / 50)))
                                        color: root.backend.preferences.accentColor
                                        radius: 2
                                    }
                                }
                                Label {
                                    text: root.month(modelData.period)
                                    font.pixelSize: Math.round(9 * root.scale); opacity: .7
                                    Layout.alignment: Qt.AlignHCenter
                                    elide: Text.ElideRight; Layout.fillWidth: true
                                    horizontalAlignment: Text.AlignHCenter
                                }
                            }
                        }
                    }
                }

                Label {
                    Layout.fillWidth: true; wrapMode: Text.WordWrap; opacity: .7
                    font.pixelSize: Math.round(10 * root.scale)
                    text: qsTr("Les notes dépendent du moteur et de la profondeur demandée : comparez des bilans menés aux mêmes réglages. La précision d’ensemble est pondérée par le nombre de coups, celles des phases sont une moyenne par partie.")
                }
            }
        }
    }
}
