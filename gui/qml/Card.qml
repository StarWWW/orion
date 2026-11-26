// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Orion contributors. See LICENSE.txt.
import QtQuick
import QtQuick.Layouts
import Orion

// Panel with an optional title row; children stack in a column.
Rectangle {
    id: root

    property string title
    property string subtitle
    property bool highlight: false
    property color accentColor: Theme.accent
    property real padding: 20
    default property alias content: body.data
    property alias headerItems: headerSlot.data
    property alias bodySpacing: body.spacing

    implicitWidth: column.implicitWidth + 2 * padding
    implicitHeight: column.implicitHeight + 2 * padding
    radius: Theme.radius
    border.width: 1
    border.color: highlight ? Qt.rgba(accentColor.r, accentColor.g, accentColor.b, 0.55) : Theme.border
    gradient: Gradient {
        GradientStop { position: 0.0; color: Qt.rgba(0.09, 0.13, 0.27, 0.86) }
        GradientStop { position: 1.0; color: Qt.rgba(0.05, 0.08, 0.18, 0.86) }
    }

    Behavior on border.color { ColorAnimation { duration: Theme.normal } }

    // Thin top highlight, like light catching the edge of glass
    Rectangle {
        anchors { left: parent.left; right: parent.right; top: parent.top; margins: root.radius }
        height: 1
        opacity: root.highlight ? 0.9 : 0.35
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0.0; color: "transparent" }
            GradientStop { position: 0.5; color: root.highlight ? root.accentColor : Theme.borderHi }
            GradientStop { position: 1.0; color: "transparent" }
        }
        Behavior on opacity { NumberAnimation { duration: Theme.normal } }
    }

    ColumnLayout {
        id: column
        anchors.fill: parent
        anchors.margins: root.padding
        spacing: 14

        RowLayout {
            visible: root.title !== ""
            Layout.fillWidth: true
            spacing: 12

            ColumnLayout {
                spacing: 2
                Layout.fillWidth: true
                Text {
                    text: root.title
                    color: Theme.text
                    font.pixelSize: 15
                    font.weight: Font.DemiBold
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                }
                Text {
                    visible: root.subtitle !== ""
                    text: root.subtitle
                    color: Theme.textDim
                    font.pixelSize: 12
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
            }
            Row {
                id: headerSlot
                spacing: 8
                Layout.alignment: Qt.AlignTop | Qt.AlignRight
            }
        }

        ColumnLayout {
            id: body
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 10
        }
    }
}
