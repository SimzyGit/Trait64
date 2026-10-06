import QtQuick
import QtQuick.Controls
Button {
    id: control
    property bool accent: false
    property color accentColor: "#9dcc9a"
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
        color: !control.enabled ? "#69766f" : control.accent ? "#142319" : control.palette.buttonText
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
    background: Rectangle {
        radius: 7
        color: control.accent ? (control.down ? "#81b47e" : control.accentColor) : control.down ? control.palette.mid : control.hovered ? control.palette.alternateBase : control.palette.button
        border.color: control.accent ? "transparent" : control.palette.mid
        opacity: control.enabled ? 1 : 0.5
    }
    Accessible.name: text
}
