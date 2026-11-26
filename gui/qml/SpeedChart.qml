// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Orion contributors. See LICENSE.txt.
import QtQuick
import Orion
import "Fmt.js" as Fmt

// Area chart of the recent speed samples (MK/s), newest on the right.
Item {
    id: root

    property var samples: []
    property bool live: false

    implicitHeight: 180

    readonly property real scaleTop: {
        var m = 0
        for (var i = 0; i < samples.length; i++) m = Math.max(m, samples[i])
        if (m <= 0) return 100
        var step = Math.pow(10, Math.floor(Math.log(m * 1.2) / Math.LN10))
        return Math.ceil(m * 1.2 / step) * step
    }
    property real shownTop: scaleTop
    Behavior on shownTop { NumberAnimation { duration: 600; easing.type: Easing.OutCubic } }

    onSamplesChanged: canvas.requestPaint()
    onShownTopChanged: canvas.requestPaint()

    Canvas {
        id: canvas
        anchors.fill: parent
        anchors.leftMargin: 44
        antialiasing: true

        onPaint: {
            var c = getContext("2d")
            c.reset()
            var w = width, h = height - 18
            var n = root.samples.length

            // Grid
            c.strokeStyle = Qt.rgba(1, 1, 1, 0.06)
            c.lineWidth = 1
            for (var g = 0; g <= 4; g++) {
                var gy = Math.round(h * g / 4) + 0.5
                c.beginPath(); c.moveTo(0, gy); c.lineTo(w, gy); c.stroke()
            }
            if (n < 2) return

            // The first minute fills the width quickly, then the newest 240 readings scroll by
            var cap = Math.min(240, Math.max(60, n))
            var dx = w / (cap - 1)
            var x0 = w - (n - 1) * dx
            function px(i) { return x0 + i * dx }
            function py(v) { return h - Math.max(0, Math.min(1, v / root.shownTop)) * (h - 6) }

            // Smoothed path through the samples
            c.beginPath()
            c.moveTo(px(0), py(root.samples[0]))
            for (var i = 1; i < n; i++) {
                var xm = (px(i - 1) + px(i)) / 2
                c.bezierCurveTo(xm, py(root.samples[i - 1]), xm, py(root.samples[i]), px(i), py(root.samples[i]))
            }

            // Area fill
            c.lineTo(px(n - 1), h); c.lineTo(px(0), h); c.closePath()
            var grad = c.createLinearGradient(0, 0, 0, h)
            grad.addColorStop(0, Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.32))
            grad.addColorStop(1, Qt.rgba(Theme.accent2.r, Theme.accent2.g, Theme.accent2.b, 0.02))
            c.fillStyle = grad
            c.fill()

            // Line with a soft glow
            c.beginPath()
            c.moveTo(px(0), py(root.samples[0]))
            for (var j = 1; j < n; j++) {
                var xm2 = (px(j - 1) + px(j)) / 2
                c.bezierCurveTo(xm2, py(root.samples[j - 1]), xm2, py(root.samples[j]), px(j), py(root.samples[j]))
            }
            c.strokeStyle = Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.25)
            c.lineWidth = 6
            c.stroke()
            c.strokeStyle = Theme.accent
            c.lineWidth = 2
            c.stroke()

            // Newest sample
            var lx = px(n - 1), ly = py(root.samples[n - 1])
            c.fillStyle = Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.3)
            c.beginPath(); c.arc(lx, ly, 7, 0, Math.PI * 2); c.fill()
            c.fillStyle = "white"
            c.beginPath(); c.arc(lx, ly, 3, 0, Math.PI * 2); c.fill()
        }
    }

    // Y axis labels
    Repeater {
        model: 5
        delegate: Text {
            required property int index
            readonly property real v: root.shownTop * (4 - index) / 4
            text: v >= 1000 ? (v / 1000).toFixed(1) + "k" : Math.round(v)
            color: Theme.textFaint
            font.pixelSize: 10
            width: 38
            horizontalAlignment: Text.AlignRight
            y: (root.height - 18) * index / 4 - height / 2
        }
    }

    Text {
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        text: root.samples.length > 1 ? "last " + root.samples.length + " readings" : (root.live ? "waiting for readings…" : "")
        color: Theme.textFaint
        font.pixelSize: 10
    }
}
