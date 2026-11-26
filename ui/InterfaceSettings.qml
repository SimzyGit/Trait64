// SPDX-License-Identifier: AGPL-3.0-or-later
// Le style de l'interface : ChessBase (ruban et onglets), En Croissant (barre latérale et
// menus) ou un profil personnalisé — barres, page de démarrage, disposition des panneaux
// et thème, enregistrés sous un nom.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: root
    required property var win
    readonly property var prefs: controller.preferences
    readonly property string style: prefs.interfaceStyle || "chessbase"
    readonly property var profiles: prefs.interfaceProfiles || []
    spacing: 10
    // Le brouillon du profil : les cases partent de ce qui est affiché maintenant.
    property bool ribbon: !!prefs.showRibbon
    property bool menus: !!prefs.showMenuBar
    property bool sidebar: !!prefs.showSidebar
    property bool tabs: !!prefs.showPageTabs
    property int start: prefs.startPage || 0
    readonly property var pages: [{v: 0, t: qsTr("Bases")}, {v: 1, t: qsTr("Parties")}, {v: 2, t: qsTr("Analyse")}, {v: 9, t: qsTr("Accueil")}]

    Label { text: qsTr("Interface"); font.bold: true }
    Flow {
        Layout.fillWidth: true
        spacing: 8
        Repeater {
            model: [{key: "chessbase", title: qsTr("ChessBase"), text: qsTr("Ruban de commandes, fenêtres en onglets, fenêtre des bases au démarrage.")},
                    {key: "croissant", title: qsTr("En Croissant"), text: qsTr("Barre latérale à gauche, barre de menus, accueil au démarrage.")},
                    {key: "custom", title: qsTr("Personnalisé"), text: qsTr("Vos barres, vos panneaux, votre thème, enregistrés en profils.")}]
            AbstractButton {
                id: card
                required property var modelData
                readonly property bool active: root.style === modelData.key
                objectName: "style_" + modelData.key
                width: Math.min(210 * root.prefs.uiScale, (root.width - 16) / 3)
                height: cardText.implicitHeight + 20
                hoverEnabled: true
                Accessible.role: Accessible.RadioButton
                Accessible.checked: active
                Accessible.name: modelData.title
                background: Rectangle {
                    radius: 4
                    readonly property color accent: root.prefs.accentColor
                    color: card.active ? Qt.rgba(accent.r, accent.g, accent.b, .14) : card.hovered ? root.prefs.raised : root.prefs.panel
                    border.width: card.active || card.visualFocus ? 2 : 1
                    border.color: card.active || card.visualFocus ? accent : root.prefs.line
                }
                contentItem: Column {
                    id: cardText
                    padding: 10
                    spacing: 3
                    Label { text: card.modelData.title; font.weight: Font.DemiBold; color: root.prefs.foreground }
                    Label { width: card.width - 20; text: card.modelData.text; wrapMode: Text.WordWrap; color: root.prefs.muted
                            font.pixelSize: Math.round(11 * root.prefs.fontScale * root.prefs.uiScale) }
                }
                onClicked: {
                    if (modelData.key === "custom") {
                        if (root.profiles.length) root.win.applyInterfaceProfile(root.profiles[0])
                        else controller.setPreference("interfaceStyle", "custom")
                    } else root.win.applyStyle(modelData.key)
                }
            }
        }
    }
    // La notation : en liste (un coup blanc et un coup noir par ligne) ou en ligne, à la
    // suite comme dans un livre. Le même choix que les boutons du panneau de notation.
    RowLayout {
        spacing: 8
        Label { text: qsTr("Notation des coups") }
        Repeater {
            model: [{key: "table", text: qsTr("En liste")}, {key: "flow", text: qsTr("En ligne")}]
            TButton {
                required property var modelData
                objectName: "notation_" + modelData.key
                text: modelData.text
                accent: (root.prefs.notebookLayout || "table") === modelData.key
                onClicked: controller.setPreference("notebookLayout", modelData.key)
            }
        }
    }
    // Le profil personnalisé : choisi, composé, enregistré.
    ColumnLayout {
        visible: root.style === "custom"
        Layout.fillWidth: true
        spacing: 8
        RowLayout {
            Layout.fillWidth: true
            Label { text: qsTr("Profil") }
            TComboBox {
                id: profileBox
                objectName: "interfaceProfileBox"
                Layout.fillWidth: true
                model: root.profiles.map(function(p) { return p.name })
                currentIndex: Math.max(0, model.indexOf(root.prefs.interfaceProfile))
                displayText: count ? currentText : qsTr("Aucun profil enregistré")
                onActivated: root.win.applyInterfaceProfile(root.profiles[currentIndex])
            }
            TButton { text: qsTr("Supprimer"); enabled: profileBox.count > 0; onClicked: controller.removeInterfaceProfile(profileBox.currentText) }
        }
        Flow {
            Layout.fillWidth: true
            spacing: 12
            TCheckBox { objectName: "customRibbon"; text: qsTr("Ruban"); checked: root.ribbon; onToggled: root.ribbon = checked }
            TCheckBox { text: qsTr("Barre de menus"); checked: root.menus; onToggled: root.menus = checked }
            TCheckBox { text: qsTr("Barre latérale"); checked: root.sidebar; onToggled: root.sidebar = checked }
            TCheckBox { text: qsTr("Onglets des fenêtres"); checked: root.tabs; onToggled: root.tabs = checked }
        }
        RowLayout {
            Label { text: qsTr("Au démarrage") }
            TComboBox {
                textRole: "t"; valueRole: "v"
                model: root.pages
                currentIndex: Math.max(0, root.pages.findIndex(function(p) { return p.v === root.start }))
                onActivated: root.start = currentValue
            }
        }
        RowLayout {
            Layout.fillWidth: true
            TTextField { id: profileName; objectName: "interfaceProfileName"; Layout.fillWidth: true
                         placeholderText: qsTr("Nom du profil"); text: root.prefs.interfaceProfile || "" }
            TButton {
                objectName: "saveInterfaceProfile"
                text: qsTr("Enregistrer le profil")
                accent: true
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Garde aussi la disposition actuelle des panneaux et le thème.")
                onClicked: controller.saveInterfaceProfile({
                    name: profileName.text, showRibbon: root.ribbon, showMenuBar: root.menus,
                    showSidebar: root.sidebar, showPageTabs: root.tabs, startPage: root.start,
                    dock: JSON.stringify(root.win.dockLayout), lightTheme: !!root.prefs.lightTheme,
                    accentColor: root.prefs.accentColor, backgroundColor: root.prefs.backgroundColor || "",
                    boardLight: root.prefs.boardLight, boardDark: root.prefs.boardDark})
            }
        }
    }
}
