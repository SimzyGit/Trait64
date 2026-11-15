// SPDX-License-Identifier: AGPL-3.0-or-later
// Un échiquier en vignette, tiré d'une FEN : pas de coups, pas de sélection, les couleurs
// et les pièces de l'apparence choisie. La dernière case jouée est marquée.
import QtQuick
Item {
    id:root
    property string fen
    property string lastMove // « e2e4 »
    property bool flipped:false
    property var prefs:controller.preferences
    readonly property real cell:width/8
    // La partie « pièces » de la FEN, case par case de a8 à h1.
    readonly property var cells:{
        var rows=(fen||"").split(" ")[0].split("/"),out=[]
        for(var r=0;r<8;++r){var row=rows[r]||"8"
            for(var i=0;i<row.length;++i){var c=row[i]
                if(c>="1"&&c<="8"){for(var k=0;k<Number(c);++k)out.push("")}
                else out.push((c===c.toUpperCase()?"w":"b")+c.toUpperCase())}}
        while(out.length<64)out.push("")
        return out.slice(0,64)
    }
    function squareName(index){return "abcdefgh"[index%8]+(8-Math.floor(index/8))}
    implicitWidth:160;implicitHeight:width
    Repeater {
        model:64
        Rectangle {
            required property int index
            // Index affiché → index de la FEN, retourné si l'on regarde du côté des Noirs.
            readonly property int square:root.flipped?63-index:index
            x:(index%8)*root.cell;y:Math.floor(index/8)*root.cell;width:root.cell;height:root.cell
            color:(Math.floor(square/8)+square%8)%2===0?root.prefs.boardLight:root.prefs.boardDark
            Rectangle {anchors.fill:parent;color:root.prefs.boardHighlight;opacity:.5
                visible:root.lastMove.length>=4&&(root.lastMove.slice(0,2)===root.squareName(parent.square)||root.lastMove.slice(2,4)===root.squareName(parent.square))}
            Piece {anchors.fill:parent;anchors.margins:root.cell*.04;code:root.cells[parent.square];visible:code.length>0
                pieceStyle:root.prefs.pieceStyle==="unicode"?"cburnett":root.prefs.pieceStyle;customPath:root.prefs.customPiecePath}
        }
    }
}
