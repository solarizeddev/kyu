// The top of a page (output, settings files, programs, what changed, a diff):
// back, the title, an optional fact on the right. In the terminal look it is
// the status line: "‹ back" in the link colour and "kyu · title".
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import "Style.js" as Style

Rectangle {
    id: header
    property string title
    property string rightText
    signal back()
    readonly property bool terminal: Panel.style === "terminal"

    Kirigami.Theme.colorSet: terminal ? Kirigami.Theme.Header : Kirigami.Theme.Window
    Kirigami.Theme.inherit: false
    color: terminal ? Kirigami.Theme.backgroundColor : "transparent"
    Layout.fillWidth: true
    implicitHeight: row.implicitHeight + (terminal ? Kirigami.Units.largeSpacing * 2 : Kirigami.Units.largeSpacing * 3)

    RowLayout {
        id: row
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.bottomMargin: header.terminal ? Kirigami.Units.largeSpacing : 0
        anchors.leftMargin: header.terminal ? Kirigami.Units.gridUnit : Kirigami.Units.largeSpacing
        anchors.rightMargin: Kirigami.Units.gridUnit
        spacing: Kirigami.Units.largeSpacing * 1.5
        QQC2.ToolButton {
            visible: !header.terminal
            icon.name: "go-previous"
            text: "Back"
            display: QQC2.AbstractButton.IconOnly
            onClicked: header.back()
        }
        QQC2.AbstractButton {
            id: termBack
            visible: header.terminal
            text: "‹ back"
            onClicked: header.back()
            Accessible.name: "Back"
            contentItem: QQC2.Label {
                text: termBack.text
                color: Kirigami.Theme.linkColor
                font.underline: termBack.hovered || termBack.visualFocus
            }
        }
        Kirigami.Heading {
            visible: !header.terminal
            level: 2
            text: header.title
            elide: Text.ElideMiddle
            Layout.fillWidth: true
        }
        QQC2.Label {
            visible: header.terminal
            text: header.title
            font.weight: Font.Bold
            elide: Text.ElideMiddle
            Layout.fillWidth: true
        }
        QQC2.Label {
            visible: text.length > 0
            text: header.rightText
            font.weight: Font.DemiBold
            color: Style.muted(Kirigami.Theme.textColor)
        }
    }
}
