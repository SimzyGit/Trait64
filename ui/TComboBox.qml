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
    // À la hauteur des boutons, avec un seul chevron : le double chevron du thème Basic et
    // son fond blanc sans bord faisaient tache dans les rangées d'outils.
    readonly property real unit:controller.preferences.uiScale
    implicitHeight:28*unit
    leftPadding:8*unit
    rightPadding:24*unit
    hoverEnabled:true
    font.pixelSize:Math.round(12*controller.preferences.fontScale*unit)
    contentItem:Text {
        text:control.displayText
        font:control.font
        color:control.palette.buttonText
        opacity:control.enabled?1:.45
        verticalAlignment:Text.AlignVCenter
        elide:Text.ElideRight
    }
    indicator:Canvas {
        x:control.width-width-8*control.unit
        y:(control.height-height)/2
        width:8*control.unit;height:5*control.unit
        readonly property color ink:control.palette.placeholderText
        onInkChanged:requestPaint()
        onPaint:{var g=getContext("2d");g.reset();g.strokeStyle=ink;g.lineWidth=1.4;g.beginPath();g.moveTo(.5,.5);g.lineTo(width/2,height-.5);g.lineTo(width-.5,.5);g.stroke()}
    }
    background:Rectangle {
        implicitWidth:120*control.unit
        radius:3
        color:control.down?control.palette.mid:control.hovered?control.palette.alternateBase:control.palette.button
        border.width:control.visualFocus?2:1
        border.color:control.visualFocus?control.palette.highlight:control.palette.mid
        opacity:control.enabled?1:.55
    }
    delegate:ItemDelegate {
        required property var modelData
        required property int index
        width:control.width
        text:control.translated(control.textRole?modelData[control.textRole]:modelData,index)
        highlighted:control.highlightedIndex===index
        font:control.font
    }
}
