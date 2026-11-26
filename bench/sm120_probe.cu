// Phase 0 microbenchmark for NVIDIA Blackwell (sm_120):
// Measures actual pipe mapping and throughput of:
//  1) IADD.64 / IADD.64.X vs 32-bit IADD / IADD.X
//  2) SHF.R (x >> 3) vs mul.hi.u32 (IMAD.HI.U32)
//  3) IMAD (32-bit) vs IMAD.WIDE.U32
//  4) _ModMult (current 64-bit) vs 32-bit-limb ModMult + direct (2^32+977) reduction vs Karatsuba
//  5) _ModSqr (current 64-bit) vs single-shift 32-bit ModSqr
// Build: nvcc -O3 -std=c++17 -arch=sm_120 -I.. -o sm120_probe.exe sm120_probe.cu
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cuda_runtime.h>
#include "../GPU/GPUMath.h"

#define ITER 2048
#define CH 8

__device__ __constant__ uint32_t c_s1 = 0x20000000u;
__device__ __constant__ uint32_t c_s2 = 0x12345679u;

// --- Pipe probes ---
__global__ void p_shfr(uint32_t *out, uint32_t s) {
  uint32_t x[CH];
  for (int c = 0; c < CH; c++) x[c] = threadIdx.x * 13 + c + s;
  for (int i = 0; i < ITER; i++) {
#pragma unroll
    for (int c = 0; c < CH; c++) {
      uint32_t t;
      asm volatile("shr.u32 %0, %1, 3;" : "=r"(t) : "r"(x[c]));
      x[c] ^= t;
    }
  }
  uint32_t r = 0; for (int c = 0; c < CH; c++) r ^= x[c];
  if (r == 0x12345678) out[0] = r;
}

__global__ void p_mulhi(uint32_t *out, uint32_t s) {
  uint32_t m = c_s1;
  uint32_t x[CH];
  for (int c = 0; c < CH; c++) x[c] = threadIdx.x * 13 + c + s;
  for (int i = 0; i < ITER; i++) {
#pragma unroll
    for (int c = 0; c < CH; c++) {
      uint32_t t;
      asm volatile("mul.hi.u32 %0, %1, %2;" : "=r"(t) : "r"(x[c]), "r"(m));
      x[c] = t;
    }
  }
  uint32_t r = 0; for (int c = 0; c < CH; c++) r ^= x[c];
  if (r == 0x12345678) out[0] = r;
}

__global__ void p_mulhi_imm(uint32_t *out, uint32_t s) {
  uint32_t x[CH];
  for (int c = 0; c < CH; c++) x[c] = threadIdx.x * 13 + c + s;
  for (int i = 0; i < ITER; i++) {
#pragma unroll
    for (int c = 0; c < CH; c++) {
      uint32_t t;
      asm volatile("mul.hi.u32 %0, %1, 0x20000000;" : "=r"(t) : "r"(x[c]));
      x[c] = t;
    }
  }
  uint32_t r = 0; for (int c = 0; c < CH; c++) r ^= x[c];
  if (r == 0x12345678) out[0] = r;
}

__global__ void p_add64cc(uint32_t *out, uint32_t s) {
  uint64_t a[4], b[4];
  for (int c = 0; c < 4; c++) { a[c] = ((uint64_t)s << 32) | (threadIdx.x + c); b[c] = a[c] * 3 + 1; }
  for (int i = 0; i < ITER; i++) {
    UADDO1(a[0], b[0]);
    UADDC1(a[1], b[1]);
    UADDC1(a[2], b[2]);
    UADD1(a[3], b[3]);
    UADDO1(b[0], a[0]);
    UADDC1(b[1], a[1]);
    UADDC1(b[2], a[2]);
    UADD1(b[3], a[3]);
  }
  uint64_t r = a[0] ^ a[1] ^ a[2] ^ a[3] ^ b[0] ^ b[1] ^ b[2] ^ b[3];
  if (r == 0x12345678ULL) out[0] = (uint32_t)r;
}

