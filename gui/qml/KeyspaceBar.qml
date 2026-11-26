// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Orion contributors. See LICENSE.txt.
import QtQuick
import QtQuick.Layouts
import Orion
import "Fmt.js" as Fmt

// The key range as a bar. Sequential: the scanned share fills it from the left (at least a sliver,
// so progress is visible on astronomically large ranges). Random: sparks land anywhere, since every
// step jumps to a new random offset.
ColumnLayout {
    id: root

    property string startHex
    property string endHex
    property real percent: 0
    property bool random: false
    property bool live: false
    property real keysDone: 0
    property real range: 0

    spacing: 10

    Item {
        id: track
        Layout.fillWidth: true
        Layout.preferredHeight: 34

        Rectangle {
            anchors.fill: parent
            radius: 10
            color: Theme.field
            border.color: Theme.border
        }

        // Faint scale: 16 segments
        Repeater {
            model: 15
            delegate: Rectangle {
                required property int index
                x: track.width * (index + 1) / 16
                y: 8
                width: 1
                height: track.height - 16
                color: Qt.rgba(1, 1, 1, 0.05)
            }
        }

        Rectangle {
            id: fill
            visible: !root.random
            x: 2; y: 2
            height: parent.height - 4
            radius: 8
            width: root.percent > 0 ? Math.max(6, (track.width - 4) * Math.min(1, root.percent / 100)) : 0
            Behavior on width { NumberAnimation { duration: Theme.slow; easing.type: Easing.OutCubic } }
            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0.0; color: Qt.rgba(Theme.accent2.r, Theme.accent2.g, Theme.accent2.b, 0.85) }
                GradientStop { position: 1.0; color: Theme.accent }
            }
            clip: true

            // Shimmer sweeping across the filled part while scanning
            Rectangle {
                width: 80
                height: parent.height
                visible: root.live
                gradient: Gradient {
                    orientation: Gradient.Horizontal
                    GradientStop { position: 0.0; color: "transparent" }
                    GradientStop { position: 0.5; color: Qt.rgba(1, 1, 1, 0.45) }
                    GradientStop { position: 1.0; color: "transparent" }
                }
                NumberAnimation on x {
                    running: root.live && !root.random
                    from: -80; to: Math.max(fill.width, 80)
                    duration: 1800
                    loops: Animation.Infinite
                }
            }
        }

        // Scan head
        Rectangle {
            visible: !root.random && root.live
            width: 3
            height: parent.height + 8
            y: -4
            radius: 1.5
            x: fill.x + fill.width - 1
            color: "white"
            opacity: 0.9
            SequentialAnimation on opacity {
                running: root.live && !root.random
                loops: Animation.Infinite
                NumberAnimation { to: 0.35; duration: 500 }
                NumberAnimation { to: 0.95; duration: 500 }
            }
        }

        // Random mode: sparks at random offsets
        Repeater {
            model: root.random ? 42 : 0
            delegate: Rectangle {
                id: spark
                width: 4; height: 4; radius: 2
                y: 4 + Math.random() * (track.height - 12)
                x: Math.random() * (track.width - 8) + 2
                color: Math.random() < 0.5 ? Theme.accent : Qt.lighter(Theme.accent2, 1.4)
                opacity: 0
                SequentialAnimation {
                    running: root.random && root.live
                    loops: Animation.Infinite
                    PauseAnimation { duration: Math.random() * 2500 }
                    ScriptAction {
                        script: {
                            spark.x = Math.random() * (track.width - 8) + 2
                            spark.y = 4 + Math.random() * (track.height - 12)
                        }
                    }
                    NumberAnimation { target: spark; property: "opacity"; to: 1; duration: 140 }
                    NumberAnimation { target: spark; property: "opacity"; to: 0; duration: 1300; easing.type: Easing.OutCubic }
                }
            }
        }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 12
        Text {
            text: "0x" + Fmt.shortHex(root.startHex, 8)
            color: Theme.textFaint
            font.family: Theme.mono
            font.pixelSize: 12
        }
        Item { Layout.fillWidth: true }
        Text {
            text: root.random
                  ? "random sampling · " + Fmt.pow2(root.keysDone) + " keys tried of " + Fmt.pow2(root.range)
                  : Fmt.percent(root.percent) + " scanned"
            color: Theme.text
            font.pixelSize: 13
            font.weight: Font.DemiBold
            textFormat: Text.RichText
        }
        Item { Layout.fillWidth: true }
        Text {
            text: "0x" + Fmt.shortHex(root.endHex, 8)
            color: Theme.textFaint
            font.family: Theme.mono
            font.pixelSize: 12
        }
    }
}
