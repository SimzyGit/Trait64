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
    property real expandedWidth: 204 * preferences.uiScale
    property real compactWidth: 44 * preferences.uiScale
    implicitWidth: collapsed ? compactWidth : expandedWidth
    implicitHeight: 640
    signal pageRequested(int index)
    signal actionRequested(string action)

    // Les index de pages sont ceux de Main.qml en octobre 2026.
    // Les sections sans page autonome ouvrent un dialogue existant.
    readonly property var sections: [
        // « hint » dit ce qu'on trouve derrière chaque entrée : le seul nom ne suffit pas
        // à qui ouvre le logiciel pour la première fois.
        { text: qsTr("Accueil"), icon: "home", page: 9,
          hint: qsTr("Reprendre où vous en étiez, ouvrir une partie d’exemple.") },
        { text: qsTr("Bases"), icon: "base", page: 0,
          hint: qsTr("Vos bases en icônes, rangées par catégorie.") },
        { text: qsTr("Analyse"), icon: "analysis", page: 2,
          hint: qsTr("Échiquier, notation, moteur et bilan de la partie en cours.") },
        { text: qsTr("Bibliothèque"), icon: "library", page: 1,
          hint: qsTr("Vos bases : importer du PGN, chercher, trier, sauvegarder.") },
        { text: qsTr("Ouvertures"), icon: "openings", page: 3,
          hint: qsTr("Arbre statistique de votre base, livres et répertoires.") },
        { text: qsTr("Entraînement"), icon: "training", page: 4,
          hint: qsTr("Puzzles, cartes à répétition espacée, révision de répertoire.") },
        { text: qsTr("Préparation"), icon: "preparation", action: "preparationDialog",
          hint: qsTr("Constituer un dossier contre un adversaire donné.") },
        { text: qsTr("Moteurs"), icon: "engines", action: "engineSettings",
          hint: qsTr("Choisir le moteur, sa puissance et ses variantes.") },
        { text: qsTr("Joueurs"), icon: "players", page: 6,
          hint: qsTr("Fiches des joueurs de votre base et de vos comptes.") },
        { text: qsTr("Profils"), icon: "profiles", page: 7,
          hint: qsTr("Vos comptes sous un seul nom, le bilan de vos parties et vos progrès.") },
        { text: qsTr("Études"), icon: "studies", page: 8,
          hint: qsTr("Vos répertoires : les créer, les réviser, importer ceux d'un autre logiciel.") },
        { text: qsTr("Outils"), icon: "tools", page: 5,
          hint: qsTr("Comparer des moteurs, organiser un tournoi, jouer une partie.") },
        { text: qsTr("Paramètres"), icon: "settings", action: "settingsDialog",
          hint: qsTr("Apparence, langue, moteur, sauvegardes et mises à jour.") }
    ]

    Rectangle { anchors.fill: parent; color: root.preferences.background
        Rectangle { anchors.right: parent.right; width: 1; height: parent.height; color: root.preferences.line }
    }
    ColumnLayout {
        anchors.fill: parent
        anchors.topMargin: root.collapsed ? 10 : 20
        anchors.bottomMargin: root.collapsed ? 6 : 14
        spacing: root.collapsed ? 8 : 14
        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: root.collapsed ? 10 : 18
            Layout.rightMargin: 13
            spacing: 8
            Label {
                text: "♗"
                color: root.preferences.accentColor
                font.family: "DejaVu Serif"
                font.pixelSize:Math.round(30*controller.preferences.fontScale*controller.preferences.uiScale)
                Accessible.name: qsTr("Trait64")
            }
            Label {
                visible: !root.collapsed
                text: "Trait64"
                color: root.preferences.foreground
                font.pixelSize:Math.round(21*controller.preferences.fontScale*controller.preferences.uiScale)
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
                // Un nombre, pas les libellés traduits : changer de langue ne doit pas
                // recréer les boutons (ni perdre le focus ou une infobulle ouverte).
                model: root.sections.length
                delegate: Button {
                    id: navButton
                    required property int index
                    readonly property var modelData: root.sections[index]
                    property bool active: modelData.page !== undefined && root.currentPage === modelData.page
                    Layout.fillWidth: true
                    Layout.leftMargin: root.collapsed ? 4 : 8
                    Layout.rightMargin: root.collapsed ? 4 : 8
                    // Jamais plus large que la barre : replié, le texte caché gardait sa place.
                    Layout.minimumWidth: 0
                    Layout.maximumWidth: navigation.width - Layout.leftMargin - Layout.rightMargin
                    objectName: modelData.page !== undefined ? "sectionNav" + modelData.page : "nav" + modelData.action
                    // 36 px : douze entrées de 44 px occupaient toute la hauteur d'un portable.
                    implicitHeight: root.collapsed ? 34 * root.preferences.uiScale : Math.max(36 * root.preferences.uiScale, 22 * root.preferences.fontScale * root.preferences.uiScale + 12)
                    hoverEnabled: true
                    Accessible.name: modelData.text
                    Accessible.role: modelData.page !== undefined ? Accessible.PageTab : Accessible.Button
                    Accessible.selected: active
                    onActiveFocusChanged: if (activeFocus) navScroll.contentItem.contentY = Math.max(0,Math.min(y,navScroll.contentItem.contentHeight-navScroll.availableHeight))
                    // Replié, le nom manque ; déplié, c'est ce qu'il y a derrière qui manque.
                    ToolTip.visible: hovered || visualFocus
                    ToolTip.delay: 350
                    ToolTip.text: root.collapsed && modelData.hint ? modelData.text + " — " + modelData.hint
                                                                   : (modelData.hint || modelData.text)
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
                            font.pixelSize:Math.round(13*controller.preferences.fontScale*controller.preferences.uiScale)
                            font.weight: navButton.active ? Font.DemiBold : Font.Normal
                            elide: Text.ElideRight
                        }
                    }
                    // Un fond plein et un trait d'accent pour la page ouverte, une simple
                    // nuance au survol : le cadre ajouté autour de l'entrée active et le liseré
                    // vert du survol doublaient le même signal.
                    background: Rectangle {
                        readonly property color accent: root.preferences.accentColor
                        radius: 4
                        color: navButton.active ? root.preferences.raised
                             : navButton.hovered ? Qt.rgba(accent.r, accent.g, accent.b,
                                                           root.preferences.highContrast ? .3 : .08)
                             : "transparent"
                        border.width: navButton.visualFocus ? 2 : 0
                        border.color: accent
                        Rectangle {
                            visible: navButton.active
                            anchors.left: parent.left
                            anchors.verticalCenter: parent.verticalCenter
                            height: parent.height - 12
                            width: 3
                            radius: 1.5
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
        // Replier en icônes, ou déplier : on garde plus de place pour l'échiquier.
        Button {
            objectName: "sidebarToggle"
            Layout.alignment: root.collapsed ? Qt.AlignHCenter : Qt.AlignRight
            Layout.rightMargin: root.collapsed ? 0 : 10
            flat: true
            Layout.preferredWidth: 32 * root.preferences.uiScale
            text: root.collapsed ? "»" : "«"
            Accessible.name: root.collapsed ? qsTr("Déplier la barre latérale") : qsTr("Replier la barre latérale")
            ToolTip.visible: hovered
            ToolTip.text: Accessible.name
            onClicked: root.actionRequested("collapse")
        }
    }
}