__global__ void p_add32cc(uint32_t *out, uint32_t s) {
  uint32_t a[8], b[8];
  for (int c = 0; c < 8; c++) { a[c] = threadIdx.x + c + s; b[c] = a[c] * 3 + 1; }
  for (int i = 0; i < ITER; i++) {
    asm volatile(
      "add.cc.u32 %0, %0, %8;\n\t"
      "addc.cc.u32 %1, %1, %9;\n\t"
      "addc.cc.u32 %2, %2, %10;\n\t"
      "addc.cc.u32 %3, %3, %11;\n\t"
      "addc.cc.u32 %4, %4, %12;\n\t"
      "addc.cc.u32 %5, %5, %13;\n\t"
      "addc.cc.u32 %6, %6, %14;\n\t"
      "addc.u32 %7, %7, %15;\n\t"
      : "+r"(a[0]), "+r"(a[1]), "+r"(a[2]), "+r"(a[3]),
        "+r"(a[4]), "+r"(a[5]), "+r"(a[6]), "+r"(a[7])
      : "r"(b[0]), "r"(b[1]), "r"(b[2]), "r"(b[3]),
        "r"(b[4]), "r"(b[5]), "r"(b[6]), "r"(b[7]));
    asm volatile(
      "add.cc.u32 %0, %0, %8;\n\t"
      "addc.cc.u32 %1, %1, %9;\n\t"
      "addc.cc.u32 %2, %2, %10;\n\t"
      "addc.cc.u32 %3, %3, %11;\n\t"
      "addc.cc.u32 %4, %4, %12;\n\t"
      "addc.cc.u32 %5, %5, %13;\n\t"
      "addc.cc.u32 %6, %6, %14;\n\t"
      "addc.u32 %7, %7, %15;\n\t"
      : "+r"(b[0]), "+r"(b[1]), "+r"(b[2]), "+r"(b[3]),
        "+r"(b[4]), "+r"(b[5]), "+r"(b[6]), "+r"(b[7])
      : "r"(a[0]), "r"(a[1]), "r"(a[2]), "r"(a[3]),
        "r"(a[4]), "r"(a[5]), "r"(a[6]), "r"(a[7]));
  }
  uint32_t r = 0; for (int c = 0; c < 8; c++) r ^= a[c] ^ b[c];
  if (r == 0x12345678u) out[0] = r;
}

// --- 32-bit limb field arithmetic candidates ---
#define MULLO32(d, a, b)       asm volatile("mul.lo.u32 %0, %1, %2;" : "=r"(d) : "r"(a), "r"(b))
#define MADLO_CC(d, a, b, c)   asm volatile("mad.lo.cc.u32 %0, %1, %2, %3;" : "=r"(d) : "r"(a), "r"(b), "r"(c))
#define MADCLO_CC(d, a, b, c)  asm volatile("madc.lo.cc.u32 %0, %1, %2, %3;" : "=r"(d) : "r"(a), "r"(b), "r"(c))
#define MADHI_CC(d, a, b, c)   asm volatile("mad.hi.cc.u32 %0, %1, %2, %3;" : "=r"(d) : "r"(a), "r"(b), "r"(c))
#define MADCHI_CC(d, a, b, c)  asm volatile("madc.hi.cc.u32 %0, %1, %2, %3;" : "=r"(d) : "r"(a), "r"(b), "r"(c))
#define MADCHI(d, a, b, c)     asm volatile("madc.hi.u32 %0, %1, %2, %3;" : "=r"(d) : "r"(a), "r"(b), "r"(c))
#define ADDC0(d)               ADDC0_32(d)

