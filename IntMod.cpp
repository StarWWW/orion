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

#include "Int.h"
#include <emmintrin.h>
#include <string.h>

static Int     _P;       // Field characteristic
static Int     _R2;      // Montgomery multiplication R2
static int32_t  Msize;    // Montgomery mult size
static uint64_t MM64;     // 64bits lsb negative inverse of P
#define MSK62  0x3FFFFFFFFFFFFFFF

extern Int _ONE;

// ------------------------------------------------

void Int::ModAdd(Int *a) {
  Int p;
  Add(a);
  p.Sub(this,&_P);
  if(p.IsPositive())
    Set(&p);
}

// ------------------------------------------------

void Int::ModAdd(Int *a, Int *b) {
  Int p;
  Add(a,b);
  p.Sub(this,&_P);
  if(p.IsPositive())
    Set(&p);
}

// ------------------------------------------------

void Int::ModDouble() {
  Int p;
  Add(this);
  p.Sub(this,&_P);
  if(p.IsPositive())
    Set(&p);
}

// ------------------------------------------------

void Int::ModAdd(uint64_t a) {
  Int p;
  Add(a);
  p.Sub(this,&_P);
  if(p.IsPositive())
    Set(&p);
}

// ------------------------------------------------

void Int::ModSub(Int *a) {
  Sub(a);
  if (IsNegative())
    Add(&_P);
}

// ------------------------------------------------

void Int::ModSub(uint64_t a) {
  Sub(a);
  if (IsNegative())
    Add(&_P);
}

// ------------------------------------------------

void Int::ModSub(Int *a,Int *b) {
  Sub(a,b);
  if (IsNegative())
    Add(&_P);
}

// ------------------------------------------------

void Int::ModNeg() {
  Neg();
  Add(&_P);
}

// ------------------------------------------------

void Int::DivStep62(Int* u,Int* v,int64_t* eta,int* pos,int64_t* uu,int64_t* uv,int64_t* vu,int64_t* vv) {

  // u' = (uu*u + uv*v) >> bitCount
  // v' = (vu*u + vv*v) >> bitCount
  // Do not maintain a matrix for r and s, the number of 
  // 'added P' can be easily calculated
  // Performance are measured on a I5-8500 for P=2^256 - 0x1000003D1 (VS2019 compilation)

  int bitCount;
  uint64_t u0 = u->bits64[0];
  uint64_t v0 = v->bits64[0];

  #define SWAP(tmp,x,y) tmp = x; x = y; y = tmp;

  // divstep62 var time implementation (Thomas Pornin's method)
  // (see https://github.com/pornin/bingcd)
  // Avg 780 Kinv/s, Avg number of divstep62: 6.13
  // "Make u,v positive" in the macro loop must be enabled

  uint64_t uh;
  uint64_t vh;
  uint64_t w,x;

  // Extract 64 MSB of u and v
  // u and v must be positive

  while(*pos>=1 && (u->bits64[*pos] | v->bits64[*pos])==0) (*pos)--;
  if(*pos==0) {
    uh = u->bits64[0];
    vh = v->bits64[0];
  } else {
    uint64_t s = LZC(u->bits64[*pos] | v->bits64[*pos]);
    if(s == 0) {
      uh = u->bits64[*pos];
      vh = v->bits64[*pos];
    } else {
      uh = __shiftleft128(u->bits64[*pos-1],u->bits64[*pos],(uint8_t)s);
      vh = __shiftleft128(v->bits64[*pos-1],v->bits64[*pos],(uint8_t)s);
    }
  }

  bitCount = 62;

  __m128i _u;
  __m128i _v;
  __m128i _t;

#ifdef WIN64
  _u.m128i_u64[0] = 1;
  _u.m128i_u64[1] = 0;
  _v.m128i_u64[0] = 0;
  _v.m128i_u64[1] = 1;
#else
  ((int64_t *)&_u)[0] = 1;
  ((int64_t *)&_u)[1] = 0;
  ((int64_t *)&_v)[0] = 0;
  ((int64_t *)&_v)[1] = 1;
#endif

  while(true) {

    // Use a sentinel bit to count zeros only up to bitCount
    uint64_t zeros = TZC(v0 | 1ULL << bitCount);
    vh >>= zeros;
    v0 >>= zeros;
    _u = _mm_slli_epi64(_u,(int)zeros);
    bitCount -= (int)zeros;

    if(bitCount <= 0) {
      break;
    }

    if( vh < uh ) {
      SWAP(w,uh,vh);
      SWAP(x,u0,v0);
      SWAP(_t,_u,_v);
    }

    vh -= uh;
    v0 -= u0;
    _v = _mm_sub_epi64(_v,_u);

  }

#ifdef WIN64
  *uu = _u.m128i_u64[0];
  *uv = _u.m128i_u64[1];
  *vu = _v.m128i_u64[0];
  *vv = _v.m128i_u64[1];
#else
  *uu = ((int64_t *)&_u)[0];
  *uv = ((int64_t *)&_u)[1];
  *vu = ((int64_t *)&_v)[0];
  *vv = ((int64_t *)&_v)[1];
#endif

}

