import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
ScrollView {id:root
    signal preparePlayer(string name)
    property var backend
    property var p: backend.preferences
    property var profile: backend.playerProfile
;contentWidth:availableWidth;clip:true;ScrollBar.horizontal.policy:ScrollBar.AlwaysOff
ColumnLayout {width:root.availableWidth;height:Math.max(root.availableHeight,implicitHeight);
    spacing:16
    Flow {Layout.fillWidth:true;spacing:8
        Label {text:qsTr("Joueurs & préparation");font.family:controller.preferences.fontFamily;font.pixelSize:Math.round(30*controller.preferences.fontScale*controller.preferences.uiScale)}
        Item {Layout.fillWidth:true}
        TextField {id:filter;placeholderText:qsTr("Rechercher un joueur");Layout.preferredWidth:240}
    }
    RowLayout {Layout.fillWidth:true;Label {text:qsTr("Statistiques calculées depuis les parties de votre bibliothèque locale.");wrapMode:Text.WordWrap;color:root.p.muted;Layout.fillWidth:true} TButton {text:qsTr("Préparer cette position");enabled:!!root.profile.name;onClicked:root.preparePlayer(root.profile.name)}}
    SplitView {Layout.fillWidth:true;Layout.fillHeight:true;Layout.minimumHeight:350*controller.preferences.fontScale
        ListView {id:players;SplitView.preferredWidth:240;SplitView.minimumWidth:160;clip:true;model:root.backend.players.filter(function(row){return row.name.toLowerCase().indexOf(filter.text.toLowerCase())>=0});spacing:4;ScrollBar.vertical:ScrollBar{}
            delegate:ItemDelegate {required property var modelData;width:players.width;implicitHeight:55*controller.preferences.fontScale*controller.preferences.uiScale
                text:modelData.name+"\n"+modelData.games+qsTr(" parties");onClicked:root.backend.inspectPlayer(modelData.name)
            }
        }
        ScrollView {id:profileScroll;SplitView.fillWidth:true;contentWidth:availableWidth;clip:true
            ColumnLayout {width:profileScroll.availableWidth;spacing:18
                Label {text:root.profile.name||qsTr("Sélectionnez un joueur");font.pixelSize:Math.round(24*controller.preferences.fontScale*controller.preferences.uiScale);font.bold:true;Layout.leftMargin:20}
                Label {text:root.profile.name?(root.profile.games+qsTr(" parties · Elo récent : ")+(root.profile.elo||qsTr("non renseigné"))):"";color:root.p.muted;Layout.leftMargin:20}
                RowLayout {visible:!!root.profile.name;Layout.fillWidth:true;Layout.leftMargin:20
                    Repeater {model:[{label:qsTr("Victoires"),key:"wins"},{label:qsTr("Nulles"),key:"draws"},{label:qsTr("Défaites"),key:"losses"},{label:"Score",key:"score"}]
                        delegate:Rectangle {required property var modelData;Layout.fillWidth:true;implicitHeight:98;color:root.p.panel;radius:2
                            ColumnLayout {anchors.centerIn:parent;Label {text:Math.round(root.profile[modelData.key]||0)+(modelData.key==="score"?" %":"");font.pixelSize:Math.round(25*controller.preferences.fontScale*controller.preferences.uiScale);font.bold:true} Label {text:modelData.label;color:root.p.muted;font.pixelSize:Math.round(11*controller.preferences.fontScale*controller.preferences.uiScale)}}
                        }
                    }
                }
                Label {visible:!!root.profile.name;text:(root.profile.whiteGames||0)+qsTr(" avec les Blancs · ")+(root.profile.blackGames||0)+qsTr(" avec les Noirs · ")+(root.profile.unfinished||0)+qsTr(" inachevées");Layout.leftMargin:20;color:root.p.muted;font.pixelSize:Math.round(11*controller.preferences.fontScale*controller.preferences.uiScale)}
                Label {text:qsTr("OUVERTURES JOUÉES");visible:!!root.profile.name;Layout.leftMargin:20;font.bold:true}
                Repeater {model:root.profile.openings||[]
                    delegate:Rectangle {required property var modelData;Layout.fillWidth:true;Layout.leftMargin:20;implicitHeight:40;color:root.p.panel;radius:2
                        RowLayout {anchors.fill:parent;anchors.margins:10;Label {text:modelData.label;Layout.fillWidth:true} Label {text:modelData.count+qsTr(" parties · ")+(modelData.finished?Math.round(100*modelData.points/modelData.finished)+" %":"—");color:root.p.muted}}
                    }
                }
                Label {text:qsTr("PARTIES RÉCENTES");visible:!!root.profile.name;Layout.leftMargin:20;font.bold:true}
                Repeater {model:root.profile.recent||[]
                    delegate:Label {required property var modelData;Layout.leftMargin:20;Layout.fillWidth:true;text:modelData.date+" · "+modelData.opponent+" · "+modelData.result;elide:Text.ElideRight}
                }
            }
        }
    }
}
}
