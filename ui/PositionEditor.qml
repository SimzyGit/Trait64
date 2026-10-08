import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
Dialog {
    id: root
    property var backend
    property var pieces: []
    property string brush: "wP"
    property bool flipped: false
    title: "Éditeur de position"
    anchors.centerIn: parent
    width: Math.min(780,parent ? parent.width-24 : 780)
    height: Math.min(650,parent ? parent.height-24 : 650)
    modal:true
    standardButtons: Dialog.Cancel
    function loadFen(fen) {
        var fields=fen.split(" "), ranks=fields[0].split("/"), array=[]
        for(var r=7;r>=0;r--)for(var i=0;i<ranks[r].length;i++){
            var c=ranks[r][i], n=Number(c)
            if(n>0){for(var k=0;k<n;k++)array.push("")}
            else array.push((c===c.toUpperCase()?"w":"b")+c.toUpperCase())
        }
        pieces=array;side.currentIndex=fields[1]==="b"?1:0;castles.text=fields[2]||"-";ep.text=fields[3]||"-";half.value=Number(fields[4])||0;full.value=Number(fields[5])||1
    }
    function fen() {
        var rows=[]
        for(var r=7;r>=0;r--){var row="",empty=0;for(var c=0;c<8;c++){var p=pieces[r*8+c];if(!p){empty++;continue}if(empty){row+=empty;empty=0}row+=p[0]==="w"?p[1]:p[1].toLowerCase()}if(empty)row+=empty;rows.push(row)}
        return rows.join("/")+" "+(side.currentIndex===0?"w":"b")+" "+(castles.text||"-")+" "+(ep.text||"-")+" "+half.value+" "+full.value
    }
    onOpened:{loadFen(backend.state.fen);variant.checked=backend.state.chess960}
    Connections {target:root.backend;function onShowAnalysis(){if(root.visible)root.close()}}
    RowLayout {
        anchors.fill:parent;spacing:18
        ColumnLayout {
            Layout.fillWidth:true;Layout.fillHeight:true
            Item {Layout.fillWidth:true;Layout.fillHeight:true
                Grid {anchors.centerIn:parent;width:Math.min(parent.width,parent.height);height:width;columns:8
                    Repeater {model:64
                        delegate:Rectangle {
                            required property int index
                            property int sq:root.flipped?Math.floor(index/8)*8+7-index%8:(7-Math.floor(index/8))*8+index%8
                            width:parent.width/8;height:width
                            color:(Math.floor(index/8)+index%8)%2===0?root.backend.preferences.boardLight:root.backend.preferences.boardDark
                            Piece {anchors.fill:parent;anchors.margins:2;code:root.pieces[parent.sq]||"";pieceStyle:root.backend.preferences.pieceStyle;customPath:root.backend.preferences.customPiecePath}
                            MouseArea {anchors.fill:parent;acceptedButtons:Qt.LeftButton|Qt.RightButton;onPressed:function(mouse){var list=root.pieces.slice();list[parent.sq]=mouse.button===Qt.RightButton?"":root.brush;root.pieces=list}}
                        }
                    }
                }
            }
            RowLayout {Layout.fillWidth:true
                TButton {text:"Initiale";onClicked:root.loadFen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1")}
                TButton {text:"Vider";onClicked:{var empty=[];for(var i=0;i<64;i++)empty.push("");root.pieces=empty;castles.text="-";ep.text="-"}}
                TButton {text:"Retourner";onClicked:root.flipped=!root.flipped}
            }
        }
        ScrollView {Layout.preferredWidth:244;Layout.fillHeight:true;contentWidth:availableWidth;clip:true
            ColumnLayout {width:parent.width;spacing:12
                Label {text:"Pièce à placer";font.bold:true}
                GridLayout {columns:6;rowSpacing:3;columnSpacing:1
                    Repeater {model:["wK","wQ","wR","wB","wN","wP","bK","bQ","bR","bB","bN","bP"]
                        delegate:Button {required property string modelData;Layout.preferredWidth:38;Layout.preferredHeight:42
                            background:Rectangle {color:root.brush===modelData?root.backend.preferences.boardHighlight:root.backend.preferences.panel;radius:4}
                            contentItem:Piece {code:modelData;pieceStyle:root.backend.preferences.pieceStyle;customPath:root.backend.preferences.customPiecePath}
                            onClicked:root.brush=modelData;Accessible.name:modelData
                        }
                    }
                }
                TButton {text:"Gomme (ou clic droit)";onClicked:root.brush="";Layout.fillWidth:true}
                ComboBox {id:side;model:["Trait aux Blancs","Trait aux Noirs"];Layout.fillWidth:true}
                CheckBox {id:variant;text:"Règles Chess960"}
                Label {text:"Roques : KQkq ou lettres des tours (960)";font.pixelSize:11;wrapMode:Text.WordWrap;Layout.fillWidth:true}
                TextField {id:castles;Layout.fillWidth:true;maximumLength:4;selectByMouse:true}
                Label {text:"Case en passant (ou -)";font.pixelSize:11}
                TextField {id:ep;Layout.fillWidth:true;maximumLength:2}
                Label {text:"Demi-coups sans prise ni pion";font.pixelSize:11}
                SpinBox {id:half;from:0;to:255;editable:true;Layout.fillWidth:true}
                Label {text:"Numéro du coup";font.pixelSize:11}
                SpinBox {id:full;from:1;to:32000;value:1;editable:true;Layout.fillWidth:true}
                TButton {text:"Créer l’étude";accent:true;Layout.fillWidth:true;onClicked:root.backend.setPosition(root.fen(),variant.checked)}
            }
        }
    }
}
