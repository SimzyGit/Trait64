import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
ColumnLayout {
    id: root
    signal preparePlayer(string name)
    property var backend
    property var p: backend.preferences
    property var profile: backend.playerProfile
    spacing:16
    RowLayout {Layout.fillWidth:true
        Label {text:"Joueurs & préparation";font.family:"DejaVu Serif";font.pixelSize:30}
        Item {Layout.fillWidth:true}
        TextField {id:filter;placeholderText:"Rechercher un joueur";Layout.preferredWidth:240}
    }
    RowLayout {Layout.fillWidth:true;Label {text:"Statistiques calculées depuis les parties de votre bibliothèque locale.";color:root.p.muted;Layout.fillWidth:true} TButton {text:"Préparer cette position";enabled:!!root.profile.name;onClicked:root.preparePlayer(root.profile.name)}}
    SplitView {Layout.fillWidth:true;Layout.fillHeight:true
        ListView {id:players;SplitView.preferredWidth:240;SplitView.minimumWidth:160;clip:true;model:root.backend.players.filter(function(row){return row.name.toLowerCase().indexOf(filter.text.toLowerCase())>=0});spacing:4;ScrollBar.vertical:ScrollBar{}
            delegate:ItemDelegate {required property var modelData;width:players.width;implicitHeight:55
                text:modelData.name+"\n"+modelData.games+" parties";onClicked:root.backend.inspectPlayer(modelData.name)
            }
        }
        ScrollView {SplitView.fillWidth:true;contentWidth:availableWidth;clip:true
            ColumnLayout {width:parent.width;spacing:18
                Label {text:root.profile.name||"Sélectionnez un joueur";font.pixelSize:24;font.bold:true;Layout.leftMargin:20}
                Label {text:root.profile.name?(root.profile.games+" parties · Elo récent : "+(root.profile.elo||"non renseigné")):"";color:root.p.muted;Layout.leftMargin:20}
                RowLayout {visible:!!root.profile.name;Layout.fillWidth:true;Layout.leftMargin:20
                    Repeater {model:[{label:"Victoires",key:"wins"},{label:"Nulles",key:"draws"},{label:"Défaites",key:"losses"},{label:"Score",key:"score"}]
                        delegate:Rectangle {required property var modelData;Layout.fillWidth:true;implicitHeight:98;color:root.p.panel;radius:2
                            ColumnLayout {anchors.centerIn:parent;Label {text:Math.round(root.profile[modelData.key]||0)+(modelData.key==="score"?" %":"");font.pixelSize:25;font.bold:true} Label {text:modelData.label;color:root.p.muted;font.pixelSize:11}}
                        }
                    }
                }
                Label {visible:!!root.profile.name;text:(root.profile.whiteGames||0)+" avec les Blancs · "+(root.profile.blackGames||0)+" avec les Noirs · "+(root.profile.unfinished||0)+" inachevées";Layout.leftMargin:20;color:root.p.muted;font.pixelSize:11}
                Label {text:"OUVERTURES JOUÉES";visible:!!root.profile.name;Layout.leftMargin:20;font.bold:true}
                Repeater {model:root.profile.openings||[]
                    delegate:Rectangle {required property var modelData;Layout.fillWidth:true;Layout.leftMargin:20;implicitHeight:40;color:root.p.panel;radius:2
                        RowLayout {anchors.fill:parent;anchors.margins:10;Label {text:modelData.label;Layout.fillWidth:true} Label {text:modelData.count+" parties · "+(modelData.finished?Math.round(100*modelData.points/modelData.finished)+" %":"—");color:root.p.muted}}
                    }
                }
                Label {text:"PARTIES RÉCENTES";visible:!!root.profile.name;Layout.leftMargin:20;font.bold:true}
                Repeater {model:root.profile.recent||[]
                    delegate:Label {required property var modelData;Layout.leftMargin:20;Layout.fillWidth:true;text:modelData.date+" · "+modelData.opponent+" · "+modelData.result;elide:Text.ElideRight}
                }
            }
        }
    }
}
