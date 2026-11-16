// Pending updates: count, size, check time, news, the list, "Update all".
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

ColumnLayout {
    spacing: 0

    ColumnLayout {
        Layout.fillWidth: true
        Layout.margins: Kirigami.Units.gridUnit
        Layout.bottomMargin: Kirigami.Units.largeSpacing
        spacing: Kirigami.Units.smallSpacing
        RowLayout {
            Layout.fillWidth: true
            Kirigami.Heading {
                level: 1
                font.pointSize: Kirigami.Theme.defaultFont.pointSize * 1.6
                font.weight: Font.DemiBold
                text: Panel.title
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }
            QQC2.BusyIndicator {
                visible: Panel.checking
                running: visible
                Layout.preferredWidth: Kirigami.Units.iconSizes.smallMedium
                Layout.preferredHeight: Kirigami.Units.iconSizes.smallMedium
            }
            QQC2.Button {
                text: "Check again"
                flat: true
                enabled: !Panel.checking
                onClicked: Panel.checkAgain()
                QQC2.ToolTip.visible: hovered
                QQC2.ToolTip.text: "Check for updates now (c)"
            }
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

    ColumnLayout {
        Layout.fillWidth: true
        Layout.leftMargin: Kirigami.Units.gridUnit
        Layout.rightMargin: Kirigami.Units.gridUnit
        Layout.bottomMargin: Kirigami.Units.largeSpacing
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

    // One line, only when there is something: Arch news since the last full
    // upgrade (count and newest title, which is often the warning itself),
    // or Manjaro's update announcements. A failed fetch says so.
    Hairline { visible: newsLine.visible }
    QQC2.ItemDelegate {
        id: newsLine
        visible: Panel.newsText.length > 0
        Layout.fillWidth: true
        leftPadding: Kirigami.Units.gridUnit
        rightPadding: Kirigami.Units.gridUnit
        topPadding: Kirigami.Units.smallSpacing * 1.5
        bottomPadding: Kirigami.Units.smallSpacing * 1.5
        onClicked: Panel.openLink(Panel.newsUrl)
        Accessible.name: Panel.newsText
        QQC2.ToolTip.visible: hovered && headline.truncated
        QQC2.ToolTip.text: Panel.newsHeadline
        background: Rectangle {
            color: newsLine.hovered || newsLine.visualFocus ? Kirigami.Theme.alternateBackgroundColor : "transparent"
        }
        contentItem: RowLayout {
            spacing: Kirigami.Units.largeSpacing
            ColumnLayout {
                spacing: 0
                Layout.fillWidth: true
                QQC2.Label {
                    text: Panel.newsSource
                    font: Kirigami.Theme.smallFont
                    color: Kirigami.Theme.disabledTextColor
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
                QQC2.Label {
                    id: headline
                    text: Panel.newsHeadline
                    color: Kirigami.Theme.linkColor
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
            }
            Kirigami.Icon {
                source: "go-next"
                Layout.preferredWidth: Kirigami.Units.iconSizes.small
                Layout.preferredHeight: Kirigami.Units.iconSizes.small
            }
        }
    }
    QQC2.Label {
        visible: text.length > 0
        text: Panel.newsError
        font: Kirigami.Theme.smallFont
        color: Kirigami.Theme.disabledTextColor
        elide: Text.ElideRight
        Layout.fillWidth: true
        Layout.leftMargin: Kirigami.Units.gridUnit
        Layout.rightMargin: Kirigami.Units.gridUnit
        Layout.bottomMargin: Kirigami.Units.smallSpacing
    }

    Hairline {}
    PackageList {
        visible: Panel.updates.count > 0
        Layout.fillWidth: true
        Layout.fillHeight: true
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
    Hairline {}

    ColumnLayout {
        Layout.fillWidth: true
        Layout.margins: Kirigami.Units.gridUnit
        Layout.topMargin: Kirigami.Units.largeSpacing * 1.5
        spacing: Kirigami.Units.largeSpacing
        QQC2.Label {
            text: Panel.commandPreview
            visible: text.length > 0
            font.family: Kirigami.Theme.fixedWidthFont.family
            font.pointSize: Kirigami.Theme.smallFont.pointSize
            color: Kirigami.Theme.disabledTextColor
            wrapMode: Text.Wrap
            Layout.fillWidth: true
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: Kirigami.Units.largeSpacing
            QQC2.Button {
                text: Panel.starting ? "Waiting for authorization" : "Update all"
                highlighted: true
                enabled: Panel.canUpdate
                onClicked: Panel.updateAll()
                Layout.fillWidth: true
                Layout.preferredHeight: Kirigami.Units.gridUnit * 2.2
                font.weight: Font.DemiBold
            }
            QQC2.Button {
                id: more
                icon.name: "arrow-down"
                text: "More"
                display: QQC2.AbstractButton.IconOnly
                Layout.preferredWidth: Kirigami.Units.gridUnit * 2.2
                Layout.preferredHeight: Kirigami.Units.gridUnit * 2.2
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
        KeyHints {
            text: (Panel.canUpdate ? "⏎ update all · " : "") + "c check · esc close"
        }
    }
}
