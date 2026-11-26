// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Orion contributors. See LICENSE.txt.
import QtQuick
import Orion

// Short message that slides up from the bottom and fades away.
Rectangle {
    id: root

    function show(message, kind) {
        text.text = message
        root.kind = kind || "info"
        anim.restart()
    }

    property string kind: "info"
    readonly property color tint: kind === "error" ? Theme.bad : (kind === "good" ? Theme.good : Theme.accent)

    width: Math.min(560, text.implicitWidth + 60)
    height: 46
    radius: 12
    color: Qt.rgba(0.06, 0.09, 0.2, 0.96)
    border.color: tint
    opacity: 0
    visible: opacity > 0
    property real lift: 0
    anchors.horizontalCenter: parent.horizontalCenter
    y: parent.height - height - 28 + lift

    Rectangle {
        x: 16
        anchors.verticalCenter: parent.verticalCenter
        width: 8; height: 8; radius: 4
        color: root.tint
    }
    Text {
        id: text
        x: 34
        anchors.verticalCenter: parent.verticalCenter
        width: root.width - 50
        elide: Text.ElideRight
        color: Theme.text
        font.pixelSize: 13
    }

    SequentialAnimation {
        id: anim
        ParallelAnimation {
            NumberAnimation { target: root; property: "opacity"; from: 0; to: 1; duration: 180 }
            NumberAnimation { target: root; property: "lift"; from: 20; to: 0; duration: 260; easing.type: Easing.OutBack }
        }
        PauseAnimation { duration: 2600 }
        NumberAnimation { target: root; property: "opacity"; to: 0; duration: 400 }
    }
}
