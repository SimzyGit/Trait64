// SPDX-License-Identifier: AGPL-3.0-or-later
// Le livre de Lichess dans l'analyse : ce qui se joue dans la position affichée, lu en
// direct dans les parties classées du site ou dans celles des maîtres. Il suit
// l'échiquier coup après coup, et un clic sur une ligne joue le coup.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: root
    required property var backend
    // Le panneau n'interroge Lichess que lorsqu'on le regarde.
    property bool active: false
    readonly property var p: backend.preferences
    readonly property var report: backend.onlineReport || ({})
    readonly property real total: Number(report.total || 0)
    readonly property string source: ["lichess", "masters", "tablebase"][base.currentIndex]
    readonly property bool tables: !!report.tablebase
    // Le verdict des tables, dit du camp au trait.
    function verdict(category) {
        return category === "win" ? qsTr("gagne") : category === "loss" ? qsTr("perd")
             : category === "draw" ? qsTr("nulle")
             : category === "cursed-win" ? qsTr("gagne, mais nulle aux 50 coups")
             : category === "blessed-loss" ? qsTr("perd, mais nulle aux 50 coups")
             : category === "maybe-win" ? qsTr("gagne peut-être")
             : category === "maybe-loss" ? qsTr("perd peut-être") : qsTr("inconnu")
    }
    function tone(category) {
        return category.indexOf("win") >= 0 ? "#3fa66b" : category.indexOf("loss") >= 0 ? "#cf3d3d" : p.muted
    }
    spacing: 4

    function s(base) { return Math.round(base * p.fontScale * p.uiScale) }
    function ask() {
        if (!root.active)
            return
        root.backend.buildOnlineReport(root.source, {
            speeds: speed.currentIndex === 0 ? [] : speed.model[speed.currentIndex].values,
            ratings: floor.currentIndex === 0 ? [] : floor.model[floor.currentIndex].values
        })
    }
    onActiveChanged: ask()
    // On parcourt une partie vite : attendre un souffle évite une requête par coup
    // quand on ne fait que passer.
    Timer { id: settle; interval: 300; onTriggered: root.ask() }
    Connections {
        target: root.backend
        function onChanged() {
            if (root.active && root.report.fen !== root.backend.state.fen)
                settle.restart()
        }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 6
        TComboBox {
            id: base
            objectName: "onlineBase"
            Layout.preferredWidth: root.s(150)
            model: [qsTr("Lichess"), qsTr("Maîtres"), qsTr("Finales (7 pièces)")]
            onActivated: root.ask()
        }
        TComboBox {
            id: speed
            objectName: "onlineSpeeds"
            visible: base.currentIndex === 0
            Layout.preferredWidth: root.s(150)
            textRole: "label"
            translateOptions: false
            onActivated: root.ask()
            model: [{label: qsTr("Toutes cadences"), values: []},
                    {label: qsTr("Blitz et rapide"), values: ["blitz", "rapid"]},
                    {label: qsTr("Classique et correspondance"), values: ["classical", "correspondence"]},
                    {label: qsTr("Bullet"), values: ["ultraBullet", "bullet"]}]
        }
        TComboBox {
            id: floor
            objectName: "onlineRatings"
            visible: base.currentIndex === 0
            Layout.preferredWidth: root.s(120)
            textRole: "label"
            translateOptions: false
            onActivated: root.ask()
            model: [{label: qsTr("Tous niveaux"), values: []},
                    {label: qsTr("1600 et plus"), values: ["1600", "1800", "2000", "2200", "2500"]},
                    {label: qsTr("2000 et plus"), values: ["2000", "2200", "2500"]},
                    {label: qsTr("2200 et plus"), values: ["2200", "2500"]}]
        }
        Item { Layout.fillWidth: true }
    }

    Label {
        Layout.fillWidth: true
        visible: root.total > 0
        elide: Text.ElideRight
        color: root.p.muted
        font.pixelSize: root.s(11)
        text: root.tables
              ? qsTr("Le camp au trait %1").arg(root.verdict(root.report.verdict || ""))
                + (root.report.dtm !== undefined && root.report.dtm !== null ? qsTr(" · mat en %1 demi-coups").arg(root.report.dtm) : "")
              : (root.report.name ? root.report.name + " · " : "") + qsTr("%1 partie(s)").arg(root.total)
    }

    ListView {
        id: list
        objectName: "onlineMoves"
        Layout.fillWidth: true
        Layout.fillHeight: true
        clip: true
        model: root.report.moves || []
        ScrollBar.vertical: ScrollBar {}
        header: RowLayout {
            width: list.width; spacing: 8
            visible: root.total > 0 && !root.tables
            Label {text: qsTr("Coup"); Layout.preferredWidth: root.s(52); color: root.p.muted; font.pixelSize: root.s(11)}
            Label {text: qsTr("Parties"); Layout.preferredWidth: root.s(70); horizontalAlignment: Text.AlignRight
                   color: root.p.muted; font.pixelSize: root.s(11)}
            Label {text: qsTr("Blancs · nulles · Noirs"); Layout.fillWidth: true; color: root.p.muted
                   font.pixelSize: root.s(11)}
            Label {text: qsTr("Elo"); Layout.preferredWidth: root.s(40); horizontalAlignment: Text.AlignRight
                   color: root.p.muted; font.pixelSize: root.s(11)}
        }
        delegate: ItemDelegate {
            id: row
            required property var modelData
            readonly property real played: Math.max(1, Number(modelData.count || 0))
            width: list.width; padding: 3
            onClicked: root.backend.move(modelData.uci)
            Accessible.name: root.tables ? modelData.san + ", " + root.verdict(modelData.verdict || "")
                                         : modelData.san + ", " + modelData.count + qsTr(" partie(s)")
            contentItem: RowLayout {
                spacing: 8
                Label {text: row.modelData.san; Layout.preferredWidth: root.s(52); font.weight: Font.DemiBold}
                // Les tables de finales : un verdict et une distance, pas des parties.
                Label {visible: root.tables; Layout.fillWidth: true; elide: Text.ElideRight
                       text: root.tables ? root.verdict(row.modelData.verdict || "") : ""
                       color: root.tables ? root.tone(row.modelData.verdict || "") : root.p.muted}
                Label {visible: root.tables; color: root.p.muted; font.pixelSize: root.s(11)
                       text: !root.tables ? ""
                             : row.modelData.dtm !== undefined && row.modelData.dtm !== null
                               ? qsTr("mat en %1").arg(row.modelData.dtm)
                             : row.modelData.dtz !== undefined && row.modelData.dtz !== null
                               ? qsTr("DTZ %1").arg(row.modelData.dtz) : ""}
                Label {visible: !root.tables; text: row.modelData.count || ""; Layout.preferredWidth: root.s(70); horizontalAlignment: Text.AlignRight}
                Row {
                    visible: !root.tables
                    Layout.fillWidth: true; height: 10; Layout.alignment: Qt.AlignVCenter
                    Rectangle {width: parent.width * row.modelData.white / row.played; height: 10
                               color: "#f4f4f1"; border.color: root.p.line}
                    Rectangle {width: parent.width * row.modelData.draw / row.played; height: 10; color: "#a3a8ad"}
                    Rectangle {width: parent.width * row.modelData.black / row.played; height: 10; color: "#2c3035"}
                }
                Label {visible: !root.tables; text: Number(row.modelData.rating || 0) > 0 ? row.modelData.rating : "—"
                       Layout.preferredWidth: root.s(40); horizontalAlignment: Text.AlignRight; color: root.p.muted}
            }
        }
        // Les parties en vue sous les coups : un clic en rapatrie une dans la bibliothèque.
        footer: Column {
            width: list.width
            topPadding: 6
            visible: (root.report.games || []).length > 0
            Label {text: qsTr("Parties en vue"); color: root.p.muted; font.pixelSize: root.s(11)}
            Repeater {
                model: root.report.games || []
                ItemDelegate {
                    required property var modelData
                    width: list.width; padding: 3
                    onClicked: root.backend.importOnlineGame(String(modelData.id))
                    ToolTip.visible: hovered; ToolTip.text: qsTr("Rapatrier cette partie dans la bibliothèque")
                    contentItem: Label {
                        elide: Text.ElideRight
                        font.pixelSize: root.s(11)
                        text: (modelData.white || "?") + " (" + modelData.whiteElo + ") — "
                              + (modelData.black || "?") + " (" + modelData.blackElo + ")  "
                              + (modelData.result || "*") + "  " + (modelData.date || "")
                    }
                }
            }
        }
        Label {
            anchors.centerIn: parent
            width: parent.width - 24
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            visible: root.total === 0
            color: root.report.error ? "#cf3d3d" : root.p.muted
            font.pixelSize: root.s(11)
            text: root.report.running ? qsTr("Interrogation de Lichess…")
                  : root.report.error ? root.backend.uiText(root.report.error)
                  : root.source === "tablebase" ? qsTr("Position terminée : rien à jouer.")
                  : qsTr("Aucune partie de cette base n’est passée par cette position.")
        }
    }
}
