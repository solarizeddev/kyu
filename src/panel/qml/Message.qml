// A fact the user should see: a failed check, a refused start, a question.
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Rectangle {
    id: message
    property alias text: label.text
    property string iconName: "dialog-warning"
    // Long messages (an error with a whole URL) take three lines until
    // clicked, so they never push the list and buttons off the window.
    // collapsible: false shows everything (the update's own result, which
    // sits in a scrolling area).
    property bool collapsible: true
    property bool expanded: false

    visible: text.length > 0
    color: Kirigami.Theme.neutralBackgroundColor
    radius: Kirigami.Units.smallSpacing
    implicitHeight: row.implicitHeight + Kirigami.Units.largeSpacing * 2
    Layout.fillWidth: true

    MouseArea {
        anchors.fill: parent
        enabled: label.truncated || message.expanded
        cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
        onClicked: message.expanded = !message.expanded
    }
    Accessible.name: label.text

    RowLayout {
        id: row
        anchors.fill: parent
        anchors.margins: Kirigami.Units.largeSpacing
        spacing: Kirigami.Units.largeSpacing
        Kirigami.Icon {
            source: message.iconName
            Layout.preferredWidth: Kirigami.Units.iconSizes.small
            Layout.preferredHeight: Kirigami.Units.iconSizes.small
            Layout.alignment: Qt.AlignTop
        }
        QQC2.Label {
            id: label
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            maximumLineCount: message.expanded || !message.collapsible ? 1000 : 3
            elide: Text.ElideRight
            color: Kirigami.Theme.textColor
        }
    }
}
