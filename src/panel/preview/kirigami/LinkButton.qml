// Preview stand-in for Kirigami.LinkButton (see preview.h).
import QtQuick
import QtQuick.Controls as QQC2

QQC2.Label {
    id: link
    signal clicked()
    color: "#2980b9"
    font.underline: area.containsMouse
    MouseArea {
        id: area
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: link.clicked()
    }
}