// Direct 512 -> 256 reduction using 32-bit limbs: L + H*(2^32 + 977)
__device__ __forceinline__ void Reduce512_32(uint32_t out[8], const uint32_t r[16]) {
  // Step 1: t[0..7] = L[0..7] + (H[0..7] << 32), plus carry into ov (t8)
  uint32_t t0 = r[0], t1, t2, t3, t4, t5, t6, t7, ov_lo, ov_hi;
  ADD32_CC(t1, r[1], r[8]);
  ADDC32_CC(t2, r[2], r[9]);
  ADDC32_CC(t3, r[3], r[10]);
  ADDC32_CC(t4, r[4], r[11]);
  ADDC32_CC(t5, r[5], r[12]);
  ADDC32_CC(t6, r[6], r[13]);
  ADDC32_CC(t7, r[7], r[14]);
  ADDC32_CC(ov_lo, r[15], 0u);
  ADDC0(ov_hi);

  // Step 2: add H[0..7] * 977 using mad.lo.cc + mad.hi.cc (pure 32-bit!)
  const uint32_t C = 977u;
  MADLO_CC(t0, r[8], C, t0);
  MADCLO_CC(t1, r[9], C, t1);
  MADCLO_CC(t2, r[10], C, t2);
  MADCLO_CC(t3, r[11], C, t3);
  MADCLO_CC(t4, r[12], C, t4);
  MADCLO_CC(t5, r[13], C, t5);
  MADCLO_CC(t6, r[14], C, t6);
  MADCLO_CC(t7, r[15], C, t7);
  ADDC32_CC(ov_lo, ov_lo, 0u);
  ADDC32(ov_hi, ov_hi, 0u);

  MADHI_CC(t1, r[8], C, t1);
  MADCHI_CC(t2, r[9], C, t2);
  MADCHI_CC(t3, r[10], C, t3);
  MADCHI_CC(t4, r[11], C, t4);
  MADCHI_CC(t5, r[12], C, t5);
  MADCHI_CC(t6, r[13], C, t6);
  MADCHI_CC(t7, r[14], C, t7);
  MADCHI_CC(ov_lo, r[15], C, ov_lo);
  ADDC32(ov_hi, ov_hi, 0u);

  // Step 3: reduce 33-bit overflow (ov_hi:ov_lo) * (2^32 + 977)
  uint32_t p0, p1, p2;
  MULLO32(p0, ov_lo, C);
  MADHI_CC(p1, ov_lo, C, ov_lo);
  ADDC32(p2, ov_hi, 0u);
  uint32_t ov_hi_c = ov_hi * C;
  ADD32_CC(p1, p1, ov_hi_c);
  ADDC32(p2, p2, 0u);

  ADD32_CC(out[0], t0, p0);
  ADDC32_CC(out[1], t1, p1);
  ADDC32_CC(out[2], t2, p2);
  ADDC32_CC(out[3], t3, 0u);
  ADDC32_CC(out[4], t4, 0u);
  ADDC32_CC(out[5], t5, 0u);
  ADDC32_CC(out[6], t6, 0u);
  ADDC32(out[7], t7, 0u);
}

// 128x128 -> 256 Comba helper (16 IMAD.WIDE, only 2 IADD3.X!)
__device__ __forceinline__ void Mul128_Wide32(uint32_t out[8], const uint32_t a[4], const uint32_t b[4]) {
  uint64_t acc = 0;
  uint32_t c2 = 0;
  asm volatile("mul.wide.u32 %0, %1, %2;" : "=l"(acc) : "r"(a[0]), "r"(b[0]));
  out[0] = (uint32_t)acc; acc >>= 32;
  COMBA_MAC0(acc, a[0], b[1]); COMBA_MAC_INIT(acc, c2, a[1], b[0]);
  COMBA_STEP(out[1], acc, c2);
  COMBA_MAC0(acc, a[0], b[2]); COMBA_MAC_INIT(acc, c2, a[1], b[1]); COMBA_MAC(acc, c2, a[2], b[0]);
  COMBA_STEP(out[2], acc, c2);
  COMBA_MAC_INIT(acc, c2, a[0], b[3]); COMBA_MAC(acc, c2, a[1], b[2]); COMBA_MAC(acc, c2, a[2], b[1]); COMBA_MAC(acc, c2, a[3], b[0]);
  COMBA_STEP(out[3], acc, c2);
  COMBA_MAC_INIT(acc, c2, a[1], b[3]); COMBA_MAC(acc, c2, a[2], b[2]); COMBA_MAC(acc, c2, a[3], b[1]);
  COMBA_STEP(out[4], acc, c2);
  COMBA_MAC_INIT(acc, c2, a[2], b[3]); COMBA_MAC(acc, c2, a[3], b[2]);
  COMBA_STEP(out[5], acc, c2);
  COMBA_MAC0(acc, a[3], b[3]);
  out[6] = (uint32_t)acc;
  out[7] = (uint32_t)(acc >> 32);
}

