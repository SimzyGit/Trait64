// SPDX-License-Identifier: AGPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: root
    required property var backend
    Layout.fillWidth: true
    spacing: 8
    Label { text: "Options déclarées par le moteur"; font.bold: true }
    Label {
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        text: root.backend.engineOptions.length === 0
              ? "Démarrez le moteur pour lire ses options."
              : "Appliquez les changements avec « Démarrer / appliquer », puis mémorisez le profil. La limitation de force est réservée au jeu."
        font.pixelSize: 11
    }
    Repeater {
        model: root.backend.engineOptions
        delegate: RowLayout {
            id: row
            required property var modelData
            property string kind: modelData.type
            property var saved: root.backend.preferences.uciOptions || ({})
            property string current: saved[modelData.name] === undefined ? String(modelData.default || "") : String(saved[modelData.name])
            visible: ["Threads", "Hash", "MultiPV", "Skill Level", "SyzygyPath", "UCI_Chess960"].indexOf(modelData.name) < 0
            Layout.fillWidth: true
            Label { text: row.modelData.name; Layout.preferredWidth: 170; wrapMode: Text.WrapAnywhere }
            CheckBox {
                visible: row.kind === "check"
                checked: row.current === "true" || row.current === "1"
                onToggled: root.backend.setEngineOption(row.modelData.name, checked)
            }
            TextField {
                Layout.fillWidth: true
                visible: row.kind === "spin" || row.kind === "string"
                text: row.current
                selectByMouse: true
                placeholderText: row.kind === "spin" ? row.modelData.min + " … " + row.modelData.max : "Valeur vide"
                onEditingFinished: root.backend.setEngineOption(row.modelData.name, text)
            }
            ComboBox {
                Layout.fillWidth: true
                visible: row.kind === "combo"
                model: row.modelData.choices
                currentIndex: model.indexOf(row.current)
                onActivated: root.backend.setEngineOption(row.modelData.name, currentText)
            }
            TButton {
                visible: row.kind === "button"
                text: "Exécuter"
                onClicked: root.backend.pressEngineOption(row.modelData.name)
            }
        }
    }
}
