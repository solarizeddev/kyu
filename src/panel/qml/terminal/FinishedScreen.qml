// What happened, stated plainly, and what is left to do.
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import ".."
import "../Style.js" as Style

ColumnLayout {
    signal showOutput()
    signal showChanges()
    signal showConfigs()
    signal showProcesses()
    spacing: 0

    ColumnLayout {
        Layout.fillWidth: true
        Layout.margins: Kirigami.Units.gridUnit
        spacing: Kirigami.Units.smallSpacing
        Kirigami.Heading {
            level: 1
            font.pointSize: Kirigami.Theme.defaultFont.pointSize * 1.6
            font.weight: Font.DemiBold
            text: (Panel.success ? "✓ " : "") + Panel.resultTitle
            wrapMode: Text.Wrap
            Layout.fillWidth: true
        }
        QQC2.Label {
            text: Panel.resultDetails
            visible: text.length > 0
            font.family: Kirigami.Theme.fixedWidthFont.family
            font.pointSize: Kirigami.Theme.smallFont.pointSize
            color: Style.muted(Kirigami.Theme.textColor)
            wrapMode: Text.Wrap
            Layout.fillWidth: true
        }
        QQC2.Label {
            text: Panel.transactionNote
            visible: text.length > 0
            font.pointSize: Kirigami.Theme.smallFont.pointSize
            color: Kirigami.Theme.neutralTextColor
            wrapMode: Text.Wrap
            Layout.fillWidth: true
        }
    }

    Hairline {}
    QQC2.ScrollView {
        Layout.fillWidth: true
        Layout.fillHeight: true
        contentWidth: availableWidth
        clip: true

        ColumnLayout {
            width: parent.width
            spacing: 0

            // Result and restart, or failure, first.
            ColumnLayout {
                Layout.fillWidth: true
                Layout.margins: Kirigami.Units.gridUnit
                spacing: Kirigami.Units.largeSpacing
                Notice {
                    text: Panel.resultText
                    type: "error"
                    collapsible: false
                }
                QQC2.Label {
                    visible: Panel.restartRequired
                    text: "Restart required"
                    font.weight: Font.DemiBold
                    color: Kirigami.Theme.neutralTextColor
                }
                QQC2.Label {
                    visible: Panel.restartRequired
                    text: Panel.restartReason
                    wrapMode: Text.Wrap
                    Layout.fillWidth: true
                }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: Kirigami.Units.largeSpacing
                    QQC2.Button {
                        text: Panel.restartRequired ? "Restart now" : "Done"
                        highlighted: true
                        onClicked: Panel.restartRequired ? Panel.restartNow() : Panel.done()
                        Layout.fillWidth: true
                        Layout.preferredHeight: Kirigami.Units.gridUnit * 2.2
                        font.weight: Font.DemiBold
                    }
                    QQC2.Button {
                        visible: Panel.restartRequired
                        text: "Later"
                        onClicked: Panel.done()
                        Layout.preferredHeight: Kirigami.Units.gridUnit * 2.2
                    }
                }
            }

            // Why it failed: an explanation, what it is about, pacman's own lines.
            Hairline { visible: Panel.failureTitle.length > 0 }
            ColumnLayout {
                visible: Panel.failureTitle.length > 0
                Layout.fillWidth: true
                Layout.margins: Kirigami.Units.gridUnit
                spacing: Kirigami.Units.smallSpacing
                QQC2.Label {
                    text: Panel.failureTitle
                    font.weight: Font.DemiBold
                    wrapMode: Text.Wrap
                    Layout.fillWidth: true
                }
                QQC2.Label {
                    text: Panel.failureText
                    wrapMode: Text.Wrap
                    Layout.fillWidth: true
                }
                QQC2.Label {
                    visible: Panel.failureItems.length > 0
                    text: Panel.failureItems.slice(0, 8).join("\n") + (Panel.failureItems.length > 8 ? "\n… " + (Panel.failureItems.length - 8) + " more" : "")
                    font.family: Kirigami.Theme.fixedWidthFont.family
                    font.pointSize: Kirigami.Theme.smallFont.pointSize
                    wrapMode: Text.WrapAnywhere
                    Layout.fillWidth: true
                }
                QQC2.Label {
                    text: "pacman:\n" + Panel.failureLines.slice(0, 6).join("\n")
                    font.family: Kirigami.Theme.fixedWidthFont.family
                    font.pointSize: Kirigami.Theme.smallFont.pointSize
                    color: Style.muted(Kirigami.Theme.textColor)
                    wrapMode: Text.WrapAnywhere
                    Layout.fillWidth: true
                }
                RowLayout {
                    visible: Panel.failureCommand.length > 0
                    Layout.fillWidth: true
                    QQC2.Label {
                        text: "$ " + Panel.failureCommand
                        font.family: Kirigami.Theme.fixedWidthFont.family
                        font.pointSize: Kirigami.Theme.smallFont.pointSize
                        wrapMode: Text.WrapAnywhere
                        Layout.fillWidth: true
                    }
                    QQC2.Button {
                        text: "Copy"
                        icon.name: "edit-copy"
                        onClicked: Panel.copyText(Panel.failureCommand)
                    }
                }
            }

            Hairline {}
            LinkRow {
                visible: Panel.configs.length > 0
                title: Panel.configs.length === 1 ? "1 settings file changed" : Panel.configs.length + " settings files changed"
                subtitle: "Optional. Compare yours with the new defaults"
                key: "s"
                onClicked: showConfigs()
            }
            LinkRow {
                visible: Panel.processCount > 0
                title: Panel.processCount === 1 ? "1 program uses replaced libraries" : Panel.processCount + " programs use replaced libraries"
                subtitle: "They run the old versions until they restart"
                key: "p"
                onClicked: showProcesses()
            }
            LinkRow {
                visible: Panel.changes.length > 0
                title: "What changed"
                subtitle: Panel.changes.length === 1 ? "1 package, with old and new version" : Panel.changes.length + " packages, with old and new versions"
                key: "w"
                onClicked: showChanges()
            }
        }
    }
    Hairline {}
    LogTail { lines: 4 }
    Hairline {}

    KeyHints {
        Layout.margins: Kirigami.Units.gridUnit
        Layout.topMargin: Kirigami.Units.largeSpacing
        text: [Panel.configs.length > 0 ? "s settings files" : "",
               Panel.processCount > 0 ? "p programs" : "",
               Panel.changes.length > 0 ? "w what changed" : "",
               "l output", "esc done"].filter(s => s.length > 0).join(" · ")
    }
}