// 1-level Unsigned Karatsuba 256x256 -> 512 + Reduce512_Wide32 (48 IMAD.WIDE vs 64!)
__device__ __forceinline__ void ModMult_Karatsuba32(uint32_t out[8], const uint32_t a[8], const uint32_t b[8]) {
  uint32_t p0[8], p1[8], p2[8], sa[4], sb[4];
  uint32_t ca, cb;
  Mul128_Wide32(p0, a, b);
  Mul128_Wide32(p1, a + 4, b + 4);

  ADD32_CC(sa[0], a[0], a[4]);
  ADDC32_CC(sa[1], a[1], a[5]);
  ADDC32_CC(sa[2], a[2], a[6]);
  ADDC32_CC(sa[3], a[3], a[7]);
  ADDC0(ca);

  ADD32_CC(sb[0], b[0], b[4]);
  ADDC32_CC(sb[1], b[1], b[5]);
  ADDC32_CC(sb[2], b[2], b[6]);
  ADDC32_CC(sb[3], b[3], b[7]);
  ADDC0(cb);

  Mul128_Wide32(p2, sa, sb);
  // Full (ca:sa)*(cb:sb) = p2 + (ca ? sb<<128 : 0) + (cb ? sa<<128 : 0) + (ca&cb)<<256
  uint32_t m_hi = ca & cb;
  ADD32_CC(p2[4], p2[4], ca ? sb[0] : 0u);
  ADDC32_CC(p2[5], p2[5], ca ? sb[1] : 0u);
  ADDC32_CC(p2[6], p2[6], ca ? sb[2] : 0u);
  ADDC32_CC(p2[7], p2[7], ca ? sb[3] : 0u);
  ADDC32(m_hi, m_hi, 0u);

  ADD32_CC(p2[4], p2[4], cb ? sa[0] : 0u);
  ADDC32_CC(p2[5], p2[5], cb ? sa[1] : 0u);
  ADDC32_CC(p2[6], p2[6], cb ? sa[2] : 0u);
  ADDC32_CC(p2[7], p2[7], cb ? sa[3] : 0u);
  ADDC32(m_hi, m_hi, 0u);

  // Mid = p2 - p0 - p1 (exact non-negative 258-bit value in p2[0..7], m_hi)
  asm volatile(
    "sub.cc.u32  %0, %0, %9;\n\t"
    "subc.cc.u32 %1, %1, %10;\n\t"
    "subc.cc.u32 %2, %2, %11;\n\t"
    "subc.cc.u32 %3, %3, %12;\n\t"
    "subc.cc.u32 %4, %4, %13;\n\t"
    "subc.cc.u32 %5, %5, %14;\n\t"
    "subc.cc.u32 %6, %6, %15;\n\t"
    "subc.cc.u32 %7, %7, %16;\n\t"
    "subc.u32    %8, %8, 0;\n\t"
    : "+r"(p2[0]), "+r"(p2[1]), "+r"(p2[2]), "+r"(p2[3]),
      "+r"(p2[4]), "+r"(p2[5]), "+r"(p2[6]), "+r"(p2[7]), "+r"(m_hi)
    : "r"(p0[0]), "r"(p0[1]), "r"(p0[2]), "r"(p0[3]),
      "r"(p0[4]), "r"(p0[5]), "r"(p0[6]), "r"(p0[7]));
  asm volatile(
    "sub.cc.u32  %0, %0, %9;\n\t"
    "subc.cc.u32 %1, %1, %10;\n\t"
    "subc.cc.u32 %2, %2, %11;\n\t"
    "subc.cc.u32 %3, %3, %12;\n\t"
    "subc.cc.u32 %4, %4, %13;\n\t"
    "subc.cc.u32 %5, %5, %14;\n\t"
    "subc.cc.u32 %6, %6, %15;\n\t"
    "subc.cc.u32 %7, %7, %16;\n\t"
    "subc.u32    %8, %8, 0;\n\t"
    : "+r"(p2[0]), "+r"(p2[1]), "+r"(p2[2]), "+r"(p2[3]),
      "+r"(p2[4]), "+r"(p2[5]), "+r"(p2[6]), "+r"(p2[7]), "+r"(m_hi)
    : "r"(p1[0]), "r"(p1[1]), "r"(p1[2]), "r"(p1[3]),
      "r"(p1[4]), "r"(p1[5]), "r"(p1[6]), "r"(p1[7]));

  // Combine into 512-bit r[0..15] = p0 + (Mid << 128) + (p1 << 256)
  uint32_t r[16];
  r[0] = p0[0]; r[1] = p0[1]; r[2] = p0[2]; r[3] = p0[3];
  ADD32_CC(r[4],  p0[4], p2[0]);
  ADDC32_CC(r[5], p0[5], p2[1]);
  ADDC32_CC(r[6], p0[6], p2[2]);
  ADDC32_CC(r[7], p0[7], p2[3]);
  ADDC32_CC(r[8], p1[0], p2[4]);
  ADDC32_CC(r[9], p1[1], p2[5]);
  ADDC32_CC(r[10], p1[2], p2[6]);
  ADDC32_CC(r[11], p1[3], p2[7]);
  ADDC32_CC(r[12], p1[4], m_hi);
  ADDC32_CC(r[13], p1[5], 0u);
  ADDC32_CC(r[14], p1[6], 0u);
  ADDC32(r[15],    p1[7], 0u);

  Reduce512_Wide32(out, r);
}

