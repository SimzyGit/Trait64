// SPDX-License-Identifier: AGPL-3.0-or-later
import QtQuick
import QtQuick.Controls
Dialog {
    id:control
    // Popups must use the same user palette as the application window.
    readonly property var appearance:controller.preferences
    readonly property color accent:appearance.accentColor
    palette.window:appearance.background;palette.windowText:appearance.foreground
    palette.base:appearance.panel;palette.text:appearance.foreground
    palette.button:appearance.panel;palette.buttonText:appearance.foreground
    palette.mid:appearance.line;palette.alternateBase:appearance.raised
    palette.highlight:accent;palette.highlightedText:contrast(accent)
    palette.placeholderText:appearance.muted
    palette.dark:appearance.muted;palette.light:appearance.foreground
    palette.midlight:appearance.line;palette.shadow:appearance.line;palette.brightText:appearance.foreground
    function contrast(c){function linear(v){return v<=.04045?v/12.92:Math.pow((v+.055)/1.055,2.4)}return .2126*linear(c.r)+.7152*linear(c.g)+.0722*linear(c.b)>.179?"#171b21":"#ffffff"}
    font.family:controller.preferences.fontFamily
    font.pixelSize:Math.round(13*controller.preferences.fontScale*controller.preferences.uiScale)
    // Les boutons du bas à droite, à la taille des autres : ceux du thème Basic, en demi-
    // largeur et 40 px de haut, faisaient de « Fermer » le plus gros bouton de la fenêtre.
    footer:DialogButtonBox {
        alignment:Qt.AlignRight
        spacing:8
        padding:12
        topPadding:8
        background:Rectangle {color:"transparent";Rectangle {width:parent.width;height:1;color:control.palette.mid;opacity:.7}}
        delegate:TButton {
            accent:DialogButtonBox.buttonRole===DialogButtonBox.AcceptRole||DialogButtonBox.buttonRole===DialogButtonBox.YesRole
        }
    }
    header:Label {
        text:control.title
        visible:text.length>0
        elide:Text.ElideRight
        font.pixelSize:Math.round(15*controller.preferences.fontScale*controller.preferences.uiScale)
        font.weight:Font.DemiBold
        color:control.palette.windowText
        leftPadding:16;rightPadding:16
        topPadding:14;bottomPadding:4
    }
    // Mesures fixes, sans l'échelle de l'interface : liées à elle, la largeur des dialogues
    // sans largeur propre bouclait sur elle-même.
    leftPadding:16;rightPadding:16
    background:Rectangle {radius:6;color:control.palette.window;border.color:control.palette.mid}
    property string uiLanguage:controller.preferences.language
    function translateButtons() {
        var buttons=[[Dialog.Close,qsTr("Fermer")],[Dialog.Cancel,qsTr("Annuler")],[Dialog.Ok,"OK"],[Dialog.Yes,qsTr("Oui")],[Dialog.No,qsTr("Non")],[Dialog.Save,qsTr("Enregistrer")],[Dialog.Open,qsTr("Ouvrir")]]
        for(var i=0;i<buttons.length;++i){var button=standardButton(buttons[i][0]);if(button)button.text=buttons[i][1]}
    }
    onUiLanguageChanged:Qt.callLater(translateButtons)
    onStandardButtonsChanged:Qt.callLater(translateButtons)
    Component.onCompleted:Qt.callLater(translateButtons)
}
