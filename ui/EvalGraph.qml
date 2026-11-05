// SPDX-License-Identifier: AGPL-3.0-or-later
import QtQuick
import QtQuick.Controls

// Courbe du bilan, du point de vue des Blancs : au-dessus de l'axe ils mènent. Un clic
// ramène à la position correspondante. Le trait vertical dit où l'on se trouve — c'est
// ce qui manque à un graphe posé loin de l'échiquier.
Item {
    id: root
    required property var backend
    readonly property var rows: backend.review || []
    readonly property bool ready: rows.length > 0
    readonly property int current: {
        for (var i = 0; i < rows.length; i++)
            if (rows[i].node === backend.state.node)
                return i
        return -1
    }
    implicitHeight: 92

    function abscissa(index, span) { return rows.length === 1 ? span / 2 : index * span / (rows.length - 1) }
    function ordinate(score, span) { return span / 2 - Math.max(-5, Math.min(5, score)) * span / 10 }
    function rowAt(x, span) {
        return Math.round(Math.max(0, Math.min(1, span > 0 ? x / span : 0)) * (rows.length - 1))
    }

    Rectangle {
        anchors.fill: parent
        radius: 8
        color: root.backend.preferences.panel

        Canvas {
            id: plot
            anchors.fill: parent
            anchors.margins: 8
            onWidthChanged: requestPaint()
            onHeightChanged: requestPaint()
            onPaint: {
                var ctx = getContext("2d")
                ctx.clearRect(0, 0, width, height)
                ctx.strokeStyle = root.backend.preferences.line
                ctx.lineWidth = 1
                ctx.beginPath(); ctx.moveTo(0, height / 2); ctx.lineTo(width, height / 2); ctx.stroke()
                var series = root.rows
                if (!series.length)
                    return
                var accent = root.backend.preferences.accentColor
                // Aplat sous la courbe : à cette hauteur, un trait seul se lit mal.
                ctx.beginPath()
                ctx.moveTo(0, height / 2)
                for (var i = 0; i < series.length; i++)
                    ctx.lineTo(root.abscissa(i, width), root.ordinate(series[i].score, height))
                ctx.lineTo(root.abscissa(series.length - 1, width), height / 2)
                ctx.closePath()
                ctx.globalAlpha = .18; ctx.fillStyle = accent; ctx.fill(); ctx.globalAlpha = 1
                ctx.strokeStyle = accent; ctx.lineWidth = 2; ctx.lineJoin = "round"
                ctx.beginPath()
                for (var j = 0; j < series.length; j++) {
                    var x = root.abscissa(j, width), y = root.ordinate(series[j].score, height)
                    if (j === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y)
                }
                ctx.stroke()
                for (var k = 0; k < series.length; k++)
                    if (series[k].loss >= 100) {
                        ctx.fillStyle = series[k].loss >= 250 ? "#dc6659" : "#d7a355"
                        ctx.beginPath()
                        ctx.arc(root.abscissa(k, width), root.ordinate(series[k].score, height), 3.5, 0, Math.PI * 2)
                        ctx.fill()
                    }
                if (root.current >= 0) {
                    var cx = root.abscissa(root.current, width)
                    ctx.strokeStyle = root.backend.preferences.foreground
                    ctx.globalAlpha = .55; ctx.lineWidth = 1
                    ctx.beginPath(); ctx.moveTo(cx, 0); ctx.lineTo(cx, height); ctx.stroke()
                    ctx.globalAlpha = 1
                }
            }
        }

        Label {
            anchors.centerIn: parent
            visible: !root.ready
            text: qsTr("Analysez la partie pour voir la courbe.")
            color: root.backend.preferences.muted
            font.pixelSize: Math.round(11 * root.backend.preferences.fontScale * root.backend.preferences.uiScale)
        }

        MouseArea {
            anchors.fill: plot
            hoverEnabled: true
            enabled: root.ready
            cursorShape: root.ready ? Qt.PointingHandCursor : Qt.ArrowCursor
            property int under: -1
            onPositionChanged: function(mouse) { under = root.rowAt(mouse.x, width) }
            onExited: under = -1
            onClicked: function(mouse) { root.backend.navigate(root.rows[root.rowAt(mouse.x, width)].node) }
            ToolTip.visible: containsMouse && under >= 0 && under < root.rows.length
            ToolTip.text: under >= 0 && under < root.rows.length
                          ? root.rows[under].move + " · " + root.rows[under].eval + " · " + root.rows[under].label
                          : ""
        }
    }

    Connections {
        target: root.backend
        function onReviewChanged() { plot.requestPaint() }
        function onChanged() { plot.requestPaint() }
    }
}
