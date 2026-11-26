/*
 * This file is part of the BSGS distribution (https://github.com/JeanLucPons/VanitySearch).
 * Copyright (c) 2020 Jean Luc PONS.
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

// Big integer class (Fixed size)

#ifndef BIGINTH
#define BIGINTH

#include <string>
#include <inttypes.h>

// We need 1 extra block for Knuth div algorithm , Montgomery multiplication and ModInv
#define BISIZE 256

#if BISIZE==256
  #define NB64BLOCK 5
  #define NB32BLOCK 10
#elif BISIZE==512
  #define NB64BLOCK 9
  #define NB32BLOCK 18
#else
  #error Unsuported size
#endif

class Int {

public:

  Int();
  Int(int64_t i64);
  Int(uint64_t u64);
  Int(Int *a);

  // Op
  void Add(uint64_t a);
  void Add(Int *a);
  void Add(Int *a,Int *b);
  void AddOne();
  void Sub(uint64_t a);
  void Sub(Int *a);
  void Sub(Int *a, Int *b);
  void SubOne();
  void Mult(Int *a);
  uint64_t Mult(uint64_t a);
  uint64_t Mult(Int *a,uint64_t b);
  uint64_t IMult(Int *a, int64_t b);
  void Mult(Int *a,Int *b);
  void Div(Int *a,Int *mod = NULL);
  void Neg();

  // Right shift (signed)
  void ShiftR(uint32_t n);
  void ShiftR64Bit();
  // Left shift
  void ShiftL(uint32_t n);
  void ShiftL64Bit();

  // Comp
  bool IsGreater(Int *a);
  bool IsGreaterOrEqual(Int *a);
  bool IsLower(Int *a);
  bool IsEqual(Int *a);
  bool IsZero();
  bool IsOne();
  bool IsPositive();
  bool IsNegative();
  bool IsEven();

  double ToDouble();

  // Modular arithmetic

  // Setup field: n is the field characteristic
  static void SetupField(Int *n);

  void Mod(Int *n);                          // this <- this (mod n)
  void ModInv();                             // this <- this^-1 (mod n)
  void MontgomeryMult(Int *a,Int *b);        // this <- a*b*R^-1 (mod n)
  void ModAdd(Int *a);                       // this <- this+a (mod n) [0<a<P]
  void ModAdd(Int *a,Int *b);                // this <- a+b (mod n) [0<a,b<P]
  void ModAdd(uint64_t a);                   // this <- this+a (mod n) [0<a<P]
  void ModSub(Int *a);                       // this <- this-a (mod n) [0<a<P]
  void ModSub(Int *a, Int *b);               // this <- a-b (mod n) [0<a,b<P]
  void ModSub(uint64_t a);                   // this <- this-a (mod n) [0<a<P]
  void ModMul(Int *a,Int *b);                // this <- a*b (mod n)
  void ModDouble();                          // this <- 2*this (mod n)
  void ModNeg();                             // this <- -this (mod n)

  // Specific SecpK1
  void ModMulK1(Int *a, Int *b);
  void ModMulK1(Int *a);
  void ModSquareK1(Int *a);

  // Size
  int GetSize();       // Number of significant 32bit limbs
  int GetSize64();     // Number of significant 64bit limbs

  // Setter
  void SetInt32(uint32_t value);
  void Set(Int *a);
  void SetBase16(char *value);
  void SetBaseN(int n,char *charset,char *value);

  // Getter
  unsigned char GetByte(int n);
  void Get32Bytes(unsigned char *buff);

  // To String
  std::string GetBase16();
  std::string GetBaseN(int n,char *charset);


  union {
    uint32_t bits[NB32BLOCK];
    uint64_t bits64[NB64BLOCK];
  };

private:

  void MatrixVecMul(Int *u,Int *v,int64_t _11,int64_t _12,int64_t _21,int64_t _22,uint64_t *cu,uint64_t* cv);
  void MatrixVecMul(Int* u,Int* v,int64_t _11,int64_t _12,int64_t _21,int64_t _22);
  uint64_t AddCh(Int *a,uint64_t ca,Int* b,uint64_t cb);
  uint64_t AddCh(Int* a,uint64_t ca);
  uint64_t AddC(Int* a);
  void AddAndShift(Int* a,Int* b,uint64_t cH);
  void ShiftL64BitAndSub(Int *a,int n);
  void CLEAR();
  void CLEARFF();
  void DivStep62(Int* u,Int* v,int64_t* eta,int *pos,int64_t* uu,int64_t* uv,int64_t* vu,int64_t* vv);

};

// Inline routines

#ifndef WIN64

// Missing intrinsics
static uint64_t inline _umul128(uint64_t a, uint64_t b, uint64_t *h) {
  uint64_t rhi;
  uint64_t rlo;
  __asm__( "mulq  %[b];" :"=d"(rhi),"=a"(rlo) :"1"(a),[b]"rm"(b));
  *h = rhi;
  return rlo;
}

static int64_t inline _mul128(int64_t a, int64_t b, int64_t *h) {
  uint64_t rhi;
  uint64_t rlo;
  __asm__( "imulq  %[b];" :"=d"(rhi),"=a"(rlo) :"1"(a),[b]"rm"(b));
  *h = rhi;
  return rlo;  
}

static uint64_t inline _udiv128(uint64_t hi, uint64_t lo, uint64_t d,uint64_t *r) {
  uint64_t q;
  uint64_t _r;
  __asm__( "divq  %[d];" :"=d"(_r),"=a"(q) :"d"(hi),"a"(lo),[d]"rm"(d));
  *r = _r;
  return q;  
}

#define __shiftright128(a,b,n) ((a)>>(n))|((b)<<(64-(n)))
#define __shiftleft128(a,b,n) ((b)<<(n))|((a)>>(64-(n)))


#define _subborrow_u64(a,b,c,d) __builtin_ia32_sbb_u64(a,b,c,(long long unsigned int*)d);
#define _addcarry_u64(a,b,c,d) __builtin_ia32_addcarryx_u64(a,b,c,(long long unsigned int*)d);
#define _byteswap_uint64 __builtin_bswap64
#define LZC(x) __builtin_clzll(x)
#define TZC(x) __builtin_ctzll(x)

#else

#include <intrin.h>
#define TZC(x) _tzcnt_u64(x)
#define LZC(x) _lzcnt_u64(x)

#endif

static void inline imm_mul(uint64_t *x, uint64_t y, uint64_t *dst,uint64_t *carryH) {

  unsigned char c = 0;
  uint64_t h, carry;
  dst[0] = _umul128(x[0], y, &h); carry = h;
  c = _addcarry_u64(c, _umul128(x[1], y, &h), carry, dst + 1); carry = h;
  c = _addcarry_u64(c, _umul128(x[2], y, &h), carry, dst + 2); carry = h;
  c = _addcarry_u64(c, _umul128(x[3], y, &h), carry, dst + 3); carry = h;
  c = _addcarry_u64(c, _umul128(x[4], y, &h), carry, dst + 4); carry = h;
#if NB64BLOCK > 5
  c = _addcarry_u64(c, _umul128(x[5], y, &h), carry, dst + 5); carry = h;
  c = _addcarry_u64(c, _umul128(x[6], y, &h), carry, dst + 6); carry = h;
  c = _addcarry_u64(c, _umul128(x[7], y, &h), carry, dst + 7); carry = h;
  c = _addcarry_u64(c, _umul128(x[8], y, &h), carry, dst + 8); carry = h;
#endif
  *carryH = carry;

}

static void inline imm_imul(uint64_t* x,uint64_t y,uint64_t* dst,uint64_t* carryH) {

  unsigned char c = 0;
  uint64_t h,carry;
  dst[0] = _umul128(x[0],y,&h); carry = h;
  c = _addcarry_u64(c,_umul128(x[1],y,&h),carry,dst + 1); carry = h;
  c = _addcarry_u64(c,_umul128(x[2],y,&h),carry,dst + 2); carry = h;
  c = _addcarry_u64(c,_umul128(x[3],y,&h),carry,dst + 3); carry = h;
#if NB64BLOCK > 5
  c = _addcarry_u64(c,_umul128(x[4],y,&h),carry,dst + 4); carry = h;
  c = _addcarry_u64(c,_umul128(x[5],y,&h),carry,dst + 5); carry = h;
  c = _addcarry_u64(c,_umul128(x[6],y,&h),carry,dst + 6); carry = h;
  c = _addcarry_u64(c,_umul128(x[7],y,&h),carry,dst + 7); carry = h;
#endif
  c = _addcarry_u64(c,_mul128(x[NB64BLOCK - 1],y,(int64_t*)&h),carry,dst + NB64BLOCK - 1); carry = h;
  * carryH = carry;

}

static void inline imm_umul(uint64_t *x, uint64_t y, uint64_t *dst) {

  // Assume that x[NB64BLOCK-1] is 0
  unsigned char c = 0;
  uint64_t h, carry;
  dst[0] = _umul128(x[0], y, &h); carry = h;
  c = _addcarry_u64(c, _umul128(x[1], y, &h), carry, dst + 1); carry = h;
  c = _addcarry_u64(c, _umul128(x[2], y, &h), carry, dst + 2); carry = h;
  c = _addcarry_u64(c, _umul128(x[3], y, &h), carry, dst + 3); carry = h;
#if NB64BLOCK > 5
  c = _addcarry_u64(c, _umul128(x[4], y, &h), carry, dst + 4); carry = h;
  c = _addcarry_u64(c, _umul128(x[5], y, &h), carry, dst + 5); carry = h;
  c = _addcarry_u64(c, _umul128(x[6], y, &h), carry, dst + 6); carry = h;
  c = _addcarry_u64(c, _umul128(x[7], y, &h), carry, dst + 7); carry = h;
#endif
  _addcarry_u64(c, 0ULL, carry, dst + (NB64BLOCK - 1));

}

static void inline shiftR(unsigned char n, uint64_t *d) {

  d[0] = __shiftright128(d[0], d[1], n);
  d[1] = __shiftright128(d[1], d[2], n);
  d[2] = __shiftright128(d[2], d[3], n);
  d[3] = __shiftright128(d[3], d[4], n);
#if NB64BLOCK > 5
  d[4] = __shiftright128(d[4], d[5], n);
  d[5] = __shiftright128(d[5], d[6], n);
  d[6] = __shiftright128(d[6], d[7], n);
  d[7] = __shiftright128(d[7], d[8], n);
#endif
  d[NB64BLOCK-1] = ((int64_t)d[NB64BLOCK-1]) >> n;

}

static void inline shiftR(unsigned char n,uint64_t* d,uint64_t h) {

  d[0] = __shiftright128(d[0],d[1],n);
  d[1] = __shiftright128(d[1],d[2],n);
  d[2] = __shiftright128(d[2],d[3],n);
  d[3] = __shiftright128(d[3],d[4],n);
#if NB64BLOCK > 5
  d[4] = __shiftright128(d[4],d[5],n);
  d[5] = __shiftright128(d[5],d[6],n);
  d[6] = __shiftright128(d[6],d[7],n);
  d[7] = __shiftright128(d[7],d[8],n);
#endif

  d[NB64BLOCK-1] = __shiftright128(d[NB64BLOCK-1],h,n);

}

static void inline shiftL(unsigned char n, uint64_t *d) {

#if NB64BLOCK > 5
  d[8] = __shiftleft128(d[7], d[8], n);
  d[7] = __shiftleft128(d[6], d[7], n);
  d[6] = __shiftleft128(d[5], d[6], n);
  d[5] = __shiftleft128(d[4], d[5], n);
#endif
  d[4] = __shiftleft128(d[3], d[4], n);
  d[3] = __shiftleft128(d[2], d[3], n);
  d[2] = __shiftleft128(d[1], d[2], n);
  d[1] = __shiftleft128(d[0], d[1], n);
  d[0] = d[0] << n;

}

static inline int isStrictGreater128(uint64_t h1,uint64_t l1,uint64_t h2,uint64_t l2) {
  if(h1>h2) return 1;
  if(h1==h2) return l1>l2;
  return 0;
}

#endif // BIGINTH
