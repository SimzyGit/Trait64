import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs as Native
TDialog {
    id: root
    property var backend
    function systemText(value) {var language=backend.preferences.language;return backend.uiText(value)}
    title: qsTr("Apparence et langue")
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
    Native.FolderDialog {id: customFolder;title:qsTr("Dossier contenant les 12 pièces SVG");onAccepted:root.backend.importPieceSet(selectedFolder.toString())}
    ScrollView {
        id:appearanceScroll
        anchors.fill: parent;contentWidth: availableWidth;clip:true
        ColumnLayout {
            width: appearanceScroll.availableWidth;spacing:16
            Label {text:qsTr("INTERFACE");font.bold:true}
            GridLayout {columns:2;Layout.fillWidth:true;columnSpacing:16;rowSpacing:10
                Label {text:qsTr("Langue")}
                TComboBox {objectName:"languageSelector";Layout.fillWidth:true;model:["Français","English","Système"];property var values:["fr","en","auto"];currentIndex:values.indexOf(root.backend.preferences.language);onActivated:root.backend.setPreference("language",values[currentIndex])}
                Label {text:qsTr("Police")}
                ComboBox {objectName:"fontSelector";Layout.fillWidth:true;model:root.backend.preferences.fontFamilies;currentIndex:model.indexOf(root.backend.preferences.fontFamily);onActivated:root.backend.setPreference("fontFamily",currentText)}
                Label {text:qsTr("Taille du texte")}
                SpinBox {objectName:"fontScaleControl";from:90;to:150;stepSize:10;value:Math.round(root.backend.preferences.fontScale*100);textFromValue:function(v){return v+" %"};onValueModified:root.backend.setPreference("fontScale",value/100)}
                Label {text:qsTr("Taille de l’interface")}
                SpinBox {objectName:"uiScaleControl";from:90;to:125;stepSize:5;value:Math.round(root.backend.preferences.uiScale*100);textFromValue:function(v){return v+" %"};onValueModified:root.backend.setPreference("uiScale",value/100)}
            }
            CheckBox {objectName:"figurineNotation";text:qsTr("Notation figurine (♘ ♗ ♖ ♕ ♔)");checked:root.backend.preferences.figurines;onToggled:root.backend.setPreference("figurines",checked)}
            CheckBox {text:qsTr("Navigation compacte");checked:root.backend.preferences.sidebarCompact;onToggled:root.backend.setPreference("sidebarCompact",checked)}
            CheckBox {text:qsTr("Contraste renforcé");checked:root.backend.preferences.highContrast;onToggled:root.backend.setPreference("highContrast",checked)}
            CheckBox {text:qsTr("Réduire les animations");checked:root.backend.preferences.reduceMotion;onToggled:root.backend.setPreference("reduceMotion",checked)}
            TButton {text:qsTr("Rétablir la largeur des panneaux");onClicked:root.backend.setPreference("analysisPanelRatio",.58)}
            Label {text:qsTr("JEU DE PIÈCES");font.bold:true}
            Flow {Layout.fillWidth:true;spacing:8
                Repeater {model:[{key:"cburnett",label:"Classique"},{key:"merida",label:"Merida"},{key:"spatial",label:"Spatial"},{key:"pixel",label:"Pixel"},{key:"unicode",label:"Unicode"}]
                    delegate:Button {
                        required property var modelData
                        width:Math.max(122,122*root.backend.preferences.fontScale);height:Math.max(106,106*root.backend.preferences.fontScale)
                        background:Rectangle {color:root.backend.preferences.panel;radius:2;border.width:root.backend.preferences.pieceStyle===modelData.key?2:1;border.color:root.backend.preferences.pieceStyle===modelData.key?root.backend.preferences.accentColor:root.backend.preferences.line}
                        contentItem:ColumnLayout {
                            Rectangle {Layout.alignment:Qt.AlignHCenter;Layout.preferredWidth:96;Layout.preferredHeight:52;color:root.backend.preferences.boardLight;radius:1
                                Row {anchors.centerIn:parent;Piece {code:"wN";pieceStyle:modelData.key;width:45;height:49} Piece {code:"bQ";pieceStyle:modelData.key;width:45;height:49}}
                            }
                            Label {text:root.systemText(modelData.label);Layout.alignment:Qt.AlignHCenter}
                        }
                        onClicked:root.backend.setPreference("pieceStyle",modelData.key)
                    }
                }
            }
            Flow {Layout.fillWidth:true;spacing:8
                TButton {text:qsTr("Importer mes 12 SVG…");onClicked:customFolder.open()}
                TButton {text:qsTr("Mes pièces");visible:!!root.backend.preferences.customPiecePath;onClicked:root.backend.setPreference("pieceStyle","custom")}
            }
            Label {text:qsTr("PALETTES DE L’ÉCHIQUIER");font.bold:true}
            Flow {Layout.fillWidth:true;spacing:8
                Repeater {model:["Trait64","Sauge","Noyer","Océan","Lavande","Rose","Ardoise","Contraste","Sable"]
                    delegate:TButton {required property string modelData;text:modelData;onClicked:root.backend.applyBoardPreset(modelData)}
                }
            }
            GridLayout {columns:3;Layout.fillWidth:true;columnSpacing:12;rowSpacing:9
                Repeater {model:[{key:"boardLight",label:qsTr("Cases claires")},{key:"boardDark",label:qsTr("Cases foncées")},{key:"boardHighlight",label:qsTr("Dernier coup")},{key:"backgroundColor",label:qsTr("Fond de l’application")},{key:"accentColor",label:qsTr("Couleur d’accent")}]
                    delegate:RowLayout {
                        required property var modelData
                        Layout.columnSpan:3;Layout.fillWidth:true
                        Label {text:root.systemText(modelData.label);Layout.preferredWidth:180}
                        TextField {Layout.fillWidth:true;text:root.backend.preferences[modelData.key]||root.backend.preferences.background;onEditingFinished:root.backend.setPreference(modelData.key,text);selectByMouse:true;maximumLength:7}
                        Button {Layout.preferredWidth:46;implicitHeight:32
                            background:Rectangle {color:root.backend.preferences[modelData.key]||root.backend.preferences.background;radius:5;border.color:root.backend.preferences.foreground}
                            onClicked:{picker.key=modelData.key;picker.selectedColor=root.backend.preferences[modelData.key]||root.backend.preferences.background;picker.open()}
                            Accessible.name:qsTr("Choisir ")+modelData.label
                        }
                    }
                }
            }
            Flow {Layout.fillWidth:true;spacing:12
                CheckBox {text:qsTr("Thème clair");checked:root.backend.preferences.lightTheme;onToggled:{root.backend.setPreference("lightTheme",checked);root.backend.setPreference("backgroundColor","")}}
                CheckBox {text:qsTr("Coordonnées");checked:root.backend.preferences.coordinates;onToggled:root.backend.setPreference("coordinates",checked)}
                CheckBox {text:qsTr("Coups possibles");checked:root.backend.preferences.legalMoves;onToggled:root.backend.setPreference("legalMoves",checked)}
                CheckBox {text:qsTr("Dernier coup");checked:root.backend.preferences.lastMove;onToggled:root.backend.setPreference("lastMove",checked)}
            }
            TButton {text:qsTr("Rétablir l’apparence par défaut");onClicked:root.backend.resetAppearance()}
            Label {text:qsTr("Pièces : C. M. L. Burnett et A. Hernandez Marroquin (GPL-2.0+), Maurizio Monge (MIT), therealqtpi (AGPL-3.0+). Les choix sont enregistrés automatiquement.");wrapMode:Text.WordWrap;Layout.fillWidth:true;font.pixelSize:Math.round(10*controller.preferences.fontScale*controller.preferences.uiScale);opacity:.7}
        }
    }
}
