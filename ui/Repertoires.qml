// SPDX-License-Identifier: AGPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs as Native

Dialog {
    id: root
    parent: Overlay.overlay
    required property var backend
    property var detail: backend.repertoireState
    property double selectedId: detail.id || 0
    property bool editable: !backend.busy && !backend.state.training
    property string editFen: ""
    property var editedMove: ({})
    title: qsTr("Mes répertoires")
    width:Math.min(1000,parent.width-32); height:Math.min(790,parent.height-32)
    anchors.centerIn:parent; modal:true; standardButtons:Dialog.Close
    function syncSelection() {
        var rows = backend.repertoires
        var selected = -1
        for (var i = 0; i < rows.length; ++i)
            if (Number(rows[i].id) === Number(selectedId)) { selected = i; break }
        selector.currentIndex = selected
    }
    onOpened: {
        if (!selectedId && backend.repertoires.length) backend.selectRepertoire(backend.repertoires[0].id)
        syncSelection()
    }
    onSelectedIdChanged: syncSelection()
    ScrollView {id:repertoiresViewport;anchors.fill:parent;contentWidth:availableWidth;clip:true;ScrollBar.horizontal.policy:ScrollBar.AlwaysOff
ColumnLayout {width:repertoiresViewport.availableWidth;height:Math.max(repertoiresViewport.availableHeight,implicitHeight);
         spacing:10
        RowLayout {
            Layout.fillWidth:true
            ComboBox {
                id: selector; objectName:"repertoireSelector"; enabled:!root.backend.busy
                Layout.fillWidth:true; textRole:"name"; valueRole:"id"; model:root.backend.repertoires
                onActivated:root.backend.selectRepertoire(currentValue)
                onModelChanged:root.syncSelection()
                Component.onCompleted:root.syncSelection()
            }
            Connections {target:root.backend;function onRepertoiresChanged(){root.syncSelection()}}
            TButton {objectName:"createRepertoire";text:qsTr("Créer depuis l’étude");enabled:root.editable;onClicked:{createName.clear();createDialog.open()}}
        }
        RowLayout {
            Layout.fillWidth:true
            Label {Layout.fillWidth:true;wrapMode:Text.WordWrap;text:root.detail.loading?qsTr("Actualisation des statistiques et des réponses adverses…"):(root.detail.error||"")}
            TButton {text:root.detail.loading?qsTr("Annuler l’actualisation"):"Actualiser";onClicked:root.detail.loading?root.backend.cancelRepertoireOverview():root.backend.reloadRepertoires()}
        }
        RowLayout {
            Layout.fillWidth:true;visible:!!root.backend.repertoireTask.status
            BusyIndicator {running:!!root.backend.repertoireTask.running;visible:running;implicitWidth:24;implicitHeight:24}
            Label {Layout.fillWidth:true;wrapMode:Text.WordWrap;text:(root.backend.repertoireTask.error||root.backend.repertoireTask.status||"")+(root.backend.repertoireTask.running?" · "+root.backend.repertoireTask.positions+qsTr(" positions traitées"):"")}
            TButton {text:qsTr("Annuler");visible:!!root.backend.repertoireTask.running;onClicked:root.backend.cancelTask()}
        }
        Label {
            Layout.fillWidth:true;wrapMode:Text.WordWrap
            text:root.selectedId ? (root.detail.white?qsTr("Répertoire Blancs"):qsTr("Répertoire Noirs")) + (root.detail.chess960?" · Chess960":" · Classique") + " · "+(root.detail.positions||0)+qsTr(" positions à apprendre · ")+(root.detail.due||0)+qsTr(" à réviser") : qsTr("Préparez une étude avec ses variantes, puis créez votre répertoire.")
        }
        Flow {
            Layout.fillWidth:true; spacing:8; enabled:root.selectedId>0 && root.editable
            TButton {text:qsTr("Ajouter l’étude et ses variantes");onClicked:root.backend.addStudyToRepertoire(root.selectedId)}
            TButton {text:qsTr("Importer PGN");onClicked:{importPgn.repertoireId=root.selectedId;importPgn.open()}}
            TButton {text:qsTr("Exporter PGN");onClicked:{exportPgn.repertoireId=root.selectedId;exportPgn.open()}}
            TButton {objectName:"analyseRepertoire";text:qsTr("Analyser le répertoire");enabled:!!root.backend.analysisQueue.state.ready;onClicked:analysisDialog.open()}
            TButton {text:"Renommer";onClicked:{renameField.text=root.detail.name;renameDialog.open()}}
            TButton {text:qsTr("Supprimer");onClicked:deleteDialog.open()}
        }
        RowLayout {
            Layout.fillWidth:true
            TButton {text:qsTr("Réviser les positions dues");enabled:root.editable && (root.detail.due||0)>0;onClicked:{root.backend.startRepertoireTraining(root.selectedId);root.close()}}
            TButton {text:qsTr("Révision libre");enabled:root.editable && (root.detail.positions||0)>0;onClicked:{root.backend.startRepertoireTraining(root.selectedId,true);root.close()}}
            Label {text:(root.detail.successes||0)+qsTr(" rappels réussis / ")+(root.detail.reviews||0);Layout.fillWidth:true;horizontalAlignment:Text.AlignRight}
        }
        TabBar {
            id:section;Layout.fillWidth:true
            TabButton {text:qsTr("Position actuelle")}
            TabButton {text:qsTr("Réponses à préparer")}
            TabButton {text:qsTr("Positions à retravailler")}
        }
        StackLayout {
            currentIndex:section.currentIndex;Layout.fillWidth:true;Layout.fillHeight:true
            ColumnLayout {
                RowLayout {
                    TButton {text:qsTr("Racine du répertoire");enabled:root.selectedId>0 && root.editable;onClicked:root.backend.openRepertoirePosition(root.selectedId,root.detail.root)}
                    TButton {text:"Reculer";enabled:root.editable && root.backend.state.canBack;onClicked:root.backend.step(-1)}
                    TButton {text:qsTr("Voir l’échiquier");onClicked:{root.close();root.backend.showAnalysis()}}
                }
                Label {text:root.backend.state.fen;Layout.fillWidth:true;elide:Text.ElideMiddle;font.pixelSize:Math.round(11*controller.preferences.fontScale*controller.preferences.uiScale)}
                ListView {
                    id:moves;objectName:"repertoireMoves";Layout.fillWidth:true;Layout.fillHeight:true;Layout.minimumHeight:170;clip:true;spacing:8
                    model:root.detail.moves || [];ScrollBar.vertical:ScrollBar {}
                    delegate:Rectangle {
                        required property var modelData; required property int index
                        width:moves.width;height:moveBody.implicitHeight+20;color:root.palette.alternateBase
                        ColumnLayout {
                            id:moveBody;anchors.left:parent.left;anchors.right:parent.right;anchors.top:parent.top;anchors.margins:10
                            RowLayout {
                                Label {text:modelData.san+(index===0?" · principal":" · alternative");font.bold:true;Layout.fillWidth:true}
                                TButton {text:qsTr("Jouer");enabled:root.editable;onClicked:root.backend.move(modelData.uci)}
                                TButton {text:qsTr("Note / priorité");enabled:root.editable;onClicked:{root.editFen=root.backend.state.fen;root.editedMove=modelData;noteField.text=modelData.note;mainCheck.checked=index===0;editDialog.open()}}
                                TButton {text:qsTr("Partie modèle");visible:modelData.model>0;enabled:root.editable;onClicked:{root.backend.loadGame(modelData.model);root.close()}}
                                TButton {text:qsTr("Retirer");enabled:root.editable;onClicked:{root.editFen=root.backend.state.fen;root.editedMove=modelData;removeMove.open()}}
                            }
                            Label {text:modelData.note;visible:text.length>0;Layout.fillWidth:true;wrapMode:Text.WordWrap}
                        }
                    }
                    Label {anchors.centerIn:parent;text:qsTr("Aucune suite préparée à cette position.");visible:moves.count===0}
                }
            }
            ColumnLayout {
                Label {Layout.fillWidth:true;wrapMode:Text.WordWrap;text:qsTr("Réponses adverses présentes dans la bibliothèque mais absentes du répertoire, classées par fréquence (100 maximum). Une absence ne prouve pas une nouveauté théorique.")}
                ListView {
                    id:gaps;Layout.fillWidth:true;Layout.fillHeight:true;Layout.minimumHeight:170;clip:true;spacing:6;model:root.detail.gaps||[]
                    ScrollBar.vertical:ScrollBar {}
                    delegate:Rectangle {
                        required property var modelData;width:gaps.width;height:72;color:root.palette.alternateBase
                        RowLayout {anchors.fill:parent;anchors.margins:10
                            ColumnLayout {Layout.fillWidth:true
                                Label {text:modelData.san+" · "+modelData.count+qsTr(" partie(s) dans la base");font.bold:true}
                                Label {text:modelData.fen;Layout.fillWidth:true;elide:Text.ElideMiddle;font.pixelSize:Math.round(10*controller.preferences.fontScale*controller.preferences.uiScale)}
                            }
                            TButton {text:qsTr("Préparer");enabled:root.editable;onClicked:{root.backend.openRepertoirePosition(root.selectedId,modelData.fen);root.close()}}
                        }
                    }
                    Label {anchors.centerIn:parent;visible:gaps.count===0 && !!root.detail.gapsLoaded;text:qsTr("Aucune réponse manquante trouvée dans la base locale.")}
                }
            }
            ColumnLayout {
                Label {Layout.fillWidth:true;wrapMode:Text.WordWrap;text:qsTr("Positions où au moins un rappel a échoué. Ce suivi mesure la mémorisation de vos coups préparés, pas leur qualité échiquéenne.")}
                ListView {
                    id:weak;Layout.fillWidth:true;Layout.fillHeight:true;Layout.minimumHeight:170;clip:true;spacing:6;model:root.detail.weaknesses||[]
                    ScrollBar.vertical:ScrollBar {}
                    delegate:Rectangle {
                        required property var modelData;width:weak.width;height:72;color:root.palette.alternateBase
                        RowLayout {anchors.fill:parent;anchors.margins:10
                            ColumnLayout {Layout.fillWidth:true
                                Label {text:modelData.successes+qsTr(" réussites / ")+modelData.reviews+qsTr(" rappels · prochaine révision : ")+modelData.due}
                                Label {text:modelData.fen;Layout.fillWidth:true;elide:Text.ElideMiddle;font.pixelSize:Math.round(10*controller.preferences.fontScale*controller.preferences.uiScale)}
                            }
                            TButton {text:qsTr("Étudier");enabled:root.editable;onClicked:{root.backend.openRepertoirePosition(root.selectedId,modelData.fen);root.close()}}
                        }
                    }
                    Label {anchors.centerIn:parent;visible:weak.count===0 && !!root.detail.gapsLoaded;text:qsTr("Aucun rappel manqué enregistré.")}
                }
            }
        }
    }
}
    Dialog {
        id:createDialog;title:qsTr("Nouveau répertoire depuis l’étude");anchors.centerIn:parent;modal:true;standardButtons:Dialog.Ok|Dialog.Cancel;width:460
        ColumnLayout {anchors.fill:parent
            TextField {id:createName;objectName:"repertoireName";placeholderText:qsTr("Nom du répertoire");Layout.fillWidth:true;maximumLength:200}
            ComboBox {id:colour;model:[qsTr("Je prépare les Blancs"),qsTr("Je prépare les Noirs")];Layout.fillWidth:true}
            Label {text:qsTr("Toutes les variantes accessibles de l’étude sont ajoutées. Les transpositions partagent leurs réponses et leurs révisions.");Layout.fillWidth:true;wrapMode:Text.WordWrap}
        }
        onAccepted:root.backend.createRepertoire(createName.text,colour.currentIndex===0)
    }
    Dialog {id:renameDialog;title:"Renommer";anchors.centerIn:parent;modal:true;standardButtons:Dialog.Ok|Dialog.Cancel
        TextField {id:renameField;width:350;maximumLength:200} onAccepted:root.backend.renameRepertoire(root.selectedId,renameField.text)
    }
    Dialog {id:deleteDialog;title:qsTr("Supprimer ce répertoire ?");anchors.centerIn:parent;modal:true;standardButtons:Dialog.Yes|Dialog.No;width:450
        Label {width:parent.width;wrapMode:Text.WordWrap;text:qsTr("Les coups préparés et leurs révisions seront supprimés. Les parties de la bibliothèque seront conservées.")} onAccepted:root.backend.deleteRepertoire(root.selectedId)
    }
    Dialog {id:removeMove;title:qsTr("Retirer ce coup ?");anchors.centerIn:parent;modal:true;standardButtons:Dialog.Yes|Dialog.No;width:450
        Label {width:parent.width;wrapMode:Text.WordWrap;text:qsTr("Les suites devenues inaccessibles seront retirées. Les transpositions encore accessibles conserveront leurs révisions.")}
        onAccepted:root.backend.removeRepertoireMove(root.selectedId,root.editFen,root.editedMove.uci)
    }
    Dialog {id:editDialog;title:qsTr("Coup préparé");anchors.centerIn:parent;modal:true;standardButtons:Dialog.Ok|Dialog.Cancel;width:500;height:310
        ColumnLayout {anchors.fill:parent
            CheckBox {id:mainCheck;text:qsTr("Choisir comme coup principal")}
            ScrollView {Layout.fillWidth:true;Layout.fillHeight:true;TextArea {id:noteField;wrapMode:TextEdit.Wrap;placeholderText:qsTr("Idée, plan ou rappel…")}}
        }
        onAccepted:root.backend.editRepertoireMove(root.selectedId,root.editFen,root.editedMove.uci,noteField.text,mainCheck.checked)
    }
    Connections {target:root.backend;function onShowAnalysisQueue(){root.close()}}
    Dialog {
        id:analysisDialog;title:qsTr("Analyser toutes les variantes");anchors.centerIn:parent;modal:true
        standardButtons:Dialog.Ok|Dialog.Cancel;width:500
        ColumnLayout {
            anchors.fill:parent
            Label {text:qsTr("Profil moteur")}
            ComboBox {
                id:analysisProfile;Layout.fillWidth:true
                model: { var names=[qsTr("Réglages actuels")];var saved=root.backend.preferences.engineProfiles||[];for(var i=0;i<saved.length;++i) names.push(saved[i].name);return names }
            }
            Label {text:qsTr("Temps par position")}
            ComboBox {id:analysisBudget;Layout.fillWidth:true;model:["0,1 seconde","0,5 seconde","1 seconde","2 secondes","5 secondes","10 secondes"];currentIndex:1}
            Label {Layout.fillWidth:true;wrapMode:Text.WordWrap;text:qsTr("Une copie de toutes les variantes est transmise à la file d’analyses. Chaque position de transposition est développée une seule fois. Vos notes et vos révisions restent conservées.")}
        }
        onAccepted:root.backend.analyseRepertoire(root.selectedId,[100,500,1000,2000,5000,10000][analysisBudget.currentIndex],analysisProfile.currentIndex-1)
    }
    Native.FileDialog {id:importPgn;property double repertoireId:0;title:qsTr("Ajouter un PGN au répertoire");nameFilters:["PGN (*.pgn *.PGN)"];onAccepted:root.backend.importRepertoire(repertoireId,selectedFile.toString())}
    Native.FileDialog {id:exportPgn;property double repertoireId:0;title:qsTr("Exporter le répertoire");fileMode:Native.FileDialog.SaveFile;defaultSuffix:"pgn";nameFilters:["PGN (*.pgn)"];onAccepted:root.backend.exportRepertoire(repertoireId,selectedFile.toString())}
}
