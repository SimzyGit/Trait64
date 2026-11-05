// SPDX-License-Identifier: AGPL-3.0-or-later
// Le portrait d'un profil. Sans image, les initiales tiennent la place plutôt qu'un
// carré vide : la carte garde sa hauteur et le profil reste reconnaissable d'un coup
// d'œil. Carré et non rond : arrondir une image demande un module d'effets que Qt 6.4
// n'a pas partout, et un faux arrondi laisse des coins qui dépassent.
import QtQuick
import QtQuick.Controls

Rectangle {
    id: root
    required property var preferences
    property string source: ""
    property string playerName: ""
    property int side: 72

    implicitWidth: side
    implicitHeight: side
    color: preferences.raised
    border.color: preferences.line
    border.width: 1

    // Deux lettres au plus : « Jean-Marc Dupont » donne JD, « Simzy » donne S.
    readonly property string initials: {
        var words = playerName.trim().split(/[\s\-_]+/).filter(function (w) { return w.length > 0 })
        if (words.length === 0)
            return ""
        var first = words[0].charAt(0)
        return (words.length > 1 ? first + words[words.length - 1].charAt(0) : first).toUpperCase()
    }

    Label {
        anchors.centerIn: parent
        visible: portrait.status !== Image.Ready
        text: root.initials
        color: root.preferences.muted
        font.pixelSize: Math.round(root.side * .38)
        font.weight: Font.DemiBold
    }

    Image {
        id: portrait
        anchors.fill: parent
        anchors.margins: 1
        source: root.source.length > 0 ? "file://" + root.source : ""
        // La vignette est déjà réduite à 256 pixels à l'enregistrement ; on recadre pour
        // remplir le carré plutôt que de déformer le visage.
        fillMode: Image.PreserveAspectCrop
        sourceSize.width: root.side * 2
        sourceSize.height: root.side * 2
        asynchronous: true
        smooth: true
        clip: true
    }
}
