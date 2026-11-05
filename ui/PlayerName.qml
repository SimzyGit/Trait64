// SPDX-License-Identifier: AGPL-3.0-or-later
// Un joueur tel que Lichess l'affiche : le titre détaché du nom, dans sa propre teinte,
// et le drapeau de sa fédération quand on la connaît. Le code à trois lettres prend le
// relais pour les fédérations dont nous n'avons pas l'image : mieux vaut un code exact
// qu'un drapeau approximatif.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

RowLayout {
    id: root
    required property var preferences
    property string playerName: ""
    property string title: ""
    property string federation: ""
    property real pointSize: 13
    property int nameWeight: Font.Normal
    spacing: 6

    readonly property string code: federation.trim().toLowerCase()
    readonly property color titleColor: preferences.lightTheme ? "#8a6400" : "#e3b55c"

    Image {
        id: flag
        visible: root.code.length > 0 && status === Image.Ready
        source: root.code.length > 0 ? "qrc:/assets/flags/" + root.code + ".png" : ""
        Layout.preferredHeight: Math.round(root.pointSize * root.preferences.fontScale
                                           * root.preferences.uiScale)
        Layout.preferredWidth: Math.round(Layout.preferredHeight * 4 / 3)
        Layout.alignment: Qt.AlignVCenter
        sourceSize.width: 32; sourceSize.height: 24
        fillMode: Image.PreserveAspectFit
        smooth: true
        ToolTip.visible: federationHover.hovered
        ToolTip.text: root.federation.toUpperCase()
        HoverHandler { id: federationHover }
    }
    // Sans image, le code reste : il dit la fédération sans rien inventer.
    Label {
        visible: root.code.length > 0 && flag.status !== Image.Ready
        text: root.federation.toUpperCase()
        Layout.alignment: Qt.AlignVCenter
        color: root.preferences.muted
        font.pixelSize: Math.round(root.pointSize * .78 * root.preferences.fontScale
                                   * root.preferences.uiScale)
        font.weight: Font.DemiBold
    }

    Label {
        visible: root.title.length > 0
        text: root.title.toUpperCase()
        Layout.alignment: Qt.AlignVCenter
        color: root.titleColor
        font.pixelSize: Math.round(root.pointSize * .85 * root.preferences.fontScale
                                   * root.preferences.uiScale)
        font.weight: Font.Bold
    }

    Label {
        Layout.fillWidth: true
        Layout.alignment: Qt.AlignVCenter
        text: root.playerName
        elide: Text.ElideRight
        font.weight: root.nameWeight
        font.pixelSize: Math.round(root.pointSize * root.preferences.fontScale
                                   * root.preferences.uiScale)
    }
}
