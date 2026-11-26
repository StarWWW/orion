// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Orion contributors. See LICENSE.txt.
import QtQuick

// Line icons drawn at any size, so no image files or icon fonts are needed on any platform.
Canvas {
    id: root

    property string name
    property color color: "white"
    property real lineWidth: Math.max(1.5, width / 13)

    // Size inside layouts, which go by the implicit size
    property real size: 20
    implicitWidth: size
    implicitHeight: size
    antialiasing: true

    onNameChanged: requestPaint()
    onColorChanged: requestPaint()
    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()

    onPaint: {
        var c = getContext("2d")
        c.reset()
        var s = Math.min(width, height) / 24
        c.save()
        c.translate((width - 24 * s) / 2, (height - 24 * s) / 2)
        c.scale(s, s)
        c.strokeStyle = root.color
        c.fillStyle = root.color
        c.lineWidth = root.lineWidth / s
        c.lineCap = "round"
        c.lineJoin = "round"

        function circle(x, y, r, fill) {
            c.beginPath(); c.arc(x, y, r, 0, Math.PI * 2)
            if (fill) c.fill(); else c.stroke()
        }
        function line(pts) {
            c.beginPath(); c.moveTo(pts[0], pts[1])
            for (var i = 2; i < pts.length; i += 2) c.lineTo(pts[i], pts[i + 1])
            c.stroke()
        }
        function rrect(x, y, w, h, r) {
            c.beginPath()
            c.moveTo(x + r, y); c.lineTo(x + w - r, y); c.arcTo(x + w, y, x + w, y + r, r)
            c.lineTo(x + w, y + h - r); c.arcTo(x + w, y + h, x + w - r, y + h, r)
            c.lineTo(x + r, y + h); c.arcTo(x, y + h, x, y + h - r, r)
            c.lineTo(x, y + r); c.arcTo(x, y, x + r, y, r)
            c.closePath(); c.stroke()
        }

        switch (root.name) {
        case "setup":       // sliders
            line([4, 7, 20, 7]); line([4, 17, 20, 17])
            circle(9, 7, 2.4, true); circle(15, 17, 2.4, true)
            break
        case "monitor":     // gauge
            c.beginPath(); c.arc(12, 14, 8, Math.PI * 0.85, Math.PI * 2.15); c.stroke()
            line([12, 14, 16.5, 9.5]); circle(12, 14, 1.6, true)
            break
        case "key":
            circle(8, 12, 4.2, false)
            line([12.2, 12, 21, 12]); line([18, 12, 18, 15.5]); line([21, 12, 21, 14.5])
            break
        case "learn":       // open book
            line([12, 6.5, 12, 19.5])
            c.beginPath(); c.moveTo(12, 6.5); c.quadraticCurveTo(7, 4.5, 3, 6); c.lineTo(3, 18)
            c.quadraticCurveTo(7, 16.5, 12, 19.5); c.stroke()
            c.beginPath(); c.moveTo(12, 6.5); c.quadraticCurveTo(17, 4.5, 21, 6); c.lineTo(21, 18)
            c.quadraticCurveTo(17, 16.5, 12, 19.5); c.stroke()
            break
        case "play":
            c.beginPath(); c.moveTo(7, 4.5); c.lineTo(19.5, 12); c.lineTo(7, 19.5); c.closePath(); c.fill()
            break
        case "pause":
            c.fillRect(6, 5, 4, 14); c.fillRect(14, 5, 4, 14)
            break
        case "stop":
            c.fillRect(6, 6, 12, 12)
            break
        case "copy":
            rrect(8, 8, 12, 12, 2.5)
            line([5, 15, 4, 15, 4, 4, 15, 4, 15, 5])
            break
        case "folder":
            c.beginPath(); c.moveTo(3, 7); c.lineTo(3, 18.5); c.lineTo(21, 18.5); c.lineTo(21, 8.5)
            c.lineTo(11.5, 8.5); c.lineTo(9.5, 6); c.lineTo(3, 6); c.closePath(); c.stroke()
            break
        case "eye":
            c.beginPath(); c.moveTo(2.5, 12); c.quadraticCurveTo(12, 2.5, 21.5, 12)
            c.quadraticCurveTo(12, 21.5, 2.5, 12); c.stroke(); circle(12, 12, 3, true)
            break
        case "eyeoff":
            c.beginPath(); c.moveTo(2.5, 12); c.quadraticCurveTo(12, 2.5, 21.5, 12)
            c.quadraticCurveTo(12, 21.5, 2.5, 12); c.stroke(); line([4, 20, 20, 4])
            break
        case "refresh":
            c.beginPath(); c.arc(12, 12, 7.5, Math.PI * 0.2, Math.PI * 1.75); c.stroke()
            line([19.5, 4.5, 19.5, 9.5, 14.5, 9.5])
            break
        case "file":
            line([6, 3, 14, 3, 19, 8, 19, 21, 6, 21, 6, 3]); line([14, 3, 14, 8, 19, 8])
            break
        case "spark":       // four-point star
            c.beginPath(); c.moveTo(12, 2.5); c.quadraticCurveTo(13, 11, 21.5, 12)
            c.quadraticCurveTo(13, 13, 12, 21.5); c.quadraticCurveTo(11, 13, 2.5, 12)
            c.quadraticCurveTo(11, 11, 12, 2.5); c.fill()
            break
        case "check":
            line([5, 12.5, 10, 17.5, 19.5, 6.5])
            break
        case "warning":
            line([12, 3.5, 21.5, 20, 2.5, 20, 12, 3.5]); line([12, 9.5, 12, 14]); circle(12, 17, 1.1, true)
            break
        case "info":
            circle(12, 12, 9, false); line([12, 11, 12, 16.5]); circle(12, 7.8, 1.1, true)
            break
        case "gpu":         // chip
            rrect(6, 6, 12, 12, 2)
            for (var i = 0; i < 3; i++) {
                var p = 9 + i * 3
                line([p, 3, p, 6]); line([p, 18, p, 21]); line([3, p, 6, p]); line([18, p, 21, p])
            }
            break
        case "dice":        // random mode
            rrect(4, 4, 16, 16, 3.5)
            circle(9, 9, 1.4, true); circle(15, 15, 1.4, true); circle(15, 9, 1.4, true); circle(9, 15, 1.4, true)
            break
        case "arrow":       // sequential mode
            line([4, 12, 19, 12]); line([14, 7, 19, 12, 14, 17])
            break
        case "target":
            circle(12, 12, 8.5, false); circle(12, 12, 4.5, false); circle(12, 12, 1.4, true)
            break
        case "shield":
            c.beginPath(); c.moveTo(12, 3); c.lineTo(20, 6); c.lineTo(20, 12)
            c.quadraticCurveTo(20, 18, 12, 21.5); c.quadraticCurveTo(4, 18, 4, 12); c.lineTo(4, 6)
            c.closePath(); c.stroke(); line([8.5, 12, 11, 14.5, 15.5, 9.5])
            break
        case "close":
            line([6, 6, 18, 18]); line([18, 6, 6, 18])
            break
        }
        c.restore()
    }
}
