// SPDX-License-Identifier: AGPL-3.0-or-later
import QtQuick
import QtQuick.Controls
// Translate presentation only: canonical values and selections must remain stable.
ComboBox {
    id:control
    property bool translateOptions:true
    property bool onlyFirstOption:false
    function translated(value,index) {var language=controller.preferences.language;return translateOptions&&(!onlyFirstOption||index===0)?controller.uiText(String(value)):String(value)}
    Accessible.name:displayText
    displayText:translated(currentText,currentIndex)
    delegate:ItemDelegate {
        required property var modelData
        required property int index
        width:control.width
        text:control.translated(control.textRole?modelData[control.textRole]:modelData,index)
        highlighted:control.highlightedIndex===index
        font:control.font
    }
}
