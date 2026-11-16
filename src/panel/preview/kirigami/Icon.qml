// Preview stand-in for Kirigami.Icon (see preview.h).
import QtQuick

Item {
    property var source
    property var fallback
    property color color: "transparent"
    implicitWidth: 32
    implicitHeight: 32
    Image {
        anchors.fill: parent
        sourceSize: Qt.size(width, height)
        source: parent.source ? "image://icon/" + parent.source + (parent.color.a > 0 ? "?color=" + encodeURIComponent(parent.color) : "") : ""
        smooth: true
    }
}
