// Terminal style: progress as a bar of block characters and a percentage,
// as wide as the line.
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

RowLayout {
    id: progress
    property int percent: 0
    spacing: Kirigami.Units.largeSpacing
    Accessible.role: Accessible.ProgressBar
    Accessible.name: "Update progress " + percent + "%"
    TextMetrics { id: cell; text: "█"; font: blocks.font }
    QQC2.Label {
        id: blocks
        readonly property int cells: Math.max(4, Math.floor(width / Math.max(1, cell.advanceWidth)))
        readonly property int filled: Math.round(cells * Math.max(0, Math.min(100, progress.percent)) / 100)
        text: "█".repeat(filled)
        color: Kirigami.Theme.linkColor
        Layout.fillWidth: true
        clip: true
        QQC2.Label {
            x: blocks.filled * cell.advanceWidth
            text: "░".repeat(Math.max(0, blocks.cells - blocks.filled))
            font: blocks.font
            color: Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.25)
        }
    }
    QQC2.Label {
        text: progress.percent + "%"
        font.weight: Font.Bold
    }
}
