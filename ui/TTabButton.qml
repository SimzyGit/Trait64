// SPDX-License-Identifier: AGPL-3.0-or-later
import QtQuick
import QtQuick.Controls

TabButton {
    id: control
    implicitHeight: Math.max(36*controller.preferences.uiScale,contentItem.implicitHeight+18)
    hoverEnabled: true
    contentItem: Text {
        text: control.text
        font: control.font
        color: control.checked ? control.palette.text : control.palette.placeholderText
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
    background: Rectangle {
        color: control.hovered || control.visualFocus ? control.palette.alternateBase : "transparent"
        Rectangle {
            anchors.bottom: parent.bottom
            width: parent.width; height: control.checked || control.visualFocus ? 2 : 1
            color: control.checked || control.visualFocus ? control.palette.highlight : control.palette.mid
        }
    }
}
