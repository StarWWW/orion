// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Orion contributors. See LICENSE.txt.
pragma Singleton
import QtQuick

// Night-sky palette: deep navy, starlight cyan and nebula violet.
QtObject {
    readonly property color bg: "#060a16"
    readonly property color bgTop: "#0b1330"
    readonly property color panel: "#0f1730"
    readonly property color panelHi: "#152043"
    readonly property color field: "#0a1124"
    readonly property color border: "#22305c"
    readonly property color borderHi: "#3a4c86"

    readonly property color text: "#e8edff"
    readonly property color textDim: "#9aa6d1"
    readonly property color textFaint: "#5d6a98"

    readonly property color accent: "#4cd6ff"
    readonly property color accent2: "#8f6bff"
    readonly property color good: "#3ee0a1"
    readonly property color warn: "#ffc857"
    readonly property color bad: "#ff6b81"
    readonly property color gold: "#ffd479"

    readonly property string mono: Qt.platform.os === "windows" ? "Consolas" : "monospace"
    readonly property int radius: 16
    readonly property int gap: 16

    // Duration of the standard transitions (ms)
    readonly property int fast: 160
    readonly property int normal: 320
    readonly property int slow: 700

    function stateColor(s) {
        switch (s) {
        case "running": return good
        case "starting": case "resuming": case "pausing": case "stopping": return accent
        case "paused": return warn
        case "error": return bad
        case "allfound": return gold
        default: return textDim
        }
    }

    function stateText(s) {
        switch (s) {
        case "idle": return "Ready"
        case "starting": return "Starting"
        case "running": return "Searching"
        case "pausing": return "Pausing"
        case "paused": return "Paused"
        case "resuming": return "Resuming"
        case "stopping": return "Saving and stopping"
        case "finished": return "Range finished"
        case "allfound": return "All targets found"
        case "stopped": return "Stopped"
        case "error": return "Error"
        default: return s
        }
    }
}
