// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Orion contributors. See LICENSE.txt.
import QtQuick
import Orion

// Segmented control with a sliding highlight. model: [{text, icon}] or plain strings.
Rectangle {
    id: root

    property var model: []
    property int currentIndex: 0
    signal activated(int index)

    implicitHeight: 40
    implicitWidth: row.implicitWidth + 8
    radius: 12
    color: Theme.field
    border.color: Theme.border

    readonly property real segmentWidth: (width - 8) / Math.max(1, model.length)

    Rectangle {
        id: highlight
        x: 4 + root.currentIndex * root.segmentWidth
        y: 4
        width: root.segmentWidth
        height: root.height - 8
        radius: 9
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0.0; color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.28) }
            GradientStop { position: 1.0; color: Qt.rgba(Theme.accent2.r, Theme.accent2.g, Theme.accent2.b, 0.28) }
        }
        border.color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.6)
        Behavior on x { NumberAnimation { duration: Theme.normal; easing.type: Easing.OutCubic } }
    }

    Row {
        id: row
        x: 4
        y: 4
        Repeater {
            model: root.model
            delegate: MouseArea {
                id: seg
                required property var modelData
                required property int index
                readonly property string label: typeof modelData === "string" ? modelData : modelData.text
                readonly property string iconName: typeof modelData === "string" ? "" : (modelData.icon || "")
                width: root.segmentWidth
                height: root.height - 8
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    root.currentIndex = index
                    root.activated(index)
                }
                Row {
                    anchors.centerIn: parent
                    spacing: 7
                    Icon {
                        visible: seg.iconName !== ""
                        name: seg.iconName
                        width: 15; height: 15
                        anchors.verticalCenter: parent.verticalCenter
                        color: seg.index === root.currentIndex ? Theme.text : Theme.textDim
                    }
                    Text {
                        text: seg.label
                        font.pixelSize: 13
                        font.weight: seg.index === root.currentIndex ? Font.DemiBold : Font.Normal
                        color: seg.index === root.currentIndex ? Theme.text : (seg.containsMouse ? Theme.text : Theme.textDim)
                        anchors.verticalCenter: parent.verticalCenter
                        Behavior on color { ColorAnimation { duration: Theme.fast } }
                    }
                }
            }
        }
    }
}
