// SPDX-License-Identifier: AGPL-3.0-or-later
// Le ruban : des onglets de commandes rangées par groupes, de grands boutons pour les plus
// courantes et des petits, empilés par trois, pour les autres, à la manière des logiciels
// de bureau et de ChessBase. Chaque commande est un objet {text, icon, run, on, checked,
// large} : « on » et « checked » sont des fonctions, réévaluées quand ce qu'elles lisent change.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: ribbon
    required property var tabs
    property int current: 1
    // Replié, seuls les onglets restent ; un clic sur l'un d'eux le déplie.
    property bool minimized: false
    // Dans une fenêtre basse, le corps se replie de lui-même ; un clic sur un onglet le
    // montre le temps d'une commande.
    property bool autoCollapse: false
    property bool peek: false
    readonly property bool collapsed: minimized || (autoCollapse && !peek)
    readonly property var prefs: controller.preferences
    readonly property real scale: prefs.uiScale
    readonly property real textScale: prefs.fontScale * prefs.uiScale
    color: prefs.panel
    implicitHeight: tabRow.height + (collapsed ? 0 : body.height)
    Accessible.role: Accessible.ToolBar
    Accessible.name: qsTr("Ruban")
    Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: ribbon.prefs.line }

    // Les petites commandes d'un groupe se rangent en colonnes de trois ; une grande occupe
    // une colonne à elle seule.
    function columns(items) {
        var out = [], pack = []
        for (var i = 0; i < items.length; ++i) {
            if (items[i].large) {
                if (pack.length) { out.push(pack); pack = [] }
                out.push([items[i]])
            } else {
                pack.push(items[i])
                if (pack.length === 3) { out.push(pack); pack = [] }
            }
        }
        if (pack.length) out.push(pack)
        return out
    }
    function run(item) {
        if (item.on && !item.on()) return
        peek = false
        item.run()
    }

    Rectangle {
        id: tabRow
        width: parent.width
        height: Math.round(26 * ribbon.textScale)
        color: ribbon.prefs.background
        Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: ribbon.prefs.line }
        ListView {
            id: tabList
            objectName: "ribbonTabs"
            anchors.fill: parent
            anchors.leftMargin: 4
            orientation: ListView.Horizontal
            clip: true
            spacing: 0
            interactive: contentWidth > width
            model: ribbon.tabs
            cacheBuffer: 4000
            boundsBehavior: Flickable.StopAtBounds
            delegate: AbstractButton {
                id: tabButton
                required property var modelData
                required property int index
                // Le premier onglet, Fichier, se distingue par la couleur d'accent comme dans
                // les logiciels de bureau.
                readonly property bool file: index === 0
                readonly property bool active: index === ribbon.current && !ribbon.collapsed
                objectName: "ribbonTab" + index
                height: tabList.height
                width: Math.max(implicitContentWidth + 26 * ribbon.scale, 56 * ribbon.scale)
                hoverEnabled: true
                focusPolicy: Qt.TabFocus
                Accessible.role: Accessible.PageTab
                Accessible.name: modelData.title
                Accessible.selected: active
                contentItem: Label {
                    text: tabButton.modelData.title
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    color: tabButton.file ? "#ffffff" : tabButton.active ? ribbon.prefs.accentColor : ribbon.prefs.foreground
                    font.pixelSize: Math.round(12 * ribbon.textScale)
                    font.weight: tabButton.active || tabButton.file ? Font.DemiBold : Font.Normal
                }
                background: Rectangle {
                    color: tabButton.file ? (tabButton.hovered ? Qt.darker(ribbon.prefs.accentColor, 1.15) : ribbon.prefs.accentColor)
                         : tabButton.active ? ribbon.prefs.panel
                         : tabButton.hovered ? ribbon.prefs.raised : "transparent"
                    border.width: tabButton.visualFocus ? 2 : 0
                    border.color: ribbon.prefs.accentColor
                    // L'onglet ouvert se fond dans le corps du ruban : un cadre sur trois côtés,
                    // pas de filet dessous.
                    Rectangle { visible: tabButton.active && !tabButton.file; anchors.left: parent.left; width: 1; height: parent.height; color: ribbon.prefs.line }
                    Rectangle { visible: tabButton.active && !tabButton.file; anchors.right: parent.right; width: 1; height: parent.height; color: ribbon.prefs.line }
                    Rectangle { visible: tabButton.active && !tabButton.file; width: parent.width; height: 1; color: ribbon.prefs.line }
                    Rectangle { visible: tabButton.active && !tabButton.file; anchors.bottom: parent.bottom; width: parent.width - 2; x: 1; height: 1; z: 1; color: ribbon.prefs.panel }
                }
                onClicked: {
                    if (ribbon.autoCollapse && !ribbon.minimized)
                        ribbon.peek = !(ribbon.peek && ribbon.current === index)
                    else if (ribbon.minimized)
                        ribbon.minimized = false
                    ribbon.current = index
                }
                onDoubleClicked: if (!file) ribbon.minimized = !ribbon.minimized
            }
        }
        AbstractButton {
            objectName: "ribbonMinimize"
            anchors.right: parent.right
            anchors.rightMargin: 6
            anchors.verticalCenter: parent.verticalCenter
            width: height; height: parent.height - 6
            hoverEnabled: true
            Accessible.name: ribbon.minimized ? qsTr("Déplier le ruban") : qsTr("Replier le ruban")
            ToolTip.visible: hovered
            ToolTip.text: Accessible.name + "  (Ctrl+F1)"
            contentItem: Label { text: ribbon.minimized ? "⌄" : "⌃"; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; color: ribbon.prefs.muted }
            background: Rectangle { radius: 3; color: parent.hovered ? ribbon.prefs.raised : "transparent" }
            onClicked: ribbon.minimized = !ribbon.minimized
        }
    }

    Flickable {
        id: body
        objectName: "ribbonBody"
        visible: !ribbon.collapsed
        anchors.top: tabRow.bottom
        width: parent.width
        height: Math.round(88 * ribbon.textScale)
        contentWidth: groupRow.width
        contentHeight: height
        clip: true
        interactive: contentWidth > width
        flickableDirection: Flickable.HorizontalFlick
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.horizontal: ScrollBar { policy: body.interactive ? ScrollBar.AsNeeded : ScrollBar.AlwaysOff; height: 4 }
        Row {
            id: groupRow
            height: body.height
            leftPadding: 4
            Repeater {
                model: (ribbon.tabs[ribbon.current] || {groups: []}).groups
                Item {
                    id: group
                    required property var modelData
                    width: groupColumns.width + 12 * ribbon.scale
                    height: groupRow.height
                    Row {
                        id: groupColumns
                        x: 6 * ribbon.scale
                        y: 4 * ribbon.scale
                        height: parent.height - groupTitle.height - 8 * ribbon.scale
                        spacing: 2
                        Repeater {
                            model: ribbon.columns(group.modelData.items)
                            Column {
                                id: column
                                required property var modelData
                                readonly property bool large: modelData.length === 1 && !!modelData[0].large
                                height: groupColumns.height
                                spacing: 0
                                Repeater {
                                    model: column.modelData
                                    AbstractButton {
                                        id: command
                                        required property var modelData
                                        objectName: modelData.name || ""
                                        readonly property bool on: modelData.on ? !!modelData.on() : true
                                        readonly property bool ticked: modelData.checked ? !!modelData.checked() : false
                                        enabled: on
                                        hoverEnabled: true
                                        focusPolicy: Qt.TabFocus
                                        width: column.large ? Math.max(58 * ribbon.scale, bigLabel.width + 12)
                                                            : smallRow.implicitWidth + 12
                                        height: column.large ? column.height : column.height / 3
                                        Accessible.role: modelData.checked ? Accessible.CheckBox : Accessible.Button
                                        Accessible.name: modelData.text
                                        Accessible.checked: ticked
                                        ToolTip.visible: hovered && !!modelData.hint && Window.window !== null
                                        ToolTip.delay: 500
                                        ToolTip.text: modelData.hint || ""
                                        opacity: enabled ? 1 : .4
                                        background: Rectangle {
                                            radius: 3
                                            readonly property color accent: ribbon.prefs.accentColor
                                            color: command.pressed ? Qt.rgba(accent.r, accent.g, accent.b, .28)
                                                 : command.ticked ? Qt.rgba(accent.r, accent.g, accent.b, .16)
                                                 : command.hovered ? Qt.rgba(accent.r, accent.g, accent.b, .1) : "transparent"
                                            border.width: command.visualFocus ? 2 : command.ticked ? 1 : 0
                                            border.color: Qt.rgba(accent.r, accent.g, accent.b, .6)
                                        }
                                        contentItem: Item {
                                            Column {
                                                visible: column.large
                                                anchors.centerIn: parent
                                                spacing: 3
                                                T64Icon {
                                                    anchors.horizontalCenter: parent.horizontalCenter
                                                    width: Math.round(26 * ribbon.scale); height: width
                                                    source: "qrc:/assets/icons/" + command.modelData.icon + ".svg"
                                                    color: ribbon.prefs.accentColor
                                                }
                                                Label {
                                                    id: bigLabel
                                                    anchors.horizontalCenter: parent.horizontalCenter
                                                    // Au-delà de 84 px, le texte passe à la ligne.
                                                    width: Math.min(implicitWidth, 84 * ribbon.textScale)
                                                    text: command.modelData.text
                                                    horizontalAlignment: Text.AlignHCenter
                                                    wrapMode: Text.WordWrap
                                                    maximumLineCount: 2
                                                    elide: Text.ElideRight
                                                    lineHeight: .9
                                                    color: ribbon.prefs.foreground
                                                    font.pixelSize: Math.round(11 * ribbon.textScale)
                                                }
                                            }
                                            Row {
                                                id: smallRow
                                                visible: !column.large
                                                anchors.verticalCenter: parent.verticalCenter
                                                x: 4
                                                spacing: 6
                                                T64Icon {
                                                    anchors.verticalCenter: parent.verticalCenter
                                                    width: Math.round(16 * ribbon.scale); height: width
                                                    source: "qrc:/assets/icons/" + command.modelData.icon + ".svg"
                                                    color: ribbon.prefs.accentColor
                                                }
                                                Label {
                                                    anchors.verticalCenter: parent.verticalCenter
                                                    text: command.modelData.text
                                                    color: ribbon.prefs.foreground
                                                    font.pixelSize: Math.round(11.5 * ribbon.textScale)
                                                }
                                            }
                                        }
                                        onClicked: ribbon.run(modelData)
                                    }
                                }
                            }
                        }
                    }
                    Label {
                        id: groupTitle
                        anchors.bottom: parent.bottom
                        anchors.bottomMargin: 2
                        anchors.horizontalCenter: groupColumns.horizontalCenter
                        width: Math.min(implicitWidth, parent.width - 4)
                        elide: Text.ElideRight
                        text: group.modelData.title
                        color: ribbon.prefs.muted
                        font.pixelSize: Math.round(10 * ribbon.textScale)
                    }
                    Rectangle { anchors.right: parent.right; y: 6; width: 1; height: parent.height - 12; color: ribbon.prefs.line }
                }
            }
        }
    }
}
