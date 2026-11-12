import QtQuick
import QtQuick.Controls

Item {
    id: root
    property var backend
    property bool coordinates: true
    property var prefs: backend.preferences
    property var arrows: backend.state.arrows || []
    // L'échiquier sert aussi d'aperçu : il peut alors montrer une position qui n'est pas
    // celle de l'étude ouverte, et ne doit pas réagir aux clics.
    // Laissée vide, l'échiquier suit l'étude ouverte, case par case.
    property var squares: null
    function cellAt(n) { return (squares ? squares[n] : backend.cells["s"+n]) || ({}) }
    // Les pièces qui glissent : cases d'arrivée masquées le temps du trajet.
    property var hidden: ({})
    property var shown: []
    property var sliders: []
    property bool dropped: false
    property string position: squares ? "" : backend.state.fen
    onPositionChanged: { slide(); if (hintMoves) drawing.requestPaint() }
    onSquaresChanged: slide()
    Component.onCompleted: slide()
    function place(n) {
        var d = root.flipped ? Math.floor(n/8)*8+7-n%8 : (7-Math.floor(n/8))*8+n%8
        return Qt.point(d%8*cell, Math.floor(d/8)*cell)
    }
    // On compare la position affichée à la nouvelle : une pièce disparue d'une case et
    // réapparue sur une autre glisse de l'une à l'autre. Cela vaut pour un coup joué,
    // un retour en arrière ou un roque, sans rien savoir du coup lui-même.
    function slide() {
        var now = [], gone = [], come = [], i
        for (i = 0; i < 64; ++i) now.push(cellAt(i).code || "")
        var before = shown
        shown = now
        for (i = 0; i < sliders.length; ++i) sliders[i].destroy()
        sliders = []
        if (Object.keys(hidden).length) hidden = ({})
        var quiet = dropped || prefs.reduceMotion || before.length !== 64
        dropped = false
        if (quiet) return
        for (i = 0; i < 64; ++i) {
            if (before[i] === now[i]) continue
            if (before[i]) gone.push(i)
            if (now[i]) come.push(i)
        }
        if (come.length === 0 || come.length > 2) return
        var mask = {}
        for (i = 0; i < come.length; ++i) {
            var to = come[i], from = -1
            for (var k = 0; k < gone.length; ++k)
                if (before[gone[k]] === now[to] && gone[k] !== to) { from = gone[k]; gone.splice(k, 1); break }
            if (from < 0) continue
            mask[to] = true
            sliders.push(slider.createObject(root, {code: now[to], from: from, to: to}))
        }
        if (sliders.length) hidden = mask
    }
    Component {
        id: slider
        Piece {
            id: sliding
            property int from: 0
            property int to: 0
            z: 2
            width: root.cell*.91; height: width
            pieceStyle: root.prefs.pieceStyle; customPath: root.prefs.customPiecePath
            x: root.place(from).x + (root.cell-width)/2
            y: root.place(from).y + (root.cell-height)/2
            ParallelAnimation {
                running: true
                NumberAnimation { target: sliding; property: "x"; to: root.place(sliding.to).x + (root.cell-sliding.width)/2; duration: 140; easing.type: Easing.OutCubic }
                NumberAnimation { target: sliding; property: "y"; to: root.place(sliding.to).y + (root.cell-sliding.height)/2; duration: 140; easing.type: Easing.OutCubic }
                // La pièce reste en place, invisible, jusqu'au prochain coup : la détruire
                // ici, en pleine animation, ferait disparaître l'objet qui s'exécute.
                onFinished: {
                    var rest = Object.assign({}, root.hidden)
                    delete rest[sliding.to]
                    root.hidden = rest
                    sliding.visible = false
                }
            }
        }
    }
    property bool interactive: true
    // Premiers coups des lignes du moteur, « e2e4 d2d4 » : dessinés en flèches discrètes,
    // la meilleure plus marquée que les suivantes.
    property string hintMoves: ""
    onHintMovesChanged: drawing.requestPaint()
    property bool flipped: backend.state.flipped
    property string arrowFrom: ""
    property string draggedFrom: ""
    property real cell: width / 8
    implicitWidth: 560
    implicitHeight: width
    function displayIndex(square) {
        var n = square.charCodeAt(0)-97+(square.charCodeAt(1)-49)*8
        return root.flipped ? Math.floor(n/8)*8+7-n%8 : (7-Math.floor(n/8))*8+n%8
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
                property int sqIndex: root.flipped ? Math.floor(index/8)*8+7-index%8 : (7-Math.floor(index/8))*8+index%8
                property var cellData: root.cellAt(sqIndex)
                width: root.cell; height: root.cell
                color: (Math.floor(index/8)+index%8)%2===0 ? root.prefs.boardLight : root.prefs.boardDark
                Rectangle {anchors.fill:parent;color:tile.cellData.check ? "#d34545" : root.prefs.boardHighlight;opacity:tile.cellData.check?.78:root.backend.state.selected===tile.sqIndex?.85:.48;visible:!!tile.cellData.check || root.backend.state.selected===tile.sqIndex || (!!tile.cellData.last && root.prefs.lastMove)}
                Piece {
                    anchors.centerIn: parent
                    width:root.cell*.91;height:width
                    code: tile.cellData.code || ""
                    pieceStyle:root.prefs.pieceStyle;customPath:root.prefs.customPiecePath
                    opacity: root.draggedFrom===tile.cellData.square ? 0.35 : 1
                    visible: !root.hidden[tile.sqIndex]
                }
                Rectangle {
                    anchors.centerIn: parent
                    width: tile.cellData.occupied ? root.cell*0.88 : root.cell*0.22
                    height: width
                    radius: width/2
                    color: tile.cellData.occupied ? "transparent" : "#55596751"
                    border.color: tile.cellData.occupied ? "#8a384e39" : "transparent"
                    border.width: tile.cellData.occupied ? 5 : 0
                    visible: root.prefs.legalMoves && !!tile.cellData.legal
                }
                Text {
                    anchors.left: parent.left; anchors.top: parent.top; anchors.margins: 4
                    text: tile.cellData.square ? tile.cellData.square[1] : ""
                    visible: root.coordinates && tile.index%8===0
                    color: (Math.floor(tile.index/8)+tile.index%8)%2===0 ? root.prefs.boardDark:root.prefs.boardLight
                    font.pixelSize: Math.max(10, root.cell*0.15); font.bold: true
                }
                Text {
                    anchors.right: parent.right; anchors.bottom: parent.bottom; anchors.margins: 4
                    text: tile.cellData.square ? tile.cellData.square[0] : ""
                    visible: root.coordinates && tile.index>=56
                    color: (Math.floor(tile.index/8)+tile.index%8)%2===0 ? root.prefs.boardDark:root.prefs.boardLight
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
            var hints=root.hintMoves ? root.hintMoves.split(" ") : []
            for(var h=hints.length-1;h>=0;h--) {
                var hf=hints[h].substring(0,2),ht=hints[h].substring(2,4)
                var piece=root.cellAt(hf.charCodeAt(0)-97+(hf.charCodeAt(1)-49)*8)
                // Une ligne calculée pour la position d'avant ne doit pas flécher celle-ci.
                if(!piece.occupied||piece.white!==root.backend.state.whiteTurn)continue
                var ha=root.displayIndex(hf),hb=root.displayIndex(ht)
                var hx1=(ha%8+0.5)*root.cell,hy1=(Math.floor(ha/8)+0.5)*root.cell,hx2=(hb%8+0.5)*root.cell,hy2=(Math.floor(hb/8)+0.5)*root.cell
                var hang=Math.atan2(hy2-hy1,hx2-hx1),hhead=root.cell*(h===0?0.3:0.22)
                ctx.strokeStyle="#3d7fc4";ctx.fillStyle="#3d7fc4";ctx.globalAlpha=h===0?.62:.3;ctx.lineWidth=root.cell*(h===0?0.12:0.07);ctx.lineCap="round"
                ctx.beginPath();ctx.moveTo(hx1,hy1);ctx.lineTo(hx2-hhead*0.45*Math.cos(hang),hy2-hhead*0.45*Math.sin(hang));ctx.stroke()
                ctx.beginPath();ctx.moveTo(hx2,hy2);ctx.lineTo(hx2-hhead*Math.cos(hang-0.5),hy2-hhead*Math.sin(hang-0.5));ctx.lineTo(hx2-hhead*Math.cos(hang+0.5),hy2-hhead*Math.sin(hang+0.5));ctx.closePath();ctx.fill()
            }
            for(var i=0;i<root.arrows.length;i++) {
                var a=root.displayIndex(root.arrows[i].from),b=root.displayIndex(root.arrows[i].to)
                var x1=(a%8+0.5)*root.cell,y1=(Math.floor(a/8)+0.5)*root.cell,x2=(b%8+0.5)*root.cell,y2=(Math.floor(b/8)+0.5)*root.cell
                var col=({G:"#599c46",R:"#d75050",B:"#4289cc",Y:"#dcac35"})[root.arrows[i].color]||"#599c46";ctx.strokeStyle=col;ctx.fillStyle=col;ctx.globalAlpha=.82;ctx.lineWidth=root.cell*0.13;ctx.lineCap="round"
                if(a===b){ctx.beginPath();ctx.arc(x1,y1,root.cell*0.4,0,Math.PI*2);ctx.stroke();continue}
                var angle=Math.atan2(y2-y1,x2-x1),head=root.cell*0.3
                ctx.beginPath();ctx.moveTo(x1,y1);ctx.lineTo(x2-head*0.45*Math.cos(angle),y2-head*0.45*Math.sin(angle));ctx.stroke()
                ctx.beginPath();ctx.moveTo(x2,y2);ctx.lineTo(x2-head*Math.cos(angle-0.5),y2-head*Math.sin(angle-0.5));ctx.lineTo(x2-head*Math.cos(angle+0.5),y2-head*Math.sin(angle+0.5));ctx.closePath();ctx.fill()
            }
        }
    }
    MouseArea {
        id: input
        // Désactivée pour un aperçu : on y fait défiler des coups, on n'y joue pas.
        enabled: root.interactive
        visible: root.interactive
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        hoverEnabled: true
        // L'échiquier vit dans un panneau défilant, qui reprend la souris dès qu'elle
        // bouge de quelques pixels : sans cela, tout glisser de pièce était annulé avant
        // d'avoir commencé, et seul le clic-clic fonctionnait.
        preventStealing: true
        property real downX: 0
        property real downY: 0
        property string from: ""
        property bool moved: false
        // Qt émet doubleClicked au second appui, avant la relâche qui suit : sans ce
        // drapeau, celle-ci reposerait aussitôt l'annotation qu'on vient d'effacer.
        property bool cleared: false
        cursorShape: pressed ? Qt.ClosedHandCursor : Qt.PointingHandCursor
        onDoubleClicked: function(mouse) {
            if(mouse.button===Qt.RightButton){root.backend.clearAnnotations();cleared=true}
        }
        onPressed: function(mouse) {downX=mouse.x;downY=mouse.y;from=root.squareAt(mouse.x,mouse.y);moved=false;if(mouse.button===Qt.RightButton)root.arrowFrom=from}
        onPositionChanged: function(mouse) {if(pressed && pressedButtons===Qt.LeftButton && Math.abs(mouse.x-downX)+Math.abs(mouse.y-downY)>12){moved=true;root.draggedFrom=from}}
        onReleased: function(mouse) {
            var to=root.squareAt(mouse.x,mouse.y)
            if(cleared){cleared=false;root.draggedFrom="";return}
            if(mouse.button===Qt.RightButton && to!==""){root.backend.toggleAnnotation(root.arrowFrom,to,(mouse.modifiers & Qt.ShiftModifier)?"R":(mouse.modifiers & Qt.ControlModifier)?"B":(mouse.modifiers & Qt.AltModifier)?"Y":"G")}
            else if(to!==""){if(moved){root.dropped=true;root.backend.tryMove(from,to);root.dropped=false}else root.backend.clickSquare(to)}
            root.draggedFrom=""
        }
        onCanceled: {root.draggedFrom="";cleared=false}
    }
    // Ctrl + molette règle la taille sur place. Sans Ctrl la molette doit continuer à
    // faire défiler le panneau qui porte l'échiquier, d'où le filtre sur le modificateur.
    WheelHandler {
        acceptedModifiers: Qt.ControlModifier
        onWheel: function(event) {
            const step = event.angleDelta.y > 0 ? .05 : -.05
            root.backend.setPreference("boardScale",
                                       Math.max(.5, Math.min(1, Number(root.prefs.boardScale) + step)))
        }
    }
    Piece {
        visible: root.draggedFrom!==""
        property int index: root.draggedFrom!=="" ? root.draggedFrom.charCodeAt(0)-97+(root.draggedFrom.charCodeAt(1)-49)*8 : 0
        property var cellData: root.cellAt(index)
        x: input.mouseX-width/2; y: input.mouseY-height/2
        width:root.cell*.94;height:width
        code:cellData.code||"";pieceStyle:root.prefs.pieceStyle;customPath:root.prefs.customPiecePath
    }
    onFlippedChanged: drawing.requestPaint()
    onArrowsChanged: drawing.requestPaint()
    onWidthChanged: drawing.requestPaint()
}
