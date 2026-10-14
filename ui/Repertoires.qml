// SPDX-License-Identifier: AGPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs as Native

Dialog {
    id: root
    required property var backend
    property var detail: backend.repertoireState
    property double selectedId: detail.id || 0
    property bool editable: !backend.busy && !backend.state.training
    property string editFen: ""
    property var editedMove: ({})
    title: "Mes répertoires"
    width:Math.min(1000,parent.width-32); height:Math.min(790,parent.height-32)
    anchors.centerIn:parent; modal:true; standardButtons:Dialog.Close
    onOpened: if (!selectedId && backend.repertoires.length) backend.selectRepertoire(backend.repertoires[0].id)
    ColumnLayout {
        anchors.fill:parent; spacing:10
        RowLayout {
            Layout.fillWidth:true
            ComboBox {
                id: selector; objectName:"repertoireSelector"
                Layout.fillWidth:true; textRole:"name"; valueRole:"id"; model:root.backend.repertoires
                onActivated:root.backend.selectRepertoire(currentValue)
                onModelChanged:currentIndex=indexOfValue(root.selectedId)
            }
            Connections {target:root.backend;function onRepertoiresChanged(){selector.currentIndex=selector.indexOfValue(root.selectedId)}}
            TButton {objectName:"createRepertoire";text:"Créer depuis l’étude";enabled:root.editable;onClicked:{createName.clear();createDialog.open()}}
        }
        Label {
            Layout.fillWidth:true;wrapMode:Text.WordWrap
            text:root.selectedId ? (root.detail.white?"Répertoire Blancs":"Répertoire Noirs") + (root.detail.chess960?" · Chess960":" · Classique") + " · "+(root.detail.positions||0)+" positions à apprendre · "+(root.detail.due||0)+" à réviser" : "Préparez une étude avec ses variantes, puis créez votre répertoire."
        }
        Flow {
            Layout.fillWidth:true; spacing:8; enabled:root.selectedId>0 && root.editable
            TButton {text:"Ajouter l’étude et ses variantes";onClicked:root.backend.addStudyToRepertoire(root.selectedId)}
            TButton {text:"Importer PGN";onClicked:importPgn.open()}
            TButton {text:"Exporter PGN";onClicked:exportPgn.open()}
            TButton {text:"Renommer";onClicked:{renameField.text=root.detail.name;renameDialog.open()}}
            TButton {text:"Supprimer";onClicked:deleteDialog.open()}
        }
        RowLayout {
            Layout.fillWidth:true
            TButton {text:"Réviser les positions dues";enabled:root.editable && (root.detail.due||0)>0;onClicked:{root.backend.startRepertoireTraining(root.selectedId);root.close()}}
            TButton {text:"Révision libre";enabled:root.editable && (root.detail.positions||0)>0;onClicked:{root.backend.startRepertoireTraining(root.selectedId,true);root.close()}}
            Label {text:(root.detail.successes||0)+" rappels réussis / "+(root.detail.reviews||0);Layout.fillWidth:true;horizontalAlignment:Text.AlignRight}
        }
        TabBar {
            id:section;Layout.fillWidth:true
            TabButton {text:"Position actuelle"}
            TabButton {text:"Réponses à préparer"}
            TabButton {text:"Positions à retravailler"}
        }
        StackLayout {
            currentIndex:section.currentIndex;Layout.fillWidth:true;Layout.fillHeight:true
            ColumnLayout {
                RowLayout {
                    TButton {text:"Racine du répertoire";enabled:root.selectedId>0 && root.editable;onClicked:root.backend.openRepertoirePosition(root.selectedId,root.detail.root)}
                    TButton {text:"Reculer";enabled:root.editable && root.backend.state.canBack;onClicked:root.backend.step(-1)}
                    TButton {text:"Voir l’échiquier";onClicked:{root.close();root.backend.showAnalysis()}}
                }
                Label {text:root.backend.state.fen;Layout.fillWidth:true;elide:Text.ElideMiddle;font.pixelSize:11}
                ListView {
                    id:moves;objectName:"repertoireMoves";Layout.fillWidth:true;Layout.fillHeight:true;clip:true;spacing:8
                    model:root.detail.moves || [];ScrollBar.vertical:ScrollBar {}
                    delegate:Rectangle {
                        required property var modelData; required property int index
                        width:moves.width;height:moveBody.implicitHeight+20;color:root.palette.alternateBase
                        ColumnLayout {
                            id:moveBody;anchors.left:parent.left;anchors.right:parent.right;anchors.top:parent.top;anchors.margins:10
                            RowLayout {
                                Label {text:modelData.san+(index===0?" · principal":" · alternative");font.bold:true;Layout.fillWidth:true}
                                TButton {text:"Jouer";enabled:root.editable;onClicked:root.backend.move(modelData.uci)}
                                TButton {text:"Note / priorité";enabled:root.editable;onClicked:{root.editFen=root.backend.state.fen;root.editedMove=modelData;noteField.text=modelData.note;mainCheck.checked=index===0;editDialog.open()}}
                                TButton {text:"Partie modèle";visible:modelData.model>0;enabled:root.editable;onClicked:{root.backend.loadGame(modelData.model);root.close()}}
                                TButton {text:"Retirer";enabled:root.editable;onClicked:{root.editFen=root.backend.state.fen;root.editedMove=modelData;removeMove.open()}}
                            }
                            Label {text:modelData.note;visible:text.length>0;Layout.fillWidth:true;wrapMode:Text.WordWrap}
                        }
                    }
                    Label {anchors.centerIn:parent;text:"Aucune suite préparée à cette position.";visible:moves.count===0}
                }
            }
            ColumnLayout {
                Label {Layout.fillWidth:true;wrapMode:Text.WordWrap;text:"Réponses adverses présentes dans la bibliothèque mais absentes du répertoire, classées par fréquence (100 maximum). Une absence ne prouve pas une nouveauté théorique."}
                ListView {
                    id:gaps;Layout.fillWidth:true;Layout.fillHeight:true;clip:true;spacing:6;model:root.detail.gaps||[]
                    ScrollBar.vertical:ScrollBar {}
                    delegate:Rectangle {
                        required property var modelData;width:gaps.width;height:72;color:root.palette.alternateBase
                        RowLayout {anchors.fill:parent;anchors.margins:10
                            ColumnLayout {Layout.fillWidth:true
                                Label {text:modelData.san+" · "+modelData.count+" partie(s) dans la base";font.bold:true}
                                Label {text:modelData.fen;Layout.fillWidth:true;elide:Text.ElideMiddle;font.pixelSize:10}
                            }
                            TButton {text:"Préparer";enabled:root.editable;onClicked:{root.backend.openRepertoirePosition(root.selectedId,modelData.fen);root.close()}}
                        }
                    }
                    Label {anchors.centerIn:parent;visible:gaps.count===0;text:"Aucune réponse manquante trouvée dans la base locale."}
                }
            }
            ColumnLayout {
                Label {Layout.fillWidth:true;wrapMode:Text.WordWrap;text:"Positions où au moins un rappel a échoué. Ce suivi mesure la mémorisation de vos coups préparés, pas leur qualité échiquéenne."}
                ListView {
                    id:weak;Layout.fillWidth:true;Layout.fillHeight:true;clip:true;spacing:6;model:root.detail.weaknesses||[]
                    ScrollBar.vertical:ScrollBar {}
                    delegate:Rectangle {
                        required property var modelData;width:weak.width;height:72;color:root.palette.alternateBase
                        RowLayout {anchors.fill:parent;anchors.margins:10
                            ColumnLayout {Layout.fillWidth:true
                                Label {text:modelData.successes+" réussites / "+modelData.reviews+" rappels · prochaine révision : "+modelData.due}
                                Label {text:modelData.fen;Layout.fillWidth:true;elide:Text.ElideMiddle;font.pixelSize:10}
                            }
                            TButton {text:"Étudier";enabled:root.editable;onClicked:{root.backend.openRepertoirePosition(root.selectedId,modelData.fen);root.close()}}
                        }
                    }
                    Label {anchors.centerIn:parent;visible:weak.count===0;text:"Aucun rappel manqué enregistré."}
                }
            }
        }
    }
    Dialog {
        id:createDialog;title:"Nouveau répertoire depuis l’étude";anchors.centerIn:parent;modal:true;standardButtons:Dialog.Ok|Dialog.Cancel;width:460
        ColumnLayout {anchors.fill:parent
            TextField {id:createName;objectName:"repertoireName";placeholderText:"Nom du répertoire";Layout.fillWidth:true;maximumLength:200}
            ComboBox {id:colour;model:["Je prépare les Blancs","Je prépare les Noirs"];Layout.fillWidth:true}
            Label {text:"Toutes les variantes accessibles de l’étude sont ajoutées. Les transpositions partagent leurs réponses et leurs révisions.";Layout.fillWidth:true;wrapMode:Text.WordWrap}
        }
        onAccepted:root.backend.createRepertoire(createName.text,colour.currentIndex===0)
    }
    Dialog {id:renameDialog;title:"Renommer";anchors.centerIn:parent;modal:true;standardButtons:Dialog.Ok|Dialog.Cancel
        TextField {id:renameField;width:350;maximumLength:200} onAccepted:root.backend.renameRepertoire(root.selectedId,renameField.text)
    }
    Dialog {id:deleteDialog;title:"Supprimer ce répertoire ?";anchors.centerIn:parent;modal:true;standardButtons:Dialog.Yes|Dialog.No;width:450
        Label {width:parent.width;wrapMode:Text.WordWrap;text:"Les coups préparés et leurs révisions seront supprimés. Les parties de la bibliothèque seront conservées."} onAccepted:root.backend.deleteRepertoire(root.selectedId)
    }
    Dialog {id:removeMove;title:"Retirer ce coup ?";anchors.centerIn:parent;modal:true;standardButtons:Dialog.Yes|Dialog.No;width:450
        Label {width:parent.width;wrapMode:Text.WordWrap;text:"Les suites devenues inaccessibles seront retirées. Les transpositions encore accessibles conserveront leurs révisions."}
        onAccepted:root.backend.removeRepertoireMove(root.selectedId,root.editFen,root.editedMove.uci)
    }
    Dialog {id:editDialog;title:"Coup préparé";anchors.centerIn:parent;modal:true;standardButtons:Dialog.Ok|Dialog.Cancel;width:500;height:310
        ColumnLayout {anchors.fill:parent
            CheckBox {id:mainCheck;text:"Choisir comme coup principal"}
            ScrollView {Layout.fillWidth:true;Layout.fillHeight:true;TextArea {id:noteField;wrapMode:TextEdit.Wrap;placeholderText:"Idée, plan ou rappel…"}}
        }
        onAccepted:root.backend.editRepertoireMove(root.selectedId,root.editFen,root.editedMove.uci,noteField.text,mainCheck.checked)
    }
    Native.FileDialog {id:importPgn;title:"Ajouter un PGN au répertoire";nameFilters:["PGN (*.pgn *.PGN)"];onAccepted:root.backend.importRepertoire(root.selectedId,selectedFile.toString())}
    Native.FileDialog {id:exportPgn;title:"Exporter le répertoire";fileMode:Native.FileDialog.SaveFile;defaultSuffix:"pgn";nameFilters:["PGN (*.pgn)"];onAccepted:root.backend.exportRepertoire(root.selectedId,selectedFile.toString())}
}
