// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Orion contributors. See LICENSE.txt.
import QtQuick
import QtQuick.Layouts
import Orion

// Left navigation: logo, pages, and the search state at the bottom.
Rectangle {
    id: root

    property int currentIndex: 0
    property int foundCount: 0
    property string searchState: "idle"
    property string engineVersion
    readonly property var pages: [
        { name: "Search", icon: "setup" },
        { name: "Monitor", icon: "monitor" },
        { name: "Found", icon: "key" },
        { name: "Learn", icon: "learn" }
    ]

    implicitWidth: 96
    color: Qt.rgba(0.03, 0.05, 0.12, 0.72)

    Rectangle {
        anchors { top: parent.top; bottom: parent.bottom; right: parent.right }
        width: 1
        color: Theme.border
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.topMargin: 22
        anchors.bottomMargin: 18
        spacing: 6

        // Logo: Orion's belt
        Item {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: 52
            Layout.preferredHeight: 52
            Rectangle {
                anchors.fill: parent
                radius: 16
                gradient: Gradient {
                    GradientStop { position: 0; color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.25) }
                    GradientStop { position: 1; color: Qt.rgba(Theme.accent2.r, Theme.accent2.g, Theme.accent2.b, 0.25) }
                }
                border.color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.5)
            }
            Repeater {
                model: 3
                delegate: Rectangle {
                    required property int index
                    width: 8; height: 8; radius: 4
                    color: "white"
                    x: 13 + index * 10
                    y: 30 - index * 6
                    SequentialAnimation on opacity {
                        loops: Animation.Infinite
                        PauseAnimation { duration: index * 400 }
                        NumberAnimation { to: 0.45; duration: 1200; easing.type: Easing.InOutSine }
                        NumberAnimation { to: 1.0; duration: 1200; easing.type: Easing.InOutSine }
                    }
                }
            }
        }
        Text {
            Layout.alignment: Qt.AlignHCenter
            text: "ORION"
            color: Theme.text
            font.pixelSize: 12
            font.letterSpacing: 3
            font.weight: Font.Bold
        }
        Item { Layout.preferredHeight: 18 }

        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: navColumn.height

            // Active page marker slides between entries
            Rectangle {
                width: 3
                height: 36
                radius: 1.5
                x: 0
                y: root.currentIndex * 76 + 18
                color: Theme.accent
                Behavior on y { NumberAnimation { duration: Theme.normal; easing.type: Easing.OutCubic } }
            }

            Column {
                id: navColumn
                width: parent.width
                Repeater {
                    model: root.pages
                    delegate: MouseArea {
                        id: entry
                        required property var modelData
                        required property int index
                        readonly property bool current: index === root.currentIndex
                        width: navColumn.width
                        height: 76
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.currentIndex = index

                        Rectangle {
                            anchors.centerIn: parent
                            width: 64; height: 64
                            radius: 16
                            color: entry.current ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.12)
                                                 : (entry.containsMouse ? Qt.rgba(1, 1, 1, 0.05) : "transparent")
                            Behavior on color { ColorAnimation { duration: Theme.fast } }
                        }
                        Column {
                            anchors.centerIn: parent
                            spacing: 5
                            Icon {
                                anchors.horizontalCenter: parent.horizontalCenter
                                name: entry.modelData.icon
                                width: 22; height: 22
                                color: entry.current ? Theme.accent : (entry.containsMouse ? Theme.text : Theme.textDim)
                            }
                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: entry.modelData.name
                                font.pixelSize: 11
                                font.weight: entry.current ? Font.DemiBold : Font.Normal
                                color: entry.current ? Theme.text : Theme.textDim
                            }
                        }
                        // Found-key badge
                        Rectangle {
                            visible: entry.index === 2 && root.foundCount > 0
                            x: parent.width / 2 + 6
                            y: 10
                            width: Math.max(18, badgeText.implicitWidth + 8)
                            height: 18
                            radius: 9
                            color: Theme.gold
                            Text {
                                id: badgeText
                                anchors.centerIn: parent
                                text: root.foundCount
                                color: Theme.bg
                                font.pixelSize: 11
                                font.weight: Font.Bold
                            }
                            SequentialAnimation on scale {
                                running: root.foundCount > 0
                                loops: Animation.Infinite
                                NumberAnimation { to: 1.15; duration: 600 }
                                NumberAnimation { to: 1.0; duration: 600 }
                            }
                        }
                    }
                }
            }
        }

        Item { Layout.fillHeight: true }

        // Search state
        Column {
            Layout.alignment: Qt.AlignHCenter
            spacing: 6
            Rectangle {
                id: stateDot
                anchors.horizontalCenter: parent.horizontalCenter
                width: 10; height: 10; radius: 5
                color: Theme.stateColor(root.searchState)
                Behavior on color { ColorAnimation { duration: Theme.normal } }
                SequentialAnimation on opacity {
                    running: root.searchState === "running"
                    loops: Animation.Infinite
                    onStopped: stateDot.opacity = 1
                    NumberAnimation { to: 0.3; duration: 700 }
                    NumberAnimation { to: 1.0; duration: 700 }
                }
            }
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                width: 84
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                text: Theme.stateText(root.searchState)
                color: Theme.textDim
                font.pixelSize: 10
            }
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: root.engineVersion !== "" ? "engine v" + root.engineVersion : "no engine"
                color: root.engineVersion !== "" ? Theme.textFaint : Theme.bad
                font.pixelSize: 10
            }
        }
    }
}
