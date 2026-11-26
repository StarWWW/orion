/*
 * This file is part of the VanitySearch distribution (https://github.com/JeanLucPons/VanitySearch).
 * Copyright (c) 2019 Jean Luc PONS.
 * Modified in 2026 for NVIDIA Blackwell (sm_120) throughput, see OPTIMIZATION.md and the git history.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

// ---------------------------------------------------------------------------------
// Target matching modes for templated comp_keys kernels
// ---------------------------------------------------------------------------------
enum TargetMode {
    TARGET_PREFIX     = 0,
    TARGET_SINGLE     = 1,
    TARGET_MULTI_FULL = 2
};

// ---------------------------------------------------------------------------------
// SHA-256 (compile-time constants, constant-folded rounds 0..3 & padding, zero IADD3)
// ---------------------------------------------------------------------------------

#define ROR(x,n) (((x) >> (n)) | ((x) << (32 - (n))))

// On sm_120 IADD3 (3-input add) only issues on the ALU-heavy pipe, which SHF/LOP3 already
// saturate, while IMAD runs on the FMA pipe. _fadd(a,b) = a*1+b is one IMAD; the 1 comes
// from constant memory so ptxas cannot fold it back into IADD3.
// ORION_FADD bits:
//   1 = SHA256 round adds
//   2 = SHA256 message schedule (WMIX & WMIX_init)
//   4 = RIPEMD160 round adds
//   8 = eliminate remaining 3-input IADD3 (h+k+w in SHA256, a+x+k in RIPEMD160) via extra _fadd
#ifndef ORION_FADD
#define ORION_FADD 7
#endif

__device__ __constant__ uint32_t _one = 1;
__device__ __forceinline__ uint32_t _fadd(uint32_t a, uint32_t b) {
    uint32_t r;
    asm("mad.lo.u32 %0, %1, %2, %3;" : "=r"(r) : "r"(a), "r"(_one), "r"(b));
    return r;
}

#define S0(x) (ROR(x,2) ^ ROR(x,13) ^ ROR(x,22))
#define S1(x) (ROR(x,6) ^ ROR(x,11) ^ ROR(x,25))
#define s0(x) (ROR(x,7) ^ ROR(x,18) ^ ((x) >> 3))
#define s1(x) (ROR(x,17) ^ ROR(x,19) ^ ((x) >> 10))

#define Maj(x,y,z) (((x) & (y)) | ((z) & ((x) | (y))))
#define Ch(x,y,z)  ((z) ^ ((x) & ((y) ^ (z))))

#if (ORION_FADD & 1) && (ORION_FADD & 8)
#define S2Round(a, b, c, d, e, f, g, h, k, w) \
    t1 = _fadd(S1(e), _fadd(Ch(e,f,g), _fadd(h, (w) + (k)))); \
    d += t1; \
    h = _fadd(t1, S0(a) + Maj(a,b,c));
#define S2Round_ZeroW(a, b, c, d, e, f, g, h, k) \
    t1 = _fadd(S1(e), _fadd(Ch(e,f,g), (h) + (k))); \
    d += t1; \
    h = _fadd(t1, S0(a) + Maj(a,b,c));
#elif ORION_FADD & 1
#define S2Round(a, b, c, d, e, f, g, h, k, w) \
    t1 = _fadd(S1(e), _fadd(Ch(e,f,g), (h) + (k) + (w))); \
    d += t1; \
    h = _fadd(t1, S0(a) + Maj(a,b,c));
#define S2Round_ZeroW(a, b, c, d, e, f, g, h, k) \
    t1 = _fadd(S1(e), _fadd(Ch(e,f,g), (h) + (k))); \
    d += t1; \
    h = _fadd(t1, S0(a) + Maj(a,b,c));
#else
#define S2Round(a, b, c, d, e, f, g, h, k, w) \
    t1 = (h) + S1(e) + Ch(e,f,g) + (k) + (w); \
    d += t1; \
    h = t1 + (S0(a) + Maj(a,b,c));
#define S2Round_ZeroW(a, b, c, d, e, f, g, h, k) \
    t1 = (h) + S1(e) + Ch(e,f,g) + (k); \
    d += t1; \
    h = t1 + (S0(a) + Maj(a,b,c));
#endif

// WMIX_init for compressed 33-byte pubkey: w[9..14] == 0, w[15] == 0x108
// s1(0x108) = 0x00A50000u, s0(0x108) = 0x10420023u
#if ORION_FADD & 2
#define WMIX_init() { \
    w[0]  = w[0] + s0(w[1]); \
    w[1]  = _fadd(s0(w[2]), w[1] + 0x00A50000u); \
    w[2]  = _fadd(s1(w[0]), w[2] + s0(w[3])); \
    w[3]  = _fadd(s1(w[1]), w[3] + s0(w[4])); \
    w[4]  = _fadd(s1(w[2]), w[4] + s0(w[5])); \
    w[5]  = _fadd(s1(w[3]), w[5] + s0(w[6])); \
    w[6]  = _fadd(s1(w[4]), _fadd(s0(w[7]), w[6] + 0x108u)); \
    w[7]  = _fadd(s1(w[5]), _fadd(s0(w[8]), w[7] + w[0])); \
    w[8]  = _fadd(s1(w[6]), w[8] + w[1]); \
    w[9]  = s1(w[7]) + w[2]; \
    w[10] = s1(w[8]) + w[3]; \
    w[11] = s1(w[9]) + w[4]; \
    w[12] = s1(w[10]) + w[5]; \
    w[13] = s1(w[11]) + w[6]; \
    w[14] = _fadd(s1(w[12]), w[7] + 0x10420023u); \
    w[15] = _fadd(s1(w[13]), _fadd(s0(w[0]), w[8] + 0x108u)); \
}
#else
#define WMIX_init() { \
    w[0] += s0(w[1]); \
    w[1] += 0x00A50000u + s0(w[2]); \
    w[2] += s1(w[0]) + s0(w[3]); \
    w[3] += s1(w[1]) + s0(w[4]); \
    w[4] += s1(w[2]) + s0(w[5]); \
    w[5] += s1(w[3]) + s0(w[6]); \
    w[6] += s1(w[4]) + 0x108u + s0(w[7]); \
    w[7] += s1(w[5]) + w[0] + s0(w[8]); \
    w[8] += s1(w[6]) + w[1]; \
    w[9]  = s1(w[7]) + w[2]; \
    w[10] = s1(w[8]) + w[3]; \
    w[11] = s1(w[9]) + w[4]; \
    w[12] = s1(w[10]) + w[5]; \
    w[13] = s1(w[11]) + w[6]; \
    w[14] = s1(w[12]) + w[7] + 0x10420023u; \
    w[15] = 0x108u + s1(w[13]) + w[8] + s0(w[0]); \
}
#endif

// General WMIX
#if ORION_FADD & 2
#define WMIX() { \
    w[0]  = _fadd(s1(w[14]), _fadd(s0(w[1]),  w[0]  + w[9]));  \
    w[1]  = _fadd(s1(w[15]), _fadd(s0(w[2]),  w[1]  + w[10])); \
    w[2]  = _fadd(s1(w[0]),  _fadd(s0(w[3]),  w[2]  + w[11])); \
    w[3]  = _fadd(s1(w[1]),  _fadd(s0(w[4]),  w[3]  + w[12])); \
    w[4]  = _fadd(s1(w[2]),  _fadd(s0(w[5]),  w[4]  + w[13])); \
    w[5]  = _fadd(s1(w[3]),  _fadd(s0(w[6]),  w[5]  + w[14])); \
    w[6]  = _fadd(s1(w[4]),  _fadd(s0(w[7]),  w[6]  + w[15])); \
    w[7]  = _fadd(s1(w[5]),  _fadd(s0(w[8]),  w[7]  + w[0]));  \
    w[8]  = _fadd(s1(w[6]),  _fadd(s0(w[9]),  w[8]  + w[1]));  \
    w[9]  = _fadd(s1(w[7]),  _fadd(s0(w[10]), w[9]  + w[2]));  \
    w[10] = _fadd(s1(w[8]),  _fadd(s0(w[11]), w[10] + w[3]));  \
    w[11] = _fadd(s1(w[9]),  _fadd(s0(w[12]), w[11] + w[4]));  \
    w[12] = _fadd(s1(w[10]), _fadd(s0(w[13]), w[12] + w[5]));  \
    w[13] = _fadd(s1(w[11]), _fadd(s0(w[14]), w[13] + w[6]));  \
    w[14] = _fadd(s1(w[12]), _fadd(s0(w[15]), w[14] + w[7]));  \
    w[15] = _fadd(s1(w[13]), _fadd(s0(w[0]),  w[15] + w[8]));  \
}
#else
#define WMIX() { \
    w[0]  += s1(w[14]) + w[9]  + s0(w[1]);  \
    w[1]  += s1(w[15]) + w[10] + s0(w[2]);  \
    w[2]  += s1(w[0])  + w[11] + s0(w[3]);  \
    w[3]  += s1(w[1])  + w[12] + s0(w[4]);  \
    w[4]  += s1(w[2])  + w[13] + s0(w[5]);  \
    w[5]  += s1(w[3])  + w[14] + s0(w[6]);  \
    w[6]  += s1(w[4])  + w[15] + s0(w[7]);  \
    w[7]  += s1(w[5])  + w[0]  + s0(w[8]);  \
    w[8]  += s1(w[6])  + w[1]  + s0(w[9]);  \
    w[9]  += s1(w[7])  + w[2]  + s0(w[10]); \
    w[10] += s1(w[8])  + w[3]  + s0(w[11]); \
    w[11] += s1(w[9])  + w[4]  + s0(w[12]); \
    w[12] += s1(w[10]) + w[5]  + s0(w[13]); \
    w[13] += s1(w[11]) + w[6]  + s0(w[14]); \
    w[14] += s1(w[12]) + w[7]  + s0(w[15]); \
    w[15] += s1(w[13]) + w[8]  + s0(w[0]);  \
}
#endif

#define SHA256_RND(k) { \
    S2Round(a, b, c, d, e, f, g, h, K_SHA[k],      w[0]);  \
    S2Round(h, a, b, c, d, e, f, g, K_SHA[k + 1],  w[1]);  \
    S2Round(g, h, a, b, c, d, e, f, K_SHA[k + 2],  w[2]);  \
    S2Round(f, g, h, a, b, c, d, e, K_SHA[k + 3],  w[3]);  \
    S2Round(e, f, g, h, a, b, c, d, K_SHA[k + 4],  w[4]);  \
    S2Round(d, e, f, g, h, a, b, c, K_SHA[k + 5],  w[5]);  \
    S2Round(c, d, e, f, g, h, a, b, K_SHA[k + 6],  w[6]);  \
    S2Round(b, c, d, e, f, g, h, a, K_SHA[k + 7],  w[7]);  \
    S2Round(a, b, c, d, e, f, g, h, K_SHA[k + 8],  w[8]);  \
    S2Round(h, a, b, c, d, e, f, g, K_SHA[k + 9],  w[9]);  \
    S2Round(g, h, a, b, c, d, e, f, K_SHA[k + 10], w[10]); \
    S2Round(f, g, h, a, b, c, d, e, K_SHA[k + 11], w[11]); \
    S2Round(e, f, g, h, a, b, c, d, K_SHA[k + 12], w[12]); \
    S2Round(d, e, f, g, h, a, b, c, K_SHA[k + 13], w[13]); \
    S2Round(c, d, e, f, g, h, a, b, K_SHA[k + 14], w[14]); \
    S2Round(b, c, d, e, f, g, h, a, K_SHA[k + 15], w[15]); \
}

// Perform SHA-256 on compressed 33-byte public key (w[9..14]=0, w[15]=0x108)
__device__ __forceinline__ void SHA256TransformComp(uint32_t s[8], uint32_t w[16]) {

    constexpr uint32_t K_SHA[64] = {
        0x428A2F98u, 0x71374491u, 0xB5C0FBCFu, 0xE9B5DBA5u,
        0x3956C25Bu, 0x59F111F1u, 0x923F82A4u, 0xAB1C5ED5u,
        0xD807AA98u, 0x12835B01u, 0x243185BEu, 0x550C7DC3u,
        0x72BE5D74u, 0x80DEB1FEu, 0x9BDC06A7u, 0xC19BF174u,
        0xE49B69C1u, 0xEFBE4786u, 0x0FC19DC6u, 0x240CA1CCu,
        0x2DE92C6Fu, 0x4A7484AAu, 0x5CB0A9DCu, 0x76F988DAu,
        0x983E5152u, 0xA831C66Du, 0xB00327C8u, 0xBF597FC7u,
        0xC6E00BF3u, 0xD5A79147u, 0x06CA6351u, 0x14292967u,
        0x27B70A85u, 0x2E1B2138u, 0x4D2C6DFCu, 0x53380D13u,
        0x650A7354u, 0x766A0ABBu, 0x81C2C92Eu, 0x92722C85u,
        0xA2BFE8A1u, 0xA81A664Bu, 0xC24B8B70u, 0xC76C51A3u,
        0xD192E819u, 0xD6990624u, 0xF40E3585u, 0x106AA070u,
        0x19A4C116u, 0x1E376C08u, 0x2748774Cu, 0x34B0BCB5u,
        0x391C0CB3u, 0x4ED8AA4Au, 0x5B9CCA4Fu, 0x682E6FF3u,
        0x748F82EEu, 0x78A5636Fu, 0x84C87814u, 0x8CC70208u,
        0x90BEFFFAu, 0xA4506CEBu, 0xBEF9A3F7u, 0xC67178F2u,
    };

    constexpr uint32_t IV0 = 0x6a09e667u;
    constexpr uint32_t IV1 = 0xbb67ae85u;
    constexpr uint32_t IV2 = 0x3c6ef372u;
    constexpr uint32_t IV3 = 0xa54ff53au;
    constexpr uint32_t IV4 = 0x510e527fu;
    constexpr uint32_t IV5 = 0x9b05688cu;
    constexpr uint32_t IV6 = 0x1f83d9abu;
    constexpr uint32_t IV7 = 0x5be0cd19u;

    uint32_t a = IV0, b = IV1, c = IV2, d = IV3;
    uint32_t e = IV4, f = IV5, g = IV6, h = IV7;
    uint32_t t1;

    // Round 0: all initial state words are compile-time constants!
    constexpr uint32_t R0_T1 = IV7 + S1(IV4) + Ch(IV4, IV5, IV6) + K_SHA[0];
    constexpr uint32_t R0_T2 = S0(IV0) + Maj(IV0, IV1, IV2);
    d = (IV3 + R0_T1) + w[0];
    h = (R0_T1 + R0_T2) + w[0];

    // Round 1: b=IV0, c=IV1, d_in=IV2, f=IV4, g=IV5, h_in=IV6 are constants!
    constexpr uint32_t R1_HK = IV6 + K_SHA[1];
#if ORION_FADD & 1
    t1 = _fadd(S1(d), _fadd(Ch(d, IV4, IV5), w[1] + R1_HK));
    c  = IV2 + t1;
    g  = _fadd(t1, S0(h) + Maj(h, IV0, IV1));
#else
    t1 = R1_HK + w[1] + S1(d) + Ch(d, IV4, IV5);
    c  = IV2 + t1;
    g  = t1 + (S0(h) + Maj(h, IV0, IV1));
#endif

    // Round 2: c_in=IV0, d_in=IV1, g_in=IV4, h_in=IV5 are constants!
    constexpr uint32_t R2_HK = IV5 + K_SHA[2];
#if ORION_FADD & 1
    t1 = _fadd(S1(c), _fadd(Ch(c, d, IV4), w[2] + R2_HK));
    b  = IV1 + t1;
    f  = _fadd(t1, S0(g) + Maj(g, h, IV0));
#else
    t1 = R2_HK + w[2] + S1(c) + Ch(c, d, IV4);
    b  = IV1 + t1;
    f  = t1 + (S0(g) + Maj(g, h, IV0));
#endif

    // Round 3: d_in=IV0, h_in=IV4 are constants!
    constexpr uint32_t R3_HK = IV4 + K_SHA[3];
#if ORION_FADD & 1
    t1 = _fadd(S1(b), _fadd(Ch(b, c, d), w[3] + R3_HK));
    a  = IV0 + t1;
    e  = _fadd(t1, S0(f) + Maj(f, g, h));
#else
    t1 = R3_HK + w[3] + S1(b) + Ch(b, c, d);
    a  = IV0 + t1;
    e  = t1 + (S0(f) + Maj(f, g, h));
#endif

    // Rounds 4..8: variable w[4..8]
    S2Round(e, f, g, h, a, b, c, d, K_SHA[4], w[4]);
    S2Round(d, e, f, g, h, a, b, c, K_SHA[5], w[5]);
    S2Round(c, d, e, f, g, h, a, b, K_SHA[6], w[6]);
    S2Round(b, c, d, e, f, g, h, a, K_SHA[7], w[7]);
    S2Round(a, b, c, d, e, f, g, h, K_SHA[8], w[8]);

    // Rounds 9..14: w[9..14] == 0!
    S2Round_ZeroW(h, a, b, c, d, e, f, g, K_SHA[9]);
    S2Round_ZeroW(g, h, a, b, c, d, e, f, K_SHA[10]);
    S2Round_ZeroW(f, g, h, a, b, c, d, e, K_SHA[11]);
    S2Round_ZeroW(e, f, g, h, a, b, c, d, K_SHA[12]);
    S2Round_ZeroW(d, e, f, g, h, a, b, c, K_SHA[13]);
    S2Round_ZeroW(c, d, e, f, g, h, a, b, K_SHA[14]);

    // Round 15: w[15] == 0x108u!
    S2Round_ZeroW(b, c, d, e, f, g, h, a, K_SHA[15] + 0x108u);

    WMIX_init();
    SHA256_RND(16);
    WMIX();
    SHA256_RND(32);
    WMIX();
    SHA256_RND(48);

    s[0] = IV0 + a;
    s[1] = IV1 + b;
    s[2] = IV2 + c;
    s[3] = IV3 + d;
    s[4] = IV4 + e;
    s[5] = IV5 + f;
    s[6] = IV6 + g;
    s[7] = IV7 + h;
}

// ---------------------------------------------------------------------------------
// RIPEMD-160 (compile-time K160 & padding folding + early-exit after j=75R / j=76L)
// ---------------------------------------------------------------------------------

#define K_RMD_0 0x5A827999u
#define K_RMD_1 0x6ED9EBA1u
#define K_RMD_2 0x8F1BBCDCu
#define K_RMD_3 0xA953FD4Eu
#define K_RMD_4 0x50A28BE6u
#define K_RMD_5 0x5C4DD124u
#define K_RMD_6 0x6D703EF3u
#define K_RMD_7 0x7A6D76E9u

#define ROL(x,n)  (((x) >> (32 - (n))) | ((x) << (n)))
#define ROL10(x)  (((x) >> 22) | ((x) << 10))
#define f1(x, y, z) ((x) ^ (y) ^ (z))
#define f2(x, y, z) (((x) & (y)) | (~(x) & (z)))
#define f3(x, y, z) (((x) | ~(y)) ^ (z))
#define f4(x, y, z) (((x) & (z)) | (~(z) & (y)))
#define f5(x, y, z) ((x) ^ ((y) | ~(z)))

// RIPEMD-160 addition helpers:
//   - Variable x with K != 0: avoids IADD3 when ORION_FADD & 8 by splitting x + K -> _fadd(a, x + K)
//   - Constant x (0, 0x80, 0x100): folds (K + x) at compile time -> single 2-input IADD + _fadd!
//   - Zero x and K == 0: single _fadd(f, a) (zero IADD, zero IADD3!)
#if (ORION_FADD & 4) && (ORION_FADD & 8)
#define RIPADD_VK(a, f, x, k) _fadd(f, _fadd(a, (x) + (k)))
#define RIPADD_V0(a, f, x)    _fadd(f, (a) + (x))
#define RIPADD_C(a, f, c)     _fadd(f, (a) + (c))
#define RIPADD_0(a, f)        _fadd(f, a)
#elif ORION_FADD & 4
#define RIPADD_VK(a, f, x, k) _fadd(f, (a) + (x) + (k))
#define RIPADD_V0(a, f, x)    _fadd(f, (a) + (x))
#define RIPADD_C(a, f, c)     _fadd(f, (a) + (c))
#define RIPADD_0(a, f)        _fadd(f, a)
#else
#define RIPADD_VK(a, f, x, k) ((a) + (f) + (x) + (k))
#define RIPADD_V0(a, f, x)    ((a) + (f) + (x))
#define RIPADD_C(a, f, c)     ((a) + (f) + (c))
#define RIPADD_0(a, f)        ((a) + (f))
#endif

#define RPR_VK(a,b,c,d,e,f,x,k,r) u = RIPADD_VK(a, f, x, k); a = ROL(u, r) + (e); c = ROL10(c);
#define RPR_V0(a,b,c,d,e,f,x,r)   u = RIPADD_V0(a, f, x);    a = ROL(u, r) + (e); c = ROL10(c);
#define RPR_C(a,b,c,d,e,f,C,r)    u = RIPADD_C(a, f, C);     a = ROL(u, r) + (e); c = ROL10(c);
#define RPR_0(a,b,c,d,e,f,r)      u = RIPADD_0(a, f);        a = ROL(u, r) + (e); c = ROL10(c);

// Per-group round macros for variable w[0..7] vs constant padding words (0, 0x80, 0x100)
#define R11V(a,b,c,d,e,x,r) RPR_V0(a, b, c, d, e, f1(b,c,d), x, r)
#define R11C(a,b,c,d,e,C,r) RPR_C( a, b, c, d, e, f1(b,c,d), C, r)
#define R11Z(a,b,c,d,e,r)   RPR_0( a, b, c, d, e, f1(b,c,d), r)

#define R21V(a,b,c,d,e,x,r) RPR_VK(a, b, c, d, e, f2(b,c,d), x, K_RMD_0, r)
#define R21C(a,b,c,d,e,C,r) RPR_C( a, b, c, d, e, f2(b,c,d), (uint32_t)(K_RMD_0 + (C)), r)

#define R31V(a,b,c,d,e,x,r) RPR_VK(a, b, c, d, e, f3(b,c,d), x, K_RMD_1, r)
#define R31C(a,b,c,d,e,C,r) RPR_C( a, b, c, d, e, f3(b,c,d), (uint32_t)(K_RMD_1 + (C)), r)

#define R41V(a,b,c,d,e,x,r) RPR_VK(a, b, c, d, e, f4(b,c,d), x, K_RMD_2, r)
#define R41C(a,b,c,d,e,C,r) RPR_C( a, b, c, d, e, f4(b,c,d), (uint32_t)(K_RMD_2 + (C)), r)

#define R51V(a,b,c,d,e,x,r) RPR_VK(a, b, c, d, e, f5(b,c,d), x, K_RMD_3, r)
#define R51C(a,b,c,d,e,C,r) RPR_C( a, b, c, d, e, f5(b,c,d), (uint32_t)(K_RMD_3 + (C)), r)

#define R12V(a,b,c,d,e,x,r) RPR_VK(a, b, c, d, e, f5(b,c,d), x, K_RMD_4, r)
#define R12C(a,b,c,d,e,C,r) RPR_C( a, b, c, d, e, f5(b,c,d), (uint32_t)(K_RMD_4 + (C)), r)

#define R22V(a,b,c,d,e,x,r) RPR_VK(a, b, c, d, e, f4(b,c,d), x, K_RMD_5, r)
#define R22C(a,b,c,d,e,C,r) RPR_C( a, b, c, d, e, f4(b,c,d), (uint32_t)(K_RMD_5 + (C)), r)

#define R32V(a,b,c,d,e,x,r) RPR_VK(a, b, c, d, e, f3(b,c,d), x, K_RMD_6, r)
#define R32C(a,b,c,d,e,C,r) RPR_C( a, b, c, d, e, f3(b,c,d), (uint32_t)(K_RMD_6 + (C)), r)

#define R42V(a,b,c,d,e,x,r) RPR_VK(a, b, c, d, e, f2(b,c,d), x, K_RMD_7, r)
#define R42C(a,b,c,d,e,C,r) RPR_C( a, b, c, d, e, f2(b,c,d), (uint32_t)(K_RMD_7 + (C)), r)

#define R52V(a,b,c,d,e,x,r) RPR_V0(a, b, c, d, e, f1(b,c,d), x, r)
#define R52C(a,b,c,d,e,C,r) RPR_C( a, b, c, d, e, f1(b,c,d), C, r)
#define R52Z(a,b,c,d,e,r)   RPR_0( a, b, c, d, e, f1(b,c,d), r)

// Padding constants for 32-byte SHA-256 digest input to RIPEMD-160:
// w[8] = 0x80, w[14] = 0x100, w[9,10,11,12,13,15] = 0

#ifndef HASH_INLINE
#define HASH_INLINE __forceinline__
#endif

// Fused SHA256 + RIPEMD160 + early-exit check!
// In TARGET_SINGLE & TARGET_MULTI_FULL, checks h[2] after left j=76 / right j=75,
// skipping 7 RIPEMD half-rounds + 2 ROL10s + 4 final additions on 99.99999998% of keys.
// In TARGET_PREFIX, checks h[0] after left j=78 / right j=77, skipping 3 half-rounds.
template <int MODE>
__device__ HASH_INLINE void _RipemdAndCheck(const uint32_t s[8], int32_t incr,
                                            const address_t * __restrict__ address,
                                            const uint32_t * __restrict__ lookup32,
                                            uint32_t *out) {

    // RIPEMD-160 initial state (compile-time constants)
    constexpr uint32_t R_IV0 = 0x67452301u;
    constexpr uint32_t R_IV1 = 0xEFCDAB89u;
    constexpr uint32_t R_IV2 = 0x98BADCFEu;
    constexpr uint32_t R_IV3 = 0x10325476u;
    constexpr uint32_t R_IV4 = 0xC3D2E1F0u;

    uint32_t u;
    uint32_t a1, b1 = R_IV1, c1, d1 = R_IV3, e1 = R_IV4;
    uint32_t a2, b2 = R_IV1, c2, d2 = R_IV3, e2 = R_IV4;

    // j=0: constant-folded!
    constexpr uint32_t R0L_C = R_IV0 + f1(R_IV1, R_IV2, R_IV3);
    constexpr uint32_t R0R_C = R_IV0 + f5(R_IV1, R_IV2, R_IV3) + K_RMD_4;
    constexpr uint32_t C_IV2_R10 = ROL10(R_IV2);
    u = s[0] + R0L_C; a1 = ROL(u, 11) + R_IV4; c1 = C_IV2_R10;
    u = s[5] + R0R_C; a2 = ROL(u,  8) + R_IV4; c2 = C_IV2_R10;

    // j=1..15 (w[8]=0x80, w[14]=0x100, w[9..13,15]=0)
    R11V(e1, a1, b1, c1, d1, s[1], 14);
    R12C(e2, a2, b2, c2, d2, 0x100u, 9);   // w[14]
    R11V(d1, e1, a1, b1, c1, s[2], 15);
    R12V(d2, e2, a2, b2, c2, s[7], 9);
    R11V(c1, d1, e1, a1, b1, s[3], 12);
    R12V(c2, d2, e2, a2, b2, s[0], 11);
    R11V(b1, c1, d1, e1, a1, s[4], 5);
    R12C(b2, c2, d2, e2, a2, 0u, 13);      // w[9]
    R11V(a1, b1, c1, d1, e1, s[5], 8);
    R12V(a2, b2, c2, d2, e2, s[2], 15);
    R11V(e1, a1, b1, c1, d1, s[6], 7);
    R12C(e2, a2, b2, c2, d2, 0u, 15);      // w[11]
    R11V(d1, e1, a1, b1, c1, s[7], 9);
    R12V(d2, e2, a2, b2, c2, s[4], 5);
    R11C(c1, d1, e1, a1, b1, 0x80u, 11);   // w[8]
    R12C(c2, d2, e2, a2, b2, 0u, 7);       // w[13]
    R11Z(b1, c1, d1, e1, a1, 13);          // w[9]
    R12V(b2, c2, d2, e2, a2, s[6], 7);
    R11Z(a1, b1, c1, d1, e1, 14);          // w[10]
    R12C(a2, b2, c2, d2, e2, 0u, 8);       // w[15]
    R11Z(e1, a1, b1, c1, d1, 15);          // w[11]
    R12C(e2, a2, b2, c2, d2, 0x80u, 11);   // w[8]
    R11Z(d1, e1, a1, b1, c1, 6);           // w[12]
    R12V(d2, e2, a2, b2, c2, s[1], 14);
    R11Z(c1, d1, e1, a1, b1, 7);           // w[13]
    R12C(c2, d2, e2, a2, b2, 0u, 14);      // w[10]
    R11C(b1, c1, d1, e1, a1, 0x100u, 9);   // w[14]
    R12V(b2, c2, d2, e2, a2, s[3], 12);
    R11Z(a1, b1, c1, d1, e1, 8);           // w[15]
    R12C(a2, b2, c2, d2, e2, 0u, 6);       // w[12]

    // j=16..31
    R21V(e1, a1, b1, c1, d1, s[7], 7);
    R22V(e2, a2, b2, c2, d2, s[6], 9);
    R21V(d1, e1, a1, b1, c1, s[4], 6);
    R22C(d2, e2, a2, b2, c2, 0u, 13);      // w[11]
    R21C(c1, d1, e1, a1, b1, 0u, 8);       // w[13]
    R22V(c2, d2, e2, a2, b2, s[3], 15);
    R21V(b1, c1, d1, e1, a1, s[1], 13);
    R22V(b2, c2, d2, e2, a2, s[7], 7);
    R21C(a1, b1, c1, d1, e1, 0u, 11);      // w[10]
    R22V(a2, b2, c2, d2, e2, s[0], 12);
    R21V(e1, a1, b1, c1, d1, s[6], 9);
    R22C(e2, a2, b2, c2, d2, 0u, 8);       // w[13]
    R21C(d1, e1, a1, b1, c1, 0u, 7);       // w[15]
    R22V(d2, e2, a2, b2, c2, s[5], 9);
    R21V(c1, d1, e1, a1, b1, s[3], 15);
    R22C(c2, d2, e2, a2, b2, 0u, 11);      // w[10]
    R21C(b1, c1, d1, e1, a1, 0u, 7);       // w[12]
    R22C(b2, c2, d2, e2, a2, 0x100u, 7);   // w[14]
    R21V(a1, b1, c1, d1, e1, s[0], 12);
    R22C(a2, b2, c2, d2, e2, 0u, 7);       // w[15]
    R21C(e1, a1, b1, c1, d1, 0u, 15);      // w[9]
    R22C(e2, a2, b2, c2, d2, 0x80u, 12);   // w[8]
    R21V(d1, e1, a1, b1, c1, s[5], 9);
    R22C(d2, e2, a2, b2, c2, 0u, 7);       // w[12]
    R21V(c1, d1, e1, a1, b1, s[2], 11);
    R22V(c2, d2, e2, a2, b2, s[4], 6);
    R21C(b1, c1, d1, e1, a1, 0x100u, 7);   // w[14]
    R22C(b2, c2, d2, e2, a2, 0u, 15);      // w[9]
    R21C(a1, b1, c1, d1, e1, 0u, 13);      // w[11]
    R22V(a2, b2, c2, d2, e2, s[1], 13);
    R21C(e1, a1, b1, c1, d1, 0x80u, 12);   // w[8]
    R22V(e2, a2, b2, c2, d2, s[2], 11);

    // j=32..47
    R31V(d1, e1, a1, b1, c1, s[3], 11);
    R32C(d2, e2, a2, b2, c2, 0u, 9);       // w[15]
    R31C(c1, d1, e1, a1, b1, 0u, 13);      // w[10]
    R32V(c2, d2, e2, a2, b2, s[5], 7);
    R31C(b1, c1, d1, e1, a1, 0x100u, 6);   // w[14]
    R32V(b2, c2, d2, e2, a2, s[1], 15);
    R31V(a1, b1, c1, d1, e1, s[4], 7);
    R32V(a2, b2, c2, d2, e2, s[3], 11);
    R31C(e1, a1, b1, c1, d1, 0u, 14);      // w[9]
    R32V(e2, a2, b2, c2, d2, s[7], 8);
    R31C(d1, e1, a1, b1, c1, 0u, 9);       // w[15]
    R32C(d2, e2, a2, b2, c2, 0x100u, 6);   // w[14]
    R31C(c1, d1, e1, a1, b1, 0x80u, 13);   // w[8]
    R32V(c2, d2, e2, a2, b2, s[6], 6);
    R31V(b1, c1, d1, e1, a1, s[1], 15);
    R32C(b2, c2, d2, e2, a2, 0u, 14);      // w[9]
    R31V(a1, b1, c1, d1, e1, s[2], 14);
    R32C(a2, b2, c2, d2, e2, 0u, 12);      // w[11]
    R31V(e1, a1, b1, c1, d1, s[7], 8);
    R32C(e2, a2, b2, c2, d2, 0x80u, 13);   // w[8]
    R31V(d1, e1, a1, b1, c1, s[0], 13);
    R32C(d2, e2, a2, b2, c2, 0u, 5);       // w[12]
    R31V(c1, d1, e1, a1, b1, s[6], 6);
    R32V(c2, d2, e2, a2, b2, s[2], 14);
    R31C(b1, c1, d1, e1, a1, 0u, 5);       // w[13]
    R32C(b2, c2, d2, e2, a2, 0u, 13);      // w[10]
    R31C(a1, b1, c1, d1, e1, 0u, 12);      // w[11]
    R32V(a2, b2, c2, d2, e2, s[0], 13);
    R31V(e1, a1, b1, c1, d1, s[5], 7);
    R32V(e2, a2, b2, c2, d2, s[4], 7);
    R31C(d1, e1, a1, b1, c1, 0u, 5);       // w[12]
    R32C(d2, e2, a2, b2, c2, 0u, 5);       // w[13]

    // j=48..63
    R41V(c1, d1, e1, a1, b1, s[1], 11);
    R42C(c2, d2, e2, a2, b2, 0x80u, 15);   // w[8]
    R41C(b1, c1, d1, e1, a1, 0u, 12);      // w[9]
    R42V(b2, c2, d2, e2, a2, s[6], 5);
    R41C(a1, b1, c1, d1, e1, 0u, 14);      // w[11]
    R42V(a2, b2, c2, d2, e2, s[4], 8);
    R41C(e1, a1, b1, c1, d1, 0u, 15);      // w[10]
    R42V(e2, a2, b2, c2, d2, s[1], 11);
    R41V(d1, e1, a1, b1, c1, s[0], 14);
    R42V(d2, e2, a2, b2, c2, s[3], 14);
    R41C(c1, d1, e1, a1, b1, 0x80u, 15);   // w[8]
    R42C(c2, d2, e2, a2, b2, 0u, 14);      // w[11]
    R41C(b1, c1, d1, e1, a1, 0u, 9);       // w[12]
    R42C(b2, c2, d2, e2, a2, 0u, 6);       // w[15]
    R41V(a1, b1, c1, d1, e1, s[4], 8);
    R42V(a2, b2, c2, d2, e2, s[0], 14);
    R41C(e1, a1, b1, c1, d1, 0u, 9);       // w[13]
    R42V(e2, a2, b2, c2, d2, s[5], 6);
    R41V(d1, e1, a1, b1, c1, s[3], 14);
    R42C(d2, e2, a2, b2, c2, 0u, 9);       // w[12]
    R41V(c1, d1, e1, a1, b1, s[7], 5);
    R42V(c2, d2, e2, a2, b2, s[2], 12);
    R41C(b1, c1, d1, e1, a1, 0u, 6);       // w[15]
    R42C(b2, c2, d2, e2, a2, 0u, 9);       // w[13]
    R41C(a1, b1, c1, d1, e1, 0x100u, 8);   // w[14]
    R42C(a2, b2, c2, d2, e2, 0u, 12);      // w[9]
    R41V(e1, a1, b1, c1, d1, s[5], 6);
    R42V(e2, a2, b2, c2, d2, s[7], 5);
    R41V(d1, e1, a1, b1, c1, s[6], 5);
    R42C(d2, e2, a2, b2, c2, 0u, 15);      // w[10]
    R41V(c1, d1, e1, a1, b1, s[2], 12);
    R42C(c2, d2, e2, a2, b2, 0x100u, 8);   // w[14]

    // j=64..74
    R51V(b1, c1, d1, e1, a1, s[4], 9);
    R52Z(b2, c2, d2, e2, a2, 8);           // w[12]
    R51V(a1, b1, c1, d1, e1, s[0], 15);
    R52Z(a2, b2, c2, d2, e2, 5);           // w[15]
    R51V(e1, a1, b1, c1, d1, s[5], 5);
    R52Z(e2, a2, b2, c2, d2, 12);          // w[10]
    R51C(d1, e1, a1, b1, c1, 0u, 11);      // w[9]
    R52V(d2, e2, a2, b2, c2, s[4], 9);
    R51V(c1, d1, e1, a1, b1, s[7], 6);
    R52V(c2, d2, e2, a2, b2, s[1], 12);
    R51C(b1, c1, d1, e1, a1, 0u, 8);       // w[12]
    R52V(b2, c2, d2, e2, a2, s[5], 5);
    R51V(a1, b1, c1, d1, e1, s[2], 13);
    R52C(a2, b2, c2, d2, e2, 0x80u, 14);   // w[8]
    R51C(e1, a1, b1, c1, d1, 0u, 12);      // w[10]
    R52V(e2, a2, b2, c2, d2, s[7], 6);
    R51C(d1, e1, a1, b1, c1, 0x100u, 5);   // w[14]
    R52V(d2, e2, a2, b2, c2, s[6], 8);
    R51V(c1, d1, e1, a1, b1, s[1], 12);
    R52V(c2, d2, e2, a2, b2, s[2], 13);
    R51V(b1, c1, d1, e1, a1, s[3], 13);
    R52Z(b2, c2, d2, e2, a2, 6);           // w[13]

    if constexpr (MODE == TARGET_SINGLE || MODE == TARGET_MULTI_FULL) {
        // j=75 left (needed for j=76 left)
        R51C(a1, b1, c1, d1, e1, 0x80u, 14);      // w[8]
        // j=75 right: a2 final value (skip c2 = ROL10(c2) unless hit!)
        u = RIPADD_C(a2, f1(b2, c2, d2), 0x100u); // w[14]
        a2 = ROL(u, 5) + e2;
        // j=76 left: e1 final value (skip b1 = ROL10(b1) unless hit!)
        u = RIPADD_C(e1, f5(a1, b1, c1), K_RMD_3); // w[11] == 0
        e1 = ROL(u, 11) + d1;

        uint32_t e1_f = ROL10(e1);
        uint32_t a2_f = ROL10(a2);

        if constexpr (MODE == TARGET_SINGLE) {
            // Single-word check (1 in 2^32 false positive rate)!
            if (__builtin_expect((e1_f + a2_f) == _targetH2_noIV, 0)) {
                // Rare path: finish remaining 7 half-rounds and verify all 5 words
                b1 = ROL10(b1);
                c2 = ROL10(c2);
                R52V(e2, a2, b2, c2, d2, s[0], 15);
                R51V(d1, e1, a1, b1, c1, s[6], 8);
                R52V(d2, e2, a2, b2, c2, s[3], 13);
                R51C(c1, d1, e1, a1, b1, 0u, 5);   // w[15]
                R52Z(c2, d2, e2, a2, b2, 11);      // w[9]
                R51C(b1, c1, d1, e1, a1, 0u, 6);   // w[13]
                R52Z(b2, c2, d2, e2, a2, 11);      // w[11]

                uint32_t h0 = R_IV1 + c1 + d2;
                uint32_t h1 = R_IV2 + d1 + e2;
                uint32_t h2 = R_IV3 + e1 + a2;
                uint32_t h3 = R_IV4 + a1 + b2;
                uint32_t h4 = R_IV0 + b1 + c2;
                if (h0 == _targetH160[0] && h1 == _targetH160[1] &&
                    h3 == _targetH160[3] && h4 == _targetH160[4]) {
                    RecordHit(h0, h1, h2, h3, h4, incr, out);
                }
            }
        } else {
            // TARGET_MULTI_FULL: lookup16 + lookup32 indexed by h[2]!
            uint32_t h2 = R_IV3 + e1_f + a2_f;
            address_t pr0 = (address_t)(h2 & 0xFFFFu);
            address_t hit = __ldg(address + pr0);
            if (__builtin_expect(hit != 0, 0)) {
                uint32_t off = __ldg(lookup32 + pr0);
                uint32_t st = off;
                uint32_t ed = off + hit - 1;
                bool matched = false;
                while (st <= ed) {
                    uint32_t mi = (st + ed) >> 1;
                    uint32_t lmi = __ldg(lookup32 + mi);
                    if (h2 < lmi) {
                        if (mi == 0) break;
                        ed = mi - 1;
                    } else if (h2 == lmi) {
                        matched = true;
                        break;
                    } else {
                        st = mi + 1;
                    }
                }
                if (matched) {
                    b1 = ROL10(b1);
                    c2 = ROL10(c2);
                    R52V(e2, a2, b2, c2, d2, s[0], 15);
                    R51V(d1, e1, a1, b1, c1, s[6], 8);
                    R52V(d2, e2, a2, b2, c2, s[3], 13);
                    R51C(c1, d1, e1, a1, b1, 0u, 5);
                    R52Z(c2, d2, e2, a2, b2, 11);
                    R51C(b1, c1, d1, e1, a1, 0u, 6);
                    R52Z(b2, c2, d2, e2, a2, 11);
                    uint32_t h0 = R_IV1 + c1 + d2;
                    uint32_t h1 = R_IV2 + d1 + e2;
                    uint32_t h3 = R_IV4 + a1 + b2;
                    uint32_t h4 = R_IV0 + b1 + c2;
                    RecordHit(h0, h1, h2, h3, h4, incr, out);
                }
            }
        }
    } else {
        // TARGET_PREFIX: h[0] = R_IV1 + c1_after_j78 + ROL10(d2_after_j77), skips 3 half-rounds!
        R51C(a1, b1, c1, d1, e1, 0x80u, 14);      // j=75 L (w[8])
        R52C(a2, b2, c2, d2, e2, 0x100u, 5);      // j=75 R (w[14])
        R51C(e1, a1, b1, c1, d1, 0u, 11);         // j=76 L (w[11])
        R52V(e2, a2, b2, c2, d2, s[0], 15);       // j=76 R (w[0])
        R51V(d1, e1, a1, b1, c1, s[6], 8);        // j=77 L (w[6])
        // j=77 R: d2 final before ROL10 (skip a2 = ROL10(a2) unless hit)
        u = RIPADD_V0(d2, f1(e2, a2, b2), s[3]);
        d2 = ROL(u, 13) + c2;
        // j=78 L: c1 final (skip e1 = ROL10(e1) unless hit)
        u = RIPADD_C(c1, f5(d1, e1, a1), K_RMD_3); // w[15] == 0
        c1 = ROL(u, 5) + b1;

        uint32_t h0 = R_IV1 + c1 + ROL10(d2);
        address_t pr0 = (address_t)(h0 & 0xFFFFu);
        if (__builtin_expect(__ldg(address + pr0) != 0, 0)) {
            a2 = ROL10(a2);
            e1 = ROL10(e1);
            R52Z(c2, d2, e2, a2, b2, 11);         // j=78 R (w[9])
            R51C(b1, c1, d1, e1, a1, 0u, 6);      // j=79 L (w[13])
            R52Z(b2, c2, d2, e2, a2, 11);         // j=79 R (w[11])
            uint32_t h1 = R_IV2 + d1 + e2;
            uint32_t h2 = R_IV3 + e1 + a2;
            uint32_t h3 = R_IV4 + a1 + b2;
            uint32_t h4 = R_IV0 + b1 + c2;
            RecordHit(h0, h1, h2, h3, h4, incr, out);
        }
    }
}

__device__ __forceinline__ void _PackPubkeyComp(uint32_t w[16], const uint32_t x32[8], uint32_t isOdd) {
    w[0]  = __byte_perm(x32[7], 0x2u + isOdd, 0x4321);
    w[1]  = __byte_perm(x32[7], x32[6], 0x0765);
    w[2]  = __byte_perm(x32[6], x32[5], 0x0765);
    w[3]  = __byte_perm(x32[5], x32[4], 0x0765);
    w[4]  = __byte_perm(x32[4], x32[3], 0x0765);
    w[5]  = __byte_perm(x32[3], x32[2], 0x0765);
    w[6]  = __byte_perm(x32[2], x32[1], 0x0765);
    w[7]  = __byte_perm(x32[1], x32[0], 0x0765);
    w[8]  = __byte_perm(x32[0], 0x80u,  0x0456);
    w[9]  = 0u; w[10] = 0u; w[11] = 0u; w[12] = 0u; w[13] = 0u; w[14] = 0u;
    w[15] = 0x108u;
}

template <int MODE>
__device__ HASH_INLINE void _HashAndCheckComp(const uint32_t x32[8], uint32_t isOdd, int32_t incr,
                                              const address_t * __restrict__ address,
                                              const uint32_t * __restrict__ lookup32,
                                              uint32_t *out) {
    uint32_t w[16], s[8];
    _PackPubkeyComp(w, x32, isOdd);
    SHA256TransformComp(s, w);
#pragma unroll
    for (int i = 0; i < 8; ++i) s[i] = __byte_perm(s[i], 0u, 0x0123);
    _RipemdAndCheck<MODE>(s, incr, address, lookup32, out);
}
