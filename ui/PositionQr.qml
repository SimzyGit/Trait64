// SPDX-License-Identifier: AGPL-3.0-or-later
// La position courante en QR code : un téléphone l'ouvre sur l'échiquier d'analyse de
// Lichess, pour continuer loin de l'ordinateur. Le lien se copie aussi.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
TDialog {
    id:root
    property var backend
    property var qr:({})
    title:qsTr("Ouvrir sur un téléphone")
    anchors.centerIn:parent;modal:true;standardButtons:Dialog.Close
    width:Math.min(420,parent.width-32)
    onAboutToShow:qr=backend.positionQr()
    ColumnLayout {
        anchors.fill:parent;spacing:10
        Label {Layout.fillWidth:true;wrapMode:Text.WordWrap;opacity:.75
            text:qsTr("Visez ce code avec l’appareil photo du téléphone : la position s’ouvre sur l’échiquier d’analyse de Lichess.")}
        Image {objectName:"positionQrImage";Layout.alignment:Qt.AlignHCenter;source:root.qr.image||"";smooth:false
            Layout.preferredWidth:Math.min(260,root.width-48);Layout.preferredHeight:Layout.preferredWidth
            sourceSize.width:Layout.preferredWidth*2;sourceSize.height:Layout.preferredWidth*2}
        TTextField {id:link;Layout.fillWidth:true;readOnly:true;text:root.qr.url||"";selectByMouse:true;onTextChanged:cursorPosition=0}
        TButton {text:qsTr("Copier le lien");Layout.alignment:Qt.AlignRight
            onClicked:{link.selectAll();link.copy();link.deselect()}}
    }
}
