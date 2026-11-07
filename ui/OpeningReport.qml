// SPDX-License-Identifier: AGPL-3.0-or-later
// Le dossier d'une position, lu dans l'une des trois bases : la vôtre, celle des parties
// classées de Lichess, celle des parties de maîtres sur l'échiquier. Les deux dernières
// sont interrogées en ligne, sans rien télécharger — quelques millions de parties sur
// l'échiquier, quelques milliards sur le site, pour la même question : qu'est-ce qui se
// joue ici, et qu'est-ce que ça donne.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

TDialog {
    id: root
    required property var backend
    readonly property var p: backend.preferences
    // 0 : la bibliothèque. 1 : Lichess. 2 : les maîtres.
    property int chosen: 0
    readonly property bool online: chosen > 0
    readonly property string source: chosen === 2 ? "masters" : "lichess"
    readonly property var report: online ? (backend.onlineReport || ({}))
                                         : (backend.openingReport || ({}))
    readonly property var moves: report.moves || []
    readonly property var games: online ? (report.games || []) : (report.models || [])
    readonly property real total: Number(report.total || 0)

    title: qsTr("Dossier de la position")
    modal: true
    width: Math.min(880, parent ? parent.width - 48 : 880)
    height: Math.min(720, parent ? parent.height - 64 : 720)

    function load() {
        if (!root.online) {
            root.backend.buildOpeningReport()
            return
        }
        root.backend.buildOnlineReport(root.source, {
            speeds: speed.currentIndex === 0 ? [] : speed.model[speed.currentIndex].values,
            ratings: floor.currentIndex === 0 ? [] : floor.model[floor.currentIndex].values
        })
    }
    onOpened: load()
    onClosed: backend.cancelOnlineReport()
    onChosenChanged: load()

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

        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            Label {
                Layout.fillWidth: true
                text: root.report.name || qsTr("Position hors des ouvertures nommées")
                font.pixelSize: Math.round(15 * root.p.fontScale * root.p.uiScale)
                font.weight: Font.DemiBold
                color: root.p.foreground
                elide: Text.ElideRight
            }
            TComboBox {
                objectName: "reportSource"
                Layout.preferredWidth: root.w(220)
                currentIndex: root.chosen
                onActivated: root.chosen = currentIndex
                model: [qsTr("Ma bibliothèque"), qsTr("Lichess — toutes parties"),
                        qsTr("Lichess — maîtres (échiquier)")]
            }
        }

        // Les filtres de Lichess, et seulement les siens : les parties de maîtres ne sont
        // ni classées par cadence ni réparties par Elo du compte.
        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            visible: root.chosen === 1
            Label {text: qsTr("Cadence"); color: root.p.muted}
            TComboBox {
                id: speed
                objectName: "reportSpeeds"
                Layout.preferredWidth: root.w(190)
                textRole: "label"
                translateOptions: false
                onActivated: root.load()
                model: [{label: qsTr("Toutes"), values: []},
                        {label: qsTr("Blitz et rapide"), values: ["blitz", "rapid"]},
                        {label: qsTr("Classique et correspondance"),
                         values: ["classical", "correspondence"]},
                        {label: qsTr("Bullet"), values: ["ultraBullet", "bullet"]}]
            }
            Label {text: qsTr("Niveau"); color: root.p.muted}
            TComboBox {
                id: floor
                objectName: "reportRatings"
                Layout.preferredWidth: root.w(150)
                textRole: "label"
                translateOptions: false
                onActivated: root.load()
                model: [{label: qsTr("Tous"), values: []},
                        {label: qsTr("1600 et plus"),
                         values: ["1600", "1800", "2000", "2200", "2500"]},
                        {label: qsTr("2000 et plus"), values: ["2000", "2200", "2500"]},
                        {label: qsTr("2200 et plus"), values: ["2200", "2500"]}]
            }
            Item {Layout.fillWidth: true}
        }

        Label {
            Layout.fillWidth: true
            visible: !!root.report.running
            text: root.online ? qsTr("Interrogation de Lichess…") : qsTr("Recherche dans la bibliothèque…")
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
            text: root.online
                  ? qsTr("Aucune partie de cette base n’est passée par cette position.")
                  : qsTr("Aucune partie de votre bibliothèque n’est passée par cette position. "
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
                    Label {text: qsTr("Parties"); Layout.preferredWidth: root.w(86); color: root.p.muted}
                    Label {text: qsTr("Score blanc"); Layout.preferredWidth: root.w(100); color: root.p.muted}
                    Label {visible: root.online; text: qsTr("Classement moyen")
                           Layout.preferredWidth: root.w(150); color: root.p.muted}
                    Label {visible: !root.online; text: qsTr("Club"); Layout.preferredWidth: root.w(70)
                           color: root.p.muted}
                    Label {visible: !root.online; text: qsTr("Tournoi"); Layout.preferredWidth: root.w(110)
                           color: root.p.muted}
                    Label {visible: !root.online; text: qsTr("Maîtres"); Layout.preferredWidth: root.w(90)
                           color: root.p.muted}
                    Label {visible: !root.online; text: qsTr("Dernière"); Layout.fillWidth: true
                           color: root.p.muted}
                    Item {visible: root.online; Layout.fillWidth: true}
                }
                Repeater {
                    model: root.moves
                    RowLayout {
                        required property var modelData
                        Layout.fillWidth: true; spacing: 8
                        Label {text: modelData.san || ""; Layout.preferredWidth: root.w(72)
                               font.weight: Font.DemiBold; color: root.p.foreground; elide: Text.ElideRight}
                        Label {text: modelData.count || 0; Layout.preferredWidth: root.w(86); color: root.p.foreground}
                        Label {text: root.score(modelData); Layout.preferredWidth: root.w(100); color: root.p.foreground}
                        Label {visible: root.online
                               text: Number(modelData.rating || 0) > 0 ? modelData.rating : "—"
                               Layout.preferredWidth: root.w(150); color: root.p.muted}
                        Label {visible: !root.online; text: (modelData.bands && modelData.bands.club) || "—"
                               Layout.preferredWidth: root.w(70); color: root.p.muted}
                        Label {visible: !root.online; text: (modelData.bands && modelData.bands.tournoi) || "—"
                               Layout.preferredWidth: root.w(110); color: root.p.muted}
                        Label {visible: !root.online; text: (modelData.bands && modelData.bands.maitre) || "—"
                               Layout.preferredWidth: root.w(90); color: root.p.muted}
                        Label {visible: !root.online; text: modelData.last || "—"; Layout.fillWidth: true
                               color: root.p.muted; elide: Text.ElideRight}
                        Item {visible: root.online; Layout.fillWidth: true}
                    }
                }

                Label {
                    Layout.fillWidth: true
                    Layout.topMargin: 8
                    visible: root.games.length > 0
                    text: root.online ? qsTr("Parties en vue · cliquez pour la rapatrier dans la bibliothèque")
                                      : qsTr("Parties modèles")
                    font.weight: Font.DemiBold
                    color: root.p.foreground
                }
                Repeater {
                    model: root.games
                    Label {
                        required property var modelData
                        Layout.fillWidth: true
                        // Le PGN note une date inconnue « ????.??.?? » ; un tiret se lit mieux.
                        readonly property string day: modelData.date && modelData.date.indexOf("?") < 0
                                                      ? modelData.date : "—"
                        // En ligne, le classement des deux joueurs dit à quel niveau la
                        // partie s'est jouée : c'est ce qui manque le plus à une liste de noms.
                        readonly property string elo: Number(modelData.whiteElo || 0) > 0
                                                      ? " (" + modelData.whiteElo + "/" + modelData.blackElo + ")"
                                                      : ""
                        text: (modelData.white || "?") + " — " + (modelData.black || "?") + elo + "   "
                              + (modelData.result || "*") + "   " + day
                              + (modelData.event ? "   " + modelData.event : "")
                        color: hover.hovered ? root.p.accent : root.p.foreground
                        elide: Text.ElideRight
                        HoverHandler {id: hover; cursorShape: Qt.PointingHandCursor}
                        TapHandler {
                            onTapped: {
                                if (root.online)
                                    root.backend.importOnlineGame(String(modelData.id))
                                else
                                    root.backend.loadGame(modelData.id)
                                root.close()
                            }
                        }
                    }
                }
            }
        }

        Item {Layout.fillHeight: true; Layout.fillWidth: true; visible: root.total === 0}
        RowLayout {
            Layout.fillWidth: true
            Label {
                Layout.fillWidth: true
                visible: root.online
                wrapMode: Text.WordWrap
                color: root.p.muted
                font.pixelSize: Math.round(11 * root.p.fontScale * root.p.uiScale)
                text: qsTr("Données de l’explorateur d’ouvertures de Lichess (CC0), interrogé en direct.")
            }
            Item {Layout.fillWidth: true; visible: !root.online}
            TButton {text: qsTr("Fermer"); onClicked: root.close()}
        }
    }
}