// ------------------------------------------------

void Int::ModInv() {

  // Compute modular inverse of this mop _P
  // 0 <= this < _P  , _P must be odd
  // Return 0 if no inverse
  // Delayed right shift 62 bits (Thomas Pornin's divstep method)

  Int u(&_P);
  Int v(this);
  Int r((int64_t)0);
  Int s((int64_t)1);
  Int r0_P;
  Int s0_P;

  int64_t  eta = -1;
  int64_t uu,uv,vu,vv;
  uint64_t carryS,carryR;
  int pos = NB64BLOCK - 1;
  while(pos >= 1 && (u.bits64[pos] | v.bits64[pos]) == 0) pos--;

  while (!v.IsZero()) {

    DivStep62(&u,&v,&eta,&pos,&uu,&uv,&vu,&vv);

    // Now update BigInt variables

    MatrixVecMul(&u,&v,uu,uv,vu,vv);

    // Make u,v positive (required by Pornin's method)
    if(u.IsNegative()) {
      u.Neg();
      uu = -uu;
      uv = -uv;
    }
    if(v.IsNegative()) {
      v.Neg();
      vu = -vu;
      vv = -vv;
    }

    MatrixVecMul(&r,&s,uu,uv,vu,vv,&carryR,&carryS);

    // Compute multiple of P to add to s and r to make them multiple of 2^62
    uint64_t r0 = (r.bits64[0] * MM64) & MSK62;
    uint64_t s0 = (s.bits64[0] * MM64) & MSK62;
    r0_P.Mult(&_P,r0);
    s0_P.Mult(&_P,s0);
    carryR = r.AddCh(&r0_P,carryR);
    carryS = s.AddCh(&s0_P,carryS);

    // Right shift all variables by 62bits
    shiftR(62, u.bits64);
    shiftR(62, v.bits64);
    shiftR(62, r.bits64, carryR);
    shiftR(62, s.bits64, carryS);

  }
  
  // u ends with +/-1
  if(u.IsNegative()) {
    u.Neg();
    r.Neg();
  }

  if (!u.IsOne()) {
    // No inverse
    CLEAR();
    return;
  }

  while(r.IsNegative())
    r.Add(&_P);
  while(r.IsGreaterOrEqual(&_P))
    r.Sub(&_P);

  Set(&r);

}

// ------------------------------------------------

void Int::ModMul(Int *a, Int *b) {

  Int p;
  p.MontgomeryMult(a,b);
  MontgomeryMult(&_R2,&p);

}

// ------------------------------------------------

void Int::SetupField(Int *n) {

  // Size in number of 32bit word
  int nSize = n->GetSize();

  // Last digit inversions (Newton's iteration)
  {
    int64_t x, t;
    x = t = (int64_t)n->bits64[0];
    x = x * (2 - t * x);
    x = x * (2 - t * x);
    x = x * (2 - t * x);
    x = x * (2 - t * x);
    x = x * (2 - t * x);
    MM64 = (uint64_t)(-x);
  }
  _P.Set(n);

  // Size of Montgomery mult (64bits digit)
  Msize = nSize/2;

  // R = 2^(64*Msize) mod n, ModMul() needs R2 = R^2
  Int Ri;
  Ri.MontgomeryMult(&_ONE, &_ONE); // Ri = R^-1
  _R2.MontgomeryMult(&Ri, &_ONE);  // R2 = R^-2
  _R2.ModInv();                    // R2 = R^2

}

