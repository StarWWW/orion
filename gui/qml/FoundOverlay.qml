// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Orion contributors. See LICENSE.txt.
import QtQuick
import QtQuick.Layouts
import QtQuick.Particles
import Orion

// Full-window announcement of a found key, with a burst of stars.
Rectangle {
    id: root

    signal viewRequested()

    function show(address) {
        addressText.text = address
        visible = true
        appear.restart()
        burst.burst(160)
    }

    color: Qt.rgba(0.01, 0.02, 0.06, 0.78)
    visible: false
    opacity: 0

    MouseArea { anchors.fill: parent }   // modal: keeps clicks from the page below

    ParticleSystem { id: system }
    ImageParticle {
        system: system
        source: "qrc:///particleresources/star.png"
        color: Theme.gold
        colorVariation: 0.35
        alpha: 0.9
        entryEffect: ImageParticle.Fade
    }
    Emitter {
        id: burst
        system: system
        anchors.centerIn: parent
        width: 10; height: 10
        enabled: false
        lifeSpan: 2600
        lifeSpanVariation: 800
        size: 22
        sizeVariation: 14
        endSize: 4
        velocity: AngleDirection { angleVariation: 360; magnitude: 260; magnitudeVariation: 160 }
        acceleration: PointDirection { y: 60 }
    }

    Rectangle {
        id: panel
        anchors.centerIn: parent
        width: Math.min(620, root.width - 80)
        height: content.implicitHeight + 64
        radius: 22
        color: Qt.rgba(0.07, 0.1, 0.22, 0.97)
        border.width: 1
        border.color: Theme.gold

        ColumnLayout {
            id: content
            anchors.fill: parent
            anchors.margins: 32
            spacing: 14

            Icon {
                Layout.alignment: Qt.AlignHCenter
                name: "key"
                size: 52
                color: Theme.gold
                RotationAnimation on rotation {
                    running: root.visible
                    from: -12; to: 12
                    duration: 900
                    loops: Animation.Infinite
                    easing.type: Easing.InOutSine
                    direction: RotationAnimation.Shortest
                }
            }
            Text {
                Layout.alignment: Qt.AlignHCenter
                text: "Key found"
                color: Theme.gold
                font.pixelSize: 30
                font.weight: Font.Bold
            }
            Text {
                id: addressText
                Layout.alignment: Qt.AlignHCenter
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                elide: Text.ElideMiddle
                color: Theme.text
                font.family: Theme.mono
                font.pixelSize: 15
            }
            Text {
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                text: "The key was checked independently on the CPU and saved to found.txt. "
                      + "Keep it offline and read the safety notes before spending anything."
                color: Theme.textDim
                font.pixelSize: 13
            }
            RowLayout {
                Layout.alignment: Qt.AlignHCenter
                spacing: 12
                IconButton {
                    text: "View key"
                    iconName: "key"
                    kind: "primary"
                    onClicked: {
                        root.viewRequested()
                        hide.restart()
                    }
                }
                IconButton {
                    text: "Close"
                    onClicked: hide.restart()
                }
            }
        }
    }

    ParallelAnimation {
        id: appear
        NumberAnimation { target: root; property: "opacity"; from: 0; to: 1; duration: 220 }
        NumberAnimation { target: panel; property: "scale"; from: 0.8; to: 1; duration: 420; easing.type: Easing.OutBack }
    }
    SequentialAnimation {
        id: hide
        NumberAnimation { target: root; property: "opacity"; to: 0; duration: 220 }
        PropertyAction { target: root; property: "visible"; value: false }
    }
}
