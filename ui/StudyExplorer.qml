// SPDX-License-Identifier: AGPL-3.0-or-later
// L'explorateur d'études, à la manière d'En Croissant : vos PGN restent des fichiers sur
// le disque, et c'est le dossier qu'on parcourt, pas une liste d'imports. Un fichier déjà
// lu ouvre son recueil, un fichier neuf s'importe au premier clic — la base n'est que
// l'index des fichiers, et le dossier reste lisible par les autres logiciels.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs as Native

ColumnLayout {
    id: root
    required property var backend
    readonly property var p: backend.preferences
    // Le dossier affiché, vide tant qu'aucun n'est choisi. Il part du dossier retenu
    // d'une session à l'autre, puis suit la navigation.
    property string folder: p.studyFolder
    property var view: ({path: "", parent: "", name: "", entries: []})
    // Le recueil qu'un clic vient d'ouvrir : la page des études le montre à droite.
    signal collectionOpened(double id)

    spacing: 6

    function reload() { root.view = root.backend.browseStudies(root.folder) }
    function enter(path) { root.folder = path }
    onFolderChanged: reload()
    Component.onCompleted: reload()
    // Un import ajoute un recueil : la marque des fichiers lus doit suivre. On écoute
    // les sources et non la bibliothèque, qui change à chaque recherche et ferait relire
    // le dossier pour rien.
    Connections {
        target: root.backend
        function onSourcesChanged() { root.reload() }
    }

    RowLayout {
        Layout.fillWidth: true
        Layout.leftMargin: 8; Layout.rightMargin: 8; Layout.topMargin: 8
        spacing: 6
        TButton {
            objectName: "explorerUp"
            text: "⬆"
            flat: true
            implicitWidth: Math.round(28 * root.p.uiScale)
            enabled: root.view.parent !== ""
            onClicked: root.enter(root.view.parent)
            Accessible.name: qsTr("Dossier parent")
            ToolTip.visible: hovered || visualFocus
            ToolTip.text: qsTr("Dossier parent")
        }
        Label {
            Layout.fillWidth: true
            elide: Text.ElideLeft
            color: root.view.path === "" ? root.p.muted : root.p.foreground
            text: root.view.path === "" ? qsTr("Aucun dossier d’études choisi") : root.view.path
            HoverHandler { id: pathHover }
            ToolTip.visible: pathHover.hovered && root.view.path !== ""
            ToolTip.text: root.view.path
        }
        TButton {
            objectName: "explorerChoose"
            text: qsTr("Changer…")
            onClicked: pickFolder.open()
            ToolTip.visible: hovered || visualFocus
            ToolTip.text: qsTr("Choisir le dossier où vivent vos fichiers d’études. Il sera rouvert ici la prochaine fois.")
        }
    }

    RowLayout {
        Layout.fillWidth: true
        Layout.leftMargin: 8; Layout.rightMargin: 8
        spacing: 6
        visible: root.view.path !== ""
        TButton {
            objectName: "explorerImportAll"
            text: qsTr("Tout importer")
            enabled: !root.backend.busy
            onClicked: root.backend.importStudyFolder(root.view.path)
            ToolTip.visible: hovered || visualFocus
            ToolTip.text: qsTr("Lit tous les fichiers de ce dossier et de ses sous-dossiers, un recueil par fichier.")
        }
        TButton {
            objectName: "explorerNewFolder"
            text: qsTr("Nouveau dossier")
            onClicked: { newFolderName.clear(); newFolder.open() }
        }
        TButton {
            objectName: "explorerSaveHere"
            text: qsTr("Y enregistrer l’étude ouverte")
            enabled: !root.backend.busy
            // Le nom par défaut vient des joueurs de la partie ouverte ; une barre
            // oblique y ferait un chemin, pas un nom de fichier.
            onClicked: {
                saveName.text = (root.backend.state.title || qsTr("Étude"))
                                    .replace(/[\/\\]/g, "-")
                saveHere.open()
            }
            ToolTip.visible: hovered || visualFocus
            ToolTip.text: qsTr("Écrit l’étude de l’onglet Analyse dans ce dossier, en PGN, lisible par n’importe quel logiciel.")
        }
    }

    Label {
        Layout.fillWidth: true
        Layout.margins: 16
        visible: root.view.path === "" || root.view.entries.length === 0
        wrapMode: Text.WordWrap
        horizontalAlignment: Text.AlignHCenter
        color: root.p.muted
        text: root.view.path === ""
              ? qsTr("Choisissez le dossier où vous gardez vos fichiers d’études : Trait64 le "
                     + "parcourra ici, sans rien y déplacer.")
              : qsTr("Ce dossier ne contient ni sous-dossier ni fichier de parties.")
    }

    Label {
        Layout.fillWidth: true
        Layout.leftMargin: 10; Layout.rightMargin: 10
        visible: root.view.truncated === true
        wrapMode: Text.WordWrap
        color: root.p.muted
        font.pixelSize: Math.round(11 * root.p.fontScale * root.p.uiScale)
        text: qsTr("Dossier trop nombreux : seules les %1 premières entrées sont listées.")
                .arg(root.view.entries.length)
    }

    ListView {
        id: tree
        objectName: "explorerEntries"
        Layout.fillWidth: true
        Layout.fillHeight: true
        clip: true
        model: root.view.entries
        reuseItems: true
        ScrollBar.vertical: ScrollBar {}
        delegate: Rectangle {
            id: entry
            required property var modelData
            readonly property bool imported: !modelData.folder && Number(modelData.sourceId) > 0
            width: tree.width
            height: 38 * root.p.fontScale * root.p.uiScale
            color: entryHover.hovered ? Qt.rgba(root.p.accentColor.r, root.p.accentColor.g,
                                                root.p.accentColor.b, .1)
                                      : "transparent"
            HoverHandler {
                id: entryHover
                cursorShape: modelData.folder || modelData.readable ? Qt.PointingHandCursor
                                                                    : Qt.ArrowCursor
            }
            TapHandler {
                onTapped: {
                    if (entry.modelData.folder) {
                        root.enter(entry.modelData.path)
                        return
                    }
                    // Le premier clic lit le fichier, les suivants rouvrent son recueil :
                    // relire doublerait les parties.
                    var id = root.backend.openStudyFile(entry.modelData.path)
                    if (id > 0)
                        root.collectionOpened(id)
                }
            }
            Rectangle {
                anchors.bottom: parent.bottom; width: parent.width; height: 1
                color: root.p.line; opacity: .35
            }
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 10; anchors.rightMargin: 10
                spacing: 8
                Label {
                    // Le dossier, le fichier lu, le fichier qui attend : dit par un signe
                    // plutôt que par une colonne de mots.
                    text: entry.modelData.folder ? "▸" : entry.imported ? "●" : "○"
                    color: entry.modelData.folder ? root.p.foreground
                           : entry.imported ? root.p.accentColor : root.p.muted
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0
                    Label {
                        Layout.fillWidth: true
                        elide: Text.ElideMiddle
                        text: entry.modelData.name
                        color: entry.modelData.folder || entry.modelData.readable
                               ? root.p.foreground : root.p.muted
                        font.weight: entry.modelData.folder ? Font.DemiBold : Font.Normal
                    }
                    Label {
                        Layout.fillWidth: true
                        visible: !entry.modelData.folder
                        elide: Text.ElideRight
                        color: root.p.muted
                        font.pixelSize: Math.round(11 * root.p.fontScale * root.p.uiScale)
                        text: !entry.modelData.readable
                              ? qsTr("Format non lu — exportez-le en PGN")
                              : entry.imported
                                ? qsTr("%1 partie(s) · dans la base").arg(entry.modelData.games || 0)
                                : qsTr("%1 Ko · cliquez pour lire").arg(
                                      Math.max(1, Math.round(entry.modelData.size / 1024)))
                    }
                }
            }
        }
    }

    Native.FolderDialog {
        id: pickFolder
        title: qsTr("Dossier d’études")
        onAccepted: {
            // Le dossier choisi devient celui qu'on rouvre : c'est toujours le même.
            root.backend.setPreference("studyFolder", selectedFolder.toString())
            root.folder = root.backend.preferences.studyFolder
            root.reload()
        }
    }

    TDialog {
        id: newFolder
        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true
        title: qsTr("Nouveau dossier")
        standardButtons: Dialog.Ok | Dialog.Cancel
        onAccepted: {
            root.backend.createStudyFolder(root.view.path, newFolderName.text)
            root.reload()
        }
        TextField {
            id: newFolderName
            objectName: "explorerNewFolderName"
            width: Math.round(320 * root.p.uiScale)
            placeholderText: qsTr("Nom du dossier")
            onAccepted: newFolder.accept()
        }
    }

    TDialog {
        id: saveHere
        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true
        title: qsTr("Enregistrer l’étude ouverte ici")
        standardButtons: Dialog.Save | Dialog.Cancel
        onAccepted: {
            var name = saveName.text.trim()
            if (name === "")
                return
            if (!name.toLowerCase().endsWith(".pgn"))
                name += ".pgn"
            root.backend.exportFile(root.view.path + "/" + name)
            root.reload()
        }
        TextField {
            id: saveName
            objectName: "explorerSaveName"
            width: Math.round(320 * root.p.uiScale)
            placeholderText: qsTr("Nom du fichier")
            onAccepted: saveHere.accept()
        }
    }
}