// 32-bit limb ModMult with direct 32-bit reduction
__device__ __forceinline__ void ModMult32_Full(uint32_t r[8], const uint32_t a[8], const uint32_t b[8]) {
  uint32_t t[16];
#pragma unroll
  for (int j = 0; j < 8; j++) MULLO32(t[j], a[j], b[0]);
  MADHI_CC(t[1], a[0], b[0], t[1]);
#pragma unroll
  for (int j = 1; j < 7; j++) MADCHI_CC(t[j + 1], a[j], b[0], t[j + 1]);
  MADCHI(t[8], a[7], b[0], 0);
#pragma unroll
  for (int i = 1; i < 8; i++) {
    MADLO_CC(t[i], a[0], b[i], t[i]);
#pragma unroll
    for (int j = 1; j < 8; j++) MADCLO_CC(t[i + j], a[j], b[i], t[i + j]);
    ADDC0(t[i + 8]);
    MADHI_CC(t[i + 1], a[0], b[i], t[i + 1]);
#pragma unroll
    for (int j = 1; j < 7; j++) MADCHI_CC(t[i + j + 1], a[j], b[i], t[i + j + 1]);
    MADCHI(t[i + 8], a[7], b[i], t[i + 8]);
  }
  Reduce512_32(r, t);
}

