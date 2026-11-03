// SPDX-License-Identifier: AGPL-3.0-or-later
import QtQuick
import QtQuick.Controls

TabButton {
    id: control
    implicitHeight: Math.max(26*controller.preferences.uiScale,contentItem.implicitHeight+10)
    font.pixelSize: Math.round(12*controller.preferences.fontScale*controller.preferences.uiScale)
    hoverEnabled: true
    ToolTip.visible:(hovered||visualFocus)&&contentItem.truncated
    ToolTip.text:text
    contentItem: Text {
        text: control.text
        font: control.font
        color: control.checked ? control.palette.text : control.palette.placeholderText
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
        leftPadding: 8; rightPadding: 8
    }
    // Document-style tab: the selected one takes the pane colour and opens onto it.
    background: Rectangle {
        color: control.checked ? control.palette.base : control.hovered ? control.palette.alternateBase : "transparent"
        border.color: control.checked || control.visualFocus ? control.palette.mid : "transparent"
        border.width: 1
        Rectangle { visible: control.checked; anchors.bottom: parent.bottom; width: parent.width - 2; x: 1; height: 1; color: control.palette.base }
        Rectangle { visible: control.visualFocus; anchors.top: parent.top; width: parent.width; height: 2; color: control.palette.highlight }
    }
}
