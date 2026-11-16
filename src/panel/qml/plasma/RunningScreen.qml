// Plasma style, while updating: progress in the header, each package ticking
// off, the output folded into one line that opens all of it.
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import ".."
import "../Style.js" as Style

ColumnLayout {
    id: screen
    signal showOutput()
    spacing: 0
    readonly property var lastLine: Panel.output.count >= 0 ? Panel.output.tail(1) : []

    ScreenHeader {
        busy: true
        title: "Updating"
        subtitle: Panel.progressText
        QQC2.Label {
            text: Panel.percent + "%"
            font.pointSize: Kirigami.Theme.defaultFont.pointSize * 1.4
            font.weight: Font.DemiBold
            color: Style.muted(Kirigami.Theme.textColor)
        }
    }
    QQC2.ProgressBar {
        from: 0
        to: 100
        value: Panel.percent
        Layout.fillWidth: true
        Layout.leftMargin: Kirigami.Units.gridUnit
        Layout.rightMargin: Kirigami.Units.gridUnit
        Layout.topMargin: Kirigami.Units.largeSpacing * 2
        Accessible.name: "Update progress"
    }

    ColumnLayout {
        visible: Panel.transactionNote.length > 0 || Panel.question.length > 0
        Layout.fillWidth: true
        Layout.margins: Kirigami.Units.largeSpacing * 2
        Layout.bottomMargin: 0
        spacing: Kirigami.Units.largeSpacing
        Notice { text: Panel.transactionNote; type: "information"; collapsible: false }
        Notice {
            type: "warning"
            text: Panel.question.length > 0
                  ? "pacman asked: “" + Panel.question + "”. Kyu does not recognise this question, so it stopped pacman before any package changed."
                  : ""
        }
    }

    Kirigami.Separator { Layout.fillWidth: true; Layout.topMargin: Kirigami.Units.largeSpacing * 2 }
    PackageList {
        tracking: true
        Layout.fillWidth: true
        Layout.fillHeight: true
    }
    Kirigami.Separator { Layout.fillWidth: true }

    // The output, folded: its last line; click (or L) for all of it.
    QQC2.AbstractButton {
        id: outputRow
        Layout.fillWidth: true
        leftPadding: Kirigami.Units.gridUnit
        rightPadding: Kirigami.Units.gridUnit
        topPadding: Kirigami.Units.largeSpacing * 1.5
        bottomPadding: Kirigami.Units.largeSpacing * 1.5
        onClicked: screen.showOutput()
        background: Rectangle {
            color: outputRow.hovered || outputRow.visualFocus ? Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.07) : "transparent"
        }
        Accessible.name: "Show the output"
        QQC2.ToolTip.visible: hovered
        QQC2.ToolTip.text: "Show the output (L)"
        contentItem: RowLayout {
            spacing: Kirigami.Units.largeSpacing
            QQC2.Label { text: "Output" }
            QQC2.Label {
                text: screen.lastLine.length > 0 ? screen.lastLine[0].line : ""
                font.family: Kirigami.Theme.fixedWidthFont.family
                font.pointSize: Kirigami.Theme.smallFont.pointSize
                color: Style.muted(Kirigami.Theme.textColor)
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
            Kirigami.Icon {
                source: "go-next"
                Layout.preferredWidth: Kirigami.Units.iconSizes.small
                Layout.preferredHeight: Kirigami.Units.iconSizes.small
            }
        }
    }
    Kirigami.Separator { Layout.fillWidth: true }

    QQC2.Label {
        text: "You can close this window. The update keeps running."
        font: Kirigami.Theme.smallFont
        color: Style.muted(Kirigami.Theme.textColor)
        wrapMode: Text.Wrap
        Layout.fillWidth: true
        Layout.margins: Kirigami.Units.largeSpacing * 2
        Layout.leftMargin: Kirigami.Units.gridUnit
    }
}
