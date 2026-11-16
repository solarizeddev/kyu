// Terminal style, after an update: one summary line with its mark, what is
// left to do in boxes, the pages as keyed lines, and the last output lines.
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import QtQuick.Window
import org.kde.kirigami as Kirigami
import ".."
import "../Style.js" as Style

TermScreen {
    id: screen
    signal showOutput()
    signal showChanges()
    signal showConfigs()
    signal showProcesses()

    readonly property bool ok: Panel.outcome === "success"

    TermBar {
        text: screen.ok ? "done" : Panel.outcome === "partial" ? "done, with errors" : Panel.outcome === "stopped" ? "stopped" : "failed"
        textColor: screen.ok ? Kirigami.Theme.positiveTextColor : Panel.outcome === "failed" ? Kirigami.Theme.negativeTextColor : Kirigami.Theme.neutralTextColor
    }

    QQC2.ScrollView {
        id: scroll
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.preferredHeight: 3
        contentWidth: availableWidth
        clip: true

        ColumnLayout {
            width: scroll.availableWidth
            spacing: Kirigami.Units.largeSpacing

            ColumnLayout {
                Layout.fillWidth: true
                Layout.topMargin: Kirigami.Units.gridUnit
                Layout.leftMargin: Kirigami.Units.gridUnit
                Layout.rightMargin: Kirigami.Units.gridUnit
                spacing: Kirigami.Units.smallSpacing
                QQC2.Label {
                    text: (screen.ok ? "✓ " : Panel.outcome === "failed" ? "✗ " : "! ") + Panel.resultTitle
                    font.weight: Font.Bold
                    font.pointSize: Kirigami.Theme.defaultFont.pointSize * 1.2
                    color: screen.ok ? Kirigami.Theme.textColor : Panel.outcome === "failed" ? Kirigami.Theme.negativeTextColor : Kirigami.Theme.neutralTextColor
                    wrapMode: Text.Wrap
                    Layout.fillWidth: true
                }
                QQC2.Label {
                    visible: text.length > 0
                    text: Panel.resultDetails
                    color: Style.muted(Kirigami.Theme.textColor)
                    wrapMode: Text.Wrap
                    Layout.fillWidth: true
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.leftMargin: Kirigami.Units.gridUnit
                Layout.rightMargin: Kirigami.Units.gridUnit
                spacing: Kirigami.Units.largeSpacing
                Box {
                    title: Panel.outcome === "failed" ? "failed" : "not everything finished"
                    text: Panel.resultText
                    type: Panel.outcome === "failed" ? "error" : "warning"
                    collapsible: false
                }
                Box { title: "transaction"; text: Panel.transactionNote; type: "information" }
                Box {
                    visible: Panel.restartRequired
                    title: "restart required"
                    text: Panel.restartReason
                    type: "warning"
                    KeyButton { text: "restart now"; primary: true; onClicked: Panel.restartNow() }
                    KeyButton { text: "later"; onClicked: Panel.done() }
                }
                ColumnLayout {
                    visible: Panel.failureTitle.length > 0
                    Layout.fillWidth: true
                    spacing: Kirigami.Units.smallSpacing
                    QQC2.Label {
                        text: "# " + Panel.failureTitle
                        font.weight: Font.Bold
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
                        wrapMode: Text.Wrap
                        Layout.fillWidth: true
                    }
                    QQC2.Label {
                        text: Panel.failureLines.slice(0, 6).join("\n")
                        color: Style.muted(Kirigami.Theme.textColor)
                        wrapMode: Text.Wrap
                        Layout.fillWidth: true
                    }
                    QQC2.Label {
                        visible: Panel.failureCommand.length > 0
                        text: "$ " + Panel.failureCommand
                        color: Kirigami.Theme.linkColor
                        wrapMode: Text.Wrap
                        Layout.fillWidth: true
                    }
                    KeyButton {
                        visible: Panel.failureCommand.length > 0
                        text: "copy the command"
                        onClicked: Panel.copyText(Panel.failureCommand)
                    }
                }
            }

            // The pages, by key.
            ColumnLayout {
                Layout.fillWidth: true
                Layout.bottomMargin: Kirigami.Units.largeSpacing
                spacing: 0
                Repeater {
                    model: [
                        { key: "s", show: Panel.configs.length > 0, title: "settings files", detail: Panel.configs.length + " changed", page: "configs" },
                        { key: "p", show: Panel.processCount > 0, title: "programs on old libraries", detail: String(Panel.processCount), page: "processes" },
                        { key: "w", show: Panel.changes.length > 0, title: "what changed", detail: Panel.changes.length + (Panel.changes.length === 1 ? " package" : " packages"), page: "changes" },
                    ]
                    delegate: QQC2.AbstractButton {
                        id: link
                        required property var modelData
                        visible: modelData.show
                        Layout.fillWidth: true
                        leftPadding: Kirigami.Units.gridUnit
                        rightPadding: Kirigami.Units.gridUnit
                        topPadding: Kirigami.Units.smallSpacing
                        bottomPadding: Kirigami.Units.smallSpacing
                        Accessible.name: modelData.title + ", " + modelData.detail
                        onClicked: modelData.page === "configs" ? screen.showConfigs() : modelData.page === "processes" ? screen.showProcesses() : screen.showChanges()
                        background: Rectangle {
                            color: link.hovered || link.visualFocus ? Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.08) : "transparent"
                        }
                        contentItem: RowLayout {
                            spacing: Kirigami.Units.largeSpacing * 1.5
                            QQC2.Label { text: link.modelData.key; font.weight: Font.Bold; color: Kirigami.Theme.linkColor }
                            QQC2.Label { text: link.modelData.title; elide: Text.ElideRight; Layout.fillWidth: true }
                            QQC2.Label { text: link.modelData.detail; color: Style.muted(Kirigami.Theme.textColor) }
                        }
                    }
                }
            }
        }
    }

    Rectangle {
        Layout.fillWidth: true
        implicitHeight: 1
        color: Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.2)
    }
    LogTail {
        lines: 5
        color: "transparent"
    }

    KeyBar {
        KeyButton { key: "l"; text: "full output"; onClicked: screen.showOutput() }
        KeyButton { key: "esc"; text: "done"; onClicked: Panel.done() }
    }
}
