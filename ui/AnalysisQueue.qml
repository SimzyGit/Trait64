import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
TDialog {
    id: root
    parent: Overlay.overlay
    property var backend
    readonly property var queue: backend.analysisQueue
    readonly property var job: queue.state.job || ({})
    readonly property var selectedId: queue.state.selected || 0
    title: qsTr("Analyses en arrière-plan")
    width: Math.min(900, parent ? parent.width - 24 : 900)
    height: Math.min(760, parent ? parent.height - 24 : 760)
    anchors.centerIn: parent
    modal: true
    standardButtons: Dialog.Close
    function status(value) {
        return ({queued:qsTr("En attente"), running:qsTr("En cours"), paused:qsTr("En pause"), completed:qsTr("Terminée"), failed:qsTr("À reprendre")})[value] || value
    }
    function profiles() {
        var names = ["Moteur courant"]
        var saved = backend.preferences.engineProfiles || []
        for (var i = 0; i < saved.length; ++i) names.push(saved[i].name)
        return names
    }
    ScrollView {id:analysisqueueViewport;anchors.fill:parent;contentWidth:availableWidth;clip:true;ScrollBar.horizontal.policy:ScrollBar.AlwaysOff
ColumnLayout {width:analysisqueueViewport.availableWidth;height:Math.max(analysisqueueViewport.availableHeight,implicitHeight);
        spacing: 8
        Label { text: qsTr("Ajouter une copie de l’étude ouverte à la file"); font.bold: true }
        RowLayout {
            Layout.fillWidth: true
            TComboBox { id: profile; onlyFirstOption:true; model: root.profiles(); Layout.fillWidth: true }
            TComboBox { id: budget; Layout.preferredWidth: 190; model: ["0,1 s / position", "0,5 s / position", "1 s / position", "2 s / position", "5 s / position", "10 s / position"]; currentIndex: 1 }
        }
        RowLayout {
            Layout.fillWidth: true
            CheckBox { id: branches; text: qsTr("Toutes les variantes"); checked: true }
            Item { Layout.fillWidth: true }
            TButton { objectName: "enqueueLibrary"; text: qsTr("Parties affichées de la bibliothèque")
                enabled: !!root.queue.state.ready && !root.backend.state.training
                ToolTip.visible: hovered; ToolTip.text: qsTr("Ajoute jusqu’à 200 parties correspondant aux filtres actuels de la bibliothèque (lignes principales).")
                onClicked: root.backend.enqueueLibrary([100,500,1000,2000,5000,10000][budget.currentIndex], profile.currentIndex - 1, 200)
            }
            TButton { objectName: "enqueueAnalysis"; text: qsTr("Ajouter à la file"); accent: true
                enabled: !!root.queue.state.ready && !root.backend.state.training
                onClicked: root.backend.enqueueAnalysis(branches.checked, [100,500,1000,2000,5000,10000][budget.currentIndex], profile.currentIndex - 1)
            }
        }
        Label { text: root.queue.state.error || qsTr("Calcul local. Vous pouvez fermer cette fenêtre et poursuivre votre travail."); Layout.fillWidth: true; wrapMode: Text.WordWrap }
        ListView {
            id: jobs; Layout.fillWidth: true; Layout.preferredHeight: 90; clip: true
            model: root.queue.jobs; ScrollBar.vertical: ScrollBar {}
            delegate: ItemDelegate {
                required property var modelData
                width: jobs.width; highlighted: modelData.id === root.selectedId
                text: modelData.title + " · " + root.status(modelData.status) + " · " + modelData.done + " / " + modelData.total
                onClicked: root.queue.select(modelData.id)
            }
        }
        ProgressBar { Layout.fillWidth: true; from: 0; to: Math.max(1, root.job.total || 0); value: root.job.done || 0 }
        Flow {Layout.fillWidth:true;spacing:8
TButton { text: qsTr("Pause"); enabled: root.job.status === "running" || root.job.status === "queued"; onClicked: root.queue.pause(root.selectedId) }
            TButton { text: qsTr("Reprendre"); enabled: root.job.status === "paused" || root.job.status === "failed"; onClicked: root.queue.resume(root.selectedId) }
            TButton { text: qsTr("Tout suspendre"); enabled: !!root.queue.state.ready; onClicked: root.queue.pauseAll() }
            Item { Layout.fillWidth: true }
            TButton { text: qsTr("Retirer"); enabled: root.selectedId > 0 && root.job.status !== "running"; onClicked: { removeDialog.jobId = root.selectedId; removeDialog.open() } }
        }
        Label {
            Layout.fillWidth: true; wrapMode: Text.WordWrap; font.pixelSize:Math.round(11*controller.preferences.fontScale*controller.preferences.uiScale)
            text: root.job.error || (root.job.identity && root.job.identity.name
                ? root.job.identity.name + " · MultiPV " + root.job.config.multiPV + " · " + root.job.config.milliseconds + qsTr(" ms / position") +
                  (root.job.identity.cacheable ? qsTr(" · Cache actif") : qsTr(" · Cache désactivé : ressources externes non identifiées"))
                : qsTr("Sélectionnez une analyse pour voir ses résultats."))
        }
        ListView {
            id: results; Layout.fillWidth: true; Layout.fillHeight: true; clip: true; spacing: 3
            model: root.queue.results; ScrollBar.vertical: ScrollBar {}
            delegate: Label {
                required property var modelData
                width: results.width; padding: 5; wrapMode: Text.WordWrap; textFormat: Text.PlainText
                text: {
                    var lines = modelData.lines, text = "Position " + modelData.ordinal + (modelData.cached ? " · cache" : "")
                    for (var i = 0; i < lines.length; ++i) {
                        var row = lines[i]
                        text += "\n" + row.rank + ".  " + row.eval + qsTr(" · profondeur ") + row.depth + " · " + (row.san || qsTr("Position finale"))
                        if (row.wdl) text += qsTr(" · Blancs / Nulle / Noirs ") + (row.wdl[0]/10) + "/" + (row.wdl[1]/10) + "/" + (row.wdl[2]/10) + " %"
                    }
                    return text
                }
            }
        }
        Flow {Layout.fillWidth:true;spacing:8
TButton { text: "←"; enabled: (root.queue.state.offset || 0) > 0; onClicked: root.queue.select(root.selectedId, root.queue.state.offset - 200) }
            Label { text: qsTr("Page ") + (root.queue.state.page || 1) }
            TButton { text: "→"; enabled: !!root.queue.state.hasNext; onClicked: root.queue.select(root.selectedId, root.queue.state.offset + 200) }
            TButton { text: qsTr("Vider le cache"); enabled: !!root.queue.state.ready; onClicked: root.queue.clearCache() }
            Item { Layout.fillWidth: true }
            TButton { text: qsTr("Ouvrir une copie"); enabled: (root.job.done || 0) > 0 && !root.backend.state.training; onClicked: { root.queue.openStudy(root.selectedId); root.close() } }
            TButton { text: qsTr("Exporter PGN"); enabled: (root.job.done || 0) > 0; onClicked: { exportDialog.jobId = root.selectedId; exportDialog.open() } }
        }
        Label { Layout.fillWidth: true; wrapMode: Text.WordWrap; font.pixelSize:Math.round(10*controller.preferences.fontScale*controller.preferences.uiScale); opacity: .75
            text: qsTr("Les résultats sont sauvegardés à chaque position. Après une interruption, utilisez Reprendre. WDL : estimation du moteur, vue des Blancs ; ce n’est pas une prédiction de votre résultat.") }
    }
}
    FileDialog { id: exportDialog; property var jobId: 0; title: qsTr("Exporter l’analyse"); fileMode: FileDialog.SaveFile; nameFilters: ["Partie PGN (*.pgn)"]; defaultSuffix: "pgn"; onAccepted: root.queue.exportPgn(jobId, selectedFile.toString()) }
    TDialog { id: removeDialog; width:Math.min(580,root.width-32); property var jobId: 0; title: qsTr("Retirer cette analyse ?"); modal: true; anchors.centerIn: parent; standardButtons: Dialog.Ok | Dialog.Cancel
        Label { text: qsTr("Les résultats seront retirés de la file. La partie d’origine est conservée."); wrapMode: Text.WordWrap; width: parent.width }
        onAccepted: root.queue.forget(jobId)
    }
}