// 32-bit limb ModSqr with single shift + direct 32-bit reduction
__device__ __forceinline__ void ModSqr32_Full(uint32_t r[8], const uint32_t a[8]) {
  uint32_t t[16];
  // Off-diagonal pairs i < j (28 products instead of 64!)
  // Row i = 0: j = 1..7
  MULLO32(t[1], a[1], a[0]);
  MULLO32(t[2], a[2], a[0]);
  MULLO32(t[3], a[3], a[0]);
  MULLO32(t[4], a[4], a[0]);
  MULLO32(t[5], a[5], a[0]);
  MULLO32(t[6], a[6], a[0]);
  MULLO32(t[7], a[7], a[0]);
  MADHI_CC(t[2], a[1], a[0], t[2]);
  MADCHI_CC(t[3], a[2], a[0], t[3]);
  MADCHI_CC(t[4], a[3], a[0], t[4]);
  MADCHI_CC(t[5], a[4], a[0], t[5]);
  MADCHI_CC(t[6], a[5], a[0], t[6]);
  MADCHI_CC(t[7], a[6], a[0], t[7]);
  MADCHI(t[8], a[7], a[0], 0);

  // Row i = 1: j = 2..7
  MADLO_CC(t[3], a[2], a[1], t[3]);
  MADCLO_CC(t[4], a[3], a[1], t[4]);
  MADCLO_CC(t[5], a[4], a[1], t[5]);
  MADCLO_CC(t[6], a[5], a[1], t[6]);
  MADCLO_CC(t[7], a[6], a[1], t[7]);
  MADCLO_CC(t[8], a[7], a[1], t[8]);
  ADDC0(t[9]);
  MADHI_CC(t[4], a[2], a[1], t[4]);
  MADCHI_CC(t[5], a[3], a[1], t[5]);
  MADCHI_CC(t[6], a[4], a[1], t[6]);
  MADCHI_CC(t[7], a[5], a[1], t[7]);
  MADCHI_CC(t[8], a[6], a[1], t[8]);
  MADCHI(t[9], a[7], a[1], t[9]);

  // Row i = 2: j = 3..7
  MADLO_CC(t[5], a[3], a[2], t[5]);
  MADCLO_CC(t[6], a[4], a[2], t[6]);
  MADCLO_CC(t[7], a[5], a[2], t[7]);
  MADCLO_CC(t[8], a[6], a[2], t[8]);
  MADCLO_CC(t[9], a[7], a[2], t[9]);
  ADDC0(t[10]);
  MADHI_CC(t[6], a[3], a[2], t[6]);
  MADCHI_CC(t[7], a[4], a[2], t[7]);
  MADCHI_CC(t[8], a[5], a[2], t[8]);
  MADCHI_CC(t[9], a[6], a[2], t[9]);
  MADCHI(t[10], a[7], a[2], t[10]);

  // Row i = 3: j = 4..7
  MADLO_CC(t[7], a[4], a[3], t[7]);
  MADCLO_CC(t[8], a[5], a[3], t[8]);
  MADCLO_CC(t[9], a[6], a[3], t[9]);
  MADCLO_CC(t[10], a[7], a[3], t[10]);
  ADDC0(t[11]);
  MADHI_CC(t[8], a[4], a[3], t[8]);
  MADCHI_CC(t[9], a[5], a[3], t[9]);
  MADCHI_CC(t[10], a[6], a[3], t[10]);
  MADCHI(t[11], a[7], a[3], t[11]);

  // Row i = 4: j = 5..7
  MADLO_CC(t[9], a[5], a[4], t[9]);
  MADCLO_CC(t[10], a[6], a[4], t[10]);
  MADCLO_CC(t[11], a[7], a[4], t[11]);
  ADDC0(t[12]);
  MADHI_CC(t[10], a[5], a[4], t[10]);
  MADCHI_CC(t[11], a[6], a[4], t[11]);
  MADCHI(t[12], a[7], a[4], t[12]);

  // Row i = 5: j = 6..7
  MADLO_CC(t[11], a[6], a[5], t[11]);
  MADCLO_CC(t[12], a[7], a[5], t[12]);
  ADDC0(t[13]);
  MADHI_CC(t[12], a[6], a[5], t[12]);
  MADCHI(t[13], a[7], a[5], t[13]);

  // Row i = 6: j = 7
  MADLO_CC(t[13], a[7], a[6], t[13]);
  ADDC0(t[14]);
  MADHI_CC(t[14], a[7], a[6], t[14]);
  ADDC0(t[15]); // Wait: max t[14] before hi is < 2^32, + hi(a6*a7) < 2*2^32, so t[15] can be 1!

  // Double off-diagonal t[1..14] in ONE pass (pure ALU-lite IADD/IADD.X!)
  ADD32_CC(t[1], t[1], t[1]);
#pragma unroll
  for (int k = 2; k <= 14; k++) ADDC32_CC(t[k], t[k], t[k]);
  ADDC32(t[15], t[15], t[15]);

  // Add diagonal squares a[i]^2
  MULLO32(t[0], a[0], a[0]);
  MADHI_CC(t[1], a[0], a[0], t[1]);
  MADCLO_CC(t[2], a[1], a[1], t[2]);
  MADCHI_CC(t[3], a[1], a[1], t[3]);
  MADCLO_CC(t[4], a[2], a[2], t[4]);
  MADCHI_CC(t[5], a[2], a[2], t[5]);
  MADCLO_CC(t[6], a[3], a[3], t[6]);
  MADCHI_CC(t[7], a[3], a[3], t[7]);
  MADCLO_CC(t[8], a[4], a[4], t[8]);
  MADCHI_CC(t[9], a[4], a[4], t[9]);
  MADCLO_CC(t[10], a[5], a[5], t[10]);
  MADCHI_CC(t[11], a[5], a[5], t[11]);
  MADCLO_CC(t[12], a[6], a[6], t[12]);
  MADCHI_CC(t[13], a[6], a[6], t[13]);
  MADCLO_CC(t[14], a[7], a[7], t[14]);
  MADCHI(t[15], a[7], a[7], t[15]);

  Reduce512_32(r, t);
}

