// Preview stand-in for Kirigami.Heading (see preview.h).
import QtQuick
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami

QQC2.Label {
    property int level: 1
    font.pointSize: Qt.application.font.pointSize * [1.35, 1.35, 1.2, 1.15, 1.1, 1][Math.max(0, Math.min(5, level))]
    wrapMode: Text.NoWrap
    color: Kirigami.Theme.textColor
}
