// Terminal style: the key line at the bottom, like a terminal multiplexer's
// status line. Its buttons (KeyButton) say their key.
import QtQuick
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Rectangle {
    default property alias buttons: row.data
    Kirigami.Theme.colorSet: Kirigami.Theme.Header
    Kirigami.Theme.inherit: false
    color: Kirigami.Theme.backgroundColor
    Layout.fillWidth: true
    implicitHeight: row.implicitHeight
    Flow {
        id: row
        anchors.left: parent.left
        anchors.right: parent.right
        spacing: 0
    }
}
