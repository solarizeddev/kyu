// The top of a Plasma-style screen: a status icon, the one fact that matters
// (title), a line of detail, and room on the right for a tool button.
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import "../Style.js" as Style

Rectangle {
    id: header
    property string iconName
    property string title
    property string subtitle
    property bool busy: false
    default property alias trailing: tools.data

    Kirigami.Theme.colorSet: Kirigami.Theme.Header
    Kirigami.Theme.inherit: false
    color: Kirigami.Theme.backgroundColor
    Layout.fillWidth: true
    implicitHeight: row.implicitHeight + Kirigami.Units.gridUnit * 1.5

    RowLayout {
        id: row
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        anchors.leftMargin: Kirigami.Units.gridUnit
        anchors.rightMargin: Kirigami.Units.largeSpacing * 2
        spacing: Kirigami.Units.largeSpacing * 2
        Item {
            Layout.preferredWidth: Kirigami.Units.iconSizes.large
            Layout.preferredHeight: Kirigami.Units.iconSizes.large
            Layout.alignment: Qt.AlignTop
            Kirigami.Icon {
                anchors.fill: parent
                visible: !header.busy
                source: header.iconName
            }
            QQC2.BusyIndicator {
                anchors.fill: parent
                visible: header.busy
                running: visible
            }
        }
        ColumnLayout {
            spacing: Kirigami.Units.smallSpacing / 2
            Layout.fillWidth: true
            Kirigami.Heading {
                level: 1
                text: header.title
                font.weight: Font.DemiBold
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }
            QQC2.Label {
                visible: text.length > 0
                // Each " · " part stays on one line ("checked 10:30:00").
                text: header.subtitle.split(" · ").map(part => part.replace(/ /g, "\u00a0")).join(" · ")
                color: Style.muted(Kirigami.Theme.textColor)
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }
        }
        RowLayout {
            id: tools
            spacing: Kirigami.Units.smallSpacing
            Layout.alignment: Qt.AlignTop
        }
    }
    Kirigami.Separator {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
    }
}
