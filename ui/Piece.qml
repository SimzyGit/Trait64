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
        sourceSize.width: Math.ceil(width*2)
        sourceSize.height: Math.ceil(height*2)
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
