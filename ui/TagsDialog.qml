// SPDX-License-Identifier: AGPL-3.0-or-later
// Les étiquettes d'une base, d'un recueil ou d'un répertoire : un champ, séparées par des
// virgules, et celles déjà employées ailleurs à un clic.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

TDialog {
    id: root
    required property var backend
    property string kind: "base"
    property double itemId: 0
    property string itemName: ""
    property var known: []
    objectName: "tagsDialog"
    title: qsTr("Étiquettes")
    modal: true
    anchors.centerIn: parent
    width: Math.min(440, parent ? parent.width - 32 : 440)
    standardButtons: Dialog.Ok | Dialog.Cancel
    function ask(kind, id, name, tags) {
        root.kind = kind; root.itemId = id; root.itemName = name
        field.text = (tags || []).join(", ")
        known = backend.allTags()
        open()
        field.forceActiveFocus()
    }
    function current() { return field.text.split(",").map(t => t.trim()).filter(t => t.length > 0) }
    onAccepted: backend.setTags(kind, itemId, field.text)
    ColumnLayout {
        anchors.fill: parent; spacing: 8
        Label { Layout.fillWidth: true; elide: Text.ElideRight; text: root.itemName; font.weight: Font.DemiBold }
        TTextField {
            id: field; objectName: "tagsField"; Layout.fillWidth: true
            placeholderText: qsTr("Par exemple : ouvertures, Sicilienne, à revoir")
            onAccepted: root.accept()
        }
        Label {
            Layout.fillWidth: true; wrapMode: Text.WordWrap; color: palette.placeholderText
            text: qsTr("Séparez les étiquettes par des virgules ; douze au plus.")
        }
        Flow {
            Layout.fillWidth: true; spacing: 6; visible: root.known.length > 0
            Repeater {
                model: root.known
                TChip {
                    required property string modelData
                    text: modelData
                    checked: root.current().some(t => t.toLowerCase() === modelData.toLowerCase())
                    onToggled: {
                        const list = root.current().filter(t => t.toLowerCase() !== modelData.toLowerCase())
                        if (checked) list.push(modelData)
                        field.text = list.join(", ")
                    }
                }
            }
        }
    }
}
