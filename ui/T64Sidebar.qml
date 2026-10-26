// SPDX-License-Identifier: AGPL-3.0-or-later
// Adapted from the approved interface design; routes retain the existing page indices.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    required property int currentPage
    required property var preferences
    property bool collapsed: false
    property real expandedWidth: 224 * preferences.uiScale
    property real compactWidth: 72 * preferences.uiScale
    implicitWidth: collapsed ? compactWidth : expandedWidth
    implicitHeight: 640
    signal pageRequested(int index)
    signal actionRequested(string action)

    // Les index de pages sont ceux de Main.qml en octobre 2026.
    // Les sections sans page autonome ouvrent un dialogue existant.
    readonly property var sections: [
        { text: qsTr("Accueil"), icon: "home", page: 0 },
        { text: qsTr("Analyse"), icon: "analysis", page: 2 },
        { text: qsTr("Bibliothèque"), icon: "library", page: 1 },
        { text: qsTr("Ouvertures"), icon: "openings", page: 3 },
        { text: qsTr("Entraînement"), icon: "training", page: 4 },
        { text: qsTr("Préparation"), icon: "preparation", action: "preparationDialog" },
        { text: qsTr("Moteurs"), icon: "engines", action: "engineSettings" },
        { text: qsTr("Joueurs"), icon: "players", page: 6 },
        { text: qsTr("Outils"), icon: "tools", page: 5 },
        { text: qsTr("Paramètres"), icon: "settings", action: "settingsDialog" }
    ]

    Rectangle { anchors.fill: parent; color: root.preferences.background
        Rectangle { anchors.right: parent.right; width: 1; height: parent.height; color: root.preferences.line }
    }
    ColumnLayout {
        anchors.fill: parent
        anchors.topMargin: 20
        anchors.bottomMargin: 14
        spacing: 17
        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: root.collapsed ? 13 : 18
            Layout.rightMargin: 13
            spacing: 8
            Label {
                text: "♗"
                color: root.preferences.accentColor
                font.family: "DejaVu Serif"
                font.pixelSize: 30
                Accessible.name: qsTr("Trait64")
            }
            Label {
                visible: !root.collapsed
                text: "Trait64"
                color: root.preferences.foreground
                font.pixelSize: 21
                font.weight: Font.DemiBold
            }
        }
        ScrollView { id:navScroll; Layout.fillWidth:true;Layout.fillHeight:true;contentWidth:availableWidth;clip:true;ScrollBar.horizontal.policy:ScrollBar.AlwaysOff
        ColumnLayout {
            id: navigation
            width:navScroll.availableWidth
            Layout.fillWidth: true
            spacing: 4
            Repeater {
                model: root.sections
                delegate: Button {
                    id: navButton
                    required property var modelData
                    property bool active: modelData.page !== undefined && root.currentPage === modelData.page
                    Layout.fillWidth: true
                    Layout.leftMargin: 8
                    Layout.rightMargin: 8
                    objectName: modelData.page !== undefined ? "sectionNav" + modelData.page : "nav" + modelData.action
                    implicitHeight: Math.max(44 * root.preferences.uiScale, 28 * root.preferences.fontScale * root.preferences.uiScale + 12)
                    hoverEnabled: true
                    Accessible.name: modelData.text
                    Accessible.role: modelData.page !== undefined ? Accessible.PageTab : Accessible.Button
                    Accessible.selected: active
                    onActiveFocusChanged: if (activeFocus) navScroll.contentItem.contentY = Math.max(0,Math.min(y,navScroll.contentItem.contentHeight-navScroll.availableHeight))
                    ToolTip.visible: (hovered || visualFocus) && root.collapsed
                    ToolTip.text: modelData.text
                    contentItem: RowLayout {
                        spacing: 12
                        anchors.leftMargin: 9
                        anchors.rightMargin: 7
                        T64Icon {
                            color: navButton.active ? root.preferences.accentColor : root.preferences.muted
                            Layout.preferredWidth: 20
                            Layout.preferredHeight: 20
                            source: "qrc:/assets/icons/" + navButton.modelData.icon +
                                    ".svg"
                            sourceSize.width: 40
                            sourceSize.height: 40
                            fillMode: Image.PreserveAspectFit
                        }
                        Label {
                            visible: !root.collapsed
                            Layout.fillWidth: true
                            text: navButton.modelData.text
                            color: navButton.active ? root.preferences.foreground : root.preferences.muted
                            font.pixelSize: 13
                            font.weight: navButton.active ? Font.DemiBold : Font.Normal
                            elide: Text.ElideRight
                        }
                    }
                    background: Rectangle {
                        radius: 7
                        color: navButton.active ? root.preferences.raised :
                               (navButton.hovered ? root.preferences.panel : "transparent")
                        border.width: navButton.visualFocus ? 2 : 1
                        border.color: navButton.visualFocus ? root.preferences.accentColor : navButton.active ? root.preferences.line : "transparent"
                        Rectangle {
                            visible: navButton.active
                            anchors.left: parent.left
                            anchors.verticalCenter: parent.verticalCenter
                            height: parent.height - 14
                            width: 3
                            radius: 2
                            color: root.preferences.accentColor
                        }
                    }
                    onClicked: {
                        if (modelData.page !== undefined)
                            root.pageRequested(modelData.page)
                        else
                            root.actionRequested(modelData.action)
                    }
                }
            }
        }
        }

    }
}
