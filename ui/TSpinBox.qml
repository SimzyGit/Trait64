// SPDX-License-Identifier: AGPL-3.0-or-later
import QtQuick
import QtQuick.Controls
// Compteur compact : la valeur au centre, « − » et « + » de part et d'autre dans le même
// cadre. Les nombres suivent la langue de l'interface (« 1 024 » en français, et non
// « 1,024 »). Groupés seulement au-delà de 9 999 : un Elo ou une année ne se coupent pas.
SpinBox {
    id: control
    readonly property real unit: controller.preferences.uiScale
    property bool grouping: to >= 10000
    locale: Qt.locale(controller.preferences.language || "fr")
    textFromValue: function(value, locale) {
        return grouping ? Number(value).toLocaleString(locale, "f", 0) : String(value)
    }
    valueFromText: function(text, locale) {
        const digits = text.replace(/[^0-9-]/g, "")
        return digits.length ? parseInt(digits) : value
    }
    implicitHeight: 28 * unit
    implicitWidth: Math.max(108 * unit, contentItem.implicitWidth + 2 * implicitHeight)
    leftPadding: implicitHeight
    rightPadding: implicitHeight
    font.pixelSize: Math.round(12 * controller.preferences.fontScale * unit)
    hoverEnabled: true
    // Lié au texte affiché : un textFromValue n'est pas rappelé quand la langue change.
    contentItem: TextInput {
        text: control.displayText
        font: control.font
        color: control.palette.text
        selectionColor: Qt.rgba(control.palette.highlight.r, control.palette.highlight.g, control.palette.highlight.b, .35)
        selectedTextColor: control.palette.text
        horizontalAlignment: Qt.AlignHCenter
        verticalAlignment: Qt.AlignVCenter
        readOnly: !control.editable
        validator: control.validator
        inputMethodHints: Qt.ImhFormattedNumbersOnly
        opacity: control.enabled ? 1 : .5
    }
    component Step: Rectangle {
        required property bool pressed
        required property bool hovered
        required property bool active
        property string sign
        x: 1; y: 1
        implicitWidth: control.implicitHeight - 2
        implicitHeight: control.implicitHeight - 2
        radius: 2
        color: pressed ? control.palette.mid : hovered && active ? control.palette.alternateBase : "transparent"
        Text {
            anchors.centerIn: parent
            text: parent.sign
            font.pixelSize: Math.round(14 * controller.preferences.fontScale * control.unit)
            color: parent.active ? control.palette.buttonText : control.palette.placeholderText
            opacity: parent.active && control.enabled ? 1 : .45
        }
    }
    down.indicator: Step {
        pressed: control.down.pressed; hovered: control.down.hovered
        active: control.value > control.from; sign: "−"
    }
    up.indicator: Step {
        x: control.width - width - 1
        pressed: control.up.pressed; hovered: control.up.hovered
        active: control.value < control.to; sign: "+"
    }
    background: Rectangle {
        radius: 3
        color: control.palette.base
        border.width: control.activeFocus ? 2 : 1
        border.color: control.activeFocus ? control.palette.highlight : control.palette.mid
        opacity: control.enabled ? 1 : .55
    }
}
