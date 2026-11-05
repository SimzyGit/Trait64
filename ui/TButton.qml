import QtQuick
import QtQuick.Controls
Button {
    id: control
    property bool accent: false
    property color accentColor: palette.highlight
    function contrast(c){function linear(v){return v<=.04045?v/12.92:Math.pow((v+.055)/1.055,2.4)}return .2126*linear(c.r)+.7152*linear(c.g)+.0722*linear(c.b)>.179?"#101810":"#ffffff"}
    // Compact desktop density: 26 px at 100 %, like a classic toolbar button.
    implicitHeight: Math.max(26*controller.preferences.uiScale,label.implicitHeight+10*controller.preferences.uiScale)
    implicitWidth: Math.max(28*controller.preferences.uiScale, label.implicitWidth + 18*controller.preferences.uiScale)
    padding: 4*controller.preferences.uiScale
    hoverEnabled: true
    font.pixelSize: Math.round(12*controller.preferences.fontScale*controller.preferences.uiScale)
    font.weight: control.accent ? Font.DemiBold : Font.Normal
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
        radius: 3
        Behavior on color {enabled:!controller.preferences.reduceMotion;ColorAnimation {duration:90}}
        color: control.accent ? (control.down ? Qt.darker(control.accentColor,1.15) : control.accentColor) : control.down ? control.palette.mid : control.hovered ? control.palette.alternateBase : control.flat ? "transparent" : control.palette.button
        // Un liseré d'accent au survol : la nuance de fond seule ne se distingue pas du
        // repos sur un thème clair, et l'œil ne sait plus quel bouton il vise.
        border.color: control.visualFocus ? control.accentColor
                    : control.hovered && !control.accent
                      ? Qt.rgba(control.accentColor.r, control.accentColor.g, control.accentColor.b, .55)
                    : control.accent || control.flat ? "transparent" : control.palette.mid
        border.width: control.visualFocus ? 2 : 1
        opacity: control.enabled ? 1 : 0.5
    }
    Accessible.name: text
    // Un bouton dit ce qu'il fait dès que son libellé n'y suffit pas : coupé, réduit à une
    // icône, ou doublé d'une infobulle qui en dit plus que lui.
    ToolTip.delay: 350
    ToolTip.visible: (hovered || visualFocus) && ToolTip.text.length > 0
                     && (label.truncated || text.length <= 2 || ToolTip.text !== text)
    ToolTip.text: Accessible.name
}
