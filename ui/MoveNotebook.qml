import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    objectName: "notebookRoot"
    readonly property bool followPending: followTimer.running || pageTimer.running
    required property var backend
    required property real textScale
    required property color foreground
    required property color muted
    required property color accent
    required property color raised
    required property color lineColor
    property var outerViewport: null
    property int followedNode: -1
    readonly property real numberWidth: Math.max(32, 40 * textScale)

    property bool forceFollow: false
    function requestFollow(force) {
        forceFollow = forceFollow || force
        followTimer.restart()
    }
    // Wait for Qt Quick's layout pass after changing fonts/window geometry.
    Timer {
        id: followTimer; interval: 16
        onTriggered: { var force = root.forceFollow; root.forceFollow = false; root.followSelection(force) }
    }
    Timer { id: pageTimer; interval: 16; onTriggered: root.revealInPage() }
    function revealInPage() {
        if (!visible || !outerViewport || moves.currentIndex < 0) return
        var item = moves.itemAtIndex(moves.currentIndex)
        if (!item) return
        var top = moves.mapToItem(outerViewport, 0, item.y - moves.contentY).y
        var bottom = top + item.moveHeight
        var flick = outerViewport.contentItem
        var target = flick.contentY
        if (top < 0) target += top
        else if (bottom > outerViewport.availableHeight) target += bottom - outerViewport.availableHeight
        flick.contentY = Math.max(0, Math.min(target, flick.contentHeight - flick.height))
    }
    function followSelection(force) {
        var node = backend.state.node
        var rows = backend.moveRows
        var selected = -1
        for (var i = 0; i < rows.length; ++i) {
            if ((rows[i].initial && node === 0) ||
                (rows[i].white && rows[i].white.id === node) ||
                (rows[i].black && rows[i].black.id === node)) { selected = i; break }
        }
        moves.currentIndex = selected
        if (!visible || selected < 0 || (!force && node === followedNode)) return
        followedNode = node
        // Keep the move cells visible, without trying to fit a very long comment.
        var item = moves.itemAtIndex(selected)
        if (!item) moves.positionViewAtIndex(selected, ListView.Beginning)
        else {
            var top = item.y
            var bottom = top + item.moveHeight
            if (top < moves.contentY) moves.contentY = top
            else if (bottom > moves.contentY + moves.height)
                moves.contentY = Math.min(top, bottom - moves.height)
        }
        pageTimer.restart()
    }
    Connections {
        target: root.backend
        function onNotationChanged() { root.requestFollow(false) }
    }
    onVisibleChanged: if (visible) requestFollow(true)
    onHeightChanged: requestFollow(true)
    onWidthChanged: requestFollow(true)
    onTextScaleChanged: requestFollow(true)
    Component.onCompleted: requestFollow(true)

    ColumnLayout {
        anchors.fill: parent
        spacing: 4
        Row {
            Layout.fillWidth: true
            Layout.rightMargin: 14
            Label { width: root.numberWidth; text: "N°"; color: root.muted; font.pixelSize: 10 * root.textScale }
            Repeater {
                model: [qsTr("Blancs"), qsTr("Noirs")]
                Label {
                    required property string modelData
                    width: Math.max(0, (parent.width - root.numberWidth) / 2)
                    text: modelData; color: root.muted; font.pixelSize: 10 * root.textScale
                }
            }
        }
        ListView {
            id: moves
            objectName: "moveNotebook"
            Layout.fillWidth: true
            Layout.fillHeight: true
            model: root.backend.moveRows
            clip: true
            spacing: 3
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar {}
            highlightFollowsCurrentItem: false
            delegate: Column {
                id: row
                required property var modelData
                width: Math.max(0, moves.width - 14)
                spacing: 3
                property real indent: Math.min(modelData.depth * 12, width * .16)
                property real moveHeight: modelData.initial ? initialButton.height : cells.height + (branchLabel.visible ? branchLabel.implicitHeight + row.spacing : 0)
                Label {
                    id: branchLabel
                    visible: !row.modelData.initial && row.modelData.depth > 0
                    width: row.width - row.indent
                    x: row.indent
                    text: "↳ " + qsTr("Variante") + " · " + row.modelData.depth
                    color: root.muted; font.pixelSize: 9 * root.textScale
                }
                Button {
                    id: initialButton
                    objectName: "initialPositionMove"
                    visible: !!row.modelData.initial
                    width: row.width
                    height: visible ? Math.max(30 * root.textScale, contentItem.implicitHeight + 12) : 0
                    text: qsTr("Position initiale")
                    contentItem: Label {
                        text: initialButton.text; wrapMode: Text.Wrap
                        font.pixelSize: 11 * root.textScale; color: root.foreground
                        font.bold: !!row.modelData.active
                    }
                    background: Rectangle {
                        color: row.modelData.active || initialButton.hovered ? root.raised : "transparent"
                        border.width: row.modelData.active || initialButton.visualFocus ? 1 : 0
                        border.color: root.accent; radius: 3
                    }
                    onClicked: root.backend.navigate(0)
                }
                Row {
                    id: cells
                    visible: !row.modelData.initial
                    x: row.indent
                    width: row.width - row.indent
                    height: visible ? Math.max(whiteCell.item ? whiteCell.item.implicitHeight : 0,
                                              blackCell.item ? blackCell.item.implicitHeight : 0,
                                              32 * root.textScale) : 0
                    Label {
                        width: root.numberWidth
                        height: cells.height
                        text: (row.modelData.number || "") + "."
                        verticalAlignment: Text.AlignVCenter
                        color: root.muted; font.pixelSize: 11 * root.textScale
                    }
                    Loader { id: whiteCell; width: Math.max(0, (cells.width-root.numberWidth)/2); height: cells.height; property var move: row.modelData.white || ({}); sourceComponent: moveCell }
                    Loader { id: blackCell; width: Math.max(0, (cells.width-root.numberWidth)/2); height: cells.height; property var move: row.modelData.black || ({}); sourceComponent: moveCell }
                }
                Component {
                    id: moveCell
                    Button {
                        id: cell
                        property var move: parent.move
                        objectName: move.id ? "notebookMove_" + move.id : "emptyMoveCell"
                        enabled: !!move.id
                        implicitHeight: Math.max(32 * root.textScale, contentItem.implicitHeight + 12)
                        padding: 6
                        Accessible.name: (row.modelData.number || "") + (parent === whiteCell ? ". " : "… ") + (move.text || "")
                        Accessible.description: move.comment || ""
                        contentItem: Label {
                            text: cell.move.text || ""
                            textFormat: Text.PlainText
                            wrapMode: Text.WrapAnywhere
                            verticalAlignment: Text.AlignVCenter
                            font.pixelSize: 12 * root.textScale
                            font.bold: !!cell.move.active
                            color: root.foreground
                        }
                        background: Rectangle {
                            radius: 3
                            color: cell.move.active || cell.hovered ? root.raised : "transparent"
                            border.width: cell.move.active || cell.visualFocus ? 2 : 0
                            border.color: root.accent
                        }
                        onClicked: root.backend.navigate(move.id)
                    }
                }
                Repeater {
                    model: row.modelData.initial ? [{text: "", comment: row.modelData.comment || ""}] :
                                                  [row.modelData.white || ({}), row.modelData.black || ({})]
                    Label {
                        required property var modelData
                        visible: !!modelData.comment
                        width: row.width - row.indent - 8
                        x: row.indent + 4
                        text: (modelData.text ? modelData.text + " — " : "") + (modelData.comment || "")
                        textFormat: Text.PlainText
                        wrapMode: Text.Wrap
                        color: root.muted; font.pixelSize: 10 * root.textScale
                    }
                }
                Rectangle { width: row.width; height: 1; color: root.lineColor; opacity: .5 }
            }
        }
    }
}
