// SPDX-License-Identifier: AGPL-3.0-or-later
// La base de référence : les diffusions Lichess à jour, et les grandes bases libres que
// l'on télécharge soi-même (base élite de Lichess, TWIC).
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs as Native

TDialog {
    id: root
    required property var backend
    readonly property var ref: backend.reference
    property string importKind: "elite"
    objectName: "referenceBase"
    title: qsTr("Base de référence")
    width: Math.min(640, parent.width - 32)
    anchors.centerIn: parent; modal: true; standardButtons: Dialog.Close
    ColumnLayout {
        anchors.fill: parent; spacing: 10
        Label { text: qsTr("Diffusions Lichess"); font.weight: Font.DemiBold }
        Label {
            Layout.fillWidth: true; wrapMode: Text.WordWrap; color: palette.placeholderText
            text: qsTr("Les rondes terminées des tournois retransmis sur Lichess (données libres, CC0), ajoutées sans doublon à la base « Référence — diffusions Lichess ». Seules les nouvelles rondes sont téléchargées.")
        }
        Label {
            objectName: "referenceLast"; Layout.fillWidth: true; wrapMode: Text.WordWrap
            text: (root.ref.last > 0
                   ? qsTr("Dernière mise à jour : %1").arg(new Date(root.ref.last).toLocaleString(Qt.locale(controller.preferences.language === "auto" ? "" : controller.preferences.language), Locale.ShortFormat))
                   : qsTr("Jamais mise à jour."))
                  + (root.ref.status ? " · " + root.ref.status : "")
        }
        Flow {
            Layout.fillWidth: true; Layout.preferredWidth: 1; spacing: 8
            TButton {
                objectName: "updateReference"; accent: true
                text: root.ref.running ? qsTr("Mise à jour…") : qsTr("Mettre à jour maintenant")
                enabled: !root.ref.running && !root.backend.busy
                onClicked: root.backend.updateReference()
            }
            TCheckBox {
                text: qsTr("Au démarrage"); checked: !!root.ref.atStartup
                onToggled: root.backend.setReferenceAtStartup(checked)
            }
        }
        Rectangle { Layout.fillWidth: true; height: 1; color: palette.mid; opacity: .4 }
        Label { text: qsTr("Grandes bases à télécharger"); font.weight: Font.DemiBold }
        Label {
            Layout.fillWidth: true; wrapMode: Text.WordWrap; color: palette.placeholderText
            text: qsTr("Téléchargez le fichier sur le site, puis importez-le ici : il rejoint sa base de référence sans doublon. La base élite de Lichess rassemble chaque mois les parties des joueurs à plus de 2400. Les conditions de réutilisation de TWIC sont fixées par son auteur : vérifiez-les sur son site.")
        }
        GridLayout {
            columns: 3; columnSpacing: 8; rowSpacing: 6; Layout.fillWidth: true
            Label { text: qsTr("Base élite Lichess"); Layout.fillWidth: true }
            TButton { text: qsTr("Page de téléchargement"); onClicked: Qt.openUrlExternally("https://database.nikonoel.fr/") }
            TButton { objectName: "importElite"; text: qsTr("Importer…"); enabled: !root.backend.busy; onClicked: { root.importKind = "elite"; file.open() } }
            Label { text: qsTr("TWIC (The Week in Chess)"); Layout.fillWidth: true }
            TButton { text: qsTr("Page de téléchargement"); onClicked: Qt.openUrlExternally("https://theweekinchess.com/twic") }
            TButton { text: qsTr("Importer…"); enabled: !root.backend.busy; onClicked: { root.importKind = "twic"; file.open() } }
        }
        Label {
            Layout.fillWidth: true; wrapMode: Text.WordWrap; color: palette.placeholderText
            text: qsTr("Pour suivre une partie en direct, Base → Tournois en direct ouvre les diffusions en cours : Suivre place la partie sur l’échiquier, coup après coup.")
        }
    }
    Native.FileDialog {
        id: file; title: qsTr("Fichier PGN à importer")
        nameFilters: [qsTr("Parties (*.pgn *.zip *.gz *.zst *.bz2 *.xz)"), qsTr("Tous les fichiers (*)")]
        onAccepted: root.backend.importReferenceFile(selectedFile.toString(), root.importKind)
    }
}
