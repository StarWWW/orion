// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Orion contributors. See LICENSE.txt.
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Orion
import "Fmt.js" as Fmt

// Live view of the running search.
Flickable {
    id: root

    required property Search search
    required property ListModel logModel
    signal setupRequested()

    readonly property bool live: search.state === "running"
    readonly property bool hasRun: search.state !== "idle"
    // Chance to hit one given target per unit of time at the current speed (uniform keys)
    readonly property real perSecond: search.range > 0 ? search.speed * 1e6 / search.range : 0

    contentWidth: width
    contentHeight: layout.implicitHeight + 64
    clip: true
    boundsBehavior: Flickable.StopAtBounds
    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

    component Detail: RowLayout {
        property string label
        property string value
        property bool mono: false
        Layout.fillWidth: true
        spacing: 12
        Text { text: parent.label; color: Theme.textFaint; font.pixelSize: 12; Layout.preferredWidth: 110 }
        Text {
            text: parent.value
            color: Theme.text
            font.pixelSize: 12
            font.family: parent.mono ? Theme.mono : Qt.application.font.family
            Layout.fillWidth: true
            elide: Text.ElideMiddle
            textFormat: Text.RichText
        }
    }

    // Nothing has run yet
    ColumnLayout {
        visible: !root.hasRun
        anchors.centerIn: parent
        width: Math.min(520, root.width - 80)
        spacing: 16
        Icon { name: "monitor"; size: 64; color: Theme.textFaint; Layout.alignment: Qt.AlignHCenter }
        Text {
            text: "No search running"
            color: Theme.text; font.pixelSize: 22; font.weight: Font.DemiBold
            Layout.alignment: Qt.AlignHCenter
        }
        Text {
            text: "Set up a search and its speed, progress and odds appear here live."
            color: Theme.textDim; font.pixelSize: 13
            wrapMode: Text.WordWrap; horizontalAlignment: Text.AlignHCenter
            Layout.fillWidth: true
        }
        IconButton {
            text: "Set up a search"; iconName: "setup"; kind: "primary"
            Layout.alignment: Qt.AlignHCenter
            onClicked: root.setupRequested()
        }
    }

    ColumnLayout {
        id: layout
        visible: root.hasRun
        x: 32
        y: 28
        width: root.width - 64
        spacing: Theme.gap

        // Header with state and controls
        RowLayout {
            Layout.fillWidth: true
            spacing: 14
            ColumnLayout {
                spacing: 4
                Layout.fillWidth: true
                RowLayout {
                    spacing: 12
                    Text { text: "Monitor"; color: Theme.text; font.pixelSize: 28; font.weight: Font.Bold }
                    Rectangle {
                        height: 28
                        width: badgeRow.implicitWidth + 24
                        radius: 14
                        color: Qt.rgba(Theme.stateColor(root.search.state).r, Theme.stateColor(root.search.state).g,
                                       Theme.stateColor(root.search.state).b, 0.14)
                        border.color: Theme.stateColor(root.search.state)
                        Behavior on color { ColorAnimation { duration: Theme.normal } }
                        Row {
                            id: badgeRow
                            anchors.centerIn: parent
                            spacing: 8
                            Rectangle {
                                id: dot
                                width: 8; height: 8; radius: 4
                                anchors.verticalCenter: parent.verticalCenter
                                color: Theme.stateColor(root.search.state)
                                SequentialAnimation on opacity {
                                    running: root.live
                                    loops: Animation.Infinite
                                    onStopped: dot.opacity = 1
                                    NumberAnimation { to: 0.25; duration: 650 }
                                    NumberAnimation { to: 1.0; duration: 650 }
                                }
                            }
                            Text {
                                text: Theme.stateText(root.search.state) + (root.search.random ? " · random" : " · sequential")
                                color: Theme.text
                                font.pixelSize: 12
                                font.weight: Font.DemiBold
                            }
                        }
                    }
                }
                Text {
                    text: root.search.gpuName !== "" ? root.search.gpuName : "Starting the engine…"
                    color: Theme.textDim; font.pixelSize: 12
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                }
            }
            IconButton {
                visible: root.search.active
                enabled: root.search.state === "running" || root.search.state === "paused"
                text: root.search.state === "paused" ? "Resume" : "Pause"
                iconName: root.search.state === "paused" ? "play" : "pause"
                ToolTip.visible: hovered
                ToolTip.text: root.search.state === "paused" ? "Continue the search"
                                                             : "Release the GPU for other programs; progress is kept"
                onClicked: root.search.state === "paused" ? root.search.resume() : root.search.pause()
            }
            IconButton {
                visible: root.search.active
                enabled: root.search.state !== "stopping"
                text: "Stop"
                iconName: "stop"
                kind: "danger"
                ToolTip.visible: hovered
                ToolTip.text: root.search.backups ? "Save the progress and stop" : "Stop the search"
                onClicked: root.search.stop()
            }
            IconButton {
                visible: !root.search.active
                text: "New search"
                iconName: "setup"
                kind: "primary"
                onClicked: root.setupRequested()
            }
        }

        // Error banner
        Card {
            visible: root.search.state === "error"
            Layout.fillWidth: true
            highlight: true
            accentColor: Theme.bad
            title: "The search ended with an error"
            Text {
                text: root.search.errorText
                color: Theme.bad
                font.family: Theme.mono
                font.pixelSize: 12
                wrapMode: Text.WrapAnywhere
                Layout.fillWidth: true
            }
        }

        // Gauge + tiles
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.gap

            Card {
                Layout.preferredWidth: 380
                Layout.fillHeight: true
                title: "Speed"
                highlight: root.live
                SpeedGauge {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 270
                    value: root.search.speed
                    peak: root.search.peakSpeed
                    live: root.live
                }
            }

            GridLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                columns: 2
                rowSpacing: Theme.gap
                columnSpacing: Theme.gap

                StatTile {
                    Layout.fillWidth: true; Layout.fillHeight: true
                    label: "Keys checked"
                    iconName: "check"
                    value: Fmt.keys(root.search.keysDone)
                    unit: "keys"
                    detail: Fmt.pow2(root.search.keysDone) + " · "
                            + Fmt.big(root.search.keysDone)
                }
                StatTile {
                    Layout.fillWidth: true; Layout.fillHeight: true
                    label: "Run time"
                    iconName: "monitor"
                    tint: Theme.accent2
                    value: Fmt.duration(root.search.elapsed)
                    detail: root.search.backups ? "includes earlier runs of this range" : "this run"
                }
                StatTile {
                    Layout.fillWidth: true; Layout.fillHeight: true
                    label: root.search.random ? "Chance per day" : "Time left"
                    iconName: root.search.random ? "dice" : "arrow"
                    tint: Theme.warn
                    value: root.search.random ? Fmt.chance(root.perSecond * 86400) : Fmt.duration(root.search.eta)
                    detail: root.search.random ? "for one target at the current speed" : "to scan the whole range"
                }
                StatTile {
                    Layout.fillWidth: true; Layout.fillHeight: true
                    label: "Found"
                    iconName: "key"
                    tint: root.search.found > 0 ? Theme.gold : Theme.good
                    value: root.search.found
                    detail: root.search.targets + (root.search.targets === 1 ? " target" : " targets") + " · saved to found.txt"
                }
            }
        }

        Card {
            Layout.fillWidth: true
            title: "Speed history"
            subtitle: root.search.peakSpeed > 0
                      ? "peak " + Fmt.speed(root.search.peakSpeed).value + " " + Fmt.speed(root.search.peakSpeed).unit
                        + " · one reading per kernel launch of " + Fmt.big(root.search.keysPerLaunch) + " keys"
                      : ""
            SpeedChart {
                Layout.fillWidth: true
                Layout.preferredHeight: 170
                samples: root.search.speedHistory
                live: root.live
            }
        }

        Card {
            Layout.fillWidth: true
            title: "Key range"
            subtitle: root.search.random
                      ? "Random mode: each launch tests " + Fmt.big(root.search.keysPerLaunch) + " keys at new random offsets."
                      : "Sequential mode: every GPU thread walks its own slice of the range."
            KeyspaceBar {
                Layout.fillWidth: true
                startHex: root.search.startHex
                endHex: root.search.endHex
                percent: root.search.percent
                random: root.search.random
                live: root.live
                keysDone: root.search.keysDone
                range: root.search.range
            }
            Detail { label: "First key"; value: "0x" + root.search.startHex; mono: true }
            Detail { label: "Last key"; value: "0x" + root.search.endHex; mono: true }
            Detail {
                label: "Size"
                value: root.search.range > 0
                       ? Fmt.pow2(root.search.range) + " = " + Fmt.big(root.search.range) + " keys"
                       : ""
            }
            Detail {
                label: "GPU threads"
                value: root.search.threads > 0 ? Fmt.group(root.search.threads) + " · " + Fmt.big(root.search.keysPerLaunch) + " keys per launch" : ""
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.gap

            Card {
                Layout.fillWidth: true
                Layout.preferredWidth: 2
                Layout.fillHeight: true
                title: "Odds at this speed"
                subtitle: "For one target in this range. Every key is an independent draw: the chance does not grow while you wait, it only adds up."
                Repeater {
                    model: [["per hour", 3600], ["per day", 86400], ["per month", 30.44 * 86400], ["per year", 365.25 * 86400]]
                    delegate: RowLayout {
                        required property var modelData
                        Layout.fillWidth: true
                        spacing: 12
                        Text { text: parent.modelData[0]; color: Theme.textDim; font.pixelSize: 13; Layout.preferredWidth: 90 }
                        Rectangle {
                            Layout.fillWidth: true
                            height: 6
                            radius: 3
                            color: Theme.field
                            Rectangle {
                                readonly property real p: Math.min(1, root.perSecond * parent.parent.modelData[1])
                                // Log scale from 1e-12 to 1 so tiny chances are still visible
                                width: p > 0 ? Math.max(3, parent.width * Math.max(0, 1 + Math.log(p) / Math.LN10 / 12)) : 0
                                height: parent.height
                                radius: 3
                                color: Theme.warn
                                Behavior on width { NumberAnimation { duration: Theme.slow } }
                            }
                        }
                        Text {
                            text: Fmt.chance(root.perSecond * parent.modelData[1])
                            color: Theme.text; font.pixelSize: 13; font.weight: Font.DemiBold
                            Layout.preferredWidth: 150
                            horizontalAlignment: Text.AlignRight
                        }
                    }
                }
                Text {
                    text: root.perSecond > 0 ? "Expected time to scan the whole range once: " + Fmt.duration(1 / root.perSecond) : ""
                    color: Theme.textFaint; font.pixelSize: 12
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
                Item { Layout.fillHeight: true }
            }

            Card {
                Layout.fillWidth: true
                Layout.preferredWidth: 3
                Layout.preferredHeight: 300
                title: "Engine log"
                headerItems: IconButton { text: "Data folder"; iconName: "folder"; compact: true; onClicked: Engine.openDataDir() }

                ListView {
                    id: logView
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: root.logModel
                    spacing: 2
                    boundsBehavior: Flickable.StopAtBounds
                    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                    onCountChanged: Qt.callLater(positionViewAtEnd)
                    delegate: Text {
                        required property string line
                        required property bool error
                        width: logView.width - 12
                        text: line
                        color: error ? Theme.bad : (line.charAt(0) === "$" ? Theme.accent : Theme.textDim)
                        font.family: Theme.mono
                        font.pixelSize: 11
                        wrapMode: Text.WrapAnywhere
                    }
                }
            }
        }
    }
}
