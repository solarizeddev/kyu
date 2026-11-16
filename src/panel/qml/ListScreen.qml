// Pending updates: count, size, check time, the list, "Update all".
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

ColumnLayout {
    spacing: Kirigami.Units.largeSpacing * 2

    RowLayout {
        Layout.fillWidth: true
        Layout.topMargin: Kirigami.Units.gridUnit
        Layout.leftMargin: Kirigami.Units.gridUnit
        Layout.rightMargin: Kirigami.Units.largeSpacing
        ColumnLayout {
            spacing: Kirigami.Units.smallSpacing
            Layout.fillWidth: true
            Kirigami.Heading {
                level: 1
                font.pointSize: Kirigami.Theme.defaultFont.pointSize * 1.8
                font.weight: Font.DemiBold
                text: Panel.title
                Layout.fillWidth: true
                wrapMode: Text.Wrap
            }
            QQC2.Label {
                text: Panel.details
                visible: text.length > 0
                font.family: Kirigami.Theme.fixedWidthFont.family
                font.pointSize: Kirigami.Theme.smallFont.pointSize
                color: Kirigami.Theme.disabledTextColor
                Layout.fillWidth: true
                wrapMode: Text.Wrap
            }
        }
        QQC2.BusyIndicator {
            visible: Panel.checking
            running: visible
            Layout.preferredWidth: Kirigami.Units.iconSizes.medium
            Layout.preferredHeight: Kirigami.Units.iconSizes.medium
            Layout.alignment: Qt.AlignTop
        }
        QQC2.ToolButton {
            icon.name: "view-refresh"
            text: "Check again"
            display: QQC2.AbstractButton.IconOnly
            enabled: !Panel.checking
            onClicked: Panel.checkAgain()
            Layout.alignment: Qt.AlignTop
            QQC2.ToolTip.visible: hovered
            QQC2.ToolTip.text: text
        }
    }

    ColumnLayout {
        Layout.fillWidth: true
        Layout.leftMargin: Kirigami.Units.gridUnit
        Layout.rightMargin: Kirigami.Units.gridUnit
        visible: Panel.message.length > 0 || Panel.checkProblems.length > 0 || Panel.interruptedText.length > 0 || Panel.canRemoveLock
        spacing: Kirigami.Units.smallSpacing
        Repeater {
            model: Panel.checkProblems
            Message { required property string modelData; text: modelData }
        }
        Message { text: Panel.message; iconName: "dialog-error" }
        Message { text: Panel.interruptedText }
        QQC2.Button {
            visible: Panel.canRemoveLock
            text: "Remove the stale lock"
            icon.name: "object-unlocked"
            onClicked: Panel.removeStaleLock()
        }
    }

    // The distro's news since the last full upgrade, before updating.
    Card {
        visible: Panel.newsTitle.length > 0
        Layout.fillWidth: true
        Layout.leftMargin: Kirigami.Units.gridUnit
        Layout.rightMargin: Kirigami.Units.gridUnit
        implicitHeight: newsColumn.implicitHeight + Kirigami.Units.largeSpacing * 2
        ColumnLayout {
            id: newsColumn
            anchors.fill: parent
            anchors.margins: Kirigami.Units.largeSpacing
            anchors.leftMargin: Kirigami.Units.largeSpacing * 2
            spacing: Kirigami.Units.smallSpacing
            QQC2.Label {
                text: Panel.newsTitle
                font: Kirigami.Theme.smallFont
                color: Kirigami.Theme.disabledTextColor
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }
            Repeater {
                model: Panel.newsItems.slice(0, 3)
                delegate: QQC2.Label {
                    required property var modelData
                    text: "<a href=\"" + modelData.link + "\">" + modelData.title + "</a> · " + modelData.date
                    textFormat: Text.StyledText
                    linkColor: Kirigami.Theme.linkColor
                    wrapMode: Text.Wrap
                    Layout.fillWidth: true
                    onLinkActivated: link => Panel.openLink(link)
                }
            }
            QQC2.Label {
                visible: Panel.newsItems.length > 3
                text: (Panel.newsItems.length - 3) + " more"
                font: Kirigami.Theme.smallFont
                color: Kirigami.Theme.disabledTextColor
            }
            QQC2.Label {
                visible: text.length > 0
                text: Panel.newsError
                font: Kirigami.Theme.smallFont
                color: Kirigami.Theme.disabledTextColor
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }
        }
    }

    PackageList {
        visible: Panel.updates.count > 0
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.leftMargin: Kirigami.Units.gridUnit
        Layout.rightMargin: Kirigami.Units.gridUnit
    }
    ColumnLayout {
        visible: Panel.updates.count === 0
        Layout.fillWidth: true
        Layout.fillHeight: true
        Item { Layout.fillHeight: true }
        Kirigami.Icon {
            source: "update-none"
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: Kirigami.Units.iconSizes.huge
            Layout.preferredHeight: Kirigami.Units.iconSizes.huge
        }
        Item { Layout.fillHeight: true }
    }

    QQC2.Label {
        text: Panel.flatpakNote
        visible: text.length > 0
        wrapMode: Text.Wrap
        font: Kirigami.Theme.smallFont
        color: Kirigami.Theme.disabledTextColor
        Layout.fillWidth: true
        Layout.leftMargin: Kirigami.Units.gridUnit
        Layout.rightMargin: Kirigami.Units.gridUnit
    }

    RowLayout {
        Layout.fillWidth: true
        Layout.leftMargin: Kirigami.Units.gridUnit
        Layout.rightMargin: Kirigami.Units.gridUnit
        Layout.bottomMargin: Kirigami.Units.gridUnit
        spacing: Kirigami.Units.largeSpacing
        QQC2.Button {
            text: Panel.starting ? "Waiting for authorization" : "Update all"
            highlighted: true
            enabled: Panel.canUpdate
            onClicked: Panel.updateAll()
            Layout.fillWidth: true
            Layout.preferredHeight: Kirigami.Units.gridUnit * 2.5
            font.weight: Font.DemiBold
        }
        QQC2.Button {
            id: more
            icon.name: "arrow-down"
            text: "More"
            display: QQC2.AbstractButton.IconOnly
            Layout.preferredWidth: Kirigami.Units.gridUnit * 2.5
            Layout.preferredHeight: Kirigami.Units.gridUnit * 2.5
            onClicked: menu.popup(more, 0, more.height)
            QQC2.Menu {
                id: menu
                QQC2.MenuItem {
                    text: "Copy as command (sudo pacman -Syu)"
                    icon.name: "edit-copy"
                    onTriggered: Panel.copyCommand()
                }
                QQC2.MenuItem {
                    text: "Check again"
                    icon.name: "view-refresh"
                    enabled: !Panel.checking
                    onTriggered: Panel.checkAgain()
                }
            }
        }
    }
}
