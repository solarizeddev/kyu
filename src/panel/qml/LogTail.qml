// The last lines of the output, live. "l" opens all of it.
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import "Style.js" as Style

Rectangle {
    id: tail
    property int lines: 5
    // Re-read whenever lines arrive.
    readonly property var shown: Panel.output.count >= 0 ? Panel.output.tail(lines) : []
    Kirigami.Theme.colorSet: Kirigami.Theme.View
    Kirigami.Theme.inherit: false
    color: Kirigami.Theme.backgroundColor
    Layout.fillWidth: true
    implicitHeight: column.implicitHeight + Kirigami.Units.largeSpacing * 2

    Accessible.role: Accessible.StaticText
    Accessible.name: "Last lines of the output"

    ColumnLayout {
        id: column
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: Kirigami.Units.largeSpacing
        anchors.leftMargin: Kirigami.Units.gridUnit
        anchors.rightMargin: Kirigami.Units.gridUnit
        spacing: 0
        Repeater {
            model: tail.lines
            delegate: QQC2.Label {
                required property int index
                // Keep the height fixed: empty rows until there is output.
                readonly property var entry: index < tail.shown.length ? tail.shown[index] : null
                Layout.fillWidth: true
                text: entry && entry.line.length > 0 ? entry.line : " "
                elide: Text.ElideRight
                font.family: Kirigami.Theme.fixedWidthFont.family
                font.pointSize: Kirigami.Theme.smallFont.pointSize
                color: !entry ? Style.muted(Kirigami.Theme.textColor)
                     : entry.stream === "kyu" ? Kirigami.Theme.linkColor
                     : entry.stream === "stderr" ? Kirigami.Theme.neutralTextColor
                     : index === tail.shown.length - 1 ? Kirigami.Theme.textColor
                     : Style.muted(Kirigami.Theme.textColor)
            }
        }
    }
}
