// Plasma style, after an update: the outcome first (large icon, one sentence),
// then what is left to do (restart, why it failed), then pages to open.
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import ".."
import "../Style.js" as Style

ColumnLayout {
    id: screen
    signal showOutput()
    signal showChanges()
    signal showConfigs()
    signal showProcesses()
    spacing: 0

    QQC2.ScrollView {
        id: scroll
        Layout.fillWidth: true
        Layout.fillHeight: true
        contentWidth: availableWidth
        clip: true

        ColumnLayout {
            width: scroll.availableWidth
            spacing: Kirigami.Units.largeSpacing * 2

            // The outcome.
            ColumnLayout {
                Layout.fillWidth: true
                Layout.topMargin: Kirigami.Units.gridUnit * 1.5
                Layout.leftMargin: Kirigami.Units.gridUnit
                Layout.rightMargin: Kirigami.Units.gridUnit
                spacing: Kirigami.Units.largeSpacing
                Kirigami.Icon {
                    // Status icons drawn for large sizes (the emblem-* set
                    // stops at 22 px).
                    source: Panel.outcome === "success" ? "data-success"
                          : Panel.outcome === "failed" ? "data-error"
                          : "data-warning"
                    fallback: Panel.outcome === "success" ? "dialog-positive" : Panel.outcome === "failed" ? "dialog-error" : "dialog-warning"
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth: Kirigami.Units.iconSizes.huge
                    Layout.preferredHeight: Kirigami.Units.iconSizes.huge
                }
                Kirigami.Heading {
                    level: 1
                    text: Panel.outcome === "success" && !Panel.restartRequired ? "Your system is up to date" : Panel.resultTitle
                    font.weight: Font.DemiBold
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.Wrap
                    Layout.fillWidth: true
                }
                QQC2.Label {
                    // The plain facts: what was updated, when, how long.
                    text: (Panel.outcome === "success" && !Panel.restartRequired ? Panel.resultTitle + "\n" : "") + Panel.resultDetails
                    visible: text.length > 0
                    font: Kirigami.Theme.smallFont
                    color: Style.muted(Kirigami.Theme.textColor)
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.Wrap
                    Layout.fillWidth: true
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.leftMargin: Kirigami.Units.largeSpacing * 2
                Layout.rightMargin: Kirigami.Units.largeSpacing * 2
                spacing: Kirigami.Units.largeSpacing
                Notice {
                    text: Panel.resultText
                    type: Panel.outcome === "failed" ? "error" : "warning"
                    collapsible: false
                }
                Notice { text: Panel.transactionNote; type: "information"; collapsible: false }
                Notice {
                    visible: Panel.restartRequired
                    type: "warning"
                    title: "Restart to finish"
                    text: Panel.restartReason
                    ButtonPair {
                        Layout.fillWidth: true
                        secondaryText: "Later"
                        primaryText: "Restart now"
                        onSecondaryClicked: Panel.done()
                        onPrimaryClicked: Panel.restartNow()
                    }
                }

                // Why it failed: an explanation, what it is about, pacman's
                // own lines, and the command that usually fixes it.
                ColumnLayout {
                    visible: Panel.failureTitle.length > 0
                    Layout.fillWidth: true
                    Layout.topMargin: Kirigami.Units.largeSpacing
                    spacing: Kirigami.Units.smallSpacing
                    Kirigami.Heading {
                        level: 3
                        text: Panel.failureTitle
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
                        wrapMode: Text.Wrap
                        Layout.fillWidth: true
                    }
                    QQC2.Label {
                        text: "pacman said:\n" + Panel.failureLines.slice(0, 6).join("\n")
                        font.family: Kirigami.Theme.fixedWidthFont.family
                        font.pointSize: Kirigami.Theme.smallFont.pointSize
                        color: Style.muted(Kirigami.Theme.textColor)
                        wrapMode: Text.Wrap
                        Layout.fillWidth: true
                    }
                    RowLayout {
                        visible: Panel.failureCommand.length > 0
                        Layout.fillWidth: true
                        Layout.topMargin: Kirigami.Units.smallSpacing
                        spacing: Kirigami.Units.largeSpacing
                        QQC2.Label {
                            text: "$ " + Panel.failureCommand
                            font.family: Kirigami.Theme.fixedWidthFont.family
                            font.pointSize: Kirigami.Theme.smallFont.pointSize
                            wrapMode: Text.Wrap
                            Layout.fillWidth: true
                        }
                        QQC2.Button {
                            text: "Copy"
                            icon.name: "edit-copy"
                            onClicked: Panel.copyText(Panel.failureCommand)
                            Layout.alignment: Qt.AlignTop
                        }
                    }
                }
            }

            // Pages: settings files, programs, what changed.
            Rectangle {
                visible: Panel.configs.length > 0 || Panel.processCount > 0 || Panel.changes.length > 0
                Kirigami.Theme.colorSet: Kirigami.Theme.View
                Kirigami.Theme.inherit: false
                color: Kirigami.Theme.backgroundColor
                border.width: 1
                border.color: Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.2)
                radius: Kirigami.Units.smallSpacing
                Layout.fillWidth: true
                Layout.leftMargin: Kirigami.Units.largeSpacing * 2
                Layout.rightMargin: Kirigami.Units.largeSpacing * 2
                Layout.bottomMargin: Kirigami.Units.largeSpacing * 2
                implicitHeight: links.implicitHeight + 2
                ColumnLayout {
                    id: links
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 1
                    spacing: 0
                    LinkRow {
                        visible: Panel.configs.length > 0
                        iconName: "document-edit"
                        title: Panel.configs.length === 1 ? "1 settings file has new defaults" : Panel.configs.length + " settings files have new defaults"
                        subtitle: "Optional: compare yours with the new version"
                        onClicked: screen.showConfigs()
                    }
                    LinkRow {
                        visible: Panel.processCount > 0
                        iconName: "preferences-system-windows"
                        title: Panel.processCount === 1 ? "1 program uses replaced libraries" : Panel.processCount + " programs use replaced libraries"
                        subtitle: "They run the old versions until they restart"
                        onClicked: screen.showProcesses()
                    }
                    LinkRow {
                        visible: Panel.changes.length > 0
                        iconName: "view-list-details"
                        title: "What changed"
                        subtitle: Panel.changes.length === 1 ? "1 package, with old and new version" : Panel.changes.length + " packages, with old and new versions"
                        onClicked: screen.showChanges()
                    }
                }
            }
        }
    }

    Kirigami.Separator { Layout.fillWidth: true }
    RowLayout {
        Layout.fillWidth: true
        Layout.margins: Kirigami.Units.largeSpacing * 2
        spacing: Kirigami.Units.largeSpacing
        QQC2.Button {
            flat: true
            text: "Show output"
            icon.name: "utilities-terminal"
            onClicked: screen.showOutput()
            QQC2.ToolTip.visible: hovered
            QQC2.ToolTip.text: "Show the output (L)"
        }
        Item { Layout.fillWidth: true }
        QQC2.Button {
            text: "Done"
            onClicked: Panel.done()
            QQC2.ToolTip.visible: hovered
            QQC2.ToolTip.text: "Back to the list (Esc)"
        }
    }
}
