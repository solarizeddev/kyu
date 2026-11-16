import QtQuick
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Rectangle {
    property real value: 0 // 0..1
    Layout.fillWidth: true
    implicitHeight: Math.max(2, Math.round(Kirigami.Units.smallSpacing))
    radius: height / 2
    color: Kirigami.Theme.alternateBackgroundColor
    Rectangle {
        width: parent.width * Math.max(0, Math.min(1, parent.value))
        height: parent.height
        radius: parent.radius
        color: Kirigami.Theme.highlightColor
        Behavior on width { NumberAnimation { duration: Kirigami.Units.longDuration } }
    }
}