// ------------------------------------------------
void Int::MontgomeryMult(Int *a, Int *b) {

  // Compute a*b*R^-1 (mod n),  R=2^k (mod n), k = Msize*64
  // a and b must be lower than n
  // See SetupField()

  Int pr;
  Int p;
  uint64_t ML;
  uint64_t c;

  // i = 0
  imm_umul(a->bits64, b->bits64[0], pr.bits64);
  ML = pr.bits64[0] * MM64;
  imm_umul(_P.bits64, ML, p.bits64);
  c = pr.AddC(&p);
  memcpy(bits64,pr.bits64 + 1,8*(NB64BLOCK-1));
  bits64[NB64BLOCK-1] = c;

  for (int i = 1; i < Msize; i++) {

    imm_umul(a->bits64, b->bits64[i], pr.bits64);
    ML = (pr.bits64[0] + bits64[0]) * MM64;
    imm_umul(_P.bits64, ML, p.bits64);
	  c = pr.AddC(&p);
    AddAndShift(this, &pr, c);

  }

  p.Sub(this, &_P);
  if (p.IsPositive())
    Set(&p);

}


// SecpK1 specific section -----------------------------------------------------------------------------

void Int::ModMulK1(Int *a, Int *b) {

#ifndef WIN64
#if (__GNUC__ > 7) || (__GNUC__ == 7 && (__GNUC_MINOR__ > 2))
  unsigned char c;
#else
  #warning "GCC lass than 7.3 detected, upgrade gcc to get best perfromance"
  volatile unsigned char c;
#endif
#else
  unsigned char c;
#endif


  uint64_t ah, al;
  uint64_t t[NB64BLOCK];
#if BISIZE==256
  uint64_t r512[8];
  r512[5] = 0;
  r512[6] = 0;
  r512[7] = 0;
#else
  uint64_t r512[12];
  r512[5] = 0;
  r512[6] = 0;
  r512[7] = 0;
  r512[8] = 0;
  r512[9] = 0;
  r512[10] = 0;
  r512[11] = 0;
#endif

  // 256*256 multiplier
  imm_umul(a->bits64, b->bits64[0], r512);
  imm_umul(a->bits64, b->bits64[1], t);
  c = _addcarry_u64(0, r512[1], t[0], r512 + 1);
  c = _addcarry_u64(c, r512[2], t[1], r512 + 2);
  c = _addcarry_u64(c, r512[3], t[2], r512 + 3);
  c = _addcarry_u64(c, r512[4], t[3], r512 + 4);
  c = _addcarry_u64(c, r512[5], t[4], r512 + 5);
  imm_umul(a->bits64, b->bits64[2], t);
  c = _addcarry_u64(0, r512[2], t[0], r512 + 2);
  c = _addcarry_u64(c, r512[3], t[1], r512 + 3);
  c = _addcarry_u64(c, r512[4], t[2], r512 + 4);
  c = _addcarry_u64(c, r512[5], t[3], r512 + 5);
  c = _addcarry_u64(c, r512[6], t[4], r512 + 6);
  imm_umul(a->bits64, b->bits64[3], t);
  c = _addcarry_u64(0, r512[3], t[0], r512 + 3);
  c = _addcarry_u64(c, r512[4], t[1], r512 + 4);
  c = _addcarry_u64(c, r512[5], t[2], r512 + 5);
  c = _addcarry_u64(c, r512[6], t[3], r512 + 6);
  c = _addcarry_u64(c, r512[7], t[4], r512 + 7);

  // Reduce from 512 to 320 
  imm_umul(r512 + 4, 0x1000003D1ULL, t);
  c = _addcarry_u64(0, r512[0], t[0], r512 + 0);
  c = _addcarry_u64(c, r512[1], t[1], r512 + 1);
  c = _addcarry_u64(c, r512[2], t[2], r512 + 2);
  c = _addcarry_u64(c, r512[3], t[3], r512 + 3);

  // Reduce from 320 to 256 
  // No overflow possible here t[4]+c<=0x1000003D1ULL
  al = _umul128(t[4] + c, 0x1000003D1ULL, &ah); 
  c = _addcarry_u64(0, r512[0], al, bits64 + 0);
  c = _addcarry_u64(c, r512[1], ah, bits64 + 1);
  c = _addcarry_u64(c, r512[2], 0ULL, bits64 + 2);
  c = _addcarry_u64(c, r512[3], 0ULL, bits64 + 3);

  // Probability of carry here or that this>P is very very unlikely
  bits64[4] = 0; 
#if BISIZE==512
  bits64[5] = 0;
  bits64[6] = 0;
  bits64[7] = 0;
  bits64[8] = 0;
#endif

}

