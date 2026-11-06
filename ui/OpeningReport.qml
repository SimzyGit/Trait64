// SPDX-License-Identifier: AGPL-3.0-or-later
// Le dossier d'une position : ce que la bibliothèque sait de ce qui s'y joue. Les coups
// avec leur score, le même découpage par force des joueurs — un coup qui réussit au club
// et disparaît chez les maîtres ne se voit pas autrement — et les parties modèles.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

TDialog {
    id: root
    required property var backend
    readonly property var p: backend.preferences
    readonly property var report: backend.openingReport || ({})
    readonly property var moves: report.moves || []
    readonly property real total: Number(report.total || 0)

    title: qsTr("Dossier de la position")
    modal: true
    width: Math.min(820, parent ? parent.width - 48 : 820)
    height: Math.min(720, parent ? parent.height - 64 : 720)

    onOpened: backend.buildOpeningReport()

    // Les largeurs suivent la taille du texte choisie dans les préférences.
    function w(base) { return Math.round(base * p.fontScale * p.uiScale) }
    function percent(part) {
        return root.total > 0 ? Math.round(100 * Number(part || 0) / root.total) + " %" : "—"
    }
    // Le score du point de vue des Blancs, comme partout ailleurs dans le logiciel.
    function score(move) {
        const played = Number(move.count || 0)
        if (!played)
            return "—"
        return Math.round(100 * (Number(move.white || 0) + Number(move.draw || 0) / 2) / played) + " %"
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 12

        Label {
            Layout.fillWidth: true
            text: root.report.name || qsTr("Position hors des ouvertures nommées")
            font.pixelSize: Math.round(15 * root.p.fontScale * root.p.uiScale)
            font.weight: Font.DemiBold
            color: root.p.foreground
            wrapMode: Text.WordWrap
        }

        Label {
            Layout.fillWidth: true
            visible: !!root.report.running
            text: qsTr("Recherche dans la bibliothèque…")
            color: root.p.muted
        }
        Label {
            Layout.fillWidth: true
            visible: !!root.report.error
            text: root.backend.uiText(root.report.error || "")
            color: "#cf3d3d"
            wrapMode: Text.WordWrap
        }
        Label {
            Layout.fillWidth: true
            visible: !root.report.running && !root.report.error && root.total === 0
            // Zéro partie n'est pas une panne : c'est une nouveauté, dans cette base.
            text: qsTr("Aucune partie de votre bibliothèque n’est passée par cette position. "
                       + "C’est une nouveauté pour elle — pas nécessairement pour la théorie.")
            color: root.p.muted
            wrapMode: Text.WordWrap
        }

        Label {
            Layout.fillWidth: true
            visible: root.total > 0
            text: qsTr("%1 partie(s) · Blancs %2 · nulles %3 · Noirs %4")
                    .arg(root.total)
                    .arg(root.percent(root.report.whiteWins))
                    .arg(root.percent(root.report.draws))
                    .arg(root.percent(root.report.blackWins))
            color: root.p.muted
        }

        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: root.total > 0
            clip: true
            ColumnLayout {
                width: parent.width
                spacing: 10

                // Une seule source pour les largeurs : l'en-tête et les lignes ne
                // peuvent pas diverger, et elles suivent la taille du texte — « Tournament »
                // est plus large que « Tournoi » et débordait sur la colonne suivante.
                RowLayout {
                    Layout.fillWidth: true; spacing: 8
                    Label {text: qsTr("Coup"); Layout.preferredWidth: root.w(72); color: root.p.muted}
                    Label {text: qsTr("Parties"); Layout.preferredWidth: root.w(72); color: root.p.muted}
                    Label {text: qsTr("Score blanc"); Layout.preferredWidth: root.w(100); color: root.p.muted}
                    Label {text: qsTr("Club"); Layout.preferredWidth: root.w(70); color: root.p.muted}
                    Label {text: qsTr("Tournoi"); Layout.preferredWidth: root.w(110); color: root.p.muted}
                    Label {text: qsTr("Maîtres"); Layout.preferredWidth: root.w(90); color: root.p.muted}
                    Label {text: qsTr("Dernière"); Layout.fillWidth: true; color: root.p.muted}
                }
                Repeater {
                    model: root.moves
                    RowLayout {
                        required property var modelData
                        Layout.fillWidth: true; spacing: 8
                        Label {text: modelData.san || ""; Layout.preferredWidth: root.w(72)
                               font.weight: Font.DemiBold; color: root.p.foreground; elide: Text.ElideRight}
                        Label {text: modelData.count || 0; Layout.preferredWidth: root.w(72); color: root.p.foreground}
                        Label {text: root.score(modelData); Layout.preferredWidth: root.w(100); color: root.p.foreground}
                        Label {text: (modelData.bands && modelData.bands.club) || "—"
                               Layout.preferredWidth: root.w(70); color: root.p.muted}
                        Label {text: (modelData.bands && modelData.bands.tournoi) || "—"
                               Layout.preferredWidth: root.w(110); color: root.p.muted}
                        Label {text: (modelData.bands && modelData.bands.maitre) || "—"
                               Layout.preferredWidth: root.w(90); color: root.p.muted}
                        Label {text: modelData.last || "—"; Layout.fillWidth: true; color: root.p.muted
                               elide: Text.ElideRight}
                    }
                }

                Label {
                    Layout.fillWidth: true
                    Layout.topMargin: 8
                    visible: (root.report.models || []).length > 0
                    text: qsTr("Parties modèles")
                    font.weight: Font.DemiBold
                    color: root.p.foreground
                }
                Repeater {
                    model: root.report.models || []
                    Label {
                        required property var modelData
                        Layout.fillWidth: true
                        // Le PGN note une date inconnue « ????.??.?? » ; un tiret se lit mieux.
                        readonly property string day: modelData.date && modelData.date.indexOf("?") < 0
                                                      ? modelData.date : "—"
                        text: (modelData.white || "?") + " — " + (modelData.black || "?") + "   "
                              + (modelData.result || "*") + "   " + day
                              + (modelData.event ? "   " + modelData.event : "")
                        color: hover.hovered ? root.p.accent : root.p.foreground
                        elide: Text.ElideRight
                        HoverHandler {id: hover; cursorShape: Qt.PointingHandCursor}
                        TapHandler {onTapped: {root.backend.loadGame(modelData.id); root.close()}}
                    }
                }
            }
        }

        Item {Layout.fillHeight: true; Layout.fillWidth: true; visible: root.total === 0}
        RowLayout {
            Layout.fillWidth: true
            Item {Layout.fillWidth: true}
            TButton {text: qsTr("Fermer"); onClicked: root.close()}
        }
    }
}
