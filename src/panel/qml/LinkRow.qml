// A flat row that opens a page: title, one line under it, its key, an arrow.
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import "Style.js" as Style

Item {
    id: row
    property string title
    property string subtitle
    property string key
    property string iconName
    signal clicked()

    Layout.fillWidth: true
    implicitHeight: content.implicitHeight + Kirigami.Units.largeSpacing * 2
    Accessible.role: Accessible.Button
    Accessible.name: title
    Accessible.onPressAction: row.clicked()

    RowLayout {
        id: content
        anchors.fill: parent
        anchors.leftMargin: Kirigami.Units.gridUnit
        anchors.rightMargin: Kirigami.Units.gridUnit
        spacing: Kirigami.Units.largeSpacing
        Kirigami.Icon {
            visible: row.iconName.length > 0
            source: row.iconName
            Layout.preferredWidth: Kirigami.Units.iconSizes.smallMedium
            Layout.preferredHeight: Kirigami.Units.iconSizes.smallMedium
        }
        ColumnLayout {
            spacing: 0
            Layout.fillWidth: true
            QQC2.Label {
                text: row.title
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }
            QQC2.Label {
                text: row.subtitle
                visible: text.length > 0
                font: Kirigami.Theme.smallFont
                color: Style.muted(Kirigami.Theme.textColor)
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }
        }
        QQC2.Label {
            text: row.key
            visible: text.length > 0
            font.family: Kirigami.Theme.fixedWidthFont.family
            font.pointSize: Kirigami.Theme.smallFont.pointSize
            color: Style.muted(Kirigami.Theme.textColor)
        }
        Kirigami.Icon {
            source: "go-next"
            Layout.preferredWidth: Kirigami.Units.iconSizes.small
            Layout.preferredHeight: Kirigami.Units.iconSizes.small
        }
    }
    Kirigami.Separator {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
    }
    MouseArea {
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
        onClicked: row.clicked()
    }
}
