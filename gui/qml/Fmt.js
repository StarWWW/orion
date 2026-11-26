.pragma library
// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Orion contributors. See LICENSE.txt.
// Number and time formatting for the UI (English, the language of the whole project).

var SUP = { "0": "⁰", "1": "¹", "2": "²", "3": "³", "4": "⁴", "5": "⁵",
            "6": "⁶", "7": "⁷", "8": "⁸", "9": "⁹", "-": "⁻" }

function group(n) {
    // 1234567 -> "1,234,567" (independent of the system locale)
    var s = Math.round(n).toString()
    var neg = s.charAt(0) === "-"
    if (neg) s = s.substring(1)
    var out = ""
    while (s.length > 3) {
        out = "," + s.substring(s.length - 3) + out
        s = s.substring(0, s.length - 3)
    }
    return (neg ? "-" : "") + s + out
}

function superscript(n) {
    var s = String(n), r = ""
    for (var i = 0; i < s.length; i++) r += SUP[s.charAt(i)] || s.charAt(i)
    return r
}

// 1.18e21 -> "1.18 × 10²¹", small numbers grouped
function big(n) {
    if (!isFinite(n)) return "∞"
    if (Math.abs(n) < 1e9) return group(n)
    var e = Math.floor(Math.log(n) / Math.LN10)
    var m = n / Math.pow(10, e)
    if (m >= 9.995) { m /= 10; e += 1 }
    return m.toFixed(2) + " × 10" + superscript(e)
}

function log2(n) {
    return n >= 1 ? Math.log(n) / Math.LN2 : 0
}

// "2<sup>70</sup>" or "2<sup>34.75</sup>" for a Text with textFormat: Text.StyledText
function pow2(n) {
    var e = log2(n)
    return "2<sup>" + (Math.abs(e - Math.round(e)) < 0.005 ? Math.round(e) : e.toFixed(2)) + "</sup>"
}

// Keys checked, e.g. "1.28 T keys"
function keys(n) {
    var units = [[1e15, "P"], [1e12, "T"], [1e9, "G"], [1e6, "M"], [1e3, "k"]]
    for (var i = 0; i < units.length; i++)
        if (n >= units[i][0]) return (n / units[i][0]).toFixed(n / units[i][0] >= 100 ? 0 : 2) + " " + units[i][1]
    return group(n)
}

// MK/s -> {value, unit}
function speed(mks) {
    if (mks >= 1000) return { value: (mks / 1000).toFixed(mks >= 10000 ? 1 : 2), unit: "GK/s" }
    return { value: mks.toFixed(1), unit: "MK/s" }
}

function pad2(n) { return (n < 10 ? "0" : "") + n }

// seconds -> "04:12:09", "3d 04:12:09", "31,000 years"; "—" when unknown
function duration(t) {
    if (t === undefined || t === null || !(t >= 0) || !isFinite(t)) return "—"
    var years = t / (365.25 * 86400)
    if (years >= 100) return (years >= 1e9 ? big(years) : group(years)) + " years"
    var s = Math.floor(t)
    var d = Math.floor(s / 86400)
    var h = Math.floor((s % 86400) / 3600)
    var m = Math.floor((s % 3600) / 60)
    var sec = s % 60
    return (d > 0 ? d + "d " : "") + pad2(h) + ":" + pad2(m) + ":" + pad2(sec)
}

// Progress of a huge range: enough decimals to see it move
function percent(p) {
    if (!(p > 0)) return "0%"
    if (p >= 100) return "100%"
    if (p >= 1) return p.toFixed(2) + "%"
    var e = Math.floor(Math.log(p) / Math.LN10)
    if (e < -6) {
        var m = p / Math.pow(10, e)
        return m.toFixed(2) + " × 10" + superscript(e) + " %"
    }
    return p.toFixed(Math.max(2, 2 - e)) + "%"
}

// Probability p (0..1) as "1 in 1.4 million" or "37.2%"
function chance(p) {
    if (!(p > 0)) return "—"
    if (p >= 0.01) return (Math.min(p, 1) * 100).toFixed(p >= 0.1 ? 1 : 2) + "%"
    var inv = 1 / p
    var words = [[1e12, "trillion"], [1e9, "billion"], [1e6, "million"], [1e3, "thousand"]]
    for (var i = 0; i < words.length; i++)
        if (inv >= words[i][0] && inv < words[i][0] * 1000)
            return "1 in " + (inv / words[i][0]).toFixed(1) + " " + words[i][1]
    return "1 in " + big(inv)
}

// "0000…3D94CD64" style shortening of long hex keys
function shortHex(h, keep) {
    keep = keep || 10
    if (!h || h.length <= keep * 2 + 1) return h
    return h.substring(0, keep) + "…" + h.substring(h.length - keep)
}