void Int::ModMulK1(Int *a) {

#ifndef WIN64
#if (__GNUC__ > 7) || (__GNUC__ == 7 && (__GNUC_MINOR__ > 2))
  unsigned char c;
#else
  #warning "GCC lass than 7.3 detected, upgrade gcc to get best perfromance"
  volatile unsigned char c;
#endif
#else
  unsigned char c;
#endif

  uint64_t ah, al;
  uint64_t t[NB64BLOCK];
#if BISIZE==256
  uint64_t r512[8];
  r512[5] = 0;
  r512[6] = 0;
  r512[7] = 0;
#else
  uint64_t r512[12];
  r512[5] = 0;
  r512[6] = 0;
  r512[7] = 0;
  r512[8] = 0;
  r512[9] = 0;
  r512[10] = 0;
  r512[11] = 0;
#endif


  // 256*256 multiplier
  imm_umul(a->bits64, bits64[0], r512);
  imm_umul(a->bits64, bits64[1], t);
  c = _addcarry_u64(0, r512[1], t[0], r512 + 1);
  c = _addcarry_u64(c, r512[2], t[1], r512 + 2);
  c = _addcarry_u64(c, r512[3], t[2], r512 + 3);
  c = _addcarry_u64(c, r512[4], t[3], r512 + 4);
  c = _addcarry_u64(c, r512[5], t[4], r512 + 5);
  imm_umul(a->bits64, bits64[2], t);
  c = _addcarry_u64(0, r512[2], t[0], r512 + 2);
  c = _addcarry_u64(c, r512[3], t[1], r512 + 3);
  c = _addcarry_u64(c, r512[4], t[2], r512 + 4);
  c = _addcarry_u64(c, r512[5], t[3], r512 + 5);
  c = _addcarry_u64(c, r512[6], t[4], r512 + 6);
  imm_umul(a->bits64, bits64[3], t);
  c = _addcarry_u64(0, r512[3], t[0], r512 + 3);
  c = _addcarry_u64(c, r512[4], t[1], r512 + 4);
  c = _addcarry_u64(c, r512[5], t[2], r512 + 5);
  c = _addcarry_u64(c, r512[6], t[3], r512 + 6);
  c = _addcarry_u64(c, r512[7], t[4], r512 + 7);

  // Reduce from 512 to 320 
  imm_umul(r512 + 4, 0x1000003D1ULL, t);
  c = _addcarry_u64(0, r512[0], t[0], r512 + 0);
  c = _addcarry_u64(c, r512[1], t[1], r512 + 1);
  c = _addcarry_u64(c, r512[2], t[2], r512 + 2);
  c = _addcarry_u64(c, r512[3], t[3], r512 + 3);

  // Reduce from 320 to 256 
  // No overflow possible here t[4]+c<=0x1000003D1ULL
  al = _umul128(t[4] + c, 0x1000003D1ULL, &ah);
  c = _addcarry_u64(0, r512[0], al, bits64 + 0);
  c = _addcarry_u64(c, r512[1], ah, bits64 + 1);
  c = _addcarry_u64(c, r512[2], 0, bits64 + 2);
  c = _addcarry_u64(c, r512[3], 0, bits64 + 3);
  // Probability of carry here or that this>P is very very unlikely
  bits64[4] = 0;
#if BISIZE==512
  bits64[5] = 0;
  bits64[6] = 0;
  bits64[7] = 0;
  bits64[8] = 0;
#endif

}

