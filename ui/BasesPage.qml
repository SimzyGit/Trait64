// SPDX-License-Identifier: AGPL-3.0-or-later
// La fenêtre des bases, à la manière de ChessBase : les bases en icônes, rangées par
// catégorie à gauche ; un double-clic ouvre la liste de leurs parties.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    required property var backend
    readonly property var prefs: backend.preferences
    readonly property real scale: prefs.uiScale
    readonly property real textScale: prefs.fontScale * prefs.uiScale
    property string category: "all"
    property double selectedId: 0
    property bool details: false
    signal openBase(double id)
    signal importGames()
    signal openLibrary()
    signal editTags(var base)
    signal removeBase(double id)

    // La catégorie se lit dans l'identifiant de la base : « lichess:… » et « chesscom:… »
    // pour les comptes, « prep: » pour les préparations, « reference: » pour la référence.
    function kindOf(b) {
        var slug = String(b.slug || "")
        if (slug.indexOf("prep:") === 0) return "prep"
        if (slug.indexOf("reference:") === 0) return "reference"
        if (/^(lichess|chesscom|chess\.com):/.test(slug)) return "account"
        if (b.purpose === "study") return "study"
        return "mine"
    }
    readonly property var categories: {
        var list = [{key: "all", text: qsTr("Toutes les bases"), icon: "database"},
                    {key: "mine", text: qsTr("Mes bases"), icon: "folder"},
                    {key: "account", text: qsTr("Comptes en ligne"), icon: "accounts"},
                    {key: "prep", text: qsTr("Préparations"), icon: "preparation"},
                    {key: "reference", text: qsTr("Référence"), icon: "reference"},
                    {key: "study", text: qsTr("Études"), icon: "studies"}]
        var tags = {}
        backend.sources.forEach(function(b) { (b.tags || []).forEach(function(t) { tags[t] = true }) })
        Object.keys(tags).sort().forEach(function(t) { list.push({key: "#" + t, text: "#" + t, icon: "tag"}) })
        return list
    }
    function count(key) { return backend.sources.filter(function(b) { return matches(b, key, "") }).length }
    function matches(b, key, text) {
        if (text && String(b.title).toLocaleLowerCase().indexOf(text) < 0) return false
        if (key === "all") return true
        if (key.charAt(0) === "#") return (b.tags || []).indexOf(key.slice(1)) >= 0
        return kindOf(b) === key
    }
    readonly property var shown: {
        var text = filter.text.toLocaleLowerCase()
        return backend.sources.filter(function(b) { return root.matches(b, root.category, text) })
    }
    readonly property var selected: backend.sources.find(function(b) { return Number(b.id) === Number(root.selectedId) }) || null
    function tint(kind) {
        return kind === "account" ? "#2f7d4f" : kind === "prep" ? "#b5642a" : kind === "reference" ? "#7a4fa3"
             : kind === "study" ? "#3c7f99" : root.prefs.accentColor
    }
    function gamesText(n) { return n === 1 ? qsTr("1 partie") : qsTr("%L1 parties").arg(n || 0) }

    RowLayout {
        anchors.fill: parent
        spacing: 0
        // Les catégories, comme l'arbre de gauche de la fenêtre des bases.
        Rectangle {
            Layout.fillHeight: true
            Layout.preferredWidth: Math.round(190 * root.textScale)
            color: root.prefs.panel
            border.color: root.prefs.line
            ListView {
                id: categoryList
                objectName: "baseCategories"
                anchors.fill: parent
                anchors.margins: 4
                clip: true
                model: root.categories
                ScrollBar.vertical: ScrollBar {}
                delegate: ItemDelegate {
                    id: entry
                    required property var modelData
                    readonly property bool active: root.category === modelData.key
                    width: categoryList.width
                    height: Math.round(28 * root.textScale)
                    highlighted: active
                    Accessible.name: modelData.text
                    contentItem: RowLayout {
                        spacing: 8
                        T64Icon { Layout.preferredWidth: 16 * root.scale; Layout.preferredHeight: 16 * root.scale
                                  source: "qrc:/assets/icons/" + entry.modelData.icon + ".svg"; color: root.prefs.accentColor }
                        Label { text: entry.modelData.text; Layout.fillWidth: true; elide: Text.ElideRight
                                color: root.prefs.foreground; font.weight: entry.active ? Font.DemiBold : Font.Normal }
                        Label { text: root.count(entry.modelData.key); color: root.prefs.muted }
                    }
                    background: Rectangle {
                        radius: 3
                        readonly property color accent: root.prefs.accentColor
                        color: entry.active ? Qt.rgba(accent.r, accent.g, accent.b, .16)
                             : entry.hovered ? Qt.rgba(accent.r, accent.g, accent.b, .07) : "transparent"
                    }
                    onClicked: root.category = modelData.key
                }
            }
        }
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: 8 * root.scale
            spacing: 6
            RowLayout {
                Layout.fillWidth: true
                spacing: 6
                TTextField { id: filter; objectName: "baseFilter"; placeholderText: qsTr("Filtrer les bases…"); Layout.fillWidth: true; Layout.minimumWidth: 0 }
                TButton { text: qsTr("Importer…"); accent: true; onClicked: root.importGames() }
                TButton { text: qsTr("Ouvrir une bibliothèque…"); onClicked: root.openLibrary() }
                TButton { objectName: "basesDetails"; text: root.details ? qsTr("Icônes") : qsTr("Détails"); onClicked: root.details = !root.details }
            }
            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: root.prefs.panel
                border.color: root.prefs.line
                GridView {
                    id: grid
                    objectName: "basesGrid"
                    visible: !root.details
                    anchors.fill: parent
                    anchors.margins: 8
                    clip: true
                    model: root.shown
                    cellWidth: Math.round(128 * root.textScale)
                    cellHeight: Math.round(112 * root.textScale)
                    keyNavigationEnabled: true
                    ScrollBar.vertical: ScrollBar {}
                    delegate: AbstractButton {
                        id: tile
                        required property var modelData
                        required property int index
                        readonly property bool active: Number(modelData.id) === Number(root.selectedId)
                        width: grid.cellWidth - 6
                        height: grid.cellHeight - 6
                        hoverEnabled: true
                        focusPolicy: Qt.StrongFocus
                        Accessible.name: modelData.title + ", " + root.gamesText(modelData.games)
                        // Une icône recréée sous la souris est survolée avant d'avoir une fenêtre.
                        ToolTip.visible: hovered && Window.window !== null
                        ToolTip.delay: 600
                        ToolTip.text: modelData.title + "\n" + root.gamesText(modelData.games)
                        background: Rectangle {
                            radius: 4
                            readonly property color accent: root.prefs.accentColor
                            color: tile.active ? Qt.rgba(accent.r, accent.g, accent.b, .18)
                                 : tile.hovered ? Qt.rgba(accent.r, accent.g, accent.b, .08) : "transparent"
                            border.width: tile.active || tile.visualFocus ? 1 : 0
                            border.color: Qt.rgba(accent.r, accent.g, accent.b, .6)
                        }
                        contentItem: Column {
                            spacing: 2
                            topPadding: 6
                            T64Icon {
                                anchors.horizontalCenter: parent.horizontalCenter
                                width: Math.round(40 * root.scale); height: width
                                source: "qrc:/assets/icons/database.svg"
                                color: root.tint(root.kindOf(tile.modelData))
                            }
                            Label {
                                width: parent.width
                                text: tile.modelData.title
                                horizontalAlignment: Text.AlignHCenter
                                wrapMode: Text.Wrap
                                maximumLineCount: 2
                                elide: Text.ElideRight
                                color: root.prefs.foreground
                                font.pixelSize: Math.round(11.5 * root.textScale)
                            }
                            Label {
                                width: parent.width
                                text: root.gamesText(tile.modelData.games)
                                horizontalAlignment: Text.AlignHCenter
                                color: root.prefs.muted
                                font.pixelSize: Math.round(10 * root.textScale)
                            }
                        }
                        onClicked: { root.selectedId = modelData.id; grid.currentIndex = index }
                        onDoubleClicked: root.openBase(modelData.id)
                        Keys.onReturnPressed: root.openBase(modelData.id)
                        TapHandler { acceptedButtons: Qt.RightButton; onTapped: { root.selectedId = tile.modelData.id; baseMenu.popup() } }
                    }
                    Label {
                        anchors.centerIn: parent
                        visible: grid.count === 0
                        width: parent.width - 40
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                        text: backend.sources.length ? qsTr("Aucune base dans cette catégorie.")
                                                     : qsTr("Aucune base pour l’instant : importez un fichier PGN ou synchronisez un compte en ligne.")
                        color: root.prefs.muted
                    }
                }
                // La vue détaillée : une ligne par base, comme la liste de l'explorateur.
                ListView {
                    id: table
                    objectName: "basesTable"
                    visible: root.details
                    anchors.fill: parent
                    anchors.margins: 4
                    clip: true
                    model: root.shown
                    ScrollBar.vertical: ScrollBar {}
                    header: Rectangle {
                        width: table.width; height: Math.round(24 * root.textScale); color: root.prefs.raised
                        RowLayout { anchors.fill: parent; anchors.leftMargin: 6; anchors.rightMargin: 6; spacing: 8
                            Label { text: qsTr("Base"); Layout.fillWidth: true; color: root.prefs.muted }
                            Label { text: qsTr("Catégorie"); Layout.preferredWidth: 130 * root.textScale; color: root.prefs.muted }
                            Label { text: qsTr("Parties"); Layout.preferredWidth: 80 * root.textScale; horizontalAlignment: Text.AlignRight; color: root.prefs.muted }
                            Label { text: qsTr("Étiquettes"); Layout.preferredWidth: 160 * root.textScale; color: root.prefs.muted }
                        }
                    }
                    delegate: Rectangle {
                        id: line
                        required property var modelData
                        required property int index
                        readonly property bool active: Number(modelData.id) === Number(root.selectedId)
                        readonly property color accent: root.prefs.accentColor
                        width: table.width; height: Math.round(24 * root.textScale)
                        color: active ? Qt.rgba(accent.r, accent.g, accent.b, .18) : index % 2 ? root.prefs.background : root.prefs.panel
                        RowLayout { anchors.fill: parent; anchors.leftMargin: 6; anchors.rightMargin: 6; spacing: 8
                            T64Icon { Layout.preferredWidth: 16 * root.scale; Layout.preferredHeight: 16 * root.scale
                                      source: "qrc:/assets/icons/database.svg"; color: root.tint(root.kindOf(line.modelData)) }
                            Label { text: line.modelData.title; Layout.fillWidth: true; elide: Text.ElideRight; color: root.prefs.foreground }
                            Label { text: root.categories.find(function(c) { return c.key === root.kindOf(line.modelData) }).text
                                    Layout.preferredWidth: 130 * root.textScale; elide: Text.ElideRight; color: root.prefs.muted }
                            Label { text: Number(line.modelData.games || 0).toLocaleString(Qt.locale(), "f", 0)
                                    Layout.preferredWidth: 80 * root.textScale; horizontalAlignment: Text.AlignRight; color: root.prefs.foreground }
                            Label { text: (line.modelData.tags || []).map(function(t) { return "#" + t }).join(" ")
                                    Layout.preferredWidth: 160 * root.textScale; elide: Text.ElideRight; color: root.prefs.muted }
                        }
                        TapHandler { onTapped: root.selectedId = line.modelData.id; onDoubleTapped: root.openBase(line.modelData.id) }
                        TapHandler { acceptedButtons: Qt.RightButton; onTapped: { root.selectedId = line.modelData.id; baseMenu.popup() } }
                    }
                }
            }
            // Ce que l'on sait de la base choisie, comme la barre d'état de ChessBase.
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: info.implicitHeight + 12
                color: root.prefs.panel
                border.color: root.prefs.line
                RowLayout {
                    id: info
                    anchors.fill: parent
                    anchors.margins: 6
                    spacing: 10
                    Label {
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                        color: root.selected ? root.prefs.foreground : root.prefs.muted
                        text: root.selected ? root.selected.title + "  ·  " + root.gamesText(root.selected.games)
                                              + ((root.selected.tags || []).length ? "  ·  #" + root.selected.tags.join("  #") : "")
                                              + (root.selected.license ? "  ·  " + root.selected.license : "")
                                            : qsTr("%1 base(s), %L2 parties").arg(backend.sources.length)
                                              .arg(backend.sources.reduce(function(n, b) { return n + Number(b.games || 0) }, 0))
                    }
                    TButton { objectName: "openSelectedBase"; text: qsTr("Ouvrir"); accent: true; enabled: !!root.selected; onClicked: root.openBase(root.selectedId) }
                    TButton { text: qsTr("Étiquettes…"); enabled: !!root.selected; onClicked: root.editTags(root.selected) }
                }
            }
        }
    }
    Menu {
        id: baseMenu
        MenuItem { text: qsTr("Ouvrir"); onTriggered: root.openBase(root.selectedId) }
        MenuItem { text: qsTr("Étiquettes…"); onTriggered: root.editTags(root.selected) }
        MenuSeparator {}
        MenuItem { text: qsTr("Supprimer…"); onTriggered: root.removeBase(root.selectedId) }
    }
}
