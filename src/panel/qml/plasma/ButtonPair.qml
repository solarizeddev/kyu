// Plasma style: a secondary and a primary button, the primary on the right.
// When the window is too narrow for both, the secondary moves under it; a
// primary wider than the window shortens its text ("…", full text on hover).
// A Flow, not a layout that switches columns by width: that feeds back into
// its own width.
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import ".."

Flow {
    id: pair
    property string primaryText
    property string secondaryText
    property bool primaryEnabled: true
    property bool secondaryEnabled: true
    property bool primaryVisible: true
    signal primaryClicked()
    signal secondaryClicked()

    layoutDirection: Qt.RightToLeft
    spacing: Kirigami.Units.largeSpacing
    Layout.fillWidth: true

    PrimaryButton {
        visible: pair.primaryVisible
        text: pair.primaryText
        enabled: pair.primaryEnabled
        onClicked: pair.primaryClicked()
        width: Math.min(implicitWidth, pair.width)
    }
    QQC2.Button {
        text: pair.secondaryText
        enabled: pair.secondaryEnabled
        onClicked: pair.secondaryClicked()
        width: Math.min(implicitWidth, pair.width)
    }
}
