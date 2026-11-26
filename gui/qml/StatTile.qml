// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Orion contributors. See LICENSE.txt.
import QtQuick
import QtQuick.Layouts
import Orion

// Small metric: label, big value with unit, one line of detail. Flashes when the value changes.
Rectangle {
    id: root

    property string label
    property string value
    property string unit
    property string detail
    property string iconName
    property color tint: Theme.accent

    implicitHeight: 112
    implicitWidth: 200
    radius: Theme.radius - 2
    color: Qt.rgba(0.05, 0.08, 0.18, 0.78)
    border.color: Theme.border

    onValueChanged: flash.restart()

    Rectangle {
        id: glow
        anchors.fill: parent
        radius: parent.radius
        color: "transparent"
        border.width: 1
        border.color: root.tint
        opacity: 0
        SequentialAnimation on opacity {
            id: flash
            running: false
            NumberAnimation { to: 0.55; duration: 90 }
            NumberAnimation { to: 0; duration: 700; easing.type: Easing.OutCubic }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 4

        RowLayout {
            spacing: 7
            Icon {
                visible: root.iconName !== ""
                name: root.iconName
                size: 15
                color: root.tint
            }
            Text {
                text: root.label.toUpperCase()
                color: Theme.textDim
                font.pixelSize: 11
                font.letterSpacing: 1.2
                font.weight: Font.DemiBold
                Layout.fillWidth: true
                elide: Text.ElideRight
            }
        }
        Item { Layout.fillHeight: true }
        Row {
            spacing: 6
            Layout.fillWidth: true
            Text {
                id: valueText
                text: root.value
                color: Theme.text
                font.pixelSize: 26
                font.weight: Font.DemiBold
                width: Math.min(implicitWidth, root.width - 32 - (unitText.visible ? unitText.width + 6 : 0))
                elide: Text.ElideRight
            }
            Text {
                id: unitText
                visible: root.unit !== ""
                text: root.unit
                color: Theme.textDim
                font.pixelSize: 14
                anchors.baseline: valueText.baseline
            }
        }
        Text {
            text: root.detail
            visible: text !== ""
            color: Theme.textFaint
            font.pixelSize: 12
            Layout.fillWidth: true
            elide: Text.ElideRight
            textFormat: Text.RichText
        }
    }
}
