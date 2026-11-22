// SPDX-License-Identifier: AGPL-3.0-or-later
// Une colonne (ou une rangée) de piles de panneaux, séparées par des poignées : la hauteur
// de chaque pile se règle à la souris et se retient avec la disposition.
import QtQuick
import QtQuick.Controls

SplitView {
    id: column
    required property var dock
    required property string zone
    readonly property var stacks: dock.dockLayout[zone] || []
    // La pile qui prend la place restante : celle de la notation, sinon la dernière.
    readonly property int fillIndex: {
        for (var i = 0; i < stacks.length; ++i)
            if (stacks[i].panels.indexOf("notation") >= 0 || stacks[i].panels.indexOf("tree") >= 0) return i
        return stacks.length - 1
    }
    orientation: zone === "bottom" ? Qt.Horizontal : Qt.Vertical
    handle: Rectangle {
        implicitWidth: 5; implicitHeight: 5
        color: SplitHandle.hovered || SplitHandle.pressed ? column.dock.line : "transparent"
        Rectangle {
            anchors.centerIn: parent
            width: column.orientation === Qt.Vertical ? parent.width : 1
            height: column.orientation === Qt.Vertical ? 1 : parent.height
            color: column.dock.line
        }
    }
    onResizingChanged: if (!resizing) dock.rememberSizes(zone, column)
    Repeater {
        model: column.stacks
        DockStack {
            required property var modelData
            required property int index
            dock: column.dock; zone: column.zone; position: index; stackData: modelData
            SplitView.minimumHeight: column.orientation !== Qt.Vertical ? 0 : index === column.fillIndex ? 180 : 64
            SplitView.minimumWidth: column.orientation === Qt.Horizontal ? 180 : 0
            SplitView.fillHeight: column.orientation === Qt.Vertical && index === column.fillIndex
            SplitView.fillWidth: column.orientation === Qt.Horizontal && index === column.fillIndex
            // Une pile secondaire ne prend jamais plus du tiers : la notation garde sa place,
            // même avec une grande police.
            SplitView.preferredHeight: Math.min(modelData.size || column.dock.panelHeight(modelData.panels[0]),
                                                Math.max(64, column.height / 3))
            SplitView.preferredWidth: modelData.size || 320
        }
    }
}
