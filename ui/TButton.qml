import QtQuick
import QtQuick.Controls
Button {
    id: control
    property bool accent: false
    property color accentColor: palette.highlight
    function contrast(c){function linear(v){return v<=.04045?v/12.92:Math.pow((v+.055)/1.055,2.4)}return .2126*linear(c.r)+.7152*linear(c.g)+.0722*linear(c.b)>.179?"#101810":"#ffffff"}
    implicitHeight: 38
    implicitWidth: Math.max(38, label.implicitWidth + 26)
    padding: 10
    hoverEnabled: true
    font.pixelSize: 13
    font.weight: Font.DemiBold
    contentItem: Text {
        id: label
        text: control.text
        font: control.font
        color: control.accent ? control.contrast(control.accentColor) : control.palette.buttonText
        opacity: control.enabled ? 1 : 0.45
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
    background: Rectangle {
        radius: 2
        color: control.accent ? (control.down ? Qt.darker(control.accentColor,1.15) : control.accentColor) : control.down ? control.palette.mid : control.hovered ? control.palette.alternateBase : control.flat ? "transparent" : control.palette.button
        border.color: control.visualFocus ? control.accentColor : control.accent || control.flat ? "transparent" : control.palette.mid
        border.width: control.visualFocus ? 2 : 1
        opacity: control.enabled ? 1 : 0.5
    }
    Accessible.name: text
}
