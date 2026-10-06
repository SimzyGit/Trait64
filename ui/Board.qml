import QtQuick
import QtQuick.Controls

Item {
    id: root
    property var backend
    property bool coordinates: true
    property var arrows: []
    property string arrowFrom: ""
    property string draggedFrom: ""
    property real cell: width / 8
    implicitWidth: 560
    implicitHeight: width
    function displayIndex(square) {
        var n = square.charCodeAt(0)-97+(square.charCodeAt(1)-49)*8
        return backend.state.flipped ? Math.floor(n/8)*8+7-n%8 : (7-Math.floor(n/8))*8+n%8
    }
    function squareAt(x,y) {
        var col=Math.floor(x/cell),row=Math.floor(y/cell)
        if(col<0||col>7||row<0||row>7)return ""
        var n=backend.state.flipped ? row*8+7-col : (7-row)*8+col
        return String.fromCharCode(97+n%8)+String.fromCharCode(49+Math.floor(n/8))
    }
    Grid {
        anchors.fill: parent
        columns: 8
        Repeater {
            model: 64
            delegate: Rectangle {
                id: tile
                required property int index
                property int sqIndex: root.backend.state.flipped ? Math.floor(index/8)*8+7-index%8 : (7-Math.floor(index/8))*8+index%8
                property var cellData: root.backend.squares[sqIndex] || ({})
                width: root.cell; height: root.cell
                color: cellData.check ? "#c45b59" : root.backend.state.selected === sqIndex ? "#c5b960" : cellData.last ? ((Math.floor(index/8)+index%8)%2===0 ? "#d9d990":"#adb56b") : (Math.floor(index/8)+index%8)%2===0 ? "#dce4d3" : "#718c7c"
                Text {
                    anchors.centerIn: parent
                    text: tile.cellData.piece || ""
                    font.family: "DejaVu Sans"
                    font.pixelSize: root.cell * 0.77
                    color: tile.cellData.white ? "#fffaf0" : "#23302e"
                    style: Text.Outline
                    styleColor: tile.cellData.white ? "#39473f" : "#151d1a"
                    opacity: root.draggedFrom===tile.cellData.square ? 0.35 : 1
                }
                Rectangle {
                    anchors.centerIn: parent
                    width: tile.cellData.occupied ? root.cell*0.88 : root.cell*0.22
                    height: width
                    radius: width/2
                    color: tile.cellData.occupied ? "transparent" : "#55596751"
                    border.color: tile.cellData.occupied ? "#8a384e39" : "transparent"
                    border.width: tile.cellData.occupied ? 5 : 0
                    visible: !!tile.cellData.legal
                }
                Text {
                    anchors.left: parent.left; anchors.top: parent.top; anchors.margins: 4
                    text: tile.cellData.square ? tile.cellData.square[1] : ""
                    visible: root.coordinates && tile.index%8===0
                    color: (Math.floor(tile.index/8)+tile.index%8)%2===0 ? "#496352":"#e4ebdd"
                    font.pixelSize: Math.max(10, root.cell*0.15); font.bold: true
                }
                Text {
                    anchors.right: parent.right; anchors.bottom: parent.bottom; anchors.margins: 4
                    text: tile.cellData.square ? tile.cellData.square[0] : ""
                    visible: root.coordinates && tile.index>=56
                    color: (Math.floor(tile.index/8)+tile.index%8)%2===0 ? "#496352":"#e4ebdd"
                    font.pixelSize: Math.max(10, root.cell*0.15); font.bold: true
                }
            }
        }
    }
    Canvas {
        id: drawing
        anchors.fill: parent
        onPaint: {
            var ctx=getContext("2d");ctx.clearRect(0,0,width,height)
            for(var i=0;i<root.arrows.length;i++) {
                var a=root.displayIndex(root.arrows[i][0]),b=root.displayIndex(root.arrows[i][1])
                var x1=(a%8+0.5)*root.cell,y1=(Math.floor(a/8)+0.5)*root.cell,x2=(b%8+0.5)*root.cell,y2=(Math.floor(b/8)+0.5)*root.cell
                ctx.strokeStyle="#d477ae55";ctx.fillStyle="#d477ae55";ctx.lineWidth=root.cell*0.13;ctx.lineCap="round"
                if(a===b){ctx.beginPath();ctx.arc(x1,y1,root.cell*0.4,0,Math.PI*2);ctx.stroke();continue}
                var angle=Math.atan2(y2-y1,x2-x1),head=root.cell*0.3
                ctx.beginPath();ctx.moveTo(x1,y1);ctx.lineTo(x2-head*0.45*Math.cos(angle),y2-head*0.45*Math.sin(angle));ctx.stroke()
                ctx.beginPath();ctx.moveTo(x2,y2);ctx.lineTo(x2-head*Math.cos(angle-0.5),y2-head*Math.sin(angle-0.5));ctx.lineTo(x2-head*Math.cos(angle+0.5),y2-head*Math.sin(angle+0.5));ctx.closePath();ctx.fill()
            }
        }
    }
    MouseArea {
        id: input
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        hoverEnabled: true
        property real downX: 0
        property real downY: 0
        property string from: ""
        property bool moved: false
        cursorShape: pressed ? Qt.ClosedHandCursor : Qt.PointingHandCursor
        onPressed: function(mouse) {downX=mouse.x;downY=mouse.y;from=root.squareAt(mouse.x,mouse.y);moved=false;if(mouse.button===Qt.RightButton)root.arrowFrom=from}
        onPositionChanged: function(mouse) {if(pressed && pressedButtons===Qt.LeftButton && Math.abs(mouse.x-downX)+Math.abs(mouse.y-downY)>12){moved=true;root.draggedFrom=from}}
        onReleased: function(mouse) {
            var to=root.squareAt(mouse.x,mouse.y)
            if(mouse.button===Qt.RightButton && to!==""){var list=root.arrows.slice();list.push([root.arrowFrom,to]);root.arrows=list;drawing.requestPaint()}
            else if(to!==""){root.arrows=[];drawing.requestPaint();if(moved)root.backend.tryMove(from,to);else root.backend.clickSquare(to)}
            root.draggedFrom=""
        }
        onCanceled: root.draggedFrom=""
    }
    Text {
        visible: root.draggedFrom!==""
        property int index: root.draggedFrom!=="" ? root.draggedFrom.charCodeAt(0)-97+(root.draggedFrom.charCodeAt(1)-49)*8 : 0
        property var cellData: root.backend.squares[index] || ({})
        x: input.mouseX-width/2; y: input.mouseY-height/2
        text: cellData.piece || "";font.family: "DejaVu Sans";font.pixelSize: root.cell*0.8
        color: cellData.white ? "#fffaf0":"#23302e";style: Text.Outline;styleColor:"#39473f"
    }
    Connections {target: root.backend;function onChanged(){drawing.requestPaint()}}
    onWidthChanged: drawing.requestPaint()
}
