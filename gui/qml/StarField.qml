// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Orion contributors. See LICENSE.txt.
import QtQuick
import QtQuick.Shapes
import Orion

// Night sky behind the window: nebula glow, a fixed star field, the Orion constellation and a few
// twinkling stars. While a search runs the stars twinkle faster and the nebula slowly breathes.
Item {
    id: root

    property bool live: false

    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: Theme.bgTop }
            GradientStop { position: 1.0; color: Theme.bg }
        }
    }

    property real breathe: 0
    SequentialAnimation on breathe {
        loops: Animation.Infinite
        NumberAnimation { from: 0; to: 1; duration: root.live ? 4000 : 9000; easing.type: Easing.InOutSine }
        NumberAnimation { from: 1; to: 0; duration: root.live ? 4000 : 9000; easing.type: Easing.InOutSine }
    }

    // Nebula clouds
    Shape {
        anchors.fill: parent
        opacity: 0.55 + 0.25 * root.breathe
        ShapePath {
            strokeColor: "transparent"
            fillGradient: RadialGradient {
                centerX: root.width * 0.82; centerY: root.height * 0.12
                centerRadius: root.width * 0.42
                focalX: centerX; focalY: centerY
                GradientStop { position: 0.0; color: Qt.rgba(0.56, 0.42, 1.0, 0.22) }
                GradientStop { position: 1.0; color: "transparent" }
            }
            startX: 0; startY: 0
            PathLine { x: root.width; y: 0 }
            PathLine { x: root.width; y: root.height }
            PathLine { x: 0; y: root.height }
            PathLine { x: 0; y: 0 }
        }
        ShapePath {
            strokeColor: "transparent"
            fillGradient: RadialGradient {
                centerX: root.width * 0.18; centerY: root.height * 0.95
                centerRadius: root.width * 0.38
                focalX: centerX; focalY: centerY
                GradientStop { position: 0.0; color: Qt.rgba(0.30, 0.84, 1.0, 0.13) }
                GradientStop { position: 1.0; color: "transparent" }
            }
            startX: 0; startY: 0
            PathLine { x: root.width; y: 0 }
            PathLine { x: root.width; y: root.height }
            PathLine { x: 0; y: root.height }
            PathLine { x: 0; y: 0 }
        }
    }

    // Fixed faint stars (same pattern on every start)
    Canvas {
        id: stars
        anchors.fill: parent
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
        onPaint: {
            var c = getContext("2d")
            c.reset()
            var seed = 1234567
            function rnd() { seed = (seed * 16807) % 2147483647; return seed / 2147483647 }
            for (var i = 0; i < 260; i++) {
                var x = rnd() * width, y = rnd() * height, r = rnd() * 1.1 + 0.2, a = rnd() * 0.5 + 0.12
                c.fillStyle = Qt.rgba(0.85, 0.9, 1.0, a)
                c.beginPath(); c.arc(x, y, r, 0, Math.PI * 2); c.fill()
            }
        }
    }

    // Orion: Betelgeuse, Bellatrix, the belt (Alnitak, Alnilam, Mintaka), Saiph, Rigel
    Item {
        id: constellation
        width: 300; height: 360
        x: parent.width - width - 40
        y: parent.height - height - 30
        opacity: 0.22 + 0.08 * root.breathe

        readonly property var pts: [[60, 40], [240, 70], [128, 170], [150, 160], [172, 150], [90, 320], [250, 300]]
        readonly property var links: [[0, 2], [1, 4], [2, 3], [3, 4], [2, 5], [4, 6], [0, 1]]

        Shape {
            anchors.fill: parent
            ShapePath {
                strokeColor: Qt.rgba(0.6, 0.75, 1.0, 0.55)
                strokeWidth: 1
                fillColor: "transparent"
                PathMultiline {
                    paths: constellation.links.map(function (l) {
                        return [Qt.point(constellation.pts[l[0]][0], constellation.pts[l[0]][1]),
                                Qt.point(constellation.pts[l[1]][0], constellation.pts[l[1]][1])]
                    })
                }
            }
        }
        Repeater {
            model: constellation.pts
            delegate: Rectangle {
                required property var modelData
                required property int index
                width: index === 0 ? 9 : (index === 6 ? 8 : 6)
                height: width
                radius: width / 2
                x: modelData[0] - width / 2
                y: modelData[1] - height / 2
                color: index === 0 ? "#ffb48a" : (index === 6 ? "#bcd7ff" : "#e6eeff")
            }
        }
    }

    // Twinkling stars
    Repeater {
        model: 36
        delegate: Rectangle {
            id: star
            required property int index
            readonly property real seedX: ((index * 7919) % 1000) / 1000
            readonly property real seedY: ((index * 104729) % 997) / 997
            x: seedX * root.width
            y: seedY * root.height
            width: 2 + (index % 3)
            height: width
            radius: width / 2
            color: index % 5 === 0 ? Theme.accent : "white"
            opacity: 0.2
            SequentialAnimation on opacity {
                loops: Animation.Infinite
                PauseAnimation { duration: (star.index * 373) % 2600 }
                NumberAnimation { to: 0.95; duration: root.live ? 500 : 1400; easing.type: Easing.InOutSine }
                NumberAnimation { to: 0.15; duration: root.live ? 700 : 2000; easing.type: Easing.InOutSine }
            }
        }
    }
}
