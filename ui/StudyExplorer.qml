// SPDX-License-Identifier: AGPL-3.0-or-later
// L'explorateur d'études, à la manière d'En Croissant : vos PGN restent des fichiers sur
// le disque, et c'est leur arborescence qu'on parcourt — dossiers qu'on déplie, fichiers
// qu'on crée, renomme, déplace ou met à la corbeille, sans quitter le logiciel. La base
// n'est que l'index des fichiers, et le dossier reste lisible par les autres logiciels.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs as Native

ColumnLayout {
    id: root
    required property var backend
    readonly property var p: backend.preferences
    // La racine de l'arbre, vide tant qu'aucun dossier n'est choisi.
    property string folder: p.studyFolder
    property var expanded: backend.studyExpanded()
    property var rows: []
    // L'entrée choisie : un fichier ou un dossier, ou rien.
    property var selected: null
    // Où créer : le dossier choisi, celui du fichier choisi, ou la racine.
    readonly property string target: !selected ? folder
                                     : selected.folder ? selected.path
                                     : selected.path.substring(0, selected.path.lastIndexOf("/"))
    // Le recueil qu'un clic vient d'ouvrir : la page des études le montre à droite.
    signal collectionOpened(double id)

    spacing: 6

    function s(base) { return Math.round(base * p.fontScale * p.uiScale) }
    function reload() {
        root.rows = root.folder === "" ? [] : root.backend.studyTree(root.folder, root.expanded, search.text)
        // L'entrée choisie a pu changer de nom ou disparaître : on la retrouve par son
        // chemin, ou on l'oublie.
        if (root.selected) {
            var kept = null
            for (var i = 0; i < root.rows.length; ++i)
                if (root.rows[i].path === root.selected.path)
                    kept = root.rows[i]
            root.selected = kept
        }
    }
    function toggle(path) {
        var next = root.expanded.filter(one => one !== path)
        if (next.length === root.expanded.length)
            next.push(path)
        root.expanded = next
        root.backend.rememberStudyExpanded(next)
        reload()
    }
    function unfold(path) {
        if (root.expanded.indexOf(path) < 0) {
            root.expanded = root.expanded.concat([path])
            root.backend.rememberStudyExpanded(root.expanded)
        }
    }
    // Le fichier dont on attend la lecture : son recueil s'ouvrira de lui-même à la fin
    // de l'import. Sans cela, « Lire ce fichier » importait, puis laissait la page vide
    // — il fallait deviner qu'un second clic montrerait les parties.
    property string awaiting: ""
    function open(entry) {
        // Le premier clic lit le fichier, les suivants rouvrent son recueil : relire
        // doublerait les parties.
        var id = root.backend.openStudyFile(entry.path)
        if (id > 0)
            root.collectionOpened(id)
        else if (root.backend.busy)
            root.awaiting = entry.path
    }
    function showAwaited() {
        if (root.awaiting === "" || root.backend.busy)
            return
        root.reload()
        for (var i = 0; i < root.rows.length; ++i)
            if (root.rows[i].path === root.awaiting && Number(root.rows[i].sourceId) > 0) {
                root.selected = root.rows[i]
                root.collectionOpened(root.rows[i].sourceId)
            }
        root.awaiting = ""
    }
    function size(bytes) {
        return bytes >= 1048576 ? qsTr("%1 Mo").arg((bytes / 1048576).toFixed(1))
                                : qsTr("%1 Ko").arg(Math.max(1, Math.round(bytes / 1024)))
    }
    // Relit l'arbre et choisit l'entrée de ce chemin — jamais un objet à moitié rempli,
    // que la fiche lirait avant que l'arbre ne soit relu.
    function choose(path) {
        root.selected = null
        root.reload()
        for (var i = 0; i < root.rows.length; ++i)
            if (root.rows[i].path === path)
                root.selected = root.rows[i]
    }
    function parentOf(path) { return path.substring(0, path.lastIndexOf("/")) }

    onFolderChanged: { selected = null; reload() }
    Component.onCompleted: reload()
    // Un import ajoute un recueil : la marque des fichiers lus doit suivre. On écoute
    // les sources et non la bibliothèque, qui change à chaque recherche et ferait relire
    // le dossier pour rien.
    Connections {
        target: root.backend
        function onSourcesChanged() { root.reload(); root.showAwaited() }
        function onBusyChanged() { root.showAwaited() }
    }

    RowLayout {
        Layout.fillWidth: true
        Layout.leftMargin: 8; Layout.rightMargin: 8; Layout.topMargin: 8
        spacing: 6
        T64Icon {
            source: "qrc:/assets/icons/folder-open.svg"; color: root.p.accentColor
            Layout.preferredWidth: root.s(16); Layout.preferredHeight: root.s(16)
        }
        Label {
            Layout.fillWidth: true
            elide: Text.ElideLeft
            color: root.folder === "" ? root.p.muted : root.p.foreground
            text: root.folder === "" ? qsTr("Aucun dossier d’études choisi") : root.folder
            HoverHandler { id: pathHover }
            ToolTip.visible: pathHover.hovered && root.folder !== ""
            ToolTip.text: root.folder
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
        visible: root.folder !== ""
        TextField {
            id: search
            objectName: "explorerSearch"
            Layout.fillWidth: true
            placeholderText: qsTr("Chercher un fichier ou un dossier…")
            onTextChanged: root.reload()
        }
        TButton {
            objectName: "explorerNewFile"
            text: qsTr("+ Fichier")
            onClicked: { fileName.clear(); filePgn.clear(); newFile.open(); fileName.forceActiveFocus() }
            ToolTip.visible: hovered || visualFocus
            ToolTip.text: qsTr("Créer un fichier PGN dans %1").arg(root.target)
        }
        TButton {
            objectName: "explorerNewFolder"
            text: qsTr("+ Dossier")
            onClicked: { nameDialog.ask("folder") }
            ToolTip.visible: hovered || visualFocus
            ToolTip.text: qsTr("Créer un dossier dans %1").arg(root.target)
        }
    }

    Label {
        Layout.fillWidth: true
        Layout.margins: 16
        visible: root.folder === "" || root.rows.length === 0
        wrapMode: Text.WordWrap
        horizontalAlignment: Text.AlignHCenter
        color: root.p.muted
        text: root.folder === ""
              ? qsTr("Choisissez le dossier où vous gardez vos fichiers d’études : Trait64 le "
                     + "parcourra ici, sans rien y déplacer.")
              : search.text !== "" ? qsTr("Rien ne correspond à cette recherche.")
              : qsTr("Ce dossier ne contient ni sous-dossier ni fichier de parties.")
    }

    ListView {
        id: tree
        objectName: "explorerEntries"
        Layout.fillWidth: true
        Layout.fillHeight: true
        clip: true
        model: root.rows
        reuseItems: true
        ScrollBar.vertical: ScrollBar {}
        delegate: Rectangle {
            id: entry
            required property var modelData
            readonly property bool imported: !modelData.folder && Number(modelData.sourceId) > 0
            readonly property bool chosen: !!root.selected && root.selected.path === modelData.path
            width: tree.width
            height: root.s(30)
            color: chosen ? Qt.rgba(root.p.accentColor.r, root.p.accentColor.g, root.p.accentColor.b, .2)
                 : entryHover.hovered ? Qt.rgba(root.p.accentColor.r, root.p.accentColor.g,
                                                root.p.accentColor.b, .08)
                 : "transparent"
            HoverHandler { id: entryHover; cursorShape: Qt.PointingHandCursor }
            TapHandler {
                onSingleTapped: {
                    root.selected = entry.modelData
                    if (entry.modelData.folder)
                        root.toggle(entry.modelData.path)
                }
                onDoubleTapped: {
                    if (!entry.modelData.folder && entry.modelData.readable)
                        root.open(entry.modelData)
                }
            }
            // Le trait de l'arborescence : une ligne fine par niveau, comme dans tout
            // explorateur, pour suivre de l'œil à quel dossier une ligne appartient.
            Repeater {
                model: entry.modelData.depth
                Rectangle {
                    required property int index
                    x: root.s(16) + index * root.s(16)
                    width: 1; height: entry.height
                    color: root.p.line; opacity: .5
                }
            }
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: root.s(8) + entry.modelData.depth * root.s(16)
                anchors.rightMargin: root.s(8)
                spacing: root.s(6)
                Label {
                    Layout.preferredWidth: root.s(10)
                    text: !entry.modelData.folder ? "" : entry.modelData.expanded ? "▾" : "▸"
                    color: root.p.muted
                }
                T64Icon {
                    Layout.preferredWidth: root.s(16); Layout.preferredHeight: root.s(16)
                    source: "qrc:/assets/icons/"
                            + (entry.modelData.folder ? (entry.modelData.expanded ? "folder-open" : "folder")
                               : entry.modelData.name.toLowerCase().endsWith(".db3")
                                 || !entry.modelData.readable ? "base" : "file") + ".svg"
                    // La couleur dit l'état : accent pour ce qui est dans la base, estompé
                    // pour ce que Trait64 ne lit pas.
                    color: entry.modelData.folder ? root.p.foreground
                         : entry.imported ? root.p.accentColor : root.p.muted
                    opacity: entry.modelData.folder || entry.modelData.readable ? 1 : .55
                }
                Label {
                    Layout.fillWidth: true
                    elide: Text.ElideMiddle
                    text: entry.modelData.name
                    color: entry.modelData.folder || entry.modelData.readable ? root.p.foreground
                                                                              : root.p.muted
                    font.weight: entry.modelData.folder ? Font.DemiBold : Font.Normal
                }
                Label {
                    visible: !entry.modelData.folder && !rowActions.visible
                    color: entry.imported ? root.p.accentColor : root.p.muted
                    font.pixelSize: root.s(11)
                    text: !entry.modelData.readable ? qsTr("non lu")
                          : entry.imported ? qsTr("%1 partie(s)").arg(entry.modelData.games || 0)
                          : root.size(entry.modelData.size)
                }
                Row {
                    id: rowActions
                    visible: entryHover.hovered
                    spacing: 2
                    Repeater {
                        model: [{icon: "pencil", tip: qsTr("Renommer"), what: "rename"},
                                {icon: "move", tip: qsTr("Déplacer vers…"), what: "move"},
                                {icon: "trash", tip: qsTr("Mettre à la corbeille"), what: "trash"}]
                        TButton {
                            required property var modelData
                            objectName: "explorerRow-" + modelData.what
                            flat: true
                            implicitWidth: root.s(24); implicitHeight: root.s(24)
                            Accessible.name: modelData.tip
                            ToolTip.visible: hovered; ToolTip.text: modelData.tip
                            onClicked: { root.selected = entry.modelData; root.act(modelData.what) }
                            T64Icon {
                                anchors.centerIn: parent
                                width: root.s(14); height: root.s(14)
                                source: "qrc:/assets/icons/" + parent.modelData.icon + ".svg"
                                color: parent.modelData.what === "trash" && parent.hovered ? "#cf3d3d"
                                                                                           : root.p.foreground
                            }
                        }
                    }
                }
            }
        }
    }

    function act(what) {
        if (!root.selected)
            return
        if (what === "rename")
            nameDialog.ask("rename")
        else if (what === "move")
            moveDialog.open()
        else if (what === "trash")
            trashDialog.open()
    }

    // La fiche de l'entrée choisie : ce qu'elle est, et ce qu'on peut en faire.
    Rectangle {
        Layout.fillWidth: true
        Layout.margins: 6
        visible: !!root.selected
        implicitHeight: card.implicitHeight + root.s(16)
        radius: 6
        color: root.p.raised
        border.color: root.p.line
        ColumnLayout {
            id: card
            anchors.fill: parent
            anchors.margins: root.s(8)
            spacing: root.s(4)
            Label {
                Layout.fillWidth: true
                elide: Text.ElideMiddle
                font.weight: Font.DemiBold
                color: root.p.foreground
                text: root.selected ? root.selected.name : ""
            }
            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: root.p.muted
                font.pixelSize: root.s(11)
                text: !root.selected ? ""
                      : root.selected.folder ? qsTr("Dossier · modifié le %1").arg(root.selected.modified)
                      : !root.selected.readable
                        ? qsTr("Format non lu — exportez-le en PGN depuis le logiciel qui l’a produit.")
                      : Number(root.selected.sourceId) > 0
                        ? qsTr("%1 partie(s) dans la base · %2 · modifié le %3")
                              .arg(root.selected.games || 0).arg(root.size(root.selected.size))
                              .arg(root.selected.modified)
                        : qsTr("Pas encore lu · %1 · modifié le %2").arg(root.size(root.selected.size))
                              .arg(root.selected.modified)
            }
            Flow {
                Layout.fillWidth: true
                spacing: root.s(6)
                TButton {
                    objectName: "explorerOpen"
                    accent: true
                    visible: !!root.selected && !root.selected.folder && !!root.selected.readable
                    enabled: !root.backend.busy
                    text: root.selected && Number(root.selected.sourceId) > 0 ? qsTr("Ouvrir le recueil")
                                                                              : qsTr("Lire ce fichier")
                    onClicked: root.open(root.selected)
                }
                TButton {
                    objectName: "explorerImportAll"
                    visible: !!root.selected && !!root.selected.folder
                    enabled: !root.backend.busy
                    text: qsTr("Tout importer")
                    onClicked: root.backend.importStudyFolder(root.selected.path)
                    ToolTip.visible: hovered || visualFocus
                    ToolTip.text: qsTr("Lit tous les fichiers de ce dossier et de ses sous-dossiers, un recueil par fichier.")
                }
                TButton {
                    objectName: "explorerSaveHere"
                    visible: !!root.selected && !!root.selected.folder
                    enabled: !root.backend.busy
                    text: qsTr("Y enregistrer l’étude ouverte")
                    onClicked: nameDialog.ask("save")
                    ToolTip.visible: hovered || visualFocus
                    ToolTip.text: qsTr("Écrit l’étude de l’onglet Analyse dans ce dossier, en PGN, lisible par n’importe quel logiciel.")
                }
                TButton { text: qsTr("Renommer"); onClicked: root.act("rename") }
                TButton { text: qsTr("Déplacer vers…"); onClicked: root.act("move") }
                TButton { objectName: "explorerTrash"; text: qsTr("Corbeille"); onClicked: root.act("trash") }
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

    // Un seul dialogue pour tout ce qui ne demande qu'un nom : nouveau dossier,
    // renommage, enregistrement de l'étude ouverte.
    TDialog {
        id: nameDialog
        property string purpose: "folder"
        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true
        title: purpose === "folder" ? qsTr("Nouveau dossier")
             : purpose === "rename" ? qsTr("Renommer")
             : qsTr("Enregistrer l’étude ouverte ici")
        standardButtons: Dialog.Ok | Dialog.Cancel
        function ask(what) {
            purpose = what
            // Le nom par défaut vient de l'entrée ou des joueurs de la partie ouverte ;
            // une barre oblique y ferait un chemin, pas un nom de fichier.
            nameField.text = what === "rename" ? root.selected.name
                           : what === "save" ? (root.backend.state.title || qsTr("Étude")).replace(/[\/\\]/g, "-")
                           : ""
            open()
            nameField.forceActiveFocus()
            nameField.selectAll()
        }
        onAccepted: {
            var name = nameField.text.trim()
            if (name === "")
                return
            if (purpose === "folder") {
                root.backend.createStudyFolder(root.target, name)
                root.unfold(root.target)
            } else if (purpose === "rename") {
                var moved = root.backend.renameStudyEntry(root.selected.path, name)
                if (moved !== "") {
                    root.choose(moved)
                    return
                }
            } else {
                if (!name.toLowerCase().endsWith(".pgn"))
                    name += ".pgn"
                root.backend.exportFile(root.target + "/" + name)
                root.unfold(root.target)
            }
            root.reload()
        }
        TextField {
            id: nameField
            objectName: "explorerName"
            width: root.s(320)
            placeholderText: qsTr("Nom")
            onAccepted: nameDialog.accept()
        }
    }

    TDialog {
        id: newFile
        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true
        title: qsTr("Nouveau fichier d’études")
        standardButtons: Dialog.Ok | Dialog.Cancel
        onAccepted: {
            var made = root.backend.createStudyFile(root.target, fileName.text, filePgn.text)
            if (made === "")
                return
            root.unfold(root.target)
            root.reload()
            for (var i = 0; i < root.rows.length; ++i)
                if (root.rows[i].path === made)
                    root.selected = root.rows[i]
        }
        ColumnLayout {
            spacing: 8
            TextField {
                id: fileName
                objectName: "explorerFileName"
                Layout.preferredWidth: root.s(420)
                placeholderText: qsTr("Nom du fichier")
                onAccepted: newFile.accept()
            }
            Label {
                Layout.preferredWidth: root.s(420)
                wrapMode: Text.WordWrap
                color: root.p.muted
                text: qsTr("Collez un PGN pour partir de parties existantes, ou laissez vide : le fichier "
                           + "contiendra une partie vierge, prête à être étudiée.")
            }
            ScrollView {
                Layout.preferredWidth: root.s(420)
                Layout.preferredHeight: root.s(160)
                TextArea {
                    id: filePgn
                    objectName: "explorerFilePgn"
                    placeholderText: qsTr("PGN (facultatif)")
                    wrapMode: TextArea.Wrap
                    font.family: "monospace"
                }
            }
        }
    }

    TDialog {
        id: moveDialog
        // Les dossiers où l'entrée peut aller : la racine et ceux de l'arbre, moins le
        // sien et ses descendants.
        readonly property var folders: {
            var list = [{name: qsTr("(racine du dossier d’études)"), path: root.folder, depth: 0}]
            if (!root.selected)
                return list
            for (var i = 0; i < root.rows.length; ++i) {
                var row = root.rows[i]
                if (row.folder && row.path !== root.selected.path
                        && row.path.indexOf(root.selected.path + "/") !== 0)
                    list.push({name: row.name, path: row.path, depth: row.depth + 1})
            }
            return list
        }
        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true
        title: root.selected ? qsTr("Déplacer « %1 » vers…").arg(root.selected.name) : ""
        standardButtons: Dialog.Cancel
        ListView {
            objectName: "explorerMoveTargets"
            implicitWidth: root.s(380)
            implicitHeight: Math.min(root.s(320), contentHeight)
            clip: true
            model: moveDialog.folders
            delegate: ItemDelegate {
                required property var modelData
                width: ListView.view.width
                leftPadding: root.s(10) + modelData.depth * root.s(14)
                text: modelData.name
                // Le dossier où l'entrée se trouve déjà n'est pas une destination.
                enabled: !!root.selected && modelData.path !== root.parentOf(root.selected.path)
                onClicked: {
                    var moved = root.backend.moveStudyEntry(root.selected.path, modelData.path)
                    moveDialog.close()
                    if (moved !== "") {
                        root.unfold(modelData.path)
                        root.choose(moved)
                    }
                }
            }
        }
    }

    TDialog {
        id: trashDialog
        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true
        title: qsTr("Mettre à la corbeille ?")
        standardButtons: Dialog.Yes | Dialog.Cancel
        onAccepted: {
            if (root.backend.trashStudyEntry(root.selected.path))
                root.selected = null
            root.reload()
        }
        ColumnLayout {
            Label {
                Layout.preferredWidth: root.s(400)
                wrapMode: Text.WordWrap
                color: root.p.foreground
                text: !root.selected ? ""
                      : (root.selected.folder
                         ? qsTr("Le dossier « %1 » et tout ce qu’il contient iront dans la corbeille du système, d’où vous pourrez encore les reprendre.")
                         : qsTr("Le fichier « %1 » ira dans la corbeille du système, d’où vous pourrez encore le reprendre."))
                            .arg(root.selected.name)
                        + (Number(root.selected.sourceId) > 0
                           ? "\n\n" + qsTr("Ses parties déjà lues restent dans la bibliothèque.") : "")
            }
        }
    }
}
