// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Orion contributors. See LICENSE.txt.
#pragma once

#include <QString>
#include <cmath>
#include <cstdint>

// Unsigned 256-bit integer, just enough to check and size a private-key range given in hex.
struct U256 {
    uint64_t w[4] = {0, 0, 0, 0};   // little-endian limbs

    // false for an empty string, a non-hex digit or more than 64 digits
    static bool fromHex(QString s, U256 &out) {
        s = s.trimmed();
        if (s.startsWith(QLatin1String("0x"), Qt::CaseInsensitive))
            s = s.mid(2);
        if (s.isEmpty() || s.size() > 64)
            return false;
        out = U256();
        for (int i = 0; i < s.size(); i++) {
            // ASCII hex digits only (QChar::digitValue() would also take other scripts' digits)
            const char c = s.at(s.size() - 1 - i).toLatin1();
            int d;
            if (c >= 'a' && c <= 'f') d = c - 'a' + 10;
            else if (c >= 'A' && c <= 'F') d = c - 'A' + 10;
            else if (c >= '0' && c <= '9') d = c - '0';
            else return false;
            out.w[i / 16] |= (uint64_t)d << (4 * (i % 16));
        }
        return true;
    }

    QString toHex() const {
        static const char digits[] = "0123456789ABCDEF";
        QString r;
        for (int i = 63; i >= 0; i--) {
            int d = (int)((w[i / 16] >> (4 * (i % 16))) & 0xF);
            if (d || !r.isEmpty() || i == 0)
                r.append(QLatin1Char(digits[d]));
        }
        return r;
    }

    static U256 pow2(int bits) {          // 2^bits for 0 <= bits < 256
        U256 r;
        r.w[bits / 64] = 1ULL << (bits % 64);
        return r;
    }

    // this + b, false on overflow past 2^256
    bool add(const U256 &b, U256 &r) const {
        unsigned carry = 0;
        for (int i = 0; i < 4; i++) {
            uint64_t s = w[i] + b.w[i];
            unsigned c1 = s < w[i];
            uint64_t t = s + carry;
            unsigned c2 = t < s;
            r.w[i] = t;
            carry = c1 | c2;
        }
        return carry == 0;
    }

    U256 sub(const U256 &b) const {      // this - b, this >= b
        U256 r;
        unsigned borrow = 0;
        for (int i = 0; i < 4; i++) {
            uint64_t d = w[i] - b.w[i];
            unsigned b1 = w[i] < b.w[i];
            uint64_t t = d - borrow;
            unsigned b2 = d < borrow;
            r.w[i] = t;
            borrow = b1 | b2;
        }
        return r;
    }

    U256 addOne() const {
        U256 r;
        add(pow2(0), r);
        return r;
    }

    int compare(const U256 &b) const {
        for (int i = 3; i >= 0; i--)
            if (w[i] != b.w[i])
                return w[i] < b.w[i] ? -1 : 1;
        return 0;
    }

    bool isZero() const { return (w[0] | w[1] | w[2] | w[3]) == 0; }

    double toDouble() const {
        return std::ldexp((double)w[3], 192) + std::ldexp((double)w[2], 128) +
               std::ldexp((double)w[1], 64) + (double)w[0];
    }
};
