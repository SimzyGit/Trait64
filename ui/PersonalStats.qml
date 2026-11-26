// SPDX-License-Identifier: AGPL-3.0-or-later
// Les statistiques d'un profil, tous ses comptes réunis : force par cadence (en ligne et
// FIDE), performance par ouverture, régularité des douze dernières semaines.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ScrollView {
    id: root
    required property var backend
    property int profileIndex: 0
    property var stats: ({})
    readonly property var names: ({bullet: qsTr("Bullet"), blitz: qsTr("Blitz"), rapid: qsTr("Rapide"),
                                   classical: qsTr("Lente"), correspondence: qsTr("Par correspondance")})
    function refresh() { stats = backend.personalStats(profileIndex) }
    onProfileIndexChanged: refresh()
    Component.onCompleted: refresh()
    Connections { target: root.backend; function onLibraryChanged() { root.refresh() } }
    contentWidth: availableWidth; clip: true
    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
    ColumnLayout {
        width: root.availableWidth; spacing: 12
        Label {
            visible: !root.stats.ready || ((root.stats.cadences || []).length === 0)
            Layout.fillWidth: true; wrapMode: Text.WordWrap; color: palette.placeholderText
            text: qsTr("Aucune partie de ce profil dans la bibliothèque : rattachez-lui un compte et synchronisez-le.")
        }
        Label { text: qsTr("Force par cadence"); font.weight: Font.DemiBold; visible: (root.stats.cadences || []).length > 0 }
        GridLayout {
            columns: 5; columnSpacing: 16; rowSpacing: 4; Layout.fillWidth: true
            visible: (root.stats.cadences || []).length > 0
            Label { text: qsTr("Cadence"); color: palette.placeholderText }
            Label { text: qsTr("Classement"); color: palette.placeholderText }
            Label { text: qsTr("Moyenne récente"); color: palette.placeholderText }
            Label { text: qsTr("Parties"); color: palette.placeholderText }
            Label { text: qsTr("Score"); color: palette.placeholderText }
            Repeater {
                model: (root.stats.cadences || []).length * 5
                Label {
                    required property int index
                    readonly property var row: root.stats.cadences[Math.floor(index / 5)]
                    readonly property int column: index % 5
                    text: column === 0 ? (root.names[row.cadence] || row.cadence)
                        : column === 1 ? (row.rating > 0 ? row.rating : "—")
                        : column === 2 ? (row.recent > 0 ? row.recent : "—")
                        : column === 3 ? row.games
                        : (row.score >= 0 ? row.score + " %" : "—")
                    font.weight: column === 1 ? Font.DemiBold : Font.Normal
                }
            }
        }
        Label {
            visible: Object.keys(root.stats.fide || {}).length > 0
            Layout.fillWidth: true; wrapMode: Text.WordWrap
            text: qsTr("FIDE : %1").arg(Object.keys(root.stats.fide || {}).map(k => ({standard: qsTr("lente"), rapid: qsTr("rapide"), blitz: qsTr("blitz")})[k] + " " + root.stats.fide[k]).join(" · "))
        }
        Label {
            Layout.fillWidth: true; wrapMode: Text.WordWrap; color: palette.placeholderText
            visible: (root.stats.cadences || []).length > 0
            text: qsTr("Classement : le dernier inscrit dans vos parties ; moyenne récente : celle des vingt derniers. La cadence se lit dans le contrôle du temps de chaque partie.")
        }
        Label { text: qsTr("Régularité des douze dernières semaines"); font.weight: Font.DemiBold; visible: (root.stats.weeks || []).length > 0 }
        Row {
            objectName: "activityWeeks"; spacing: 4; height: 70
            visible: (root.stats.weeks || []).length > 0
            readonly property int most: Math.max(1, ...(root.stats.weeks || []).map(w => w.games))
            Repeater {
                model: root.stats.weeks || []
                Item {
                    required property var modelData
                    width: 22; height: 70
                    Rectangle {
                        anchors.bottom: parent.bottom; anchors.bottomMargin: 14; width: parent.width
                        height: Math.max(2, 52 * modelData.games / parent.parent.most); radius: 2
                        color: modelData.games > 0 ? palette.highlight : palette.mid; opacity: modelData.games > 0 ? .8 : .3
                        ToolTip.visible: barHover.hovered
                        ToolTip.text: modelData.weeksAgo === 0 ? qsTr("Cette semaine : %1 partie(s)").arg(modelData.games)
                                                               : qsTr("Il y a %1 semaine(s) : %2 partie(s)").arg(modelData.weeksAgo).arg(modelData.games)
                        HoverHandler { id: barHover }
                    }
                    Label { anchors.bottom: parent.bottom; anchors.horizontalCenter: parent.horizontalCenter
                            text: modelData.games; font.pixelSize: 10; color: palette.placeholderText }
                }
            }
        }
        Label { text: qsTr("Performance par ouverture"); font.weight: Font.DemiBold; visible: (root.stats.openings || []).length > 0 }
        Repeater {
            model: root.stats.openings || []
            RowLayout {
                required property var modelData
                Layout.fillWidth: true; spacing: 12
                // Le nom de l'ouverture d'abord, son code ECO à côté, plus discret.
                Label { text: modelData.name || modelData.eco; font.weight: Font.DemiBold; elide: Text.ElideRight
                        Layout.preferredWidth: 230; Layout.minimumWidth: 80
                        ToolTip.visible: nameHover.hovered && truncated; ToolTip.text: text
                        HoverHandler { id: nameHover } }
                Label { text: modelData.name ? modelData.eco : ""; color: palette.placeholderText; Layout.preferredWidth: 34 }
                Label { text: qsTr("%1 partie(s)").arg(modelData.games); Layout.preferredWidth: 90; color: palette.placeholderText }
                ProgressBar { from: 0; to: 100; value: modelData.score; Layout.fillWidth: true }
                Label { text: modelData.score + " %"; Layout.preferredWidth: 50; horizontalAlignment: Text.AlignRight }
                Label {
                    text: modelData.performance ? qsTr("perf. %1").arg(modelData.performance) : ""
                    Layout.preferredWidth: 80; horizontalAlignment: Text.AlignRight; color: palette.placeholderText
                }
            }
        }
    }
}
