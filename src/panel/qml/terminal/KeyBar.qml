// Terminal style: the key line at the bottom, like a terminal multiplexer's
// status line. One row: when narrow, the buttons' texts shorten ("…").
import QtQuick
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Rectangle {
    id: keyBar
    default property alias buttons: row.data
    Kirigami.Theme.colorSet: Kirigami.Theme.Header
    Kirigami.Theme.inherit: false
    color: Kirigami.Theme.backgroundColor
    Layout.fillWidth: true
    implicitHeight: row.implicitHeight + 1

    // A rule, in case the scheme's Header and View colours are alike.
    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 1
        color: Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.15)
    }
    RowLayout {
        id: row
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        spacing: 0
    }
    // Takes the free width after the buttons (added last, so it is on the right).
    Item {
        id: spacer
        Layout.fillWidth: true
        Layout.preferredWidth: 0
        Component.onCompleted: parent = row
    }
}
