import QtQuick
Item {
    id: root
    property string code: ""
    property string pieceStyle: "cburnett"
    property string customPath: ""
    Image {
        id: svg
        anchors.fill: parent
        source: !root.code || root.pieceStyle === "unicode" ? "" : root.pieceStyle === "custom" ? "file://"+root.customPath+"/"+root.code+".svg" : "qrc:/assets/pieces/"+root.pieceStyle+"/"+root.code+".svg"
        // Qt redessine le SVG dès que sourceSize change d'un seul pixel : en tirant la
        // poignée d'un panneau, c'était soixante-quatre pièces redécodées à chaque
        // image. On arrondit au palier supérieur — la netteté est la même, le
        // redimensionnement ne redessine plus qu'une fois tous les huit pixels.
        readonly property int raster: Math.max(32, Math.ceil(Math.max(root.width, root.height)*2/16)*16)
        sourceSize.width: raster
        sourceSize.height: raster
        fillMode: Image.PreserveAspectFit
        smooth: true
        visible: status === Image.Ready
    }
    Text {
        anchors.centerIn: parent
        visible: root.pieceStyle === "unicode" || svg.status === Image.Error
        text: root.code ? ({P:"♟",N:"♞",B:"♝",R:"♜",Q:"♛",K:"♚"})[root.code[1]] || "" : ""
        font.family: "DejaVu Sans"
        font.pixelSize: root.height*.86
        color: root.code[0] === "w" ? "#fffaf0" : "#23302e"
        style: Text.Outline
        styleColor: "#26372e"
    }
}
