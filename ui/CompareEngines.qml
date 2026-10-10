// SPDX-License-Identifier: AGPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: root
    required property var backend
    property var selected: []
    title: "Comparer les moteurs UCI"
    width: Math.min(940,parent.width-32)
    height: Math.min(730,parent.height-32)
    anchors.centerIn: parent
    modal: true
    standardButtons: Dialog.Close
    ColumnLayout {
        anchors.fill: parent
        spacing: 12
        Label {text:"Sélectionnez 2 à 4 profils enregistrés dans les paramètres. Chaque moteur analyse la même position avec son MultiPV et ses tables Syzygy.";Layout.fillWidth:true;wrapMode:Text.WordWrap}
        Flow {
            Layout.fillWidth:true
            spacing: 8
            Repeater {model:root.backend.preferences.engineProfiles
                CheckBox {required property int index;required property var modelData;text:modelData.name
                    checked:root.selected.indexOf(index)>=0
                    onToggled:{let a=root.selected.slice();let n=a.indexOf(index);if(checked&&n<0)a.push(index);else if(!checked&&n>=0)a.splice(n,1);root.selected=a}
                }
            }
        }
        Label {visible:root.backend.preferences.engineProfiles.length<2;text:"Enregistrez au moins deux profils UCI pour démarrer.";Layout.fillWidth:true;wrapMode:Text.WordWrap}
        RowLayout {
            TButton {text:"Analyser la position";accent:true;enabled:root.selected.length>=2&&root.selected.length<=4;onClicked:root.backend.compareEngines(root.selected)}
            TButton {text:"Arrêter les moteurs";enabled:root.backend.comparison.running;onClicked:root.backend.comparison.stop()}
        }
        Label {text:"La comparaison suit la navigation de l’échiquier. Ressources plafonnées : 1 Gio de hash au total et partage des processeurs disponibles.";Layout.fillWidth:true;wrapMode:Text.WordWrap;font.pixelSize:11;opacity:0.7}
        ScrollView {
            Layout.fillWidth:true;Layout.fillHeight:true;contentWidth:availableWidth;clip:true
            ColumnLayout {
                width:parent.width;spacing:16
                Repeater {model:root.backend.comparison.rows
                    Frame {
                        id:card
                        required property int index
                        required property var modelData
                        Layout.fillWidth:true
                        ColumnLayout {
                            anchors.fill:parent
                            Label {text:card.modelData.name+" · "+card.modelData.threads+" thread(s) · "+card.modelData.hash+" Mio";font.bold:true;Layout.fillWidth:true;elide:Text.ElideRight}
                            Label {text:card.modelData.status;Layout.fillWidth:true;wrapMode:Text.WordWrap;font.pixelSize:11}
                            Repeater {model:card.modelData.lines
                                RowLayout {
                                    required property int index
                                    required property var modelData
                                    Layout.fillWidth:true
                                    Label {text:modelData.eval;Layout.preferredWidth:65;font.bold:true}
                                    Label {text:"d="+modelData.depth+" · TB "+modelData.tbhits;Layout.preferredWidth:95;font.pixelSize:10}
                                    Label {text:modelData.san;Layout.fillWidth:true;wrapMode:Text.WordWrap;font.pixelSize:12}
                                    TButton {text:"Insérer";enabled:modelData.uci.length>0;onClicked:root.backend.addComparisonLine(card.index,index)}
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
