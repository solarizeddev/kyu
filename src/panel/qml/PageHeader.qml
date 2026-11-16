import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

RowLayout {
    property alias title: heading.text
    signal back()
    Layout.fillWidth: true
    Layout.topMargin: Kirigami.Units.largeSpacing * 2
    Layout.leftMargin: Kirigami.Units.largeSpacing
    Layout.rightMargin: Kirigami.Units.gridUnit
    QQC2.ToolButton {
        icon.name: "go-previous"
        text: "Back"
        display: QQC2.AbstractButton.IconOnly
        onClicked: back()
    }
    Kirigami.Heading {
        id: heading
        level: 2
        elide: Text.ElideMiddle
        Layout.fillWidth: true
    }
}
