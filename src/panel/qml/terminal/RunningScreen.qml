// Terminal style, while updating: a block progress bar, the packages ticking
// off, and the live output filling the rest.
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

    TermBar {
        text: "updating · " + Panel.progressText
    }

    ColumnLayout {
        Layout.fillWidth: true
        Layout.margins: Kirigami.Units.gridUnit
        Layout.bottomMargin: Kirigami.Units.largeSpacing
        spacing: Kirigami.Units.largeSpacing
        TextProgress {
            percent: Panel.percent
            Layout.fillWidth: true
        }
        Box { title: "transaction"; text: Panel.transactionNote; type: "information" }
        Box {
            title: "stopped"
            type: "warning"
            text: Panel.question.length > 0
                  ? "pacman asked: “" + Panel.question + "”. Kyu does not recognise this question, so it stopped pacman before any package changed."
                  : ""
        }
    }

    PackageList {
        tracking: true
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.preferredHeight: 2
    }
    Rectangle {
        Layout.fillWidth: true
        implicitHeight: 1
        color: Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.2)
    }
    LogTail {
        lines: 8
        color: "transparent"
    }

    KeyBar {
        KeyButton { key: "l"; text: "full output"; onClicked: screen.showOutput() }
        KeyButton { key: "esc"; text: "hide · keeps running"; onClicked: screen.Window.window.close() }
    }
}
