// Terminal style: a fact in a bordered box: "! restart required" in the
// status colour, the text, and any buttons under it.
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Rectangle {
    id: box
    property string title
    property alias text: label.text
    property string type: "warning" // "error", "warning", "positive", "information"
    property bool collapsible: true
    property bool expanded: false
    default property alias content: extra.data
    readonly property color tone: type === "error" ? Kirigami.Theme.negativeTextColor
                                : type === "positive" ? Kirigami.Theme.positiveTextColor
                                : type === "information" ? Kirigami.Theme.linkColor
                                : Kirigami.Theme.neutralTextColor
    // Shown when there is something to say (the title alone is a label).
    visible: label.text.length > 0
    color: "transparent"
    border.width: 1
    border.color: tone
    Layout.fillWidth: true
    implicitHeight: column.implicitHeight + Kirigami.Units.largeSpacing * 2
    Accessible.role: Accessible.StaticText
    Accessible.name: title + " " + label.text

    MouseArea {
        anchors.fill: parent
        enabled: label.truncated || box.expanded
        cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
        onClicked: box.expanded = !box.expanded
    }
    ColumnLayout {
        id: column
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: Kirigami.Units.largeSpacing
        anchors.leftMargin: Kirigami.Units.largeSpacing * 1.5
        anchors.rightMargin: Kirigami.Units.largeSpacing * 1.5
        spacing: Kirigami.Units.smallSpacing
        QQC2.Label {
            visible: box.title.length > 0
            text: (box.type === "error" ? "✗ " : box.type === "positive" ? "✓ " : box.type === "information" ? "i " : "! ") + box.title
            font.weight: Font.Bold
            color: box.tone
            wrapMode: Text.Wrap
            Layout.fillWidth: true
        }
        QQC2.Label {
            id: label
            visible: text.length > 0
            wrapMode: Text.Wrap
            maximumLineCount: box.expanded || !box.collapsible ? 1000 : 4
            elide: Text.ElideRight
            Layout.fillWidth: true
        }
        // Buttons wrap rather than push the box wider than the window.
        Flow {
            id: extra
            visible: children.length > 0
            spacing: Kirigami.Units.largeSpacing
            Layout.fillWidth: true
            Layout.topMargin: Kirigami.Units.smallSpacing
        }
    }
}
