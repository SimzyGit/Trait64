// SPDX-License-Identifier: AGPL-3.0-or-later
import QtQuick
import QtQuick.Controls
Dialog {
    id:control
    font.family:controller.preferences.fontFamily
    font.pixelSize:Math.round(13*controller.preferences.fontScale*controller.preferences.uiScale)
    property string uiLanguage:controller.preferences.language
    function translateButtons() {
        var buttons=[[Dialog.Close,qsTr("Fermer")],[Dialog.Cancel,qsTr("Annuler")],[Dialog.Ok,"OK"],[Dialog.Yes,qsTr("Oui")],[Dialog.No,qsTr("Non")],[Dialog.Save,qsTr("Enregistrer")],[Dialog.Open,qsTr("Ouvrir")]]
        for(var i=0;i<buttons.length;++i){var button=standardButton(buttons[i][0]);if(button)button.text=buttons[i][1]}
    }
    onUiLanguageChanged:Qt.callLater(translateButtons)
    onStandardButtonsChanged:Qt.callLater(translateButtons)
    Component.onCompleted:Qt.callLater(translateButtons)
}
