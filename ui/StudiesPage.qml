// SPDX-License-Identifier: AGPL-3.0-or-later
// La page des études, sur le modèle d'En Croissant et de Pawn Appétit : la liste de vos
// répertoires d'un côté, ce qu'on peut en faire de l'autre, et la marque de ce qui est
// à réviser en regard des noms. Chez eux comme ici, une étude est un PGN ordinaire dont
// les variantes s'emboîtent — d'où l'import d'un dossier entier, qui est la façon dont
// on migre réellement depuis ces logiciels.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs as Native

ColumnLayout {
    id: root
    required property var backend
    readonly property var preferences: backend.preferences
    readonly property var detail: backend.repertoireState
    readonly property double selectedId: detail.id || 0
    readonly property bool editable: !backend.busy && !backend.state.training
    // L'éditeur fin reste dans sa boîte de dialogue, déclarée par la fenêtre : la page
    // demande son ouverture plutôt que d'aller chercher un identifiant qui ne lui
    // appartient pas.
    signal openDetails()
    // Un recueil choisi : ses parties remplacent la fiche du répertoire à droite. Une
    // étude n'est pas forcément un répertoire — un PGN tient souvent des parties
    // commentées, qu'on veut lire l'une après l'autre.
    property var collection: null
    readonly property var studies: {
        var rows = []
        for (var i = 0; i < backend.repertoires.length; ++i) {
            var r = backend.repertoires[i]
            rows.push({kind: "repertoire", id: r.id, name: r.name, white: r.white,
                       positions: r.positions, reviews: r.reviews, success: r.success, due: r.due})
        }
        var collections = backend.studyCollections
        for (var j = 0; j < collections.length; ++j) {
            var c = collections[j]
            rows.push({kind: "collection", id: c.id, name: c.title, games: c.games})
        }
        return rows
    }
    spacing: 10

    RowLayout {
        Layout.fillWidth: true
        spacing: 8
        TButton {
            objectName: "studiesCreate"
            text: qsTr("Créer depuis l’étude ouverte")
            enabled: root.editable
            onClicked: { createName.clear(); createDialog.open() }
            ToolTip.visible: hovered || visualFocus
            ToolTip.text: qsTr("Prend l’étude ouverte dans l’onglet Analyse, avec toutes ses variantes.")
        }
        TButton {
            objectName: "studiesImportFile"
            text: qsTr("Importer un PGN")
            enabled: root.editable
            onClicked: importPgn.open()
            ToolTip.visible: hovered || visualFocus
            ToolTip.text: root.selectedId > 0
                ? qsTr("Ajoute les lignes du fichier à l’étude choisie, variantes et transpositions comprises.")
                : qsTr("Crée une étude nommée d’après le fichier, avec toutes ses lignes. Les PGN compressés sont acceptés.")
        }
        TButton {
            objectName: "studiesImportFolder"
            text: qsTr("Importer un dossier")
            accent: true
            enabled: root.editable
            onClicked: importFolder.open()
            ToolTip.visible: hovered || visualFocus
            ToolTip.text: qsTr("Reprend un dossier d’études tel qu’En Croissant ou Pawn Appétit le laisse : une étude par fichier PGN, sous-dossiers compris.")
        }
        Item { Layout.fillWidth: true }
        TButton {
            text: qsTr("Détails…")
            enabled: root.selectedId > 0
            onClicked: root.openDetails()
            ToolTip.visible: hovered || visualFocus
            ToolTip.text: qsTr("Éditer les coups préparés, la couverture et les réponses à préparer.")
        }
    }

    SplitView {
        Layout.fillWidth: true
        Layout.fillHeight: true
        orientation: Qt.Horizontal
        handle: Rectangle {
            implicitWidth: 5
            color: SplitHandle.hovered || SplitHandle.pressed ? root.preferences.line : "transparent"
            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                width: 1; height: parent.height; color: root.preferences.line
            }
        }

        Rectangle {
            SplitView.preferredWidth: 420
            SplitView.minimumWidth: 280
            color: root.preferences.panel
            border.color: root.preferences.line

            Label {
                anchors.centerIn: parent
                width: parent.width - 32
                visible: root.studies.length === 0
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
                color: root.preferences.muted
                text: qsTr("Aucune étude pour l’instant. Importez un dossier venu d’un autre logiciel, ou créez-en une depuis l’étude ouverte.")
            }

            ListView {
                id: list
                objectName: "studiesList"
                anchors.fill: parent
                anchors.margins: 1
                clip: true
                model: root.studies
                ScrollBar.vertical: ScrollBar {}
                delegate: Rectangle {
                    id: studyRow
                    required property var modelData
                    required property int index
                    readonly property bool isCollection: modelData.kind === "collection"
                    readonly property bool current: isCollection
                        ? (root.collection && Number(root.collection.id) === Number(modelData.id))
                        : (!root.collection && Number(modelData.id) === Number(root.selectedId))
                    width: list.width
                    height: 46 * controller.preferences.fontScale * controller.preferences.uiScale
                    color: current ? Qt.rgba(root.preferences.accentColor.r, root.preferences.accentColor.g,
                                             root.preferences.accentColor.b, .18)
                         : rowHover.hovered ? Qt.rgba(root.preferences.accentColor.r,
                                                      root.preferences.accentColor.g,
                                                      root.preferences.accentColor.b, .09)
                         : "transparent"
                    HoverHandler { id: rowHover }
                    TapHandler {
                        onTapped: {
                            if (studyRow.isCollection)
                                root.collection = modelData
                            else {
                                root.collection = null
                                root.backend.selectRepertoire(modelData.id)
                            }
                        }
                    }
                    Rectangle {
                        anchors.bottom: parent.bottom; width: parent.width; height: 1
                        color: root.preferences.line; opacity: .4
                    }
                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 10; anchors.rightMargin: 10
                        spacing: 10
                        // Le camp préparé, dit par la couleur plutôt que par un mot.
                        Rectangle {
                            Layout.preferredWidth: 11; Layout.preferredHeight: 11
                            Layout.alignment: Qt.AlignVCenter
                            radius: 3
                            // Un recueil n'a pas de camp : sa pastille porte la couleur
                            // d'accent plutôt qu'un blanc ou un noir qui mentirait.
                            color: studyRow.isCollection ? root.preferences.accentColor
                                   : modelData.white ? "#f2f4f7" : "#1d2125"
                            border.color: studyRow.isCollection ? root.preferences.accentColor
                                          : "#1d2125"
                        }
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 1
                            Label {
                                Layout.fillWidth: true
                                text: modelData.name
                                elide: Text.ElideRight
                                font.weight: studyRow.current ? Font.DemiBold : Font.Normal
                            }
                            Label {
                                Layout.fillWidth: true
                                elide: Text.ElideRight
                                color: root.preferences.muted
                                font.pixelSize: Math.round(11 * controller.preferences.fontScale
                                                           * controller.preferences.uiScale)
                                // Un recueil se compte en parties, un répertoire en positions :
                                // ce ne sont pas les mêmes objets.
                                text: studyRow.isCollection
                                      ? qsTr("Recueil · %1 partie(s)").arg(modelData.games || 0)
                                      : qsTr("%1 position(s)").arg(modelData.positions || 0)
                                        + ((modelData.reviews || 0) > 0
                                           ? qsTr(" · %1 % de réussite")
                                             .arg(Math.round(100 * (modelData.successes || 0) / modelData.reviews))
                                           : "")
                            }
                        }
                        // La marque de ce qui est dû : c'est elle qu'on cherche en arrivant.
                        Rectangle {
                            visible: (modelData.due || 0) > 0
                            Layout.alignment: Qt.AlignVCenter
                            implicitWidth: dueCount.implicitWidth + 14
                            implicitHeight: dueCount.implicitHeight + 6
                            radius: height / 2
                            color: root.preferences.accentColor
                            Label {
                                id: dueCount
                                anchors.centerIn: parent
                                text: modelData.due || 0
                                color: "#ffffff"
                                font.pixelSize: Math.round(11 * controller.preferences.fontScale
                                                           * controller.preferences.uiScale)
                                font.weight: Font.DemiBold
                            }
                        }
                    }
                }
            }
        }

        ColumnLayout {
            SplitView.fillWidth: true
            SplitView.minimumWidth: 300
            spacing: 10

            Label {
                Layout.fillWidth: true
                visible: !root.collection && root.selectedId <= 0
                wrapMode: Text.WordWrap
                color: root.preferences.muted
                text: qsTr("Choisissez une étude dans la liste.")
            }

            // Un recueil : ses parties, l'une après l'autre. Un clic en ouvre une dans
            // Analyse, avec ses variantes et ses commentaires.
            ColumnLayout {
                id: collectionPane
                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: !!root.collection
                spacing: 8
                readonly property var games: root.collection
                                             ? root.backend.studyGames(root.collection.id) : []

                Label {
                    Layout.fillWidth: true
                    text: root.collection ? root.collection.name : ""
                    font.pixelSize: Math.round(18 * controller.preferences.fontScale
                                               * controller.preferences.uiScale)
                    font.weight: Font.DemiBold
                    elide: Text.ElideRight
                }
                Label {
                    Layout.fillWidth: true
                    color: root.preferences.muted
                    wrapMode: Text.WordWrap
                    text: qsTr("%1 partie(s) · cliquez-en une pour la parcourir à côté, "
                               + "double-cliquez pour l’ouvrir dans Analyse. "
                               + "Ce recueil est aussi une base de la bibliothèque : "
                               + "recherche, filtres et bilan s’y appliquent.")
                            .arg(root.collection ? (root.collection.games || 0) : 0)
                }
                RowLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: 10
                    ListView {
                        objectName: "studyGames"
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        model: collectionPane.games
                        reuseItems: true
                        ScrollBar.vertical: ScrollBar {}
                        delegate: Rectangle {
                            required property var modelData
                            width: ListView.view.width
                            height: 34 * controller.preferences.fontScale * controller.preferences.uiScale
                            color: gameHover.hovered
                                   ? Qt.rgba(root.preferences.accentColor.r, root.preferences.accentColor.g,
                                             root.preferences.accentColor.b, .12)
                                   : "transparent"
                            HoverHandler { id: gameHover; cursorShape: Qt.PointingHandCursor }
                            // Un clic montre la partie à côté, un double-clic l'ouvre dans
                            // Analyse : parcourir un recueil ne doit pas coûter l'étude ouverte.
                            TapHandler { onSingleTapped: studyPreview.gameId = modelData.id
                                         onDoubleTapped: root.backend.loadGame(modelData.id) }
                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 8; anchors.rightMargin: 8
                                spacing: 10
                                Label {
                                    Layout.fillWidth: true
                                    elide: Text.ElideRight
                                    text: (modelData.white || "?") + " — " + (modelData.black || "?")
                                          + (modelData.event ? "   " + modelData.event : "")
                                }
                                Label {
                                    text: modelData.result || "*"
                                    color: root.preferences.muted
                                }
                                Label {
                                    // Le PGN note une date inconnue « ????.??.?? » : un tiret se lit mieux.
                                    text: modelData.date && modelData.date.indexOf("?") < 0
                                          ? modelData.date : "—"
                                    color: root.preferences.muted
                                }
                            }
                        }
                    }
                    GamePreview {
                        id: studyPreview
                        objectName: "studyPreview"
                        Layout.preferredWidth: Math.round(280 * controller.preferences.uiScale)
                        Layout.alignment: Qt.AlignTop
                        visible: parent.width > Math.round(760 * controller.preferences.uiScale)
                        backend: root.backend
                        boardSide: Math.round(240 * controller.preferences.uiScale)
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                visible: !root.collection && root.selectedId > 0
                spacing: 8

                Label {
                    Layout.fillWidth: true
                    text: root.detail.name || ""
                    elide: Text.ElideRight
                    font.pixelSize: Math.round(18 * controller.preferences.fontScale
                                               * controller.preferences.uiScale)
                    font.weight: Font.DemiBold
                }
                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    color: root.preferences.muted
                    text: (root.detail.white ? qsTr("Préparé pour les Blancs") : qsTr("Préparé pour les Noirs"))
                          + qsTr(" · %1 position(s)").arg(root.detail.positions || 0)
                          + ((root.detail.due || 0) > 0
                             ? qsTr(" · %1 à réviser aujourd’hui").arg(root.detail.due) : "")
                }

                Flow {
                    Layout.fillWidth: true
                    spacing: 8
                    TButton {
                        text: qsTr("Réviser les positions dues")
                        accent: (root.detail.due || 0) > 0
                        enabled: root.editable && (root.detail.due || 0) > 0
                        onClicked: root.backend.startRepertoireTraining(root.selectedId)
                    }
                    TButton {
                        text: qsTr("Révision libre")
                        enabled: root.editable && (root.detail.positions || 0) > 0
                        onClicked: root.backend.startRepertoireTraining(root.selectedId, true)
                    }
                    TButton {
                        text: qsTr("Ajouter l’étude ouverte")
                        enabled: root.editable
                        onClicked: root.backend.addStudyToRepertoire(root.selectedId)
                        ToolTip.visible: hovered || visualFocus
                        ToolTip.text: qsTr("Fusionne l’étude de l’onglet Analyse dans celle-ci, avec ses variantes.")
                    }
                    TButton {
                        text: qsTr("Exporter en PGN")
                        enabled: root.editable
                        onClicked: exportPgn.open()
                    }
                    TButton {
                        objectName: "repertoireGap"
                        text: qsTr("Comparer à mes parties")
                        enabled: root.editable && root.selectedId > 0
                        onClicked: { gap.load(); gap.open() }
                        ToolTip.visible: hovered || visualFocus
                        ToolTip.text: qsTr("Où vous quittez votre propre préparation, dans vos parties déjà importées.")
                    }
                }

                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    opacity: .7
                    font.pixelSize: Math.round(10 * controller.preferences.fontScale
                                               * controller.preferences.uiScale)
                    text: qsTr("Une étude est un PGN : ce que vous exportez ici se relit dans En Croissant, Pawn Appétit ou n’importe quel logiciel d’échecs.")
                }
            }
            Item { Layout.fillHeight: true }
        }
    }

    TDialog {
        id: createDialog
        objectName: "studiesCreateDialog"
        parent: Overlay.overlay
        title: qsTr("Nouvelle étude")
        anchors.centerIn: parent
        modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel
        width: 460
        onAccepted: root.backend.createRepertoire(createName.text, createWhite.checked)
        ColumnLayout {
            anchors.fill: parent
            spacing: 10
            TextField {
                id: createName
                objectName: "studiesCreateName"
                Layout.fillWidth: true
                placeholderText: qsTr("Nom de l’étude, par exemple « Italienne »")
            }
            CheckBox {
                id: createWhite
                text: qsTr("Préparée pour les Blancs")
                checked: true
            }
        }
    }

    TDialog {
        id: gap
        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true
        title: qsTr("Mon répertoire face à mes parties")
        width: Math.min(760, parent ? parent.width - 48 : 760)
        height: Math.min(620, parent ? parent.height - 64 : 620)
        property var found: ({})
        // Le premier profil : c'est celui dont les comptes et le nom désignent le joueur.
        function load() { found = root.backend.repertoireGap(root.selectedId, 0) }

        ColumnLayout {
            anchors.fill: parent
            spacing: 12
            Label {
                Layout.fillWidth: true
                visible: !gap.found.ready
                wrapMode: Text.WordWrap
                color: root.preferences.muted
                text: gap.found.reason === "noProfile"
                      ? qsTr("Créez un profil pour comparer : c’est lui qui dit quelles parties sont les vôtres.")
                      : gap.found.reason === "noAccount"
                        ? qsTr("Rattachez un compte à votre profil, ou donnez-lui le nom sous lequel vous jouez.")
                        : root.backend.uiText(gap.found.reason || "")
            }
            Label {
                Layout.fillWidth: true
                visible: !!gap.found.ready
                wrapMode: Text.WordWrap
                color: root.preferences.muted
                text: qsTr("%1 de vos parties avec cette couleur · %2 sont entrées dans ce répertoire · "
                           + "vous l’avez quitté dans %3")
                        .arg(gap.found.games || 0).arg(gap.found.touched || 0).arg(gap.found.left || 0)
            }
            Label {
                Layout.fillWidth: true
                visible: !!gap.found.ready && (gap.found.touched || 0) === 0
                wrapMode: Text.WordWrap
                color: root.preferences.muted
                text: qsTr("Aucune de vos parties n’a joué un coup de ce répertoire. Importez vos parties, "
                           + "ou vérifiez que le nom du profil est bien celui sous lequel vous jouez.")
            }
            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: (gap.found.rows || []).length > 0
                clip: true
                ColumnLayout {
                    width: parent.width
                    spacing: 6
                    Repeater {
                        model: gap.found.rows || []
                        Label {
                            required property var modelData
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            color: hovered.hovered ? root.preferences.accent : root.preferences.foreground
                            text: qsTr("Coup %1 : vous avez joué %2, le répertoire prévoyait %3")
                                    .arg(modelData.move)
                                    .arg(modelData.played)
                                    .arg((modelData.expected || []).join(", ") || qsTr("rien ici"))
                                  + "   —   " + (modelData.white || "?") + " / " + (modelData.black || "?")
                                  + "   " + (modelData.result || "*")
                            HoverHandler {id: hovered; cursorShape: Qt.PointingHandCursor}
                            TapHandler {onTapped: {root.backend.loadGame(modelData.id); gap.close()}}
                        }
                    }
                }
            }
            RowLayout {
                Layout.fillWidth: true
                Item {Layout.fillWidth: true}
                TButton {text: qsTr("Fermer"); onClicked: gap.close()}
            }
        }
    }

    Native.FileDialog {
        id: importPgn
        title: root.selectedId > 0 ? qsTr("Ajouter un PGN à cette étude")
                                   : qsTr("PGN à importer comme nouvelle étude")
        nameFilters: [qsTr("Études (*.pgn *.PGN *.pgn.gz *.pgn.zst *.pgn.bz2 *.pgn.xz)"),
                      qsTr("Tous les fichiers (*)")]
        // Un PGN n'est pas forcément un répertoire : on demande ce qu'il est plutôt que
        // de fondre ses parties dans un arbre de positions sans prévenir.
        onAccepted: { kind.file = selectedFile.toString(); kind.folder = false; kind.open() }
    }
    Native.FolderDialog {
        id: importFolder
        title: qsTr("Dossier d’études à importer")
        onAccepted: { kind.file = selectedFolder.toString(); kind.folder = true; kind.open() }
    }
    TDialog {
        id: kind
        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true
        title: qsTr("Qu’est-ce que ce fichier ?")
        property string file: ""
        property bool folder: false
        width: Math.min(560, parent ? parent.width - 48 : 560)

        ColumnLayout {
            anchors.fill: parent
            spacing: 14
            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: root.preferences.muted
                text: qsTr("Les deux se gardent dans cet onglet. Un recueil laisse chaque partie "
                           + "entière et se lit comme un livre ; un répertoire fond toutes les "
                           + "lignes en un seul arbre, pour les réviser.")
            }
            TButton {
                objectName: "importAsCollection"
                Layout.fillWidth: true
                accent: true
                text: qsTr("Recueil de parties — à parcourir")
                onClicked: {
                    if (kind.folder)
                        root.backend.importStudyFolder(kind.file)
                    else
                        root.backend.importStudyCollection(kind.file)
                    kind.close()
                }
            }
            TButton {
                objectName: "importAsRepertoire"
                Layout.fillWidth: true
                text: root.selectedId > 0 && !kind.folder
                      ? qsTr("Ajouter à l’étude choisie — à réviser")
                      : qsTr("Répertoire d’entraînement — à réviser")
                onClicked: {
                    if (kind.folder)
                        root.backend.importRepertoireFolder(kind.file)
                    else
                        root.backend.importRepertoire(root.selectedId, kind.file)
                    kind.close()
                }
            }
            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                TButton { text: qsTr("Annuler"); onClicked: kind.close() }
            }
        }
    }
    Native.FileDialog {
        id: exportPgn
        title: qsTr("Exporter cette étude")
        fileMode: Native.FileDialog.SaveFile
        defaultSuffix: "pgn"
        nameFilters: ["PGN (*.pgn)"]
        onAccepted: root.backend.exportRepertoire(root.selectedId, selectedFile.toString())
    }
}
