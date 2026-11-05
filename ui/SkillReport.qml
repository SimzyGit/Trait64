// SPDX-License-Identifier: AGPL-3.0-or-later
// Le bilan d'un profil, sans habillage : la référence est le joueur lui-même, son mois
// le plus récent contre ses mois antérieurs. Trait64 travaille sur votre machine et n'a
// donc aucune moyenne d'autres joueurs à opposer — et comparer à des chiffres inventés
// vaudrait moins que de ne rien comparer.
// Extrait de la boîte de dialogue pour servir aussi la page des profils : deux copies
// auraient fini par diverger.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    required property var backend
    property int profileIndex: 0
    // Les chiffres viennent d'une requête, pas d'une propriété observable : une liaison
    // ne saurait pas quand les relire. Ils sont donc lus explicitement à l'apparition,
    // au changement de profil, et quand un bilan vient d'alimenter la base.
    // (« data » est un nom pris : c'est une propriété finale de QML.)
    property var report: ({})
    // « scale » appartient au type de base : un nom à soi évite de l'écraser.
    readonly property real uiZoom: controller.preferences.fontScale * controller.preferences.uiScale

    function refresh() { report = backend.skillDashboard(profileIndex) }
    onVisibleChanged: if (visible) refresh()
    Component.onCompleted: if (visible) refresh()
    onProfileIndexChanged: refresh()
    Connections {
        target: root.backend
        // Inutile de recalculer un écran masqué : la lecture parcourt toute la base.
        function onLibraryChanged() { if (root.visible) root.refresh() }
    }

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
                // Les trois cas attendus arrivent sous forme de code ; tout autre texte
                // est le message d'une erreur, que l'on affiche tel quel.
                text: ({
                    noProfile: qsTr("Choisissez un profil."),
                    noAccount: qsTr("Rattachez un compte à ce profil pour mesurer vos parties."),
                    noReview: qsTr("Aucune partie passée en revue. Lancez un bilan depuis l’écran Comptes.")
                })[root.report.reason] || root.report.reason
                       || qsTr("Créez un profil pour suivre vos progrès.")
            }

            ColumnLayout {
                Layout.fillWidth: true; spacing: 12
                visible: !!root.report.ready

                Label {
                    Layout.fillWidth: true; wrapMode: Text.WordWrap
                    font.pixelSize: Math.round(11 * root.uiZoom); opacity: .75
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
                                    font.pixelSize: Math.round(11 * root.uiZoom) }
                            Label {
                                text: root.reading(root.report.all, " %")
                                font.bold: true; font.pixelSize: Math.round(26 * root.uiZoom)
                            }
                        }
                        ColumnLayout {
                            spacing: 2
                            visible: root.report.trend !== undefined && root.report.trend !== null
                            Label { text: qsTr("Ce mois-ci"); opacity: .7
                                    font.pixelSize: Math.round(11 * root.uiZoom) }
                            Label {
                                readonly property real delta: root.report.trend || 0
                                text: (delta >= 0 ? "+" : "−") + Math.abs(delta).toFixed(1) + " pt"
                                color: delta >= 0 ? "#3f9a5c" : "#c05b4d"
                                font.bold: true; font.pixelSize: Math.round(20 * root.uiZoom)
                                ToolTip.visible: hovered.hovered
                                ToolTip.text: qsTr("Écart entre votre mois le plus récent et la moyenne de vos mois précédents.")
                                HoverHandler { id: hovered }
                            }
                        }
                        Item { Layout.fillWidth: true }
                        ColumnLayout {
                            spacing: 2
                            Label { text: qsTr("Tactique"); opacity: .7
                                    font.pixelSize: Math.round(11 * root.uiZoom) }
                            Label {
                                text: (root.report.tactics && root.report.tactics.solved > 0)
                                      ? root.report.tactics.recentSuccess + " %" : "—"
                                font.bold: true; font.pixelSize: Math.round(20 * root.uiZoom)
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
                                    font.bold: true; font.pixelSize: Math.round(20 * root.uiZoom)
                                    Layout.alignment: Qt.AlignHCenter
                                }
                                Label {
                                    text: modelData.label; opacity: .7
                                    font.pixelSize: Math.round(11 * root.uiZoom)
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
                                    font.bold: true; font.pixelSize: Math.round(18 * root.uiZoom)
                                    Layout.alignment: Qt.AlignHCenter
                                }
                                Label {
                                    text: modelData.label + " (" + (root.report[modelData.total] || 0) + ")"
                                    opacity: .7; font.pixelSize: Math.round(11 * root.uiZoom)
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
                                    font.pixelSize: Math.round(10 * root.uiZoom); opacity: .8
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
                                    font.pixelSize: Math.round(9 * root.uiZoom); opacity: .7
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
                    font.pixelSize: Math.round(10 * root.uiZoom)
                    text: qsTr("Les notes dépendent du moteur et de la profondeur demandée : comparez des bilans menés aux mêmes réglages. La précision d’ensemble est pondérée par le nombre de coups, celles des phases sont une moyenne par partie.")
                }
            }
        }
    }
}
