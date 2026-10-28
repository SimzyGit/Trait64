import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
ScrollView {id:root
    signal preparePlayer(string name)
    property var backend
    property var p: backend.preferences
    function systemText(value) {var language=backend.preferences.language;return backend.uiText(value)}
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
            Column {id:profileBody;width:Math.max(1,profileScroll.availableWidth);spacing:18
                Label {text:root.profile.name||qsTr("Sélectionnez un joueur");wrapMode:Text.WordWrap;font.pixelSize:Math.round(24*controller.preferences.fontScale*controller.preferences.uiScale);font.bold:true;x:20;width:Math.max(1,profileBody.width-20)}
                Label {text:root.profile.name?(root.profile.games+qsTr(" parties · Elo récent : ")+(root.profile.elo||qsTr("non renseigné"))):"";wrapMode:Text.WordWrap;color:root.p.muted;x:20;width:Math.max(1,profileBody.width-20)}
                Grid {id:profileStats;visible:!!root.profile.name;x:20;width:Math.max(1,profileBody.width-20);columns:width>650*root.p.fontScale*root.p.uiScale?4:2;spacing:8
                    Repeater {model:[{label:"Victoires",key:"wins"},{label:"Nulles",key:"draws"},{label:"Défaites",key:"losses"},{label:"Score",key:"score"}]
                        delegate:Rectangle {required property var modelData;width:Math.max(1,(profileStats.width-(profileStats.columns-1)*8)/profileStats.columns);height:102*root.p.fontScale*root.p.uiScale;color:root.p.panel;radius:2
                            Column {width:Math.max(1,parent.width-24);anchors.centerIn:parent;spacing:6;Label {width:parent.width;text:Math.round(root.profile[modelData.key]||0)+(modelData.key==="score"?" %":"");font.pixelSize:Math.round(25*controller.preferences.fontScale*controller.preferences.uiScale);font.bold:true} Label {width:parent.width;text:root.systemText(modelData.label);wrapMode:Text.WordWrap;color:root.p.muted;font.pixelSize:Math.round(11*controller.preferences.fontScale*controller.preferences.uiScale)}}
                        }
                    }
                }
                Label {visible:!!root.profile.name;text:(root.profile.whiteGames||0)+qsTr(" avec les Blancs · ")+(root.profile.blackGames||0)+qsTr(" avec les Noirs · ")+(root.profile.unfinished||0)+qsTr(" inachevées");wrapMode:Text.WordWrap;x:20;width:Math.max(1,profileBody.width-20);color:root.p.muted;font.pixelSize:Math.round(11*controller.preferences.fontScale*controller.preferences.uiScale)}
                Label {text:qsTr("OUVERTURES JOUÉES");visible:!!root.profile.name;x:20;width:Math.max(1,profileBody.width-20);font.bold:true}
                Repeater {model:root.profile.openings||[]
                    delegate:Rectangle {required property var modelData;x:20;width:Math.max(1,profileBody.width-20);height:Math.max(40,40*root.p.fontScale*root.p.uiScale);color:root.p.panel;radius:2
                        RowLayout {anchors.fill:parent;anchors.margins:10;Label {text:modelData.label;Layout.fillWidth:true;Layout.minimumWidth:0;elide:Text.ElideRight} Label {text:modelData.count+qsTr(" parties · ")+(modelData.finished?Math.round(100*modelData.points/modelData.finished)+" %":"—");color:root.p.muted}}
                    }
                }
                Label {text:qsTr("PARTIES RÉCENTES");visible:!!root.profile.name;x:20;width:Math.max(1,profileBody.width-20);font.bold:true}
                Repeater {model:root.profile.recent||[]
                    delegate:Label {required property var modelData;x:20;width:Math.max(1,profileBody.width-20);text:modelData.date+" · "+modelData.opponent+" · "+modelData.result;elide:Text.ElideRight}
                }
            }
        }
    }
}
}
