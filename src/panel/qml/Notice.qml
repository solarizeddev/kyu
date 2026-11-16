// A fact the user should see, in the colour scheme's status colours: an error
// (failed check, refused start), a warning (restart, interrupted update), a
// success or plain information. Long text takes three lines until clicked,
// unless collapsible is false. Children (buttons) go below the text.
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Rectangle {
    id: notice
    property alias text: label.text
    property string title: ""
    property string type: "warning" // "error", "warning", "positive", "information"
    property bool collapsible: true
    property bool expanded: false
    property bool monospace: false
    default property alias content: extra.data

    readonly property color tone: type === "error" ? Kirigami.Theme.negativeTextColor
                                : type === "positive" ? Kirigami.Theme.positiveTextColor
                                : type === "information" ? Kirigami.Theme.highlightColor
                                : Kirigami.Theme.neutralTextColor
    visible: text.length > 0 || title.length > 0
    color: type === "error" ? Kirigami.Theme.negativeBackgroundColor
         : type === "positive" ? Kirigami.Theme.positiveBackgroundColor
         : type === "information" ? Qt.rgba(tone.r, tone.g, tone.b, 0.15)
         : Kirigami.Theme.neutralBackgroundColor
    border.width: 1
    border.color: Qt.rgba(tone.r, tone.g, tone.b, 0.5)
    radius: Kirigami.Units.smallSpacing
    implicitHeight: row.implicitHeight + Kirigami.Units.largeSpacing * 2
    Layout.fillWidth: true
    Accessible.role: Accessible.StaticText
    Accessible.name: (title.length > 0 ? title + ". " : "") + label.text

    MouseArea {
        anchors.fill: parent
        enabled: label.truncated || notice.expanded
        cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
        onClicked: notice.expanded = !notice.expanded
    }

    RowLayout {
        id: row
        anchors.fill: parent
        anchors.margins: Kirigami.Units.largeSpacing
        spacing: Kirigami.Units.largeSpacing
        Kirigami.Icon {
            source: notice.type === "error" ? "dialog-error"
                  : notice.type === "positive" ? "dialog-positive"
                  : notice.type === "information" ? "dialog-information"
                  : "dialog-warning"
            Layout.preferredWidth: Kirigami.Units.iconSizes.smallMedium
            Layout.preferredHeight: Kirigami.Units.iconSizes.smallMedium
            Layout.alignment: Qt.AlignTop
        }
        ColumnLayout {
            Layout.fillWidth: true
            spacing: Kirigami.Units.smallSpacing
            QQC2.Label {
                visible: notice.title.length > 0
                text: notice.title
                font.weight: Font.DemiBold
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }
            QQC2.Label {
                id: label
                visible: text.length > 0
                Layout.fillWidth: true
                wrapMode: notice.monospace ? Text.WrapAnywhere : Text.Wrap
                font.family: notice.monospace ? Kirigami.Theme.fixedWidthFont.family : Kirigami.Theme.defaultFont.family
                maximumLineCount: notice.expanded || !notice.collapsible ? 1000 : 3
                elide: Text.ElideRight
                color: Kirigami.Theme.textColor
            }
            ColumnLayout {
                id: extra
                visible: children.length > 0
                Layout.fillWidth: true
                Layout.topMargin: Kirigami.Units.smallSpacing
                spacing: Kirigami.Units.smallSpacing
            }
        }
    }
}