// Correctness check kernel
__global__ void verify_math(const uint64_t *in, uint64_t *out, int n) {
  int i = blockIdx.x * blockDim.x + threadIdx.x;
  if (i >= n) return;
  uint64_t a64[4], b64[4], m_old[4], s_old[4];
  uint32_t a32[8], b32[8], m_new[8], s_new[8], m_w[8], s_w[8], m_k[8];
  for (int k = 0; k < 4; k++) {
    a64[k] = in[8 * i + k]; b64[k] = in[8 * i + 4 + k];
    a32[2*k] = (uint32_t)a64[k]; a32[2*k+1] = (uint32_t)(a64[k] >> 32);
    b32[2*k] = (uint32_t)b64[k]; b32[2*k+1] = (uint32_t)(b64[k] >> 32);
  }
  _ModMult(m_old, a64, b64);
  ModMult32_Full(m_new, a32, b32);
  ModMult_Wide32(m_w, a32, b32);
  ModMult_Karatsuba32(m_k, a32, b32);
  _ModSqr(s_old, a64);
  ModSqr32_Full(s_new, a32);
  ModSqr_Wide32(s_w, a32);
  int bad = 0;
  for (int k = 0; k < 4; k++) {
    uint64_t mn = (uint64_t)m_new[2*k] | ((uint64_t)m_new[2*k+1] << 32);
    uint64_t mw = (uint64_t)m_w[2*k] | ((uint64_t)m_w[2*k+1] << 32);
    uint64_t mk = (uint64_t)m_k[2*k] | ((uint64_t)m_k[2*k+1] << 32);
    uint64_t sn = (uint64_t)s_new[2*k] | ((uint64_t)s_new[2*k+1] << 32);
    uint64_t sw = (uint64_t)s_w[2*k] | ((uint64_t)s_w[2*k+1] << 32);
    if (m_old[k] != mn) bad |= 1;
    if (m_old[k] != mw) bad |= 2;
    if (m_old[k] != mk) bad |= 16;
    if (s_old[k] != sn) bad |= 4;
    if (s_old[k] != sw) bad |= 8;
  }
  out[i] = bad;
}

