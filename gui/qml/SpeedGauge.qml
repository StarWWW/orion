// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Orion contributors. See LICENSE.txt.
import QtQuick
import QtQuick.Shapes
import Orion
import "Fmt.js" as Fmt

// Arc gauge of the search speed. The scale grows with the fastest speed seen, the needle eases
// toward each new reading and the arc breathes while the search runs.
Item {
    id: root

    property real value: 0          // MK/s
    property real peak: 0
    property bool live: false

    readonly property real scaleMax: {
        var top = Math.max(peak * 1.15, value * 1.15, 100)
        var step = Math.pow(10, Math.floor(Math.log(top) / Math.LN10))
        return Math.ceil(top / step) * step
    }
    property real shown: value
    Behavior on shown { NumberAnimation { duration: 900; easing.type: Easing.OutCubic } }

    readonly property real frac: Math.max(0, Math.min(1, shown / scaleMax))
    readonly property real cx: width / 2
    readonly property real cy: height * 0.56
    readonly property real r: Math.min(width, height * 1.6) * 0.40
    readonly property real startAngle: 140
    readonly property real sweep: 260

    implicitWidth: 320
    implicitHeight: 280

    property real breathe: 0
    SequentialAnimation on breathe {
        running: root.live
        loops: Animation.Infinite
        NumberAnimation { from: 0; to: 1; duration: 1600; easing.type: Easing.InOutSine }
        NumberAnimation { from: 1; to: 0; duration: 1600; easing.type: Easing.InOutSine }
    }

    Shape {
        anchors.fill: parent
        layer.enabled: true
        layer.samples: 8

        // Track
        ShapePath {
            fillColor: "transparent"
            strokeColor: Qt.rgba(1, 1, 1, 0.07)
            strokeWidth: root.r * 0.11
            capStyle: ShapePath.RoundCap
            PathAngleArc {
                centerX: root.cx; centerY: root.cy
                radiusX: root.r; radiusY: root.r
                startAngle: root.startAngle; sweepAngle: root.sweep
            }
        }
        // Glow under the value arc
        ShapePath {
            fillColor: "transparent"
            strokeColor: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.10 + 0.10 * root.breathe)
            strokeWidth: root.r * (0.24 + 0.05 * root.breathe)
            capStyle: ShapePath.RoundCap
            PathAngleArc {
                centerX: root.cx; centerY: root.cy
                radiusX: root.r; radiusY: root.r
                startAngle: root.startAngle; sweepAngle: Math.max(0.5, root.sweep * root.frac)
            }
        }
        // Value arc
        ShapePath {
            fillColor: "transparent"
            strokeWidth: root.r * 0.11
            capStyle: ShapePath.RoundCap
            strokeColor: Theme.accent
            PathAngleArc {
                centerX: root.cx; centerY: root.cy
                radiusX: root.r; radiusY: root.r
                startAngle: root.startAngle; sweepAngle: Math.max(0.5, root.sweep * root.frac)
            }
        }
        // Inner thin ring
        ShapePath {
            fillColor: "transparent"
            strokeColor: Qt.rgba(Theme.accent2.r, Theme.accent2.g, Theme.accent2.b, 0.35)
            strokeWidth: 1.5
            PathAngleArc {
                centerX: root.cx; centerY: root.cy
                radiusX: root.r * 0.78; radiusY: root.r * 0.78
                startAngle: root.startAngle; sweepAngle: root.sweep
            }
        }
    }

    // Scale ticks and labels
    Repeater {
        model: 11
        delegate: Item {
            required property int index
            readonly property real a: (root.startAngle + root.sweep * index / 10) * Math.PI / 180
            readonly property bool major: index % 2 === 0
            Rectangle {
                width: major ? 2 : 1.5
                height: major ? root.r * 0.09 : root.r * 0.05
                radius: 1
                color: index / 10 <= root.frac + 0.001 ? Theme.accent : Theme.textFaint
                opacity: major ? 0.9 : 0.6
                x: root.cx + Math.cos(parent.a) * root.r * 0.86 - width / 2
                y: root.cy + Math.sin(parent.a) * root.r * 0.86 - height / 2
                rotation: (root.startAngle + root.sweep * index / 10) + 90
                Behavior on color { ColorAnimation { duration: Theme.normal } }
            }
            Text {
                visible: major
                text: {
                    var v = root.scaleMax * index / 10
                    return v >= 1000 ? (v / 1000).toFixed(v % 1000 === 0 ? 0 : 1) + "k" : Math.round(v)
                }
                color: Theme.textFaint
                font.pixelSize: 10
                x: root.cx + Math.cos(parent.a) * root.r * 0.66 - width / 2
                y: root.cy + Math.sin(parent.a) * root.r * 0.66 - height / 2
            }
        }
    }

    // Glowing head of the value arc
    Rectangle {
        readonly property real a: (root.startAngle + root.sweep * root.frac) * Math.PI / 180
        width: root.r * 0.2
        height: width
        radius: width / 2
        x: root.cx + Math.cos(a) * root.r - width / 2
        y: root.cy + Math.sin(a) * root.r - height / 2
        visible: root.frac > 0.002
        gradient: Gradient {
            GradientStop { position: 0.0; color: Qt.rgba(1, 1, 1, 0.95) }
            GradientStop { position: 1.0; color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.6) }
        }
        scale: 0.85 + 0.25 * root.breathe
        opacity: root.live ? 1.0 : 0.6
    }

    Column {
        anchors.horizontalCenter: parent.horizontalCenter
        y: root.cy - height / 2
        spacing: 2
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: Fmt.speed(root.shown).value
            color: Theme.text
            font.pixelSize: Math.max(22, root.r * 0.34)
            font.weight: Font.Bold
        }
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: Fmt.speed(root.shown).unit
            color: Theme.accent
            font.pixelSize: 13
            font.letterSpacing: 2
            font.weight: Font.DemiBold
        }
    }

    Text {
        anchors.horizontalCenter: parent.horizontalCenter
        y: root.cy + root.r * 0.62
        text: root.peak > 0 ? "peak " + Fmt.speed(root.peak).value + " " + Fmt.speed(root.peak).unit : "keys per second"
        color: Theme.textFaint
        font.pixelSize: 12
    }
}
