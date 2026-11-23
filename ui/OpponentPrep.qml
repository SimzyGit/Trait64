// SPDX-License-Identifier: AGPL-3.0-or-later
// Préparer n'importe quel joueur : ses parties (pseudo Lichess ou Chess.com, identifiant
// ou nom FIDE, ou nom de la bibliothèque) rassemblées dans « Préparation : <nom> », puis
// son arbre par couleur, ses choix au fil des ans, ses faiblesses et ses sorties de mon
// répertoire.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs as Native

TDialog {
    id: root
    required property var backend
    readonly property var info: backend.opponent
    readonly property bool ready: info.status === "ready"
    property bool white: true
    property var path: []
    property var sans: []
    property var tree: ({})
    objectName: "opponentPrep"
    title: qsTr("Préparer un joueur")
    width: Math.min(900, parent.width - 32); height: Math.min(760, parent.height - 32)
    anchors.centerIn: parent; modal: true; standardButtons: Dialog.Close
    function refreshTree() { tree = ready ? backend.opponentTree(white, path) : ({}) }
    function go(index) { path = path.slice(0, index); sans = sans.slice(0, index); refreshTree() }
    onReadyChanged: { path = []; sans = []; refreshTree() }
    onWhiteChanged: { path = []; sans = []; refreshTree() }
    function prepareName(name) {
        site.currentIndex = 3; query.text = name; open()
        backend.prepareOpponent("library", name)
    }
    function scoreText(score) { return score < 0 ? "—" : score + " %" }

    ColumnLayout {
        anchors.fill: parent; spacing: 10
        // Flow et largeur souhaitée minime : une rangée trop large élargirait la fenêtre.
        Flow {
            Layout.fillWidth: true; Layout.preferredWidth: 1; spacing: 8
            TComboBox {
                id: site; objectName: "opponentSite"; width: 230
                model: [qsTr("Pseudo Lichess"), qsTr("Pseudo Chess.com"), qsTr("Identifiant ou nom FIDE"), qsTr("Nom dans la bibliothèque")]
            }
            TTextField {
                id: query; objectName: "opponentQuery"; width: 260
                placeholderText: site.currentIndex === 2 ? qsTr("Ex. 1503014 ou Carlsen, Magnus") : qsTr("Pseudo ou nom")
                onAccepted: start.clicked()
            }
            TButton {
                id: start; objectName: "opponentStart"; accent: true
                text: qsTr("Préparer")
                enabled: !root.backend.busy && query.text.trim().length > 0
                onClicked: root.backend.prepareOpponent(["lichess", "chesscom", "fide", "library"][site.currentIndex], query.text)
            }
        }
        Label {
            Layout.fillWidth: true; wrapMode: Text.WordWrap; color: palette.placeholderText
            text: root.info.status === "downloading" ? qsTr("Téléchargement de ses parties, à la suite des précédentes…")
                : root.info.status === "searching" ? qsTr("Recherche du joueur…")
                : root.info.status === "error" ? (root.info.error || "")
                : root.info.status === "empty" ? qsTr("Aucune partie de %1 dans la bibliothèque.").arg(root.info.name)
                : root.ready ? qsTr("%1 · %2 partie(s) · Blancs : %3 (score %4) · Noirs : %5 (score %6)")
                                   .arg(root.info.name).arg(root.info.games).arg(root.info.whiteGames)
                                   .arg(root.scoreText(root.info.whiteScore)).arg(root.info.blackGames)
                                   .arg(root.scoreText(root.info.blackScore))
                : site.currentIndex === 2
                  ? qsTr("La FIDE ne publie pas de parties : son nom officiel sert à retrouver ses parties dans votre bibliothèque, diffusions comprises (Base → Mettre à jour la base de référence).")
                  : qsTr("Ses parties arrivent dans la base « Préparation : <nom> » ; préparer de nouveau ne télécharge que les nouvelles.")
        }
        TabBar {
            id: tabs; visible: root.ready; Layout.fillWidth: true
            TTabButton { text: qsTr("Arbre") }
            TTabButton { text: qsTr("Évolution") }
            TTabButton { text: qsTr("Faiblesses") }
            TTabButton { text: qsTr("Face à mon répertoire") }
        }
        StackLayout {
            visible: root.ready; Layout.fillWidth: true; Layout.fillHeight: true
            currentIndex: tabs.currentIndex
            // L'arbre : ses coups à chaque position, avec la couleur qu'il a.
            ColumnLayout {
                spacing: 6
                Flow {
                    Layout.fillWidth: true; Layout.preferredWidth: 1; spacing: 6
                    TChip { objectName: "opponentWhite"; text: qsTr("Avec les Blancs"); checked: root.white; onToggled: root.white = true }
                    TChip { text: qsTr("Avec les Noirs"); checked: !root.white; onToggled: root.white = false }
                    TButton { text: qsTr("Départ"); flat: true; onClicked: root.go(0) }
                    Repeater {
                        model: root.sans
                        TButton {
                            required property string modelData
                            required property int index
                            text: (index % 2 === 0 ? (Math.floor(index / 2) + 1) + "." : "") + modelData
                            flat: true; onClicked: root.go(index + 1)
                        }
                    }
                }
                Label {
                    Layout.fillWidth: true; color: palette.placeholderText
                    text: qsTr("%1 partie(s) ici · %2").arg(root.tree.games || 0)
                          .arg(root.tree.hisTurn ? qsTr("à lui de jouer") : qsTr("à son adversaire de jouer"))
                }
                ListView {
                    id: treeList; objectName: "opponentTree"
                    Layout.fillWidth: true; Layout.fillHeight: true; clip: true; spacing: 2
                    model: root.tree.moves || []
                    ScrollBar.vertical: ScrollBar {}
                    delegate: Rectangle {
                        required property var modelData
                        width: treeList.width; height: 30
                        color: moveHover.hovered ? Qt.rgba(palette.highlight.r, palette.highlight.g, palette.highlight.b, .15) : "transparent"
                        HoverHandler { id: moveHover; cursorShape: Qt.PointingHandCursor }
                        TapHandler { onTapped: { root.path = root.path.concat([modelData.uci]); root.sans = root.sans.concat([modelData.san]); root.refreshTree() } }
                        RowLayout {
                            anchors.fill: parent; anchors.leftMargin: 8; anchors.rightMargin: 8; spacing: 10
                            Label { text: modelData.san; font.weight: Font.DemiBold; Layout.preferredWidth: 70 }
                            Label { text: qsTr("%1 partie(s)").arg(modelData.games); Layout.preferredWidth: 100; color: palette.placeholderText }
                            ProgressBar { from: 0; to: 100; value: Math.max(0, modelData.score); Layout.fillWidth: true }
                            Label { text: root.scoreText(modelData.score); Layout.preferredWidth: 50; horizontalAlignment: Text.AlignRight }
                            Label { text: modelData.last || ""; Layout.preferredWidth: 90; color: palette.placeholderText; horizontalAlignment: Text.AlignRight }
                        }
                    }
                }
                TButton {
                    objectName: "opponentOpenLine"; text: qsTr("Ouvrir cette position sur l’échiquier")
                    enabled: root.path.length > 0
                    onClicked: { root.backend.openLine(root.path); root.close() }
                }
            }
            // Son premier coup avec chaque couleur, année par année.
            ListView {
                id: years; clip: true; spacing: 4
                model: root.ready && tabs.currentIndex === 1 ? (root.backend.opponentEvolution().years || []) : []
                ScrollBar.vertical: ScrollBar {}
                delegate: RowLayout {
                    required property var modelData
                    width: years.width; spacing: 12
                    Label { text: modelData.year; font.weight: Font.DemiBold; Layout.preferredWidth: 60 }
                    Label {
                        Layout.fillWidth: true; elide: Text.ElideRight
                        text: modelData.white && modelData.white.move
                              ? qsTr("Blancs : %1 (%2 sur %3)").arg(modelData.white.move).arg(modelData.white.games).arg(modelData.white.total) : ""
                    }
                    Label {
                        Layout.fillWidth: true; elide: Text.ElideRight
                        text: modelData.black && modelData.black.move
                              ? qsTr("Noirs : %1 (%2 sur %3)").arg(modelData.black.move).arg(modelData.black.games).arg(modelData.black.total) : ""
                    }
                }
            }
            // Ce qu'on observe : les ouvertures jouées souvent où il marque peu.
            ColumnLayout {
                Label {
                    Layout.fillWidth: true; wrapMode: Text.WordWrap; color: palette.placeholderText
                    text: qsTr("Ouvertures (code ECO) jouées au moins cinq fois où il marque moins de 40 %. Ce sont des constats sur ses parties, pas des prédictions.")
                }
                ListView {
                    id: weak; objectName: "opponentWeaknesses"
                    Layout.fillWidth: true; Layout.fillHeight: true; clip: true; spacing: 4
                    model: root.ready && tabs.currentIndex === 2 ? root.backend.opponentWeaknesses() : []
                    delegate: Label {
                        required property var modelData
                        width: weak.width
                        text: qsTr("%1 avec les %2 : %3 partie(s), %4 %").arg(modelData.eco)
                              .arg(modelData.white ? qsTr("Blancs") : qsTr("Noirs")).arg(modelData.games).arg(modelData.score)
                    }
                    Label { anchors.centerIn: parent; visible: weak.count === 0; text: qsTr("Aucune ouverture faible relevée.") }
                }
            }
            // Où il sort de ma préparation, à son trait.
            ColumnLayout {
                spacing: 6
                TComboBox {
                    id: repertoire; objectName: "opponentRepertoire"; Layout.fillWidth: true
                    textRole: "name"; valueRole: "id"; model: root.backend.repertoires
                }
                readonly property var gap: root.ready && tabs.currentIndex === 3 && repertoire.currentValue
                                           ? root.backend.opponentDeviations(repertoire.currentValue) : ({})
                Label {
                    Layout.fillWidth: true; wrapMode: Text.WordWrap; color: palette.placeholderText
                    text: root.backend.repertoires.length === 0 ? qsTr("Créez d’abord un répertoire dans la page Études.")
                        : qsTr("%1 de ses parties passent par ce répertoire ; il en sort dans %2.")
                          .arg(parent.gap.touched || 0).arg(parent.gap.left || 0)
                }
                ListView {
                    id: gapList; objectName: "opponentDeviations"
                    Layout.fillWidth: true; Layout.fillHeight: true; clip: true; spacing: 2
                    model: parent.gap.rows || []
                    delegate: RowLayout {
                        required property var modelData
                        width: gapList.width; spacing: 10
                        Label {
                            Layout.fillWidth: true; elide: Text.ElideRight
                            text: qsTr("Coup %1 : %2 au lieu de %3").arg(modelData.move).arg(modelData.played)
                                  .arg((modelData.expected || []).join(", ") || qsTr("rien de préparé"))
                                  + " · " + modelData.white + " – " + modelData.black + " " + (modelData.date || "")
                        }
                        TButton { text: qsTr("Ouvrir"); onClicked: { root.backend.loadGameAtPly(modelData.id, modelData.ply); root.close() } }
                    }
                }
            }
        }
        TButton {
            objectName: "opponentPdf"; visible: root.ready; text: qsTr("Dossier PDF…")
            onClicked: pdfFile.open()
        }
    }
    Native.FileDialog {
        id: pdfFile; title: qsTr("Enregistrer le dossier de préparation"); fileMode: Native.FileDialog.SaveFile
        defaultSuffix: "pdf"; nameFilters: ["PDF (*.pdf)"]
        onAccepted: root.backend.exportOpponentPdf(selectedFile.toString())
    }
}
