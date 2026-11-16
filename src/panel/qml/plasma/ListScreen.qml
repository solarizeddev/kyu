// Plasma style, pending updates: one fact in the header (how many, how big,
// restart or not), problems as notices, the list on a View surface, and one
// accent "Update all". The exact commands are one click away.
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import ".."
import "../Style.js" as Style

ColumnLayout {
    id: screen
    spacing: 0
    property bool showCommands: false

    ScreenHeader {
        iconName: Panel.updates.count > 0 ? "update-low" : "update-none"
        title: Panel.title
        subtitle: Panel.details
        busy: Panel.checking
        QQC2.ToolButton {
            icon.name: "view-refresh"
            text: "Check again"
            display: screen.width < Kirigami.Units.gridUnit * 30 ? QQC2.AbstractButton.IconOnly : QQC2.AbstractButton.TextBesideIcon
            enabled: !Panel.checking
            onClicked: Panel.checkAgain()
            QQC2.ToolTip.visible: hovered
            QQC2.ToolTip.text: "Check for updates now (C)"
        }
    }

    // Problems first: they decide whether "Update all" can work.
    ColumnLayout {
        visible: Panel.message.length > 0 || Panel.checkProblems.length > 0 || Panel.interruptedText.length > 0 || Panel.canRemoveLock
        Layout.fillWidth: true
        Layout.margins: Kirigami.Units.largeSpacing * 2
        Layout.bottomMargin: 0
        spacing: Kirigami.Units.largeSpacing
        Repeater {
            model: Panel.checkProblems
            Notice { required property string modelData; text: modelData; type: "warning" }
        }
        Notice { text: Panel.message; type: "error" }
        Notice { text: Panel.interruptedText; type: "warning" }
        Notice {
            visible: Panel.canRemoveLock
            type: "warning"
            title: "pacman's lock file was left behind"
            text: "/var/lib/pacman/db.lck exists, but no running program holds it: an earlier pacman run was stopped. No update can start until it is removed."
            QQC2.Button {
                text: "Remove the stale lock"
                icon.name: "object-unlocked"
                onClicked: Panel.removeStaleLock()
            }
        }
    }

    // News since the last full upgrade: count and newest title.
    QQC2.ItemDelegate {
        id: news
        visible: Panel.newsText.length > 0
        Layout.fillWidth: true
        Layout.margins: Kirigami.Units.largeSpacing * 2
        Layout.bottomMargin: 0
        padding: Kirigami.Units.largeSpacing
        onClicked: Panel.openLink(Panel.newsUrl)
        Accessible.name: Panel.newsText
        QQC2.ToolTip.visible: hovered && headline.truncated
        QQC2.ToolTip.text: Panel.newsHeadline
        background: Rectangle {
            radius: Kirigami.Units.smallSpacing
            color: Qt.rgba(Kirigami.Theme.highlightColor.r, Kirigami.Theme.highlightColor.g, Kirigami.Theme.highlightColor.b, news.hovered || news.visualFocus ? 0.25 : 0.15)
            border.width: 1
            border.color: Qt.rgba(Kirigami.Theme.highlightColor.r, Kirigami.Theme.highlightColor.g, Kirigami.Theme.highlightColor.b, 0.5)
        }
        contentItem: RowLayout {
            spacing: Kirigami.Units.largeSpacing
            Kirigami.Icon {
                source: "news-subscribe"
                fallback: "dialog-information"
                Layout.preferredWidth: Kirigami.Units.iconSizes.smallMedium
                Layout.preferredHeight: Kirigami.Units.iconSizes.smallMedium
            }
            ColumnLayout {
                spacing: 0
                Layout.fillWidth: true
                QQC2.Label {
                    text: Panel.newsSource
                    font: Kirigami.Theme.smallFont
                    color: Style.muted(Kirigami.Theme.textColor)
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
        color: Style.muted(Kirigami.Theme.textColor)
        elide: Text.ElideRight
        Layout.fillWidth: true
        Layout.leftMargin: Kirigami.Units.largeSpacing * 2
        Layout.rightMargin: Kirigami.Units.largeSpacing * 2
        Layout.topMargin: Kirigami.Units.largeSpacing
    }

    Kirigami.Separator { Layout.fillWidth: true; Layout.topMargin: Kirigami.Units.largeSpacing * 2 }
    PackageList {
        visible: Panel.updates.count > 0
        Layout.fillWidth: true
        Layout.fillHeight: true
    }
    // Nothing pending: say so, in the middle.
    Rectangle {
        visible: Panel.updates.count === 0
        Kirigami.Theme.colorSet: Kirigami.Theme.View
        Kirigami.Theme.inherit: false
        color: Kirigami.Theme.backgroundColor
        Layout.fillWidth: true
        Layout.fillHeight: true
        ColumnLayout {
            anchors.centerIn: parent
            width: Math.min(parent.width - Kirigami.Units.gridUnit * 2, Kirigami.Units.gridUnit * 20)
            spacing: Kirigami.Units.largeSpacing
            Kirigami.Icon {
                source: "update-none"
                Layout.alignment: Qt.AlignHCenter
                Layout.preferredWidth: Kirigami.Units.iconSizes.huge
                Layout.preferredHeight: Kirigami.Units.iconSizes.huge
            }
            Kirigami.Heading {
                level: 2
                text: Panel.checkProblems.length > 0 ? "Could not check for updates" : "Everything is up to date"
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }
        }
    }
    Kirigami.Separator { Layout.fillWidth: true }

    // The exact commands, when asked (nothing hidden, but quiet).
    QQC2.Label {
        visible: screen.showCommands && Panel.updates.count > 0
        text: Panel.commandPreview
        font.family: Kirigami.Theme.fixedWidthFont.family
        font.pointSize: Kirigami.Theme.smallFont.pointSize
        wrapMode: Text.Wrap
        Layout.fillWidth: true
        Layout.leftMargin: Kirigami.Units.gridUnit
        Layout.rightMargin: Kirigami.Units.gridUnit
        Layout.topMargin: Kirigami.Units.largeSpacing * 2
    }

    RowLayout {
        Layout.fillWidth: true
        Layout.margins: Kirigami.Units.largeSpacing * 2
        Layout.leftMargin: Kirigami.Units.gridUnit
        spacing: Kirigami.Units.largeSpacing
        ColumnLayout {
            spacing: 0
            Layout.fillWidth: true
            QQC2.Label {
                visible: Panel.updates.count > 0
                text: Panel.updates.pacmanCount > 0 ? "Runs pacman -Syu, then updates Flatpaks" : "Updates the Flatpaks"
                font: Kirigami.Theme.smallFont
                color: Style.muted(Kirigami.Theme.textColor)
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }
            Kirigami.LinkButton {
                visible: Panel.updates.count > 0
                text: screen.showCommands ? "Hide the commands" : "Show the commands"
                font: Kirigami.Theme.smallFont
                onClicked: screen.showCommands = !screen.showCommands
            }
        }
        PrimaryButton {
            text: Panel.starting ? "Waiting for authorization…" : "Update all"
            enabled: Panel.canUpdate
            onClicked: Panel.updateAll()
            QQC2.ToolTip.visible: hovered && enabled
            QQC2.ToolTip.text: "Update all (Enter)"
        }
        QQC2.Button {
            id: more
            icon.name: "overflow-menu"
            text: "More"
            display: QQC2.AbstractButton.IconOnly
            Layout.preferredWidth: height
            onClicked: menu.popup(more, 0, more.height)
            QQC2.ToolTip.visible: hovered
            QQC2.ToolTip.text: text
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
                QQC2.MenuSeparator {}
                QQC2.MenuItem {
                    text: "Terminal look"
                    icon.name: "utilities-terminal"
                    onTriggered: Panel.style = "terminal"
                }
            }
        }
    }
}
