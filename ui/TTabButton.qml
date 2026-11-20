// SPDX-License-Identifier: AGPL-3.0-or-later
import QtQuick
import QtQuick.Controls

TabButton {
    id: control
    implicitHeight: Math.max(26*controller.preferences.uiScale,contentItem.implicitHeight+10)
    font.pixelSize: Math.round(12*controller.preferences.fontScale*controller.preferences.uiScale)
    hoverEnabled: true
    ToolTip.delay: 350
    ToolTip.visible:(hovered||visualFocus)&&ToolTip.text.length>0&&(contentItem.truncated||ToolTip.text!==text)
    ToolTip.text:text
    contentItem: Text {
        text: control.text
        font.family: control.font.family
        font.pixelSize: control.font.pixelSize
        color: control.checked ? control.palette.text : control.palette.placeholderText
        font.weight: control.checked ? Font.DemiBold : Font.Normal
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
        leftPadding: 8; rightPadding: 8
    }
    // Onglet à soulignement : le texte de l'onglet choisi passe en pleine encre, avec un
    // trait d'accent dessous. Les onglets en boîte, à liseré et fond, faisaient formulaire.
    background: Rectangle {
        color: control.hovered && !control.checked ? Qt.rgba(control.palette.highlight.r, control.palette.highlight.g,
                                                             control.palette.highlight.b, .07) : "transparent"
        Rectangle {
            anchors.bottom: parent.bottom; width: parent.width; height: control.checked || control.visualFocus ? 2 : 0
            color: control.palette.highlight
        }
    }
}
