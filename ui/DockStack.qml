// SPDX-License-Identifier: AGPL-3.0-or-later
// Une pile de panneaux à onglets, à la manière de Scid vs. PC : un onglet se glisse vers
// une autre pile (il y devient un onglet), au-dessus ou au-dessous d'elle (nouvelle pile),
// ou sur un bord de l'analyse. Le menu ⋯ ferme ou détache le panneau affiché.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: stack
    required property var dock
    required property string zone
    required property int position
    required property var stackData
    readonly property var panels: stackData.panels || []
    property int current: Math.max(0, Math.min(stackData.current || 0, panels.length - 1))
    function show(index) { current = index; dock.rememberCurrent(zone, position, index) }
    Connections {
        target: stack.dock
        function onDockSelect(panel) { const at = stack.panels.indexOf(panel); if (at >= 0) stack.current = at }
    }
    readonly property real scale: dock.uiScale
    color: dock.panel
    border.color: dock.line

    ColumnLayout {
        anchors.fill: parent; anchors.margins: 1; spacing: 0
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: Math.round(30 * stack.scale)
            color: dock.panel
            Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: dock.line }
            RowLayout {
                anchors.fill: parent; anchors.leftMargin: 4; spacing: 0
                Flickable {
                    Layout.fillWidth: true; Layout.fillHeight: true
                    contentWidth: tabs.width; clip: true; boundsBehavior: Flickable.StopAtBounds
                    Row {
                        id: tabs; height: parent.height
                        Repeater {
                            model: stack.panels
                            Item {
                                id: tab
                                required property string modelData
                                required property int index
                                readonly property bool selected: index === stack.current
                                objectName: modelData + "Tab"
                                width: title.implicitWidth + 22; height: tabs.height
                                Accessible.role: Accessible.PageTab
                                Accessible.name: title.text
                                Label {
                                    id: title
                                    anchors.centerIn: parent
                                    text: stack.dock.panelTitle(tab.modelData)
                                    font.pixelSize: Math.round(12 * stack.dock.textScale)
                                    font.weight: tab.selected ? Font.DemiBold : Font.Normal
                                    color: tab.selected ? stack.dock.fg : stack.dock.muted
                                }
                                // Le trait d'accent de l'onglet ouvert, comme Lichess et ChessBase.
                                Rectangle {
                                    anchors.bottom: parent.bottom; width: parent.width - 8; x: 4; height: 2
                                    radius: 1; color: stack.dock.accent; visible: tab.selected
                                }
                                HoverHandler { id: tabHover; cursorShape: drag.active ? Qt.ClosedHandCursor : Qt.PointingHandCursor }
                                Rectangle {
                                    anchors.fill: parent; anchors.margins: 3; radius: 4; z: -1
                                    color: stack.dock.fg; opacity: tabHover.hovered && !tab.selected ? .06 : 0
                                }
                                TapHandler { onTapped: stack.show(tab.index) }
                                DragHandler {
                                    id: drag; target: null; dragThreshold: 10
                                    onActiveChanged: active ? stack.dock.beginDockDrag(tab.modelData, drag)
                                                            : stack.dock.endDockDrag()
                                }
                                ToolTip.visible: tabHover.hovered && !drag.active
                                ToolTip.delay: 900
                                ToolTip.text: qsTr("Glissez l’onglet pour déplacer le panneau.")
                            }
                        }
                    }
                }
                ToolButton {
                    objectName: "dockMenu"
                    Layout.preferredWidth: Math.round(28 * stack.scale); Layout.fillHeight: true
                    text: "⋯"; flat: true
                    Accessible.name: qsTr("Options du panneau")
                    onClicked: panelMenu.popup()
                    Menu {
                        id: panelMenu
                        MenuItem {
                            text: qsTr("Détacher dans une fenêtre")
                            enabled: stack.dock.detachable(stack.panels[stack.current])
                            onTriggered: stack.dock.detach(stack.dock.detachName(stack.panels[stack.current]))
                        }
                        MenuItem { text: qsTr("Fermer le panneau"); onTriggered: stack.dock.closePanel(stack.panels[stack.current]) }
                        MenuSeparator {}
                        MenuItem { text: qsTr("Disposition d’origine"); onTriggered: stack.dock.resetDock() }
                    }
                }
            }
        }
        StackLayout {
            Layout.fillWidth: true; Layout.fillHeight: true
            currentIndex: stack.current
            Repeater {
                model: stack.panels
                Loader {
                    required property string modelData
                    required property int index
                    // Lu par les panneaux qui ne travaillent qu'à l'écran (le livre de Lichess).
                    readonly property bool panelActive: index === stack.current
                    sourceComponent: stack.dock.panelComponent(modelData)
                }
            }
        }
    }

    // Les cibles du glisser : au-dessus, en onglet, au-dessous.
    Item {
        anchors.fill: parent; z: 10
        visible: stack.dock.dockDragging
        Repeater {
            model: [{where: "before", y: 0, h: .28}, {where: "tab", y: .28, h: .44}, {where: "after", y: .72, h: .28}]
            DropArea {
                required property var modelData
                x: 0; width: stack.width
                y: stack.height * modelData.y; height: stack.height * modelData.h
                keys: ["t64panel"]
                onDropped: stack.dock.dropPanel(stack.zone, stack.position, modelData.where)
                Rectangle {
                    // Ce que deviendra le panneau : la pile entière en onglet, une moitié sinon.
                    x: 2; width: parent.width - 4; radius: 4
                    y: parent.modelData.where === "after" ? parent.height * .5 : parent.modelData.where === "tab" ? -stack.height * .28 + 2 : 2
                    height: parent.modelData.where === "tab" ? stack.height - 4 : parent.height * .5
                    visible: parent.containsDrag
                    color: Qt.rgba(stack.dock.accent.r, stack.dock.accent.g, stack.dock.accent.b, .18)
                    border.color: stack.dock.accent; border.width: 2
                }
            }
        }
    }
}
