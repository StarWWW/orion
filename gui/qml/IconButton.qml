// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Orion contributors. See LICENSE.txt.
import QtQuick
import QtQuick.Controls.Basic
import Orion

// Button with an icon. kind: "primary" (gradient), "secondary" (outline), "danger", "ghost".
Button {
    id: root

    property string iconName
    property string kind: "secondary"
    property color tint: kind === "danger" ? Theme.bad : Theme.accent
    property bool compact: false

    hoverEnabled: true
    font.pixelSize: compact ? 12 : 14
    font.weight: Font.DemiBold
    padding: compact ? 7 : 11
    leftPadding: compact ? 10 : 18
    rightPadding: compact ? 12 : 20
    implicitHeight: compact ? 32 : 44
    scale: down ? 0.97 : 1.0

    Behavior on scale { NumberAnimation { duration: 90 } }

    contentItem: Row {
        spacing: root.text !== "" && root.iconName !== "" ? 8 : 0
        opacity: root.enabled ? 1.0 : 0.45
        Icon {
            visible: root.iconName !== ""
            name: root.iconName
            width: root.compact ? 14 : 18
            height: width
            anchors.verticalCenter: parent.verticalCenter
            color: root.kind === "primary" ? Theme.bg : (root.kind === "danger" ? Theme.bad : Theme.text)
        }
        Text {
            text: root.text
            visible: text !== ""
            font: root.font
            anchors.verticalCenter: parent.verticalCenter
            color: root.kind === "primary" ? Theme.bg : (root.kind === "danger" ? Theme.bad : Theme.text)
        }
    }

    background: Rectangle {
        radius: root.compact ? 9 : 12
        opacity: root.enabled ? 1.0 : 0.5
        border.width: root.kind === "primary" ? 0 : 1
        border.color: root.kind === "danger"
                      ? Qt.rgba(Theme.bad.r, Theme.bad.g, Theme.bad.b, root.hovered ? 0.9 : 0.5)
                      : (root.hovered ? Theme.borderHi : Theme.border)
        color: root.kind === "primary" ? "transparent"
             : root.kind === "ghost" ? (root.hovered ? Qt.rgba(1, 1, 1, 0.06) : "transparent")
             : (root.hovered ? Theme.panelHi : Qt.rgba(0.06, 0.09, 0.19, 0.9))

        Behavior on color { ColorAnimation { duration: Theme.fast } }

        // Primary: starlight gradient that brightens on hover
        Rectangle {
            visible: root.kind === "primary"
            anchors.fill: parent
            radius: parent.radius
            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0.0; color: Theme.accent }
                GradientStop { position: 1.0; color: Qt.lighter(Theme.accent2, 1.25) }
            }
            opacity: root.hovered ? 1.0 : 0.88
            Behavior on opacity { NumberAnimation { duration: Theme.fast } }
        }
    }
}
