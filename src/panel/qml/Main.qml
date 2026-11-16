// Kyu update panel. Layout only: state and actions live in PanelController,
// exposed as the context property "Panel". Colours, fonts, spacing and icons
// come from Kirigami.Theme / Kirigami.Units and the icon theme, never from
// literals (HANDOFF.md rule 6).
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Window
import org.kde.kirigami as Kirigami
import "Style.js" as Style

QQC2.ApplicationWindow {
    id: root

    // Sized from the font actually in use (the grid unit does not always
    // follow a larger font), never larger than the screen allows.
    FontMetrics { id: fontMetrics; font: Kirigami.Theme.defaultFont }
    readonly property real unit: Math.max(Kirigami.Units.gridUnit, Math.ceil(fontMetrics.height))
    // The terminal look is in the fixed-width font, at the normal size.
    readonly property bool terminal: Panel.style === "terminal"
    readonly property font terminalFont: Qt.font({ family: Kirigami.Theme.fixedWidthFont.family, pointSize: Kirigami.Theme.defaultFont.pointSize })
    FontMetrics { id: fixedMetrics; font: root.terminalFont }
    readonly property int availableWidth: Screen.desktopAvailableWidth > 0 ? Screen.desktopAvailableWidth : 100000
    readonly property int availableHeight: Screen.desktopAvailableHeight > 0 ? Screen.desktopAvailableHeight : 100000
    // Monospace needs more width for the same text: about 64 columns.
    width: Math.min(Math.max(unit * 24, terminal ? Math.ceil(fixedMetrics.averageCharacterWidth * 64) : 0), Math.round(availableWidth * 0.9))
    height: Math.min(unit * 36, Math.round(availableHeight * 0.9))
    // Tall enough for the header, a message, a few rows and the buttons.
    minimumWidth: Math.min(unit * 18, availableWidth)
    minimumHeight: Math.min(unit * 25, availableHeight)
    visible: true
    title: "Updates"
    color: Kirigami.Theme.backgroundColor
    // The terminal look sits on the colour scheme's View background, edge
    // to edge (set on items: Kirigami's colour sets follow items).
    background: Rectangle {
        Kirigami.Theme.colorSet: root.terminal ? Kirigami.Theme.View : Kirigami.Theme.Window
        Kirigami.Theme.inherit: false
        color: Kirigami.Theme.backgroundColor
    }

    // The page on top of the current screen ("" when none): keys depend on it.
    property string page: ""
    function push(component, name, properties) {
        stack.push(component, properties || {})
        page = name
    }
    function back() {
        stack.pop()
        page = stack.depth > 1 ? page : ""
    }
    function showOutput() {
        push(outputPage, "output")
    }
    function showChanges() {
        push(changesPage, "changes")
    }
    function showConfigs() {
        push(configsPage, "configs")
    }
    function showDiff(path, saved) {
        push(diffPage, "diff", { path: path, saved: saved })
    }
    function showProcesses() {
        push(processesPage, "processes")
    }

    // Keyboard. Every key here is named in the screen's hint line. Nothing
    // here restarts the computer or cancels an update: those stay buttons.
    readonly property bool onScreen: stack.depth === 1
    Shortcut {
        sequences: ["Return", "Enter"]
        // Enter on a focused button (Tab to "Check again") clicks that button.
        enabled: root.onScreen && Panel.screen === "list" && Panel.canUpdate
        onActivated: {
            const button = Style.pressable(root.activeFocusItem)
            if (button) {
                button.clicked()
            } else {
                Panel.updateAll()
            }
        }
    }
    Shortcut {
        sequence: "C"
        enabled: root.onScreen && Panel.screen === "list" && !Panel.checking
        onActivated: Panel.checkAgain()
    }
    Shortcut {
        sequence: "L"
        enabled: root.onScreen && Panel.screen !== "list"
        onActivated: root.showOutput()
    }
    Shortcut {
        sequence: "S"
        enabled: root.onScreen && Panel.screen === "finished" && Panel.configs.length > 0
        onActivated: root.showConfigs()
    }
    Shortcut {
        sequence: "P"
        enabled: root.onScreen && Panel.screen === "finished" && Panel.processCount > 0
        onActivated: root.showProcesses()
    }
    Shortcut {
        sequence: "W"
        enabled: root.onScreen && Panel.screen === "finished" && Panel.changes.length > 0
        onActivated: root.showChanges()
    }
    Shortcut {
        sequence: "Y"
        enabled: root.page === "output"
        onActivated: Panel.copyLog()
    }
    Shortcut {
        // Back from a page; "done" on the result; otherwise close the window.
        // Closing never stops an update or answers a question.
        sequence: "Escape"
        onActivated: {
            if (stack.depth > 1) {
                root.back()
            } else if (Panel.screen === "finished") {
                Panel.done()
            } else {
                root.close()
            }
        }
    }

    Connections {
        target: Panel
        function onActivateRequested() {
            root.show()
            root.raise()
            root.requestActivate()
        }
    }

    QQC2.StackView {
        id: stack
        // On a wide window, a readable column in the middle rather than a
        // name at one edge and its version at the other.
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        width: root.terminal ? parent.width : Math.min(parent.width, root.unit * 44)
        // Pages inherit the look's font and, in the terminal look, the View
        // colours.
        font: root.terminal ? root.terminalFont : Kirigami.Theme.defaultFont
        Kirigami.Theme.colorSet: root.terminal ? Kirigami.Theme.View : Kirigami.Theme.Window
        Kirigami.Theme.inherit: false
        // The screens come in two looks (Panel.style): plasma/ and terminal/,
        // the same names and signals in each.
        initialItem: Item {
            Loader {
                id: screenLoader
                anchors.fill: parent
                source: Qt.resolvedUrl(Panel.style + "/"
                                       + (Panel.screen === "running" ? "RunningScreen"
                                        : Panel.screen === "question" ? "QuestionScreen"
                                        : Panel.screen === "finished" ? "FinishedScreen"
                                        : "ListScreen") + ".qml")
            }
            Connections {
                target: screenLoader.item
                ignoreUnknownSignals: true
                function onShowOutput() { root.showOutput() }
                function onShowChanges() { root.showChanges() }
                function onShowConfigs() { root.showConfigs() }
                function onShowProcesses() { root.showProcesses() }
            }
        }
    }

    Component { id: outputPage; OutputPage { onBack: root.back() } }
    Component { id: changesPage; ChangesPage { onBack: root.back() } }
    Component {
        id: configsPage
        ConfigsPage {
            onBack: root.back()
            onShowDiff: (path, saved) => root.showDiff(path, saved)
        }
    }
    Component { id: diffPage; DiffPage { onBack: root.back() } }
    Component { id: processesPage; ProcessesPage { onBack: root.back() } }
}