template <int V>
__global__ void bench_ec(uint64_t *out, uint64_t seed) {
  uint64_t x64[2][4], y64[4] = {seed, seed * 3 + 1, seed * 5 + 2, (seed * 7) & 0x7FFFFFFFFFFFFFFFULL};
  uint32_t x32[2][8], y32[8];
  for (int k = 0; k < 4; k++) { y32[2*k] = (uint32_t)y64[k]; y32[2*k+1] = (uint32_t)(y64[k] >> 32); }
  for (int c = 0; c < 2; c++) for (int k = 0; k < 4; k++) {
    x64[c][k] = (seed + threadIdx.x + 1) * (k + c + 1);
    x32[c][2*k] = (uint32_t)x64[c][k]; x32[c][2*k+1] = (uint32_t)(x64[c][k] >> 32);
  }
  for (int i = 0; i < 1024; i++) {
#pragma unroll
    for (int c = 0; c < 2; c++) {
      if (V == 0) _ModMult(x64[c], x64[c], y64);
      else if (V == 1) ModMult32_Full(x32[c], x32[c], y32);
      else if (V == 2) ModMult_Wide32(x32[c], x32[c], y32);
      else if (V == 3) _ModSqr(x64[c], x64[c]);
      else if (V == 4) ModSqr32_Full(x32[c], x32[c]);
      else if (V == 5) ModSqr_Wide32(x32[c], x32[c]);
      else if (V == 6) ModMult_Karatsuba32(x32[c], x32[c], y32);
    }
  }
  uint64_t s = 0;
  if (V == 0 || V == 3) { for (int c = 0; c < 2; c++) for (int k = 0; k < 4; k++) s ^= x64[c][k]; }
  else { for (int c = 0; c < 2; c++) for (int k = 0; k < 8; k++) s ^= x32[c][k]; }
  if (s == 0x1234ULL) out[0] = s;
}

static uint64_t rnd64() {
  return ((uint64_t)rand() << 62) ^ ((uint64_t)rand() << 47) ^ ((uint64_t)rand() << 31) ^ ((uint64_t)rand() << 15) ^ (uint64_t)rand();
}

int main() {
  const int n = 1 << 16;
  uint64_t *h = (uint64_t *)malloc(n * 64), *o = (uint64_t *)malloc(n * 8);
  uint64_t *din, *dout;
  srand(42);
  for (int i = 0; i < n * 8; i++) h[i] = rnd64();
  for (int k = 0; k < 4; k++) { h[k] = ~0ULL; h[4 + k] = ~0ULL; }
  h[8] = 0xFFFFFFFEFFFFFC2EULL; h[9] = h[10] = h[11] = ~0ULL;
  h[12] = 0xFFFFFFFEFFFFFC2EULL; h[13] = h[14] = h[15] = ~0ULL;
  for (int k = 0; k < 8; k++) h[16 + k] = 0;
  h[24] = 1; h[25] = h[26] = h[27] = 0; h[28] = 0xFFFFFFFEFFFFFC2FULL; h[29] = h[30] = h[31] = ~0ULL;

  cudaMalloc(&din, n * 64); cudaMalloc(&dout, n * 8);
  cudaMemcpy(din, h, n * 64, cudaMemcpyHostToDevice);
  verify_math<<<n / 256, 256>>>(din, dout, n);
  cudaMemcpy(o, dout, n * 8, cudaMemcpyDeviceToHost);
  int bad = 0;
  for (int i = 0; i < n; i++) bad |= (int)o[i];
  printf("Verify (%d vectors): bad_mask=0x%x\n", n, bad);

  cudaDeviceProp p; cudaGetDeviceProperties(&p, 0);
  int blocks = p.multiProcessorCount * 16, threads = 128;
  bench_ec<0><<<blocks, threads>>>(dout, 7);
  bench_ec<1><<<blocks, threads>>>(dout, 7);
  bench_ec<2><<<blocks, threads>>>(dout, 7);
  bench_ec<3><<<blocks, threads>>>(dout, 7);
  bench_ec<4><<<blocks, threads>>>(dout, 7);
  bench_ec<5><<<blocks, threads>>>(dout, 7);
  bench_ec<6><<<blocks, threads>>>(dout, 7);
  cudaDeviceSynchronize();
  cudaError_t e = cudaGetLastError();
  if (e != cudaSuccess) printf("CUDA error: %s\n", cudaGetErrorString(e));
  return bad ? 1 : 0;
}
