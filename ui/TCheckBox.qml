// SPDX-License-Identifier: AGPL-3.0-or-later
import QtQuick
import QtQuick.Controls
// Case à cocher de 16 px, pleine d'accent quand elle est cochée. Celle du thème Basic,
// deux fois plus grande, prenait plus de place que son libellé dans les dialogues.
CheckBox {
    id: control
    readonly property real unit: controller.preferences.uiScale
    hoverEnabled: true
    spacing: 8 * unit
    padding: 4 * unit
    font.pixelSize: Math.round(12 * controller.preferences.fontScale * unit)
    indicator: Rectangle {
        implicitWidth: 16 * control.unit
        implicitHeight: 16 * control.unit
        x: control.leftPadding
        y: control.topPadding + (control.availableHeight - height) / 2
        radius: 3
        readonly property bool on: control.checkState !== Qt.Unchecked
        color: on ? control.palette.highlight : control.palette.base
        border.width: control.visualFocus ? 2 : 1
        border.color: control.visualFocus || on ? control.palette.highlight
                    : control.hovered ? Qt.darker(control.palette.mid, 1.2) : control.palette.mid
        opacity: control.enabled ? 1 : .5
        Canvas {
            anchors.fill: parent
            visible: parent.on
            readonly property color ink: control.palette.highlightedText
            readonly property bool partial: control.checkState === Qt.PartiallyChecked
            onInkChanged: requestPaint()
            onPartialChanged: requestPaint()
            onWidthChanged: requestPaint()
            onPaint: {
                const g = getContext("2d"), w = width, h = height
                g.reset(); g.strokeStyle = ink; g.lineWidth = 1.8; g.lineCap = "round"; g.lineJoin = "round"
                g.beginPath()
                if (partial) { g.moveTo(w * .28, h / 2); g.lineTo(w * .72, h / 2) }
                else { g.moveTo(w * .26, h * .52); g.lineTo(w * .43, h * .7); g.lineTo(w * .75, h * .32) }
                g.stroke()
            }
        }
    }
    contentItem: Text {
        text: control.text
        font: control.font
        color: control.palette.windowText
        opacity: control.enabled ? 1 : .5
        verticalAlignment: Text.AlignVCenter
        leftPadding: control.indicator.width + control.spacing
    }
}
