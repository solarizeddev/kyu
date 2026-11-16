// A content area. Flat: no fill or rounded corners, a divider above and below.
import QtQuick
import org.kde.kirigami as Kirigami

Item {
    clip: true
    Kirigami.Separator {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        z: 1
    }
    Kirigami.Separator {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        z: 1
    }
}
