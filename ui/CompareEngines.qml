// SPDX-License-Identifier: AGPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

TDialog {
    id: root
    required property var backend
    property var selected: []
    title: qsTr("Comparer les moteurs UCI")
    width: Math.min(940,parent.width-32)
    height: Math.min(730,parent.height-32)
    anchors.centerIn: parent
    modal: true
    standardButtons: Dialog.Close
    ScrollView {id:compareenginesViewport;anchors.fill:parent;contentWidth:availableWidth;clip:true;ScrollBar.horizontal.policy:ScrollBar.AlwaysOff
ColumnLayout {width:compareenginesViewport.availableWidth;height:Math.max(compareenginesViewport.availableHeight,implicitHeight);
        spacing: 12
        Label {text:qsTr("Sélectionnez 2 à 4 profils enregistrés dans les paramètres. Chaque moteur analyse la même position avec son MultiPV et ses tables Syzygy.");Layout.fillWidth:true;wrapMode:Text.WordWrap}
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
        Label {visible:root.backend.preferences.engineProfiles.length<2;text:qsTr("Enregistrez au moins deux profils UCI pour démarrer.");Layout.fillWidth:true;wrapMode:Text.WordWrap}
        RowLayout {
            TButton {text:qsTr("Analyser la position");accent:true;enabled:root.selected.length>=2&&root.selected.length<=4;onClicked:root.backend.compareEngines(root.selected)}
            TButton {text:qsTr("Arrêter les moteurs");enabled:root.backend.comparison.running;onClicked:root.backend.comparison.stop()}
        }
        Label {text:qsTr("La comparaison suit la navigation de l’échiquier. Ressources plafonnées : 1 Gio de hash au total et partage des processeurs disponibles.");Layout.fillWidth:true;wrapMode:Text.WordWrap;font.pixelSize:Math.round(11*controller.preferences.fontScale*controller.preferences.uiScale);opacity:0.7}
        ScrollView {
            id: comparisonScroll
            Layout.fillWidth:true;Layout.fillHeight:true;contentWidth:availableWidth;clip:true
            // Positioners own transient Repeater items. Hidden Qt 6.4 layouts can
            // retain removed items until polish, then crash during a window resize.
            Column {
                id: cards
                width:comparisonScroll.availableWidth;spacing:16
                Repeater {model:root.backend.comparison.rows
                    Frame {
                        id:card
                        required property int index
                        required property var modelData
                        width:cards.width
                        contentItem: Column {
                            id: variations
                            spacing: 5
                            Label {text:card.modelData.name+" · "+card.modelData.threads+" thread(s) · "+card.modelData.hash+" Mio";font.bold:true;width:parent.width;elide:Text.ElideRight}
                            Label {text:card.modelData.status;width:parent.width;wrapMode:Text.WordWrap;font.pixelSize:Math.round(11*controller.preferences.fontScale*controller.preferences.uiScale)}
                            Repeater {model:card.modelData.lines
                                RowLayout {
                                    required property int index
                                    required property var modelData
                                    width:variations.width
                                    Label {text:modelData.eval;Layout.preferredWidth:65;font.bold:true}
                                    Label {text:"d="+modelData.depth+" · TB "+modelData.tbhits;Layout.preferredWidth:95;font.pixelSize:Math.round(10*controller.preferences.fontScale*controller.preferences.uiScale)}
                                    Label {text:modelData.san;Layout.fillWidth:true;wrapMode:Text.WordWrap;font.pixelSize:Math.round(12*controller.preferences.fontScale*controller.preferences.uiScale)}
                                    TButton {text:qsTr("Insérer");enabled:modelData.uci.length>0;onClicked:root.backend.addComparisonLine(card.index,index)}
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
}
