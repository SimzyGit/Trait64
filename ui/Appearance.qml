import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs as Native
Dialog {
    id: root
    property var backend
    title: "Pièces, échiquier et couleurs"
    width: Math.min(760,parent ? parent.width-32 : 760)
    height: Math.min(700,parent ? parent.height-32 : 700)
    anchors.centerIn: parent
    modal: true
    standardButtons: Dialog.Close
    Native.ColorDialog {
        id: picker
        property string key: ""
        onAccepted: root.backend.setPreference(key,selectedColor.toString())
    }
    Native.FolderDialog {id: customFolder;title:"Dossier contenant les 12 pièces SVG";onAccepted:root.backend.importPieceSet(selectedFolder.toString())}
    ScrollView {
        anchors.fill: parent;contentWidth: availableWidth;clip:true
        ColumnLayout {
            width: parent.width;spacing:16
            Label {text:"JEU DE PIÈCES";font.bold:true}
            Flow {Layout.fillWidth:true;spacing:8
                Repeater {model:[{key:"cburnett",label:"Classique"},{key:"merida",label:"Merida"},{key:"spatial",label:"Spatial"},{key:"pixel",label:"Pixel"},{key:"unicode",label:"Unicode"}]
                    delegate:Button {
                        required property var modelData
                        width:122;height:106
                        background:Rectangle {color:root.backend.preferences.panel;radius:8;border.width:root.backend.preferences.pieceStyle===modelData.key?2:1;border.color:root.backend.preferences.pieceStyle===modelData.key?root.backend.preferences.accentColor:root.backend.preferences.line}
                        contentItem:ColumnLayout {
                            Rectangle {Layout.alignment:Qt.AlignHCenter;Layout.preferredWidth:96;Layout.preferredHeight:52;color:root.backend.preferences.boardLight;radius:4
                                Row {anchors.centerIn:parent;Piece {code:"wN";pieceStyle:modelData.key;width:45;height:49} Piece {code:"bQ";pieceStyle:modelData.key;width:45;height:49}}
                            }
                            Label {text:modelData.label;Layout.alignment:Qt.AlignHCenter}
                        }
                        onClicked:root.backend.setPreference("pieceStyle",modelData.key)
                    }
                }
            }
            RowLayout {
                TButton {text:"Importer mes 12 SVG…";onClicked:customFolder.open()}
                TButton {text:"Mes pièces";visible:!!root.backend.preferences.customPiecePath;onClicked:root.backend.setPreference("pieceStyle","custom")}
            }
            Label {text:"PALETTES DE L’ÉCHIQUIER";font.bold:true}
            Flow {Layout.fillWidth:true;spacing:8
                Repeater {model:["Sauge","Noyer","Océan","Lavande","Rose","Ardoise","Contraste","Sable"]
                    delegate:TButton {required property string modelData;text:modelData;onClicked:root.backend.applyBoardPreset(modelData)}
                }
            }
            GridLayout {columns:3;Layout.fillWidth:true;columnSpacing:12;rowSpacing:9
                Repeater {model:[{key:"boardLight",label:"Cases claires"},{key:"boardDark",label:"Cases foncées"},{key:"boardHighlight",label:"Dernier coup"},{key:"backgroundColor",label:"Fond de l’application"},{key:"accentColor",label:"Couleur d’accent"}]
                    delegate:RowLayout {
                        required property var modelData
                        Layout.columnSpan:3;Layout.fillWidth:true
                        Label {text:modelData.label;Layout.preferredWidth:180}
                        TextField {Layout.fillWidth:true;text:root.backend.preferences[modelData.key]||root.backend.preferences.background;onEditingFinished:root.backend.setPreference(modelData.key,text);selectByMouse:true;maximumLength:7}
                        Button {Layout.preferredWidth:46;implicitHeight:32
                            background:Rectangle {color:root.backend.preferences[modelData.key]||root.backend.preferences.background;radius:5;border.color:root.backend.preferences.foreground}
                            onClicked:{picker.key=modelData.key;picker.selectedColor=root.backend.preferences[modelData.key]||root.backend.preferences.background;picker.open()}
                            Accessible.name:"Choisir "+modelData.label
                        }
                    }
                }
            }
            Flow {Layout.fillWidth:true;spacing:12
                CheckBox {text:"Thème clair";checked:root.backend.preferences.lightTheme;onToggled:{root.backend.setPreference("lightTheme",checked);root.backend.setPreference("backgroundColor","")}}
                CheckBox {text:"Coordonnées";checked:root.backend.preferences.coordinates;onToggled:root.backend.setPreference("coordinates",checked)}
                CheckBox {text:"Coups possibles";checked:root.backend.preferences.legalMoves;onToggled:root.backend.setPreference("legalMoves",checked)}
                CheckBox {text:"Dernier coup";checked:root.backend.preferences.lastMove;onToggled:root.backend.setPreference("lastMove",checked)}
            }
            TButton {text:"Rétablir l’apparence par défaut";onClicked:root.backend.resetAppearance()}
            Label {text:"Pièces : C. M. L. Burnett et A. Hernandez Marroquin (GPL-2.0+), Maurizio Monge (MIT), therealqtpi (AGPL-3.0+). Les choix sont enregistrés automatiquement.";wrapMode:Text.WordWrap;Layout.fillWidth:true;font.pixelSize:10;opacity:.7}
        }
    }
}
