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
    // Les moments clés : les trois coups qui ont le plus coûté (au moins une erreur, 100
    // centipions), dans l'ordre de la partie. Ce sont les tournants qu'un bilan doit
    // montrer d'abord ; la courbe les numérote et le bilan en fait une liste.
    readonly property var keyMoments: {
        var picked = []
        for (var i = 0; i < rows.length; i++)
            if (rows[i].loss >= 100)
                picked.push(i)
        picked.sort(function(a, b) { return rows[b].loss - rows[a].loss })
        return picked.slice(0, 3).sort(function(a, b) { return a - b })
    }

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
                // Le camp qui mène se lit à la couleur, comme sur un échiquier : sous la
                // courbe la part des Blancs, en clair ; au-dessus celle des Noirs, en sombre.
                // Un aplat de la couleur d'accent ne disait pas qui menait.
                var light = root.backend.preferences.lightTheme
                ctx.fillStyle = light ? "#3b3a37" : "#0f0f10"
                ctx.globalAlpha = light ? .82 : .9
                ctx.fillRect(0, 0, width, height)
                ctx.globalAlpha = 1
                ctx.beginPath()
                ctx.moveTo(0, height)
                for (var i = 0; i < series.length; i++)
                    ctx.lineTo(root.abscissa(i, width), root.ordinate(series[i].score, height))
                ctx.lineTo(root.abscissa(series.length - 1, width), height)
                ctx.closePath()
                ctx.fillStyle = light ? "#fbfaf7" : "#d9d5cc"; ctx.fill()
                ctx.strokeStyle = root.backend.preferences.line; ctx.lineWidth = 1
                ctx.beginPath(); ctx.moveTo(0, height / 2); ctx.lineTo(width, height / 2); ctx.stroke()
                ctx.strokeStyle = root.backend.preferences.accentColor; ctx.lineWidth = 1.5; ctx.lineJoin = "round"
                ctx.beginPath()
                for (var j = 0; j < series.length; j++) {
                    var x = root.abscissa(j, width), y = root.ordinate(series[j].score, height)
                    if (j === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y)
                }
                ctx.stroke()
                for (var k = 0; k < series.length; k++)
                    if (series[k].loss >= 100) {
                        ctx.fillStyle = series[k].loss >= 250 ? "#d0574a" : "#d39a3f"
                        ctx.beginPath()
                        ctx.arc(root.abscissa(k, width), root.ordinate(series[k].score, height), 3.5, 0, Math.PI * 2)
                        ctx.fill()
                    }
                // Les moments clés, numérotés au pied de leur trait.
                ctx.font = "bold 10px sans-serif"; ctx.textAlign = "center"
                for (var m = 0; m < root.keyMoments.length; m++) {
                    var at = root.keyMoments[m], mx = root.abscissa(at, width)
                    ctx.strokeStyle = series[at].loss >= 250 ? "#d0574a" : "#d39a3f"
                    ctx.globalAlpha = .7; ctx.lineWidth = 1
                    ctx.beginPath(); ctx.moveTo(mx, 0); ctx.lineTo(mx, height); ctx.stroke(); ctx.globalAlpha = 1
                    ctx.fillStyle = ctx.strokeStyle
                    ctx.beginPath(); ctx.arc(mx, 8, 7, 0, Math.PI * 2); ctx.fill()
                    ctx.fillStyle = "#ffffff"; ctx.fillText(String(m + 1), mx, 11.5)
                }
                if (root.current >= 0) {
                    var cx = root.abscissa(root.current, width)
                    ctx.strokeStyle = root.backend.preferences.accentColor
                    ctx.globalAlpha = .9; ctx.lineWidth = 1.5
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
