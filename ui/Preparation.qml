// SPDX-License-Identifier: AGPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs as Native

TDialog {
    id: root
    required property var backend
    property var preparationData: backend.preparation
    title:qsTr("Préparation par position")
    width:Math.min(1030,parent.width-32);height:Math.min(760,parent.height-32)
    anchors.centerIn:parent;modal:true;standardButtons:Dialog.Close
    function usePlayer(name) {player.text=name;open()}
    // Les parties en ligne de l'adversaire : on les rapatrie, puis on calcule dès que
    // l'import est fini.
    property bool fetching:false
    Connections {target:root.backend;function onBusyChanged(){if(root.fetching&&!root.backend.busy){root.fetching=false;source.currentIndex=0;root.calculate()}}}
    function calculate() {
        backend.preparePosition({sourceId:source.currentIndex>0?source.currentValue:0,player:player.text,
            colour:["both","white","black"][colour.currentIndex],fromYear:fromYear.value,toYear:toYear.value,
            minElo:minElo.value,maxElo:maxElo.value})
    }
    ScrollView {id:preparationViewport;anchors.fill:parent;contentWidth:availableWidth;clip:true;ScrollBar.horizontal.policy:ScrollBar.AlwaysOff
ColumnLayout {width:preparationViewport.availableWidth;height:Math.max(preparationViewport.availableHeight,implicitHeight);
        spacing:10
        Label {text:qsTr("Placez l’échiquier sur la position à préparer. Les suites sont calculées dans les parties correspondant aux filtres.");Layout.fillWidth:true;wrapMode:Text.WordWrap}
        GridLayout {
            columns:root.width>1050*controller.preferences.fontScale?4:2;Layout.fillWidth:true;columnSpacing:10
            Label {text:qsTr("Base")}
            TComboBox {id:source;onlyFirstOption:true;Layout.fillWidth:true;textRole:"title";valueRole:"id";model:[{id:0,title:"Toute la bibliothèque"}].concat(root.backend.sources)}
            Label {text:qsTr("Couleur du joueur")}
            TComboBox {id:colour;model:["Les deux","Blancs","Noirs"];Layout.fillWidth:true}
            Label {text:qsTr("Joueur / adversaire")}
            TTextField {id:player;objectName:"preparationPlayer";placeholderText:qsTr("Nom dans la base ou pseudo en ligne (vide : tous)");Layout.fillWidth:true;Layout.columnSpan:parent.columns-1;maximumLength:300;onAccepted:root.calculate()}
            Label {text:qsTr("Ses parties en ligne")}
            RowLayout {Layout.fillWidth:true;Layout.columnSpan:parent.columns-1
                TComboBox {id:onlineSite;model:["Lichess","Chess.com"]}
                TButton {objectName:"fetchOpponent";text:root.fetching?qsTr("Téléchargement…"):qsTr("Récupérer les parties de ce pseudo")
                    enabled:!root.backend.busy&&/^[A-Za-z0-9_-]{2,30}$/.test(player.text.trim())
                    onClicked:{root.fetching=true;root.backend.importOnline(onlineSite.currentIndex===0?"lichess":"chesscom",player.text.trim(),1000)}}
                Label {Layout.fillWidth:true;wrapMode:Text.WordWrap;opacity:.7;font.pixelSize:Math.round(11*controller.preferences.fontScale*controller.preferences.uiScale)
                    text:qsTr("Ses 1 000 dernières parties, ajoutées à la bibliothèque ; le calcul suit.")}
            }
            Label {text:qsTr("Années (0 : sans limite)")}
            RowLayout {TSpinBox {id:fromYear;from:0;to:9999;editable:true} Label {text:qsTr("à")} TSpinBox {id:toYear;from:0;to:9999;editable:true}}
            Label {text:qsTr("Elo moyen des deux joueurs")}
            RowLayout {TSpinBox {id:minElo;from:0;to:4000;stepSize:100;editable:true} Label {text:qsTr("à")} TSpinBox {id:maxElo;from:0;to:4000;stepSize:100;editable:true}}
        }
        Label {text:qsTr("Une borne Elo active exclut les parties où l’un des Elo manque ; une période active exclut les années inconnues. Aucun Elo n’est estimé.");Layout.fillWidth:true;wrapMode:Text.WordWrap;font.pixelSize:Math.round(11*controller.preferences.fontScale*controller.preferences.uiScale)}
        Flow {Layout.fillWidth:true;spacing:8
TButton {objectName:"calculatePreparation";text:qsTr("Calculer depuis l’échiquier");enabled:!root.backend.busy;onClicked:root.calculate()}
            TButton {text:qsTr("Annuler");visible:!!root.preparationData.running;onClicked:root.backend.cancelPreparation()}
            TButton {text:qsTr("Exporter CSV");enabled:!root.preparationData.running && (root.preparationData.rows||[]).length>0;onClicked:exportCsv.open()}
            Label {text:root.preparationData.status||qsTr("Choisissez les filtres puis calculez les statistiques.");Layout.fillWidth:true;wrapMode:Text.WordWrap}
        }
        Label {text:root.preparationData.error||"";visible:text.length>0;Layout.fillWidth:true;wrapMode:Text.WordWrap}
        Label {text:root.preparationData.fen||root.backend.state.fen;Layout.fillWidth:true;elide:Text.ElideMiddle;font.pixelSize:Math.round(11*controller.preferences.fontScale*controller.preferences.uiScale)}
        ListView {
            id:lines;objectName:"preparationResults";Layout.fillWidth:true;Layout.fillHeight:true;Layout.minimumHeight:170;clip:true;spacing:8
            model:root.preparationData.rows||[];ScrollBar.vertical:ScrollBar {}
            delegate:Rectangle {
                required property var modelData;width:lines.width;height:body.implicitHeight+24;color:root.palette.alternateBase
                ColumnLayout {
                    id:body;anchors.left:parent.left;anchors.right:parent.right;anchors.top:parent.top;anchors.margins:12
                    RowLayout {
                        Label {text:modelData.san;font.bold:true;font.pixelSize:Math.round(18*controller.preferences.fontScale*controller.preferences.uiScale);Layout.preferredWidth:80}
                        Label {text:modelData.count+qsTr(" partie(s)");Layout.fillWidth:true}
                        TButton {text:qsTr("Partie exemple");enabled:!root.backend.busy;onClicked:{root.backend.loadGame(modelData.example);root.close()}}
                        TButton {text:qsTr("Jouer la suite");enabled:!root.backend.busy && !root.backend.state.training;onClicked:{root.backend.playPreparedMove(modelData.uci);root.close()}}
                    }
                    Label {text:qsTr("Blancs gagnent : ")+modelData.white+qsTr(" · Nulles : ")+modelData.draw+qsTr(" · Noirs gagnent : ")+modelData.black+qsTr(" · Inachevées : ")+(modelData.count-modelData.white-modelData.draw-modelData.black);Layout.fillWidth:true;wrapMode:Text.WordWrap}
                    Label {text:qsTr("Elo moyen : ")+(modelData.rated>0?Math.round(modelData.averageElo)+" ("+modelData.rated+qsTr(" parties renseignées)"):qsTr("non renseigné"))+qsTr(" · Dernière année connue : ")+(modelData.lastYear||qsTr("non renseignée"));Layout.fillWidth:true;wrapMode:Text.WordWrap;font.pixelSize:Math.round(11*controller.preferences.fontScale*controller.preferences.uiScale)}
                }
            }
            Label {anchors.centerIn:parent;visible:lines.count===0 && !root.preparationData.running;text:qsTr("Aucune suite affichée.")}
        }
        Label {text:qsTr("Une partie compte une fois par coup, quel que soit l’ordre des coups ou les répétitions. Ces fréquences décrivent la base sélectionnée ; elles ne prouvent ni la qualité d’un coup ni sa nouveauté théorique.");Layout.fillWidth:true;wrapMode:Text.WordWrap;font.pixelSize:Math.round(11*controller.preferences.fontScale*controller.preferences.uiScale)}
    }
}
    Native.FileDialog {id:exportCsv;title:qsTr("Exporter la préparation");fileMode:Native.FileDialog.SaveFile;defaultSuffix:"csv";nameFilters:["CSV (*.csv)"];onAccepted:root.backend.exportPreparation(selectedFile.toString())}
}
