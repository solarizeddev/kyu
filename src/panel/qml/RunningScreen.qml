// A transaction is running: percentage, thin bar, the list ticking off.
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

ColumnLayout {
    signal showOutput()
    spacing: Kirigami.Units.largeSpacing * 2

    ColumnLayout {
        Layout.fillWidth: true
        Layout.topMargin: Kirigami.Units.gridUnit
        Layout.leftMargin: Kirigami.Units.gridUnit
        Layout.rightMargin: Kirigami.Units.gridUnit
        spacing: Kirigami.Units.largeSpacing
        RowLayout {
            Layout.fillWidth: true
            Kirigami.Heading {
                level: 1
                font.pointSize: Kirigami.Theme.defaultFont.pointSize * 2.6
                font.weight: Font.DemiBold
                text: Panel.percent + " %"
            }
            Item { Layout.fillWidth: true }
            QQC2.Label {
                text: Panel.progressText
                font.family: Kirigami.Theme.fixedWidthFont.family
                font.pointSize: Kirigami.Theme.smallFont.pointSize
                color: Kirigami.Theme.disabledTextColor
                Layout.alignment: Qt.AlignBottom
            }
        }
        ThinBar { value: Panel.percent / 100 }
    }

    Message {
        text: Panel.question.length > 0
              ? "pacman asked: “" + Panel.question + "”. Kyu cannot answer questions yet, so it stopped pacman before any package changed."
              : ""
        Layout.leftMargin: Kirigami.Units.gridUnit
        Layout.rightMargin: Kirigami.Units.gridUnit
    }

    PackageList {
        tracking: true
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.leftMargin: Kirigami.Units.gridUnit
        Layout.rightMargin: Kirigami.Units.gridUnit
    }

    QQC2.Label {
        text: "Keep working. Closing this panel will not stop the update."
        wrapMode: Text.Wrap
        color: Kirigami.Theme.disabledTextColor
        Layout.fillWidth: true
        Layout.leftMargin: Kirigami.Units.gridUnit
        Layout.rightMargin: Kirigami.Units.gridUnit
    }

    QQC2.Button {
        text: "Show pacman output"
        icon.name: "utilities-terminal"
        onClicked: showOutput()
        Layout.fillWidth: true
        Layout.preferredHeight: Kirigami.Units.gridUnit * 2.25
        Layout.leftMargin: Kirigami.Units.gridUnit
        Layout.rightMargin: Kirigami.Units.gridUnit
        Layout.bottomMargin: Kirigami.Units.gridUnit
    }
}
