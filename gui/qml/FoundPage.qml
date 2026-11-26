// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Orion contributors. See LICENSE.txt.
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Orion
import "Fmt.js" as Fmt

// Keys from found.txt, newest first, with copy buttons and the safety notes.
Flickable {
    id: root

    property int refreshTick: 0
    property int newThisSession: 0
    signal message(string text, string kind)

    readonly property var keys: { refreshTick; return Engine.readFoundFile().reverse() }

    contentWidth: width
    contentHeight: layout.implicitHeight + 64
    clip: true
    boundsBehavior: Flickable.StopAtBounds
    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

    function refresh() { refreshTick++ }

    component KeyField: RowLayout {
        id: kf
        property string label
        property string value
        property bool secret: false
        property bool revealed: !secret
        Layout.fillWidth: true
        spacing: 10
        Text { text: kf.label; color: Theme.textFaint; font.pixelSize: 12; Layout.preferredWidth: 96 }
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 34
            radius: 8
            color: Theme.field
            border.color: Theme.border
            Text {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                verticalAlignment: Text.AlignVCenter
                text: kf.revealed ? kf.value : kf.value.replace(/./g, "•")
                color: Theme.text
                font.family: Theme.mono
                font.pixelSize: 12
                elide: Text.ElideMiddle
            }
        }
        IconButton {
            visible: kf.secret
            compact: true
            iconName: kf.revealed ? "eyeoff" : "eye"
            ToolTip.visible: hovered
            ToolTip.text: kf.revealed ? "Hide" : "Show"
            onClicked: kf.revealed = !kf.revealed
        }
        IconButton {
            compact: true
            iconName: "copy"
            ToolTip.visible: hovered
            ToolTip.text: "Copy"
            onClicked: {
                Engine.copyText(kf.value)
                root.message(kf.label + " copied to the clipboard", "good")
            }
        }
    }

    ColumnLayout {
        id: layout
        x: 32
        y: 28
        width: root.width - 64
        spacing: Theme.gap

        RowLayout {
            Layout.fillWidth: true
            ColumnLayout {
                spacing: 4
                Layout.fillWidth: true
                Text { text: "Found keys"; color: Theme.text; font.pixelSize: 28; font.weight: Font.Bold }
                Text {
                    text: root.keys.length > 0
                          ? root.keys.length + (root.keys.length === 1 ? " key" : " keys") + " in found.txt"
                            + (root.newThisSession > 0 ? " · " + root.newThisSession + " new in this session" : "")
                          : "Every key the engine finds is verified on the CPU and appended to found.txt."
                    color: Theme.textDim; font.pixelSize: 13
                    Layout.fillWidth: true
                }
            }
            IconButton { text: "Reload"; iconName: "refresh"; compact: true; onClicked: root.refresh() }
            IconButton { text: "Open folder"; iconName: "folder"; compact: true; onClicked: Engine.openDataDir() }
        }

        // Empty state
        Card {
            visible: root.keys.length === 0
            Layout.fillWidth: true
            ColumnLayout {
                Layout.fillWidth: true
                Layout.topMargin: 20
                Layout.bottomMargin: 20
                spacing: 12
                Icon { name: "key"; size: 54; color: Theme.textFaint; Layout.alignment: Qt.AlignHCenter }
                Text {
                    text: "No keys found yet"
                    color: Theme.text; font.pixelSize: 18; font.weight: Font.DemiBold
                    Layout.alignment: Qt.AlignHCenter
                }
                Text {
                    text: "Try the Demo on the Search page: it scans puzzle #30, solved long ago, and finds its key in about a second."
                    color: Theme.textDim; font.pixelSize: 13
                    wrapMode: Text.WordWrap; horizontalAlignment: Text.AlignHCenter
                    Layout.fillWidth: true
                }
            }
        }

        Repeater {
            model: root.keys
            delegate: Card {
                id: keyCard
                required property var modelData
                required property int index
                Layout.fillWidth: true
                highlight: index < root.newThisSession
                accentColor: Theme.gold
                title: modelData.address
                subtitle: (modelData.type === "p2wpkh" ? "P2WPKH" : "P2PKH") + (modelData.time ? " · found " + modelData.time : "")

                opacity: 0
                Component.onCompleted: appear.start()
                NumberAnimation on opacity { id: appear; running: false; to: 1; duration: 300 + 60 * Math.min(keyCard.index, 8) }

                KeyField { label: "Address"; value: keyCard.modelData.address }
                KeyField { label: "Private key"; value: "0x" + keyCard.modelData.hex; secret: true }
                KeyField { label: "WIF"; value: keyCard.modelData.wif || ""; secret: true }
                KeyField { label: "Public key"; value: keyCard.modelData.pubkey || "" }
            }
        }

        Card {
            Layout.fillWidth: true
            title: "If you find a key"
            subtitle: "An unspent puzzle address has no public key on the chain; the moment you spend from it, it does. "
                      + "For a key known to lie in a small range, bots can then work it out in seconds and race your transaction."
            highlight: true
            accentColor: Theme.warn
            Repeater {
                model: [
                    ["shield", "Copy found.txt somewhere offline first: write the key down or put it on an encrypted drive."],
                    ["key", "Build and sign the spending transaction offline."],
                    ["spark", "Send it to a miner through a private channel, so the public key is not exposed in the public mempool before it is mined."]
                ]
                delegate: RowLayout {
                    required property var modelData
                    required property int index
                    Layout.fillWidth: true
                    spacing: 12
                    Rectangle {
                        width: 30; height: 30; radius: 15
                        color: Qt.rgba(Theme.warn.r, Theme.warn.g, Theme.warn.b, 0.12)
                        border.color: Theme.warn
                        Text { anchors.centerIn: parent; text: index + 1; color: Theme.warn; font.pixelSize: 13; font.weight: Font.Bold }
                    }
                    Text {
                        text: modelData[1]
                        color: Theme.text; font.pixelSize: 13
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                }
            }
            Text {
                text: "General information, not financial or security advice."
                color: Theme.textFaint; font.pixelSize: 11
            }
        }
    }
}
