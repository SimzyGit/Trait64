import QtQuick
import QtQuick.Controls
// Un filtre qu'on allume ou qu'on éteint : une pastille arrondie, pleine quand elle est
// active. Une grande case vide à côté d'un mot se lisait comme un contrôle cassé.
Button {
    id: control
    checkable: true
    hoverEnabled: true
    readonly property real scale: controller.preferences.uiScale
    implicitHeight: Math.max(24 * scale, label.implicitHeight + 8 * scale)
    implicitWidth: label.implicitWidth + 22 * scale
    font.pixelSize: Math.round(12 * controller.preferences.fontScale * scale)
    contentItem: Text {
        id: label
        text: (control.checked ? "✓ " : "") + control.text
        font: control.font
        color: control.checked ? control.palette.highlight : control.palette.buttonText
        opacity: control.enabled ? 1 : 0.45
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }
    background: Rectangle {
        radius: height / 2
        color: control.checked ? Qt.rgba(control.palette.highlight.r, control.palette.highlight.g,
                                          control.palette.highlight.b, control.hovered ? .22 : .14)
                               : control.hovered ? control.palette.alternateBase : "transparent"
        border.color: control.visualFocus || control.checked ? control.palette.highlight : control.palette.mid
        border.width: control.visualFocus ? 2 : 1
        Behavior on color {enabled: !controller.preferences.reduceMotion; ColorAnimation {duration: 90}}
    }
    Accessible.role: Accessible.CheckBox
    Accessible.checkable: true
    Accessible.checked: checked
    Accessible.name: text
}