void Int::ModSquareK1(Int *a) {

#ifndef WIN64
#if (__GNUC__ > 7) || (__GNUC__ == 7 && (__GNUC_MINOR__ > 2))
  unsigned char c;
#else
  #warning "GCC lass than 7.3 detected, upgrade gcc to get best perfromance"
  volatile unsigned char c;
#endif
#else
  unsigned char c;
#endif

  uint64_t u10, u11;
  uint64_t t1;
  uint64_t t2;
  uint64_t t[NB64BLOCK];
#if BISIZE==256
  uint64_t r512[8];
  r512[5] = 0;
  r512[6] = 0;
  r512[7] = 0;
#else
  uint64_t r512[12];
  r512[5] = 0;
  r512[6] = 0;
  r512[7] = 0;
  r512[8] = 0;
  r512[9] = 0;
  r512[10] = 0;
  r512[11] = 0;
#endif


  //k=0
  r512[0] = _umul128(a->bits64[0], a->bits64[0], &t[1]);

  //k=1
  t[3] = _umul128(a->bits64[0], a->bits64[1], &t[4]);
  c = _addcarry_u64(0, t[3], t[3], &t[3]);
  c = _addcarry_u64(c, t[4], t[4], &t[4]);
  c = _addcarry_u64(c,  0,  0, &t1);
  c = _addcarry_u64(0, t[1], t[3], &t[3]);
  c = _addcarry_u64(c, t[4],  0, &t[4]);
  c = _addcarry_u64(c, t1,  0, &t1);
  r512[1] = t[3];

  //k=2
  t[0] = _umul128(a->bits64[0], a->bits64[2], &t[1]);
  c = _addcarry_u64(0, t[0], t[0], &t[0]);
  c = _addcarry_u64(c, t[1], t[1], &t[1]);
  c = _addcarry_u64(c,  0,  0, &t2);

  u10 = _umul128(a->bits64[1], a->bits64[1], &u11);
  c = _addcarry_u64(0, t[0] , u10, &t[0]);
  c = _addcarry_u64(c, t[1] , u11, &t[1]);
  c = _addcarry_u64(c, t2 ,   0, &t2);
  c = _addcarry_u64(0, t[0], t[4], &t[0]);
  c = _addcarry_u64(c, t[1], t1, &t[1]);
  c = _addcarry_u64(c, t2, 0, &t2);
  r512[2] = t[0];

  //k=3
  t[3] = _umul128(a->bits64[0], a->bits64[3], &t[4]);
  u10 = _umul128(a->bits64[1], a->bits64[2], &u11);

  c = _addcarry_u64(0, t[3], u10, &t[3]);
  c = _addcarry_u64(c, t[4], u11, &t[4]);
  c = _addcarry_u64(c,  0,   0, &t1);
  t1 += t1;
  c = _addcarry_u64(0, t[3], t[3], &t[3]);
  c = _addcarry_u64(c, t[4], t[4], &t[4]);
  c = _addcarry_u64(c, t1, 0, &t1);
  c = _addcarry_u64(0, t[3], t[1], &t[3]);
  c = _addcarry_u64(c, t[4], t2, &t[4]);
  c = _addcarry_u64(c, t1, 0, &t1);
  r512[3] = t[3];

  //k=4
  t[0] = _umul128(a->bits64[1], a->bits64[3], &t[1]);
  c = _addcarry_u64(0, t[0], t[0], &t[0]);
  c = _addcarry_u64(c, t[1], t[1], &t[1]);
  c = _addcarry_u64(c, 0, 0, &t2);

  u10 = _umul128(a->bits64[2], a->bits64[2], &u11);
  c = _addcarry_u64(0, t[0], u10, &t[0]);
  c = _addcarry_u64(c, t[1], u11, &t[1]);
  c = _addcarry_u64(c, t2, 0, &t2);
  c = _addcarry_u64(0, t[0], t[4], &t[0]);
  c = _addcarry_u64(c, t[1], t1, &t[1]);
  c = _addcarry_u64(c, t2,  0, &t2);
  r512[4] = t[0];

  //k=5
  t[3] = _umul128(a->bits64[2], a->bits64[3], &t[4]);
  c = _addcarry_u64(0, t[3], t[3], &t[3]);
  c = _addcarry_u64(c, t[4], t[4], &t[4]);
  c = _addcarry_u64(c, 0, 0, &t1);
  c = _addcarry_u64(0, t[3], t[1], &t[3]);
  c = _addcarry_u64(c, t[4], t2, &t[4]);
  c = _addcarry_u64(c, t1,  0, &t1);
  r512[5] = t[3];

  //k=6
  t[0] = _umul128(a->bits64[3], a->bits64[3], &t[1]);
  c = _addcarry_u64(0, t[0], t[4], &t[0]);
  c = _addcarry_u64(c, t[1], t1, &t[1]);
  r512[6] = t[0];

  //k=7
  r512[7] = t[1];

  // Reduce from 512 to 320 
  // Reduce from 512 to 320 
  imm_umul(r512 + 4, 0x1000003D1ULL, t);
  c = _addcarry_u64(0, r512[0], t[0], r512 + 0);
  c = _addcarry_u64(c, r512[1], t[1], r512 + 1);
  c = _addcarry_u64(c, r512[2], t[2], r512 + 2);
  c = _addcarry_u64(c, r512[3], t[3], r512 + 3);

  // Reduce from 320 to 256 
  // No overflow possible here t[4]+c<=0x1000003D1ULL
  u10 = _umul128(t[4] + c, 0x1000003D1ULL, &u11);
  c = _addcarry_u64(0, r512[0], u10, bits64 + 0);
  c = _addcarry_u64(c, r512[1], u11, bits64 + 1);
  c = _addcarry_u64(c, r512[2], 0, bits64 + 2);
  c = _addcarry_u64(c, r512[3], 0, bits64 + 3);
  // Probability of carry here or that this>P is very very unlikely
  bits64[4] = 0;
#if BISIZE==512
  bits64[5] = 0;
  bits64[6] = 0;
  bits64[7] = 0;
  bits64[8] = 0;
#endif

}
