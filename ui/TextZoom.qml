import QtQuick
// Ctrl + molette sur la notation ou une liste : la taille du texte, comme dans ChessBase.
// Posé dans la vue qui défile, il passe avant elle ; sans Ctrl, la molette défile.
WheelHandler {
    acceptedModifiers: Qt.ControlModifier
    onWheel: function(event) {
        if (event.angleDelta.y === 0)
            return
        const scale = Number(controller.preferences.fontScale) + (event.angleDelta.y > 0 ? .05 : -.05)
        // Après l'événement : changer la taille pendant qu'il traverse la vue la fait se
        // réagencer sous lui, et Qt 6.4 s'y perd (plantage dans GridLayout).
        Qt.callLater(controller.setPreference, "fontScale", Math.round(Math.max(.9, Math.min(1.5, scale)) * 100) / 100)
    }
}
