// One muted line naming the keys that work on this screen.
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import "Style.js" as Style

QQC2.Label {
    Layout.fillWidth: true
    font.family: Kirigami.Theme.fixedWidthFont.family
    font.pointSize: Kirigami.Theme.smallFont.pointSize
    color: Style.muted(Kirigami.Theme.textColor)
    wrapMode: Text.Wrap
}
