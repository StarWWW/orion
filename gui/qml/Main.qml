// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Orion contributors. See LICENSE.txt.
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Orion

ApplicationWindow {
    id: window

    // Set from the command line (see main.cpp)
    property string startPage: ""
    property bool autoStart: false

    property bool quitting: false

    width: 1360
    height: 900
    minimumWidth: 1100
    minimumHeight: 720
    visible: true
    title: "Orion"
    color: Theme.bg

    // Dark palette for the stock parts of the controls (popups, tooltips, scroll bars)
    palette.window: Theme.panel
    palette.windowText: Theme.text
    palette.base: Theme.field
    palette.alternateBase: Theme.panelHi
    palette.text: Theme.text
    palette.button: Theme.panelHi
    palette.buttonText: Theme.text
    palette.highlight: Theme.accent2
    palette.highlightedText: "white"
    palette.toolTipBase: Theme.panelHi
    palette.toolTipText: Theme.text
    palette.mid: Theme.border
    palette.dark: Theme.borderHi
    palette.light: Theme.panelHi
    palette.placeholderText: Theme.textFaint

    Search {
        id: search
        onKeyFound: function (key) {
            foundPage.newThisSession++
            foundPage.refresh()
            overlay.show(key.address)
        }
        onLogLine: function (text, error) {
            logModel.append({ line: text, error: error })
            if (logModel.count > 500)
                logModel.remove(0, logModel.count - 500)
        }
        onEnded: function (status) {
            if (window.quitting) {
                Qt.quit()
                return
            }
            if (status === "finished") toast.show("Range finished", "good")
            else if (status === "allfound") toast.show("Every target was found", "good")
            else if (status === "stopped") toast.show(search.backups ? "Stopped, progress saved" : "Stopped", "info")
            else if (status === "error") toast.show("The search ended with an error", "error")
        }
    }

    ListModel { id: logModel }

    function startSearch(cfg) {
        cfg.engine = Engine.executable
        cfg.dataDir = Engine.dataDir
        logModel.clear()
        if (search.start(cfg))
            nav.currentIndex = 1
        else
            toast.show(search.errorText, "error")
    }

    onClosing: function (close) {
        if (search.active) {
            // Stop like Ctrl+C in a terminal: the engine saves the progress, then the window closes
            close.accepted = false
            quitting = true
            search.stop()
            saving.visible = true
        }
    }

    StarField {
        anchors.fill: parent
        live: search.state === "running"
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        NavRail {
            id: nav
            Layout.fillHeight: true
            foundCount: foundPage.newThisSession
            searchState: search.state
            engineVersion: Engine.version
            currentIndex: {
                var i = ["search", "monitor", "found", "learn"].indexOf(window.startPage)
                return i >= 0 ? i : 0
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            // Pages cross-fade and slide up slightly when selected
            component PageHost: Item {
                id: host
                property int index
                readonly property bool current: nav.currentIndex === index
                anchors.fill: parent
                opacity: current ? 1 : 0
                visible: opacity > 0
                enabled: current
                Behavior on opacity { NumberAnimation { duration: 260; easing.type: Easing.OutCubic } }
                transform: Translate {
                    y: host.current ? 0 : 14
                    Behavior on y { NumberAnimation { duration: 320; easing.type: Easing.OutCubic } }
                }
            }

            PageHost {
                index: 0
                SetupPage {
                    id: setupPage
                    anchors.fill: parent
                    search: search
                    onStartRequested: function (cfg) { window.startSearch(cfg) }
                    onMessage: function (text, kind) { toast.show(text, kind) }
                }
            }
            PageHost {
                index: 1
                MonitorPage {
                    anchors.fill: parent
                    search: search
                    logModel: logModel
                    onSetupRequested: nav.currentIndex = 0
                }
            }
            PageHost {
                index: 2
                FoundPage {
                    id: foundPage
                    anchors.fill: parent
                    onMessage: function (text, kind) { toast.show(text, kind) }
                }
            }
            PageHost {
                index: 3
                LearnPage {
                    anchors.fill: parent
                    speed: Math.max(search.peakSpeed, setupPage.lastSpeed)
                }
            }
        }
    }

    FoundOverlay {
        id: overlay
        anchors.fill: parent
        onViewRequested: nav.currentIndex = 2
    }

    // Shown while the engine saves its progress before the window closes
    Rectangle {
        id: saving
        anchors.fill: parent
        visible: false
        color: Qt.rgba(0.01, 0.02, 0.06, 0.82)
        MouseArea { anchors.fill: parent }
        Column {
            anchors.centerIn: parent
            spacing: 14
            BusyIndicator { anchors.horizontalCenter: parent.horizontalCenter; running: saving.visible }
            Text {
                text: "Saving progress and stopping the engine…"
                color: Theme.text
                font.pixelSize: 16
            }
        }
    }

    Toast { id: toast }

    // --start: once the GPUs are listed
    property bool pendingAutoStart: autoStart
    function tryAutoStart() {
        if (pendingAutoStart && !Engine.listingGpus) {
            pendingAutoStart = false
            setupPage.startNow(false)
        }
    }
    Connections {
        target: Engine
        function onGpusChanged() { window.tryAutoStart() }
    }
    Component.onCompleted: {
        if (startPage === "puzzles")
            setupPage.openPuzzles()
        tryAutoStart()
    }
}
