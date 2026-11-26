// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Orion contributors. See LICENSE.txt.
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Orion
import "Fmt.js" as Fmt
import "Puzzles.js" as Puzzles

// All Bitcoin puzzles with their address, key range and state on the chain; a click selects one.
Popup {
    id: root

    property int current: 0
    signal picked(int n)

    property int filter: 0             // 0 open, 1 solved, 2 all
    readonly property int openCount: Puzzles.list.filter(function (p) { return !p.solved }).length
    readonly property var rows: Puzzles.list.filter(function (p) {
        return filter === 2 || (filter === 0) === !p.solved
    })

    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(900, parent ? parent.width - 80 : 900)
    height: Math.min(760, parent ? parent.height - 80 : 760)
    modal: true
    focus: true
    padding: 24

    onOpened: {
        filter = Puzzles.list[current - 1] && Puzzles.list[current - 1].solved ? 1 : 0
        Qt.callLater(function () {
            for (var i = 0; i < rows.length; i++)
                if (rows[i].n === current) { list.positionViewAtIndex(i, ListView.Center); break }
        })
    }

    enter: Transition {
        ParallelAnimation {
            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 180 }
            NumberAnimation { property: "scale"; from: 0.94; to: 1; duration: 280; easing.type: Easing.OutCubic }
        }
    }
    exit: Transition {
        NumberAnimation { property: "opacity"; to: 0; duration: 140 }
    }

    Overlay.modal: Rectangle { color: Qt.rgba(0.01, 0.02, 0.06, 0.72) }

    background: Rectangle {
        radius: 20
        color: Qt.rgba(0.06, 0.09, 0.2, 0.98)
        border.color: Theme.borderHi
    }

    contentItem: ColumnLayout {
        spacing: 14

        RowLayout {
            Layout.fillWidth: true
            spacing: 12
            ColumnLayout {
                spacing: 4
                Layout.fillWidth: true
                Text { text: "Bitcoin puzzles"; color: Theme.text; font.pixelSize: 22; font.weight: Font.Bold }
                Text {
                    text: "Puzzle N keeps its key between 2^(N−1) and 2^N − 1. " + root.openCount + " of "
                          + Puzzles.list.length + " still hold a prize. State of the blockchain on " + Puzzles.asOf + "."
                    color: Theme.textDim
                    font.pixelSize: 12
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
            }
            IconButton { iconName: "close"; compact: true; kind: "ghost"; onClicked: root.close() }
        }

        Segmented {
            Layout.preferredWidth: 420
            model: [{ text: "Open (" + root.openCount + ")", icon: "target" },
                    { text: "Solved (" + (Puzzles.list.length - root.openCount) + ")", icon: "check" },
                    { text: "All", icon: "learn" }]
            currentIndex: root.filter
            onActivated: function (i) { root.filter = i }
        }

        // Column titles
        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 14
            Layout.rightMargin: 22
            spacing: 14
            Text { text: "#"; color: Theme.textFaint; font.pixelSize: 11; Layout.preferredWidth: 44 }
            Text { text: "ADDRESS AND KEY RANGE"; color: Theme.textFaint; font.pixelSize: 11; font.letterSpacing: 1; Layout.fillWidth: true }
            Text { text: "PRIZE"; color: Theme.textFaint; font.pixelSize: 11; font.letterSpacing: 1; Layout.preferredWidth: 110; horizontalAlignment: Text.AlignRight }
        }

        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 4
            model: root.rows
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

            add: Transition { NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 200 } }

            delegate: Rectangle {
                id: row
                required property var modelData
                readonly property bool selected: modelData.n === root.current
                width: list.width - 10
                height: 56
                radius: 12
                color: selected ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.14)
                     : (mouse.containsMouse ? Qt.rgba(1, 1, 1, 0.05) : "transparent")
                border.color: selected ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.6) : "transparent"
                Behavior on color { ColorAnimation { duration: Theme.fast } }

                MouseArea {
                    id: mouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        root.picked(row.modelData.n)
                        root.close()
                    }
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 14
                    anchors.rightMargin: 14
                    spacing: 14

                    Text {
                        text: row.modelData.n
                        color: row.modelData.solved ? Theme.textDim : Theme.text
                        font.pixelSize: 17
                        font.weight: Font.Bold
                        Layout.preferredWidth: 44
                    }
                    ColumnLayout {
                        spacing: 3
                        Layout.fillWidth: true
                        Text {
                            text: row.modelData.address
                            color: row.modelData.solved ? Theme.textDim : Theme.text
                            font.family: Theme.mono
                            font.pixelSize: 13
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                        }
                        Text {
                            text: "2<sup>" + (row.modelData.n - 1) + "</sup> keys from 0x" + Engine.puzzleStart(row.modelData.n)
                            textFormat: Text.RichText
                            color: Theme.textFaint
                            font.pixelSize: 11
                        }
                    }
                    Rectangle {
                        visible: !row.modelData.solved && row.modelData.pubkey
                        implicitHeight: 22
                        implicitWidth: pkText.implicitWidth + 16
                        radius: 11
                        color: Qt.rgba(Theme.warn.r, Theme.warn.g, Theme.warn.b, 0.12)
                        border.color: Qt.rgba(Theme.warn.r, Theme.warn.g, Theme.warn.b, 0.5)
                        Text {
                            id: pkText
                            anchors.centerIn: parent
                            text: "public key known"
                            color: Theme.warn
                            font.pixelSize: 11
                        }
                    }
                    Text {
                        text: row.modelData.solved ? "solved" : row.modelData.balance.toFixed(2) + " BTC"
                        color: row.modelData.solved ? Theme.textFaint : Theme.gold
                        font.pixelSize: 14
                        font.weight: row.modelData.solved ? Font.Normal : Font.DemiBold
                        Layout.preferredWidth: 110
                        horizontalAlignment: Text.AlignRight
                    }
                }
            }
        }

        Text {
            text: "Addresses from the puzzle transaction itself, state from the blockchain. Check a block explorer "
                  + "before a long run: a puzzle can be solved at any time. Refresh the list with python gui/puzzles.py."
            color: Theme.textFaint
            font.pixelSize: 11
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }
    }
}
