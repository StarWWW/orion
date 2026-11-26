// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Orion contributors. See LICENSE.txt.
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Dialogs
import Orion
import "Fmt.js" as Fmt
import "Puzzles.js" as Puzzles

// Sets up a search: targets, key range, mode, GPU. Checks everything before it reaches the engine.
Flickable {
    id: root

    required property Search search
    signal startRequested(var config)
    signal message(string text, string kind)

    contentWidth: width
    contentHeight: layout.implicitHeight + 64
    clip: true
    boundsBehavior: Flickable.StopAtBounds
    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

    // ---- state ----------------------------------------------------------------------------
    property int rangeMode: 0          // 0 puzzle, 1 start + bits, 2 start + end
    property bool randomMode: true
    property var targetInfo: Engine.checkTargets(targets.text)
    property real lastSpeed: 0         // MK/s of the previous search, for estimates
    readonly property int gpuId: gpuBox.currentIndex >= 0 && Engine.gpus.length > 0
                                 ? Engine.gpus[gpuBox.currentIndex].id : 0
    property int backupTick: 0         // bumped when a search ends: the saved progress may have changed
    property var backup: { backupTick; return Engine.readBackup(gpuId) }

    readonly property var rangeInfo: {
        if (rangeMode === 0)
            return Engine.rangeFromBits(Engine.puzzleStart(puzzle.value), puzzle.value - 1)
        if (rangeMode === 1)
            return Engine.rangeFromBits(startField.text, bitsField.value)
        return Engine.rangeFromEnd(startField.text, endField.text)
    }
    readonly property real speedRef: search.peakSpeed > 0 ? search.peakSpeed : lastSpeed

    // ---- Bitcoin puzzles -------------------------------------------------------------------
    readonly property var puzzleInfo: Puzzles.list[puzzle.value - 1]
    property string autoTargets: ""    // what the puzzle selection last wrote into the targets box
    readonly property bool customTargets: targets.text.trim() !== "" && targets.text !== autoTargets

    function puzzleTargets(n) {
        return "# Bitcoin puzzle #" + n + "\n" + Puzzles.list[n - 1].address
    }

    // In puzzle mode the targets follow the selected puzzle, unless the user typed their own
    function followPuzzle(force) {
        if (rangeMode !== 0)
            return
        if (force || targets.text.trim() === "" || targets.text === autoTargets) {
            targets.text = puzzleTargets(puzzle.value)
            autoTargets = targets.text
        }
    }

    function openPuzzles() { picker.open() }

    function pickPuzzle(n) {
        rangeMode = 0
        puzzle.set(n)
        followPuzzle(true)
    }

    readonly property string blocker: {
        if (!Engine.executableFound) return "The orion engine was not found."
        if (search.active) return "A search is already running."
        if (Engine.listingGpus) return "Looking for GPUs…"
        if (Engine.gpus.length === 0) return "No CUDA GPU available."
        if (targetInfo.problems.length > 0) return "Fix the target lines marked above."
        if (targetInfo.addresses.length === 0) return "Enter at least one address or prefix."
        if (!rangeInfo.ok) return rangeInfo.error
        return ""
    }

    function config(resume) {
        return {
            addresses: targetInfo.addresses,
            start: rangeInfo.start,
            end: rangeInfo.end,
            random: randomMode,
            stop: stopSwitch.checked,
            resume: resume,
            gpuId: gpuId
        }
    }

    function save() {
        Engine.saveConfig({
            targets: targets.text, rangeMode: rangeMode, puzzle: puzzle.value,
            start: startField.text, bits: bitsField.value, end: endField.text,
            random: randomMode, stop: stopSwitch.checked, gpuId: gpuId,
            lastSpeed: Math.max(lastSpeed, search.peakSpeed)
        })
    }

    function startNow(resume) {
        if (blocker !== "") {
            message(blocker, "error")
            return
        }
        save()
        startRequested(config(resume))
    }

    function resumeBackup() {
        rangeMode = 2
        startField.text = backup.start
        endField.text = backup.end
        randomMode = false
        startNow(true)
    }

    Component.onCompleted: {
        var c = Engine.loadConfig()
        if (c.targets !== undefined) {
            targets.text = c.targets
            rangeMode = c.rangeMode
            puzzle.value = c.puzzle
            startField.text = c.start
            bitsField.value = c.bits
            endField.text = c.end
            randomMode = c.random
            stopSwitch.checked = c.stop
            lastSpeed = c.lastSpeed || 0
            if (targets.text === puzzleTargets(puzzle.value))
                autoTargets = targets.text
        }
        followPuzzle(false)
    }

    Connections {
        target: root.search
        function onEnded() {
            root.lastSpeed = Math.max(root.lastSpeed, root.search.peakSpeed)
            root.save()
            root.backupTick++
        }
    }

    // ---- small styled controls ---------------------------------------------------------------
    component Field: TextField {
        color: Theme.text
        placeholderTextColor: Theme.textFaint
        selectionColor: Theme.accent2
        selectedTextColor: "white"
        font.family: Theme.mono
        font.pixelSize: 14
        leftPadding: 12
        implicitHeight: 40
        property bool bad: false
        background: Rectangle {
            radius: 10
            color: Theme.field
            border.color: parent.bad ? Theme.bad : (parent.activeFocus ? Theme.accent : Theme.border)
            Behavior on border.color { ColorAnimation { duration: Theme.fast } }
        }
    }

    component Stepper: RowLayout {
        id: stepper
        property int value: 1
        property int from: 1
        property int to: 256
        spacing: 6
        function set(v) { value = Math.max(from, Math.min(to, v)) }
        IconButton { text: "−"; compact: true; onClicked: stepper.set(stepper.value - 1); autoRepeat: true }
        Field {
            Layout.preferredWidth: 76
            horizontalAlignment: Text.AlignHCenter
            leftPadding: 4
            text: stepper.value
            validator: IntValidator { bottom: stepper.from; top: stepper.to }
            onEditingFinished: stepper.set(parseInt(text) || stepper.from)
        }
        IconButton { text: "+"; compact: true; onClicked: stepper.set(stepper.value + 1); autoRepeat: true }
    }

    component Caption: Text {
        color: Theme.textDim
        font.pixelSize: 12
        wrapMode: Text.WordWrap
        Layout.fillWidth: true
    }

    component Chip: Rectangle {
        property alias text: chipLabel.text
        property color tint: Theme.accent
        implicitHeight: 24
        implicitWidth: chipLabel.implicitWidth + 18
        radius: 12
        color: Qt.rgba(tint.r, tint.g, tint.b, 0.12)
        border.color: Qt.rgba(tint.r, tint.g, tint.b, 0.5)
        Text {
            id: chipLabel
            anchors.centerIn: parent
            color: parent.tint
            font.pixelSize: 12
            font.weight: Font.DemiBold
        }
    }

    PuzzlePicker {
        id: picker
        current: puzzle.value
        onPicked: function (n) { root.pickPuzzle(n) }
    }

    component Toggle: Switch {
        id: sw
        indicator: Rectangle {
            implicitWidth: 44; implicitHeight: 24
            x: sw.leftPadding; y: (sw.height - height) / 2
            radius: 12
            color: sw.checked ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.35) : Theme.field
            border.color: sw.checked ? Theme.accent : Theme.border
            Behavior on color { ColorAnimation { duration: Theme.fast } }
            Rectangle {
                width: 18; height: 18; radius: 9
                y: 3
                x: sw.checked ? parent.width - width - 3 : 3
                color: sw.checked ? Theme.accent : Theme.textDim
                Behavior on x { NumberAnimation { duration: Theme.fast; easing.type: Easing.OutCubic } }
            }
        }
        contentItem: Text {
            text: sw.text
            color: Theme.text
            font.pixelSize: 13
            leftPadding: sw.indicator.width + 10
            verticalAlignment: Text.AlignVCenter
        }
    }

    FileDialog {
        id: targetsDialog
        title: "Load addresses"
        nameFilters: ["Text files (*.txt)", "All files (*)"]
        onAccepted: {
            var t = Engine.readTextFile(selectedFile)
            if (t === "") root.message("The file is empty or could not be read.", "error")
            else targets.text = t
        }
    }
    FileDialog {
        id: engineDialog
        title: "Locate the orion engine"
        onAccepted: Engine.setExecutable(selectedFile)
    }

    // ---- layout ----------------------------------------------------------------------------
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
                Text { text: "New search"; color: Theme.text; font.pixelSize: 28; font.weight: Font.Bold }
                Text {
                    text: "Choose what to look for and where. Orion checks every key of the range on the GPU and re-checks each hit on the CPU."
                    color: Theme.textDim; font.pixelSize: 13
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
            }
        }

        // Engine missing
        Card {
            visible: !Engine.executableFound
            Layout.fillWidth: true
            highlight: true
            accentColor: Theme.bad
            title: "The orion engine was not found"
            subtitle: (Engine.problem !== "" ? Engine.problem + " " : "")
                      + "Build it with python bench/build.py opt (Windows) or make (Linux), or point to it."
            headerItems: IconButton { text: "Locate…"; iconName: "folder"; compact: true; onClicked: engineDialog.open() }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.gap

            // ---------------- left column ----------------
            ColumnLayout {
                Layout.fillWidth: true
                Layout.preferredWidth: 3
                Layout.alignment: Qt.AlignTop
                spacing: Theme.gap

                Card {
                    Layout.fillWidth: true
                    title: "Targets"
                    subtitle: "One address or prefix per line: compressed P2PKH (1…) or P2WPKH (bc1q…). Lines starting with # are ignored."
                    headerItems: [
                        IconButton { text: "Load file…"; iconName: "file"; compact: true; onClicked: targetsDialog.open() },
                        IconButton {
                            text: "Demo"
                            iconName: "spark"
                            compact: true
                            ToolTip.visible: hovered
                            ToolTip.text: "Puzzle #30, solved long ago: a search that ends in about a second"
                            onClicked: {
                                root.pickPuzzle(30)
                                root.randomMode = false
                                stopSwitch.checked = true
                            }
                        }
                    ]

                    ScrollView {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 150
                        TextArea {
                            id: targets
                            color: Theme.text
                            placeholderText: "1PWo3JeB9jrGwfHDNpdGK54CRas7fsVzXU"
                            placeholderTextColor: Theme.textFaint
                            selectionColor: Theme.accent2
                            font.family: Theme.mono
                            font.pixelSize: 13
                            wrapMode: TextEdit.NoWrap
                            padding: 12
                            background: Rectangle {
                                radius: 10
                                color: Theme.field
                                border.color: root.targetInfo.problems.length > 0 ? Theme.bad
                                              : (targets.activeFocus ? Theme.accent : Theme.border)
                            }
                        }
                    }

                    Flow {
                        Layout.fillWidth: true
                        spacing: 8
                        Repeater {
                            model: {
                                var t = root.targetInfo, chips = []
                                if (t.addresses.length > 0)
                                    chips.push((t.addresses.length) + (t.addresses.length === 1 ? " target" : " targets"))
                                if (t.type === "p2pkh") chips.push("P2PKH (1…)")
                                if (t.type === "bech32") chips.push("P2WPKH (bc1q…)")
                                if (t.full > 0) chips.push(t.full + " full")
                                if (t.prefixes > 0) chips.push(t.prefixes + (t.prefixes === 1 ? " prefix" : " prefixes"))
                                if (t.full === 1 && t.prefixes === 0) chips.push("fastest path: single target")
                                return chips
                            }
                            delegate: Rectangle {
                                required property string modelData
                                height: 26
                                width: chipText.implicitWidth + 20
                                radius: 13
                                color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.10)
                                border.color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.35)
                                Text {
                                    id: chipText
                                    anchors.centerIn: parent
                                    text: parent.modelData
                                    color: Theme.text
                                    font.pixelSize: 12
                                }
                            }
                        }
                    }

                    Repeater {
                        model: root.targetInfo.problems
                        delegate: RowLayout {
                            required property string modelData
                            Layout.fillWidth: true
                            spacing: 8
                            Icon { name: "warning"; size: 15; color: Theme.bad }
                            Text {
                                text: parent.modelData
                                color: Theme.bad
                                font.pixelSize: 12
                                wrapMode: Text.WordWrap
                                Layout.fillWidth: true
                            }
                        }
                    }
                }

                Card {
                    Layout.fillWidth: true
                    title: "Key range"
                    subtitle: "Puzzle N keeps its key between 2^(N−1) and 2^N − 1."

                    Segmented {
                        Layout.fillWidth: true
                        model: [{ text: "Puzzle", icon: "target" }, { text: "Start + bits", icon: "arrow" },
                                { text: "Start – end", icon: "setup" }]
                        currentIndex: root.rangeMode
                        onActivated: function (i) {
                            if (root.rangeInfo.ok && i !== 0) {
                                startField.text = root.rangeInfo.start
                                endField.text = root.rangeInfo.end
                            }
                            root.rangeMode = i
                            root.followPuzzle(false)
                        }
                    }

                    GridLayout {
                        Layout.fillWidth: true
                        columns: 2
                        columnSpacing: 16
                        rowSpacing: 10

                        Text { visible: root.rangeMode === 0; text: "Puzzle"; color: Theme.textDim; font.pixelSize: 13 }
                        RowLayout {
                            visible: root.rangeMode === 0
                            spacing: 14
                            Stepper {
                                id: puzzle
                                from: 1; to: Puzzles.list.length; value: 71
                                onValueChanged: root.followPuzzle(false)
                            }
                            Caption {
                                text: "keys 0x" + Engine.puzzleStart(puzzle.value) + " … 2^" + puzzle.value + " − 1"
                                font.family: Theme.mono
                                elide: Text.ElideRight
                                wrapMode: Text.NoWrap
                            }
                        }

                        // What is known about the selected puzzle
                        Item { visible: root.rangeMode === 0; Layout.preferredWidth: 1 }
                        ColumnLayout {
                            visible: root.rangeMode === 0
                            Layout.fillWidth: true
                            spacing: 8

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 8
                                Text {
                                    text: root.puzzleInfo.address
                                    color: Theme.text
                                    font.family: Theme.mono
                                    font.pixelSize: 13
                                    Layout.fillWidth: true
                                    elide: Text.ElideRight
                                }
                                Chip {
                                    text: root.puzzleInfo.solved ? "solved" : "open · " + root.puzzleInfo.balance.toFixed(2) + " BTC"
                                    tint: root.puzzleInfo.solved ? Theme.textDim : Theme.gold
                                }
                                Chip {
                                    visible: !root.puzzleInfo.solved && root.puzzleInfo.pubkey
                                    text: "public key known"
                                    tint: Theme.warn
                                }
                            }
                            Caption {
                                text: root.puzzleInfo.solved
                                      ? "Solved, its prize is gone. Handy for testing: the key is known to be in this range."
                                      : root.puzzleInfo.pubkey
                                        ? "Its public key is on the chain, so Kangaroo or BSGS find the key in about √N steps: far faster than any key scan, including this one."
                                        : "Unsolved and no public key on the chain: scanning keys is the only way. Mind the odds below."
                                color: root.puzzleInfo.pubkey && !root.puzzleInfo.solved ? Theme.warn : Theme.textDim
                            }
                            RowLayout {
                                spacing: 8
                                IconButton {
                                    visible: root.customTargets
                                    text: "Use this address"
                                    iconName: "target"
                                    compact: true
                                    ToolTip.visible: hovered
                                    ToolTip.text: "Replace the targets above with puzzle #" + puzzle.value + "'s address"
                                    onClicked: root.followPuzzle(true)
                                }
                                IconButton {
                                    text: "All " + Puzzles.list.length + " puzzles…"
                                    iconName: "learn"
                                    compact: true
                                    onClicked: picker.open()
                                }
                            }
                        }

                        Text { visible: root.rangeMode > 0; text: "Start"; color: Theme.textDim; font.pixelSize: 13 }
                        Field {
                            id: startField
                            visible: root.rangeMode > 0
                            Layout.fillWidth: true
                            placeholderText: "hex, e.g. 400000000000000000"
                            text: "400000000000000000"
                            bad: root.rangeMode > 0 && !root.rangeInfo.ok && root.rangeInfo.error.indexOf("Start") === 0
                        }

                        Text { visible: root.rangeMode === 1; text: "Bits"; color: Theme.textDim; font.pixelSize: 13 }
                        RowLayout {
                            visible: root.rangeMode === 1
                            spacing: 14
                            Stepper { id: bitsField; from: 1; to: 256; value: 40 }
                            Caption { text: "scans start … start + 2^" + bitsField.value + " − 1" }
                        }

                        Text { visible: root.rangeMode === 2; text: "End"; color: Theme.textDim; font.pixelSize: 13 }
                        Field {
                            id: endField
                            visible: root.rangeMode === 2
                            Layout.fillWidth: true
                            placeholderText: "hex, last key (inclusive)"
                            text: "7FFFFFFFFFFFFFFFFF"
                            bad: root.rangeMode === 2 && !root.rangeInfo.ok && root.rangeInfo.error.indexOf("Start") !== 0
                        }
                    }

                    // Summary of the range
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: summary.implicitHeight + 24
                        radius: 12
                        color: root.rangeInfo.ok ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.06)
                                                 : Qt.rgba(Theme.bad.r, Theme.bad.g, Theme.bad.b, 0.08)
                        border.color: root.rangeInfo.ok ? Theme.border : Theme.bad
                        RowLayout {
                            id: summary
                            anchors.fill: parent
                            anchors.margins: 12
                            spacing: 22
                            ColumnLayout {
                                visible: root.rangeInfo.ok
                                spacing: 2
                                Text { text: "SIZE"; color: Theme.textFaint; font.pixelSize: 10; font.letterSpacing: 1.2 }
                                Text {
                                    text: root.rangeInfo.ok ? Fmt.pow2(root.rangeInfo.size) + " keys" : ""
                                    textFormat: Text.RichText
                                    color: Theme.text; font.pixelSize: 18; font.weight: Font.DemiBold
                                }
                                Text { text: root.rangeInfo.ok ? Fmt.big(root.rangeInfo.size) : ""; color: Theme.textDim; font.pixelSize: 12 }
                            }
                            ColumnLayout {
                                visible: root.rangeInfo.ok
                                spacing: 2
                                Layout.fillWidth: true
                                Text {
                                    text: root.speedRef > 0 ? "AT " + Fmt.speed(root.speedRef).value + " " + Fmt.speed(root.speedRef).unit
                                                            : "ESTIMATE"
                                    color: Theme.textFaint; font.pixelSize: 10; font.letterSpacing: 1.2
                                }
                                Text {
                                    text: root.speedRef > 0
                                          ? Fmt.duration(root.rangeInfo.size / (root.speedRef * 1e6)) + " to scan it all"
                                          : "run a search once to see time estimates"
                                    color: Theme.text; font.pixelSize: 15
                                    Layout.fillWidth: true
                                    elide: Text.ElideRight
                                }
                                Text {
                                    visible: root.speedRef > 0
                                    text: "chance per day for one target: " + Fmt.chance(root.speedRef * 1e6 * 86400 / root.rangeInfo.size)
                                    color: Theme.textDim; font.pixelSize: 12
                                    Layout.fillWidth: true
                                    elide: Text.ElideRight
                                }
                            }
                            RowLayout {
                                visible: !root.rangeInfo.ok
                                spacing: 8
                                Icon { name: "warning"; size: 16; color: Theme.bad }
                                Text { text: root.rangeInfo.error || ""; color: Theme.bad; font.pixelSize: 13 }
                            }
                            Text {
                                visible: root.rangeInfo.ok && root.rangeInfo.clipped
                                text: "clipped to the curve order"
                                color: Theme.warn; font.pixelSize: 12
                            }
                        }
                    }
                }
            }

            // ---------------- right column ----------------
            ColumnLayout {
                Layout.fillWidth: true
                Layout.preferredWidth: 2
                Layout.alignment: Qt.AlignTop
                spacing: Theme.gap

                Card {
                    Layout.fillWidth: true
                    title: "Mode"

                    Segmented {
                        Layout.fillWidth: true
                        model: [{ text: "Random", icon: "dice" }, { text: "Sequential", icon: "arrow" }]
                        currentIndex: root.randomMode ? 0 : 1
                        onActivated: function (i) { root.randomMode = (i === 0) }
                    }
                    Caption {
                        text: root.randomMode
                              ? "Every step jumps to a fresh random offset in each GPU thread's slice. Best for ranges far too large to finish, like the open puzzles. Keeps no progress."
                              : "Walks the range in order. Progress is saved every 60 launches, on pause and on stop, so the search can be resumed later."
                    }
                    Toggle {
                        id: stopSwitch
                        checked: true
                        text: "Stop once every target is found"
                    }
                }

                Card {
                    Layout.fillWidth: true
                    title: "GPU"
                    headerItems: IconButton {
                        iconName: "refresh"
                        compact: true
                        enabled: !Engine.listingGpus
                        ToolTip.visible: hovered
                        ToolTip.text: "List the GPUs again"
                        onClicked: Engine.refreshGpus()
                    }

                    ComboBox {
                        id: gpuBox
                        Layout.fillWidth: true
                        visible: Engine.gpus.length > 0
                        model: Engine.gpus
                        textRole: "name"
                        font.pixelSize: 13
                    }
                    Caption {
                        visible: Engine.gpus.length > 0 && gpuBox.currentIndex >= 0
                        text: {
                            var g = Engine.gpus[gpuBox.currentIndex]
                            return g ? "sm_" + g.sm + " · " + g.sms + " SMs · " + (g.memory / 1073741824).toFixed(1) + " GB" : ""
                        }
                    }
                    Caption {
                        visible: Engine.gpus.length === 0
                        color: Engine.listingGpus ? Theme.textDim : Theme.bad
                        text: Engine.listingGpus ? "Looking for GPUs…" : Engine.gpuError
                    }
                }

                Card {
                    visible: root.backup.exists === true
                    Layout.fillWidth: true
                    highlight: true
                    accentColor: Theme.warn
                    title: "Saved progress"
                    subtitle: "A sequential search on this GPU can be resumed."

                    Caption {
                        font.family: Theme.mono
                        text: root.backup.exists ? "0x" + Fmt.shortHex(root.backup.start, 10) + "\n… 0x" + Fmt.shortHex(root.backup.end, 10) : ""
                    }
                    Rectangle {
                        Layout.fillWidth: true
                        height: 8
                        radius: 4
                        color: Theme.field
                        Rectangle {
                            height: parent.height
                            radius: 4
                            width: root.backup.exists ? Math.max(4, parent.width * root.backup.percent / 100) : 0
                            color: Theme.warn
                        }
                    }
                    Caption {
                        text: root.backup.exists
                              ? Fmt.percent(root.backup.percent) + " scanned · " + Fmt.duration(root.backup.elapsed) + " of run time"
                              : ""
                    }
                    IconButton {
                        text: "Resume this search"
                        iconName: "play"
                        Layout.fillWidth: true
                        enabled: !root.search.active && Engine.executableFound && root.targetInfo.addresses.length > 0
                                 && root.targetInfo.problems.length === 0
                        onClicked: root.resumeBackup()
                    }
                    Caption {
                        visible: root.targetInfo.addresses.length === 0
                        color: Theme.warn
                        text: "The saved file records the range, not the targets: enter the same targets to resume."
                    }
                }

                // Start
                Card {
                    Layout.fillWidth: true
                    highlight: root.blocker === ""

                    IconButton {
                        id: startButton
                        Layout.fillWidth: true
                        implicitHeight: 54
                        kind: "primary"
                        iconName: "play"
                        text: root.search.active ? "Search running" : "Start search"
                        font.pixelSize: 16
                        enabled: root.blocker === ""
                        onClicked: root.startNow(false)

                        SequentialAnimation on scale {
                            running: startButton.enabled && !startButton.hovered
                            loops: Animation.Infinite
                            NumberAnimation { to: 1.015; duration: 1100; easing.type: Easing.InOutSine }
                            NumberAnimation { to: 1.0; duration: 1100; easing.type: Easing.InOutSine }
                        }
                    }
                    Caption {
                        horizontalAlignment: Text.AlignHCenter
                        color: root.blocker === "" ? Theme.textDim : Theme.warn
                        text: root.blocker !== "" ? root.blocker
                              : (!root.randomMode && root.backup.exists
                                 ? "A new sequential search moves the saved progress to orion_backup_gpu" + root.gpuId + ".dat.prev."
                                 : "Found keys are also written to found.txt in the data folder.")
                    }
                }

                Card {
                    Layout.fillWidth: true
                    title: "Engine"
                    headerItems: IconButton { text: "Change…"; compact: true; onClicked: engineDialog.open() }
                    Caption {
                        text: Engine.executableFound ? Engine.executable + "\nversion " + Engine.version : "not found"
                        font.family: Theme.mono
                        wrapMode: Text.WrapAnywhere
                    }
                    RowLayout {
                        spacing: 8
                        IconButton { text: "Data folder"; iconName: "folder"; compact: true; onClicked: Engine.openDataDir() }
                    }
                }
            }
        }
    }
}
