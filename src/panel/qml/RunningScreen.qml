// A transaction is running: percentage, thin bar, the list ticking off, the
// last lines of the output.
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import "Style.js" as Style

ColumnLayout {
    signal showOutput()
    spacing: 0

    ColumnLayout {
        Layout.fillWidth: true
        Layout.margins: Kirigami.Units.gridUnit
        spacing: Kirigami.Units.smallSpacing
        Kirigami.Heading {
            level: 1
            font.pointSize: Kirigami.Theme.defaultFont.pointSize * 1.6
            font.weight: Font.DemiBold
            text: "Updating · " + Panel.percent + "%"
            Layout.fillWidth: true
        }
        QQC2.Label {
            text: Panel.progressText
            font.family: Kirigami.Theme.fixedWidthFont.family
            font.pointSize: Kirigami.Theme.smallFont.pointSize
            color: Style.muted(Kirigami.Theme.textColor)
            elide: Text.ElideRight
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
        ThinBar {
            value: Panel.percent / 100
            Layout.topMargin: Kirigami.Units.smallSpacing
        }
    }

    Message {
        text: Panel.question.length > 0
              ? "pacman asked: “" + Panel.question + "”. Kyu does not recognise this question, so it stopped pacman before any package changed."
              : ""
        Layout.leftMargin: Kirigami.Units.gridUnit
        Layout.rightMargin: Kirigami.Units.gridUnit
        Layout.bottomMargin: Kirigami.Units.largeSpacing
    }

    Hairline {}
    PackageList {
        tracking: true
        Layout.fillWidth: true
        Layout.fillHeight: true
    }
    Hairline {}
    LogTail { lines: 5 }
    Hairline {}

    ColumnLayout {
        Layout.fillWidth: true
        Layout.margins: Kirigami.Units.gridUnit
        Layout.topMargin: Kirigami.Units.largeSpacing * 1.5
        spacing: Kirigami.Units.largeSpacing
        QQC2.Label {
            text: "Closing this window does not stop the update."
            wrapMode: Text.Wrap
            color: Style.muted(Kirigami.Theme.textColor)
            Layout.fillWidth: true
        }
        QQC2.Button {
            text: "Full output"
            icon.name: "utilities-terminal"
            onClicked: showOutput()
            Layout.fillWidth: true
        }
        KeyHints { text: "l output · esc close (the update keeps running)" }
    }
}
