// =============================================================================
// SimulatorMenuBar.qml — Menu bar for the simulator main window.
//
// Implements the menu structure from build/simulator.md §五.1:
//   File  |  Device  |  OS  |  Test  |  Capture  |  View  |  Help
// =============================================================================
import QtQuick
import QtQuick.Controls

// Wraps ApplicationWindow.menuBar in a declarative component.
// Instantiate inside an ApplicationWindow:
//   menuBar: SimulatorMenuBar { ... }
MenuBar {
    id: root

    // ── Signals (routed to parent ApplicationWindow) ────────────────────
    signal fileNewSession()
    signal fileOpenProfile()
    signal fileSaveProfile()
    signal fileExportResult()
    signal fileExit()

    signal deviceSelectRequested()
    signal deviceOrientationToggle()
    signal deviceFitToWindow()
    signal deviceActualSize()

    signal osSelectRequested()
    signal osReloadConfig()
    signal osViewPolicy()

    signal testRunSelected()
    signal testRunSuite()
    signal testRunFullMatrix()
    signal testStop()
    signal testClearResults()

    signal captureScreenshot()
    signal captureStartRecording()
    signal captureStopRecording()
    signal captureOpenEvidence()

    signal viewToggleLog()
    signal viewToggleParameters()
    signal viewToggleResults()
    signal viewZoomIn()
    signal viewZoomOut()
    signal viewFitDevice()

    Menu {
        title: "&File"
        Action { text: "&New Test Session";    onTriggered: root.fileNewSession() }
        Action { text: "&Open Test Profile…";  onTriggered: root.fileOpenProfile() }
        Action { text: "&Save Test Profile…";  onTriggered: root.fileSaveProfile() }
        MenuSeparator {}
        Action { text: "&Export Test Result…"; onTriggered: root.fileExportResult() }
        MenuSeparator {}
        Action { text: "E&xit";                onTriggered: root.fileExit() }
    }

    Menu {
        title: "&Device"
        Action { text: "&Select Device…";         onTriggered: root.deviceSelectRequested() }
        Action { text: "Toggle &Orientation";     onTriggered: root.deviceOrientationToggle()
                 shortcut: "Ctrl+R" }
        Action { text: "&Fit to Window";          onTriggered: root.deviceFitToWindow()
                 shortcut: "Ctrl+0" }
        Action { text: "&Actual Size";           onTriggered: root.deviceActualSize()
                 shortcut: "Ctrl+1" }
        MenuSeparator {}
        Action { text: "&Reset Device View";     onTriggered: root.deviceFitToWindow() }
    }

    Menu {
        title: "&OS"
        Action { text: "&Select OS Profile…";    onTriggered: root.osSelectRequested() }
        Action { text: "&Reload OS Configuration";onTriggered: root.osReloadConfig() }
        Action { text: "&View OS Policy…";       onTriggered: root.osViewPolicy() }
    }

    Menu {
        title: "&Test"
        Action { text: "&Run Selected Test";     onTriggered: root.testRunSelected()
                 shortcut: "F5" }
        Action { text: "Run Test &Suite…";       onTriggered: root.testRunSuite()
                 shortcut: "Ctrl+F5" }
        Action { text: "Run &Full Matrix Test…"; onTriggered: root.testRunFullMatrix()
                 shortcut: "Ctrl+Shift+F5" }
        MenuSeparator {}
        Action { text: "&Stop Test";            onTriggered: root.testStop()
                 shortcut: "Shift+F5" }
        Action { text: "&Clear Results";         onTriggered: root.testClearResults() }
    }

    Menu {
        title: "&Capture"
        Action { text: "Take &Screenshot";      onTriggered: root.captureScreenshot()
                 shortcut: "F12" }
        Action { text: "Start &Recording";       onTriggered: root.captureStartRecording()
                 shortcut: "Ctrl+F12" }
        Action { text: "Stop Re&cording";        onTriggered: root.captureStopRecording()
                 shortcut: "Ctrl+Shift+F12" }
        MenuSeparator {}
        Action { text: "Open &Evidence Folder…"; onTriggered: root.captureOpenEvidence() }
        MenuSeparator {}
        Action { text: "Capture &Settings…";      onTriggered: { /* TODO: capture settings dialog */ } }
    }

    Menu {
        title: "&View"
        Action { text: "Toggle &Log Panel";     onTriggered: root.viewToggleLog()
                 checkable: true; checked: true }
        Action { text: "Toggle &Parameter Panel";onTriggered: root.viewToggleParameters()
                 checkable: true; checked: true }
        Action { text: "Toggle &Result Panel";   onTriggered: root.viewToggleResults()
                 checkable: true; checked: true }
        MenuSeparator {}
        Action { text: "Zoom &In";              onTriggered: root.viewZoomIn()
                 shortcut: "Ctrl++" }
        Action { text: "Zoom &Out";             onTriggered: root.viewZoomOut()
                 shortcut: "Ctrl+-" }
        Action { text: "&Fit Device to Window";  onTriggered: root.viewFitDevice()
                 shortcut: "Ctrl+Shift+0" }
    }

    Menu {
        title: "&Help"
        Action { text: "&About Simulator";  onTriggered: { /* TODO: about dialog */ } }
        Action { text: "&Documentation";    onTriggered: { /* TODO: open docs */ } }
    }
}
