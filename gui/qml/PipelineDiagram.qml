// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Orion contributors. See LICENSE.txt.
import QtQuick
import QtQuick.Layouts
import Orion

// private key -> public key -> SHA-256 -> RIPEMD-160 -> address, with a pulse running through the
// stages. The values are those of Bitcoin puzzle #30 (solved), so every step can be checked.
Item {
    id: root

    property bool live: true
    implicitHeight: 230

    readonly property var stages: [
        { title: "Private key", tag: "256-bit number", value: "0x3D94CD64", note: "one key of the range" },
        { title: "Public key", tag: "secp256k1  k·G", value: "030D282C…E92A5B", note: "batched point additions" },
        { title: "SHA-256", tag: "33-byte input", value: "3f906ce0…50a26cd2", note: "compile-time padding" },
        { title: "RIPEMD-160", tag: "hash160", value: "d39c4704…57d68a08", note: "early exit on word 2" },
        { title: "Address", tag: "Base58Check", value: "1LHtnpd8…Lc992bps", note: "compared with the targets" }
    ]

    property real t: 0
    NumberAnimation on t {
        running: root.live && root.visible
        from: 0; to: 1
        duration: 3600
        loops: Animation.Infinite
    }

    readonly property real nodeW: Math.min(190, (width - 4 * 34) / 5)
    readonly property real gapW: (width - 5 * nodeW) / 4

    // Connectors
    Repeater {
        model: 4
        delegate: Item {
            required property int index
            x: (index + 1) * root.nodeW + index * root.gapW
            y: 64
            width: root.gapW
            height: 2
            Rectangle {
                anchors.fill: parent
                color: Theme.border
            }
            Rectangle {   // pulse travelling along this connector
                readonly property real local: root.t * 5 - (index + 1)
                visible: local > -0.2 && local < 0.2
                x: (local + 0.2) / 0.4 * parent.width - width / 2
                y: -3
                width: 14; height: 8; radius: 4
                color: Theme.accent
                opacity: 0.9
            }
        }
    }

    Repeater {
        model: root.stages
        delegate: Rectangle {
            id: node
            required property var modelData
            required property int index
            readonly property real active: Math.max(0, 1 - Math.abs(root.t * 5 - index - 0.5) * 1.6)
            x: index * (root.nodeW + root.gapW)
            y: 20
            width: root.nodeW
            height: 190
            radius: 14
            color: Qt.rgba(0.05, 0.08, 0.18, 0.85)
            border.width: 1
            border.color: Qt.rgba(Theme.accent.r * active + Theme.border.r * (1 - active),
                                  Theme.accent.g * active + Theme.border.g * (1 - active),
                                  Theme.accent.b * active + Theme.border.b * (1 - active), 1)

            Rectangle {
                anchors.fill: parent
                radius: parent.radius
                color: Theme.accent
                opacity: node.active * 0.08
            }

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 14
                spacing: 6

                Rectangle {
                    Layout.alignment: Qt.AlignHCenter
                    width: 40; height: 40; radius: 20
                    color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.12 + 0.3 * node.active)
                    border.color: Theme.accent
                    border.width: 1
                    scale: 1 + 0.12 * node.active
                    Text {
                        anchors.centerIn: parent
                        text: node.index + 1
                        color: Theme.text
                        font.pixelSize: 16
                        font.weight: Font.Bold
                    }
                }
                Text {
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    text: node.modelData.title
                    color: Theme.text
                    font.pixelSize: 14
                    font.weight: Font.DemiBold
                }
                Text {
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    text: node.modelData.tag
                    color: Theme.accent
                    font.pixelSize: 11
                }
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 26
                    radius: 6
                    color: Theme.field
                    Text {
                        anchors.centerIn: parent
                        width: parent.width - 8
                        horizontalAlignment: Text.AlignHCenter
                        elide: Text.ElideMiddle
                        text: node.modelData.value
                        color: Theme.textDim
                        font.family: Theme.mono
                        font.pixelSize: 11
                    }
                }
                Text {
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    text: node.modelData.note
                    color: Theme.textFaint
                    font.pixelSize: 11
                }
            }
        }
    }
}
