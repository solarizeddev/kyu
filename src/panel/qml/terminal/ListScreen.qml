// Terminal style, pending updates: a status line, the exact command, problems
// and news as boxed lines, the list in columns, and a key line.
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import QtQuick.Window
import org.kde.kirigami as Kirigami
import ".."
import "../Style.js" as Style

TermScreen {
    id: screen

    TermBar {
        text: Panel.title + (Panel.details.length > 0 ? " · " + Panel.details : "")
        rightText: Panel.checking ? "checking…" : ""
    }

    ColumnLayout {
        Layout.fillWidth: true
        Layout.margins: Kirigami.Units.gridUnit
        Layout.bottomMargin: Kirigami.Units.largeSpacing
        spacing: Kirigami.Units.largeSpacing

        // The exact commands "update all" runs.
        QQC2.Label {
            visible: Panel.updates.count > 0
            text: Panel.commandPreview
            color: Style.muted(Kirigami.Theme.textColor)
            wrapMode: Text.Wrap
            Layout.fillWidth: true
        }

        Repeater {
            model: Panel.checkProblems
            Box { required property string modelData; title: "check failed"; text: modelData; type: "warning" }
        }
        Box { title: "error"; text: Panel.message; type: "error" }
        Box { title: "interrupted"; text: Panel.interruptedText; type: "warning" }
        Box {
            visible: Panel.canRemoveLock
            type: "warning"
            title: "stale lock: /var/lib/pacman/db.lck"
            text: "No running program holds it: an earlier pacman run was stopped. No update can start until it is removed."
            KeyButton {
                text: "remove the stale lock"
                primary: true
                onClicked: Panel.removeStaleLock()
            }
        }

        // News since the last full upgrade.
        QQC2.AbstractButton {
            id: news
            visible: Panel.newsText.length > 0
            Layout.fillWidth: true
            onClicked: Panel.openLink(Panel.newsUrl)
            Accessible.name: Panel.newsText
            QQC2.ToolTip.visible: hovered && headline.truncated
            QQC2.ToolTip.text: Panel.newsHeadline
            contentItem: ColumnLayout {
                spacing: 0
                QQC2.Label {
                    text: "! " + Panel.newsSource.toLowerCase()
                    color: Kirigami.Theme.neutralTextColor
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
                QQC2.Label {
                    id: headline
                    text: "  " + Panel.newsHeadline + " ↗"
                    color: Kirigami.Theme.linkColor
                    font.underline: news.hovered || news.visualFocus
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
            }
        }
        QQC2.Label {
            visible: text.length > 0
            text: Panel.newsError
            color: Style.muted(Kirigami.Theme.textColor)
            elide: Text.ElideRight
            Layout.fillWidth: true
        }
    }

    PackageList {
        visible: Panel.updates.count > 0
        Layout.fillWidth: true
        Layout.fillHeight: true
    }
    ColumnLayout {
        visible: Panel.updates.count === 0
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.margins: Kirigami.Units.gridUnit
        Item { Layout.fillHeight: true }
        QQC2.Label {
            text: Panel.checkProblems.length > 0 ? "✗ could not check for updates" : "✓ nothing to update"
            color: Panel.checkProblems.length > 0 ? Kirigami.Theme.negativeTextColor : Kirigami.Theme.positiveTextColor
            font.weight: Font.Bold
            horizontalAlignment: Text.AlignHCenter
            Layout.fillWidth: true
        }
        Item { Layout.fillHeight: true }
    }

    KeyBar {
        KeyButton {
            key: "⏎"
            text: Panel.starting ? "waiting for authorization…" : "update all"
            primary: true
            enabled: Panel.canUpdate
            onClicked: Panel.updateAll()
        }
        KeyButton {
            key: "c"
            text: "check"
            enabled: !Panel.checking
            onClicked: Panel.checkAgain()
        }
        KeyButton {
            id: menuButton
            text: "more ▾"
            onClicked: menu.popup(menuButton, 0, -menu.implicitHeight)
            QQC2.Menu {
                id: menu
                font: screen.font
                QQC2.MenuItem {
                    text: "Copy as command (sudo pacman -Syu)"
                    icon.name: "edit-copy"
                    onTriggered: Panel.copyCommand()
                }
                QQC2.MenuItem {
                    text: "Plasma look"
                    icon.name: "preferences-desktop-theme"
                    onTriggered: Panel.style = "plasma"
                }
            }
        }
        KeyButton {
            key: "esc"
            text: "close"
            onClicked: screen.Window.window.close()
        }
    }
}
