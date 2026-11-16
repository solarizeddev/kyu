// What happened, stated plainly.
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

ColumnLayout {
    signal showOutput()
    signal showChanges()
    spacing: Kirigami.Units.largeSpacing * 2

    ColumnLayout {
        Layout.fillWidth: true
        Layout.topMargin: Kirigami.Units.gridUnit
        Layout.leftMargin: Kirigami.Units.gridUnit
        Layout.rightMargin: Kirigami.Units.gridUnit
        spacing: Kirigami.Units.smallSpacing
        Kirigami.Heading {
            level: 1
            font.pointSize: Kirigami.Theme.defaultFont.pointSize * 1.8
            font.weight: Font.DemiBold
            text: Panel.resultTitle
            wrapMode: Text.Wrap
            Layout.fillWidth: true
        }
        QQC2.Label {
            text: Panel.resultDetails
            visible: text.length > 0
            font.family: Kirigami.Theme.fixedWidthFont.family
            font.pointSize: Kirigami.Theme.smallFont.pointSize
            color: Kirigami.Theme.disabledTextColor
            wrapMode: Text.Wrap
            Layout.fillWidth: true
        }
    }

    Message {
        text: Panel.resultText
        iconName: "dialog-error"
        Layout.leftMargin: Kirigami.Units.gridUnit
        Layout.rightMargin: Kirigami.Units.gridUnit
    }

    Card {
        Layout.fillWidth: true
        Layout.leftMargin: Kirigami.Units.gridUnit
        Layout.rightMargin: Kirigami.Units.gridUnit
        implicitHeight: doneColumn.implicitHeight + Kirigami.Units.gridUnit * 2
        ColumnLayout {
            id: doneColumn
            anchors.fill: parent
            anchors.margins: Kirigami.Units.gridUnit
            QQC2.Button {
                text: "Done"
                highlighted: true
                onClicked: Panel.done()
                Layout.fillWidth: true
                Layout.preferredHeight: Kirigami.Units.gridUnit * 2.5
                font.weight: Font.DemiBold
            }
        }
    }

    Card {
        visible: Panel.changes.length > 0
        Layout.fillWidth: true
        Layout.leftMargin: Kirigami.Units.gridUnit
        Layout.rightMargin: Kirigami.Units.gridUnit
        implicitHeight: changesRow.implicitHeight + Kirigami.Units.largeSpacing * 3
        RowLayout {
            id: changesRow
            anchors.fill: parent
            anchors.leftMargin: Kirigami.Units.largeSpacing * 2
            anchors.rightMargin: Kirigami.Units.largeSpacing * 2
            ColumnLayout {
                spacing: 0
                Layout.fillWidth: true
                QQC2.Label {
                    text: "What changed"
                    font.weight: Font.DemiBold
                }
                QQC2.Label {
                    text: Panel.changes.length === 1 ? "The package, with old and new version"
                                                     : "The " + Panel.changes.length + " packages, with old and new versions"
                    font: Kirigami.Theme.smallFont
                    color: Kirigami.Theme.disabledTextColor
                }
            }
            Kirigami.Icon {
                source: "go-next"
                Layout.preferredWidth: Kirigami.Units.iconSizes.small
                Layout.preferredHeight: Kirigami.Units.iconSizes.small
            }
        }
        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: showChanges()
        }
    }

    Item { Layout.fillHeight: true }

    QQC2.Button {
        text: "Show technical details"
        icon.name: "utilities-terminal"
        onClicked: showOutput()
        Layout.fillWidth: true
        Layout.preferredHeight: Kirigami.Units.gridUnit * 2.25
        Layout.leftMargin: Kirigami.Units.gridUnit
        Layout.rightMargin: Kirigami.Units.gridUnit
        Layout.bottomMargin: Kirigami.Units.gridUnit
    }
}
