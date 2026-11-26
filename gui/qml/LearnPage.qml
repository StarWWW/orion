// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Orion contributors. See LICENSE.txt.
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Orion
import "Fmt.js" as Fmt

// How a key is checked, what the modes mean and what the odds are.
Flickable {
    id: root

    property real speed: 0    // MK/s, best known speed of this machine

    contentWidth: width
    contentHeight: layout.implicitHeight + 64
    clip: true
    boundsBehavior: Flickable.StopAtBounds
    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

    component Para: Text {
        color: Theme.textDim
        font.pixelSize: 13
        lineHeight: 1.25
        wrapMode: Text.WordWrap
        Layout.fillWidth: true
        textFormat: Text.RichText
    }

    component Point: RowLayout {
        id: pt
        property string icon
        property string head
        property string body
        Layout.fillWidth: true
        spacing: 14
        Rectangle {
            Layout.alignment: Qt.AlignTop
            width: 36; height: 36; radius: 12
            color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.10)
            border.color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.4)
            Icon { anchors.centerIn: parent; name: pt.icon; width: 18; height: 18; color: Theme.accent }
        }
        ColumnLayout {
            spacing: 3
            Layout.fillWidth: true
            Text { text: pt.head; color: Theme.text; font.pixelSize: 14; font.weight: Font.DemiBold }
            Para { text: pt.body }
        }
    }

    ColumnLayout {
        id: layout
        x: 32
        y: 28
        width: root.width - 64
        spacing: Theme.gap

        ColumnLayout {
            spacing: 4
            Text { text: "How it works"; color: Theme.text; font.pixelSize: 28; font.weight: Font.Bold }
            Para { text: "A Bitcoin address is the end of a fixed pipeline. Orion runs it for every key of the range and compares the result with your targets." }
        }

        Card {
            Layout.fillWidth: true
            title: "From a private key to an address"
            subtitle: "The values are those of puzzle #30, solved long ago, so every step can be checked."
            PipelineDiagram {
                Layout.fillWidth: true
                Layout.preferredHeight: 230
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.gap

            Card {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredWidth: 1
                title: "What the GPU does for every key"
                Point {
                    icon: "gpu"
                    head: "Point additions in batches"
                    body: "Each GPU thread holds one point and reaches 1,024 neighbouring keys as P ± i·G. The slow modular inverse is shared by the whole batch: about 3.5 multiplications and one squaring per key."
                }
                Point {
                    icon: "spark"
                    head: "Native 32-bit arithmetic for Blackwell"
                    body: "Field multiplication uses IMAD.WIDE.U32 in Comba order and a branch-free modular add and subtract, tuned on sm_120."
                }
                Point {
                    icon: "target"
                    head: "Early exit in RIPEMD-160"
                    body: "For full addresses, one 32-bit word of the hash is compared before the last seven half-rounds; almost every key stops there."
                }
                Point {
                    icon: "shield"
                    head: "Every hit is checked again"
                    body: "The CPU rebuilds the private key, derives its address with its own code and reports it only on an exact match."
                }
                Item { Layout.fillHeight: true }   // keeps the points at the top when the other card is taller
            }

            Card {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredWidth: 1
                title: "Sequential or random"
                Point {
                    icon: "arrow"
                    head: "Sequential"
                    body: "Each GPU thread walks its own slice of the range in order. Progress is meaningful, saved, and a stopped search can be resumed. Use it for ranges you can finish."
                }
                Point {
                    icon: "dice"
                    head: "Random"
                    body: "At every step all threads jump to the same random offset inside their slice. Over a range too large to finish, where you look does not matter: each key is an independent draw, and two machines rarely repeat each other's work."
                }
                Point {
                    icon: "info"
                    head: "Why not Kangaroo or BSGS?"
                    body: "Those need the public key. An address that never spent has only its hash160 on the chain, so every candidate key must go through the full pipeline."
                }
                Item { Layout.fillHeight: true }
            }
        }

        Card {
            Layout.fillWidth: true
            title: "A lottery, not an investment"
            highlight: true
            accentColor: Theme.warn
            Para {
                text: {
                    var s = root.speed > 0 ? root.speed : 1200
                    var range = Math.pow(2, 70)
                    var perDay = s * 1e6 * 86400 / range
                    return "Puzzle #71 hides one key among 2<sup>70</sup> ≈ 1.2 × 10<sup>21</sup>. At "
                           + Fmt.speed(s).value + " " + Fmt.speed(s).unit
                           + (root.speed > 0 ? " (this machine's best speed)" : "")
                           + " the chance of finding it in a day is <b>" + Fmt.chance(perDay) + "</b>, and scanning the whole range would take "
                           + Fmt.duration(range / (s * 1e6)) + ". More GPUs multiply the chance by their number and nothing else. "
                           + "It is a fun way to put a GPU to work and to learn how this works, but the expected value is far below the cost of the electricity."
                }
            }
        }
    }
}
