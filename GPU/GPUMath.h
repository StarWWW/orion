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
// 256(+64) bits integer CUDA libray for SECPK1
// ---------------------------------------------------------------------------------



// We need 1 extra block for ModInv
#define NBBLOCK 5
#define BIFULLSIZE 40

// Assembly directives
#define UADDO(c, a, b) asm volatile ("add.cc.u64 %0, %1, %2;" : "=l"(c) : "l"(a), "l"(b));
#define UADDC(c, a, b) asm volatile ("addc.cc.u64 %0, %1, %2;" : "=l"(c) : "l"(a), "l"(b));
#define UADD(c, a, b) asm volatile ("addc.u64 %0, %1, %2;" : "=l"(c) : "l"(a), "l"(b));

#define UADDO1(c, a) asm volatile ("add.cc.u64 %0, %0, %1;" : "+l"(c) : "l"(a));
#define UADDC1(c, a) asm volatile ("addc.cc.u64 %0, %0, %1;" : "+l"(c) : "l"(a));
#define UADD1(c, a) asm volatile ("addc.u64 %0, %0, %1;" : "+l"(c) : "l"(a));

#define USUBO(c, a, b) asm volatile ("sub.cc.u64 %0, %1, %2;" : "=l"(c) : "l"(a), "l"(b));
#define USUBC(c, a, b) asm volatile ("subc.cc.u64 %0, %1, %2;" : "=l"(c) : "l"(a), "l"(b));
#define USUB(c, a, b) asm volatile ("subc.u64 %0, %1, %2;" : "=l"(c) : "l"(a), "l"(b));

#define USUBO1(c, a) asm volatile ("sub.cc.u64 %0, %0, %1;" : "+l"(c) : "l"(a));
#define USUBC1(c, a) asm volatile ("subc.cc.u64 %0, %0, %1;" : "+l"(c) : "l"(a));
#define USUB1(c, a) asm volatile ("subc.u64 %0, %0, %1;" : "+l"(c) : "l"(a) );

#define UMULLO(lo,a, b) asm volatile ("mul.lo.u64 %0, %1, %2;" : "=l"(lo) : "l"(a), "l"(b));
#define UMULHI(hi,a, b) asm volatile ("mul.hi.u64 %0, %1, %2;" : "=l"(hi) : "l"(a), "l"(b));
#define MADDO(r,a,b,c) asm volatile ("mad.hi.cc.u64 %0, %1, %2, %3;" : "=l"(r) : "l"(a), "l"(b), "l"(c));
#define MADDC(r,a,b,c) asm volatile ("madc.hi.cc.u64 %0, %1, %2, %3;" : "=l"(r) : "l"(a), "l"(b), "l"(c));
#define MADD(r,a,b,c) asm volatile ("madc.hi.u64 %0, %1, %2, %3;" : "=l"(r) : "l"(a), "l"(b), "l"(c));



//#define MM64 0xD838091DD2253531ULL
//#define MSK62 0x3FFFFFFFFFFFFFFF


__device__ __constant__ uint64_t MM64 = 0xD838091DD2253531ULL;
__device__ __constant__ uint64_t MSK62 = 0x3FFFFFFFFFFFFFFFULL;

// ---------------------------------------------------------------------------------------

#define _IsPositive(x) (((int64_t)(x[4]))>=0LL)
#define _IsNegative(x) (((int64_t)(x[4]))<0LL)
#define _IsEqual(a,b)  ((a[4] == b[4]) && (a[3] == b[3]) && (a[2] == b[2]) && (a[1] == b[1]) && (a[0] == b[0]))
#define _IsZero(a)     ((a[4] | a[3] | a[2] | a[1] | a[0]) == 0ULL)
#define _IsOne(a)      ((a[4] == 0ULL) && (a[3] == 0ULL) && (a[2] == 0ULL) && (a[1] == 0ULL) && (a[0] == 1ULL))

#define IDX threadIdx.x


// ---------------------------------------------------------------------------------------

#define AddP(r) { \
  UADDO1(r[0], 0xFFFFFFFEFFFFFC2FULL); \
  UADDC1(r[1], 0xFFFFFFFFFFFFFFFFULL); \
  UADDC1(r[2], 0xFFFFFFFFFFFFFFFFULL); \
  UADDC1(r[3], 0xFFFFFFFFFFFFFFFFULL); \
  UADD1(r[4], 0ULL);}

// ---------------------------------------------------------------------------------------

#define SubP(r) { \
  USUBO1(r[0], 0xFFFFFFFEFFFFFC2FULL); \
  USUBC1(r[1], 0xFFFFFFFFFFFFFFFFULL); \
  USUBC1(r[2], 0xFFFFFFFFFFFFFFFFULL); \
  USUBC1(r[3], 0xFFFFFFFFFFFFFFFFULL); \
  USUB1(r[4], 0ULL);}



// ---------------------------------------------------------------------------------------

#define Neg(r) {\
USUBO(r[0],0ULL,r[0]); \
USUBC(r[1],0ULL,r[1]); \
USUBC(r[2],0ULL,r[2]); \
USUBC(r[3],0ULL,r[3]); \
USUB(r[4],0ULL,r[4]); }

// ---------------------------------------------------------------------------------------

#define UMult(r, a, b) {\
  UMULLO(r[0],a[0],b); \
  UMULLO(r[1],a[1],b); \
  MADDO(r[1], a[0],b,r[1]); \
  UMULLO(r[2],a[2], b); \
  MADDC(r[2], a[1], b, r[2]); \
  UMULLO(r[3],a[3], b); \
  MADDC(r[3], a[2], b, r[3]); \
  MADD(r[4], a[3], b, 0ULL);}


#define UMultSpecial(r, a) {\
  uint64_t temp; /* Dichiarazione di temp qui */\
  r[0] = (a[0] << 32) + (a[0] << 9) + (a[0] << 8) + (a[0] << 7) + (a[0] << 6) + (a[0] << 4) + a[0]; \
  r[1] = (a[1] << 32) + (a[1] << 9) + (a[1] << 8) + (a[1] << 7) + (a[1] << 6) + (a[1] << 4) + a[1]; \
  MADDO(r[1], a[0], 0x1000003D1ULL, r[1]); \
  r[2] = (a[2] << 32) + (a[2] << 9) + (a[2] << 8) + (a[2] << 7) + (a[2] << 6) + (a[2] << 4) + a[2]; \
  MADDC(r[2], a[1], 0x1000003D1ULL, r[2]); \
  r[3] = (a[3] << 32) + (a[3] << 9) + (a[3] << 8) + (a[3] << 7) + (a[3] << 6) + (a[3] << 4) + a[3]; \
  temp = r[3]; \
  MADDC(r[3], a[2], 0x1000003D1ULL, r[3]); \
  r[4] = temp + a[3]; \
  MADD(r[4], a[3], 0x1000003D1ULL, 0ULL); \
}





// Multiply by 0x1000003D1 for the 512->320 reduction step. UMultSpecial expands it into
// shifts/adds (ALU pipe); ORION_REDMUL=1 uses real multiplies so the work goes to the FMA pipe.
#ifndef ORION_REDMUL
#define ORION_REDMUL 1
#endif
#if ORION_REDMUL
#define ReduceMult(r, a) UMult(r, a, 0x1000003D1ULL)
#else
#define ReduceMult(r, a) UMultSpecial(r, a)
#endif

// ---------------------------------------------------------------------------------------

#define Load(r, a) {\
  (r)[0] = (a)[0]; \
  (r)[1] = (a)[1]; \
  (r)[2] = (a)[2]; \
  (r)[3] = (a)[3]; \
  (r)[4] = (a)[4];}

// ---------------------------------------------------------------------------------------

#define _LoadI64(r, a) {\
  (r)[0] = a; \
  (r)[1] = a>>63; \
  (r)[2] = (r)[1]; \
  (r)[3] = (r)[1]; \
  (r)[4] = (r)[1];}
// ---------------------------------------------------------------------------------------

#define Load256(r, a) {\
  (r)[0] = (a)[0]; \
  (r)[1] = (a)[1]; \
  (r)[2] = (a)[2]; \
  (r)[3] = (a)[3];}

// ---------------------------------------------------------------------------------------

#define Load256A(r, a) {\
  (r)[0] = (a)[IDX]; \
  (r)[1] = (a)[IDX+blockDim.x]; \
  (r)[2] = (a)[IDX+2*blockDim.x]; \
  (r)[3] = (a)[IDX+3*blockDim.x];}

// ---------------------------------------------------------------------------------------

#define Store256A(r, a) {\
  (r)[IDX] = (a)[0]; \
  (r)[IDX+blockDim.x] = (a)[1]; \
  (r)[IDX+2*blockDim.x] = (a)[2]; \
  (r)[IDX+3*blockDim.x] = (a)[3];}

// ---------------------------------------------------------------------------------------

__device__ void ShiftR62(uint64_t *r) {

  r[0] = (r[1] << 2) | (r[0] >> 62);
  r[1] = (r[2] << 2) | (r[1] >> 62);
  r[2] = (r[3] << 2) | (r[2] >> 62);
  r[3] = (r[4] << 2) | (r[3] >> 62);
  // With sign extent
  r[4] = (int64_t)(r[4]) >> 62;

}

// ---------------------------------------------------------------------------------------

__device__ void IMult(uint64_t* r, uint64_t* a, int64_t b) {

    uint64_t t[NBBLOCK];

    // Make b positive
    if (b < 0) {
        b = -b;
        USUBO(t[0], 0ULL, a[0]);
        USUBC(t[1], 0ULL, a[1]);
        USUBC(t[2], 0ULL, a[2]);
        USUBC(t[3], 0ULL, a[3]);
        USUB(t[4], 0ULL, a[4]);
    }
    else {
        Load(t, a);
    }

    UMULLO(r[0], t[0], b);
    UMULLO(r[1], t[1], b);
    MADDO(r[1], t[0], b, r[1]);
    UMULLO(r[2], t[2], b);
    MADDC(r[2], t[1], b, r[2]);
    UMULLO(r[3], t[3], b);
    MADDC(r[3], t[2], b, r[3]);
    UMULLO(r[4], t[4], b);
    MADD(r[4], t[3], b, r[4]);

}



// ---------------------------------------------------------------------------------------

__device__ void MulP(uint64_t *r, uint64_t a) {

  uint64_t ah;
  uint64_t al;

  UMULLO(al, a, 0x1000003D1ULL);
  UMULHI(ah, a, 0x1000003D1ULL);

  USUBO(r[0], 0ULL, al);
  USUBC(r[1], 0ULL, ah);
  USUBC(r[2], 0ULL, 0ULL);
  USUBC(r[3], 0ULL, 0ULL);
  USUB(r[4], a, 0ULL);

}

// ---------------------------------------------------------------------------------------

__device__ void ModNeg256(uint64_t* r,uint64_t* a) {

  uint64_t t[4];
  USUBO(t[0],0ULL,a[0]);
  USUBC(t[1],0ULL,a[1]);
  USUBC(t[2],0ULL,a[2]);
  USUBC(t[3],0ULL,a[3]);
  UADDO(r[0],t[0],0xFFFFFFFEFFFFFC2FULL);
  UADDC(r[1],t[1],0xFFFFFFFFFFFFFFFFULL);
  UADDC(r[2],t[2],0xFFFFFFFFFFFFFFFFULL);
  UADD(r[3],t[3],0xFFFFFFFFFFFFFFFFULL);

}

// ---------------------------------------------------------------------------------------

__device__ void ModNeg256(uint64_t* r) {

  uint64_t t[4];
  USUBO(t[0],0ULL,r[0]);
  USUBC(t[1],0ULL,r[1]);
  USUBC(t[2],0ULL,r[2]);
  USUBC(t[3],0ULL,r[3]);
  UADDO(r[0],t[0],0xFFFFFFFEFFFFFC2FULL);
  UADDC(r[1],t[1],0xFFFFFFFFFFFFFFFFULL);
  UADDC(r[2],t[2],0xFFFFFFFFFFFFFFFFULL);
  UADD(r[3],t[3],0xFFFFFFFFFFFFFFFFULL);

}

// ---------------------------------------------------------------------------------------



__device__ void ModSub256(uint64_t* r, uint64_t* a, uint64_t* b) {
    uint64_t borrow;
    uint64_t p[4] = { 0xFFFFFFFEFFFFFC2FULL, 0xFFFFFFFFFFFFFFFFULL,
                      0xFFFFFFFFFFFFFFFFULL, 0xFFFFFFFFFFFFFFFFULL };

    USUBO(r[0], a[0], b[0]);
    USUBC(r[1], a[1], b[1]);
    USUBC(r[2], a[2], b[2]);
    USUBC(r[3], a[3], b[3]);
    USUB(borrow, 0ULL, 0ULL); 

    if (borrow) {
        UADDO1(r[0], p[0]);
        UADDC1(r[1], p[1]);
        UADDC1(r[2], p[2]);
        UADD1(r[3], p[3]);
    }
}


__device__ void ModSub256(uint64_t* r, uint64_t* b) {
    uint64_t borrow;
    uint64_t p[4] = { 0xFFFFFFFEFFFFFC2FULL, 0xFFFFFFFFFFFFFFFFULL,
                      0xFFFFFFFFFFFFFFFFULL, 0xFFFFFFFFFFFFFFFFULL };

    USUBO1(r[0], b[0]);
    USUBC1(r[1], b[1]);
    USUBC1(r[2], b[2]);
    USUBC1(r[3], b[3]);
    USUB(borrow, 0ULL, 0ULL); 

    if (borrow) {
        UADDO1(r[0], p[0]);
        UADDC1(r[1], p[1]);
        UADDC1(r[2], p[2]);
        UADD1(r[3], p[3]);
    }
}


__device__ void ModSub256isOdd(uint64_t* a, uint64_t* b, uint8_t* parity) {    //no need to compute py, we need only parity

    uint64_t t;   
    uint64_t T[4]; 

    USUBO(T[0], a[0], b[0]);   
    USUBC(T[1], a[1], b[1]);   
    USUBC(T[2], a[2], b[2]);   
    USUBC(T[3], a[3], b[3]);  

    USUB(t, 0ULL, 0ULL);  // borrow

    *parity = (T[0] & 1) ^ (t & 1);  // LSB of T[0] and LSB of t -> parity od sub
}



// ---------------------------------------------------------------------------------------
// Compute a*b*(mod n)
// a and b must be lower than n
// ---------------------------------------------------------------------------------------


__device__ void _ModMult(uint64_t *r, uint64_t *a, uint64_t *b) {

  uint64_t r512[8];
  uint64_t t[NBBLOCK];
  uint64_t ah, al;

  r512[5] = 0;
  r512[6] = 0;
  r512[7] = 0;

  // 256*256 multiplier
  UMult(r512, a, b[0]);
  UMult(t, a, b[1]);
  UADDO1(r512[1], t[0]);
  UADDC1(r512[2], t[1]);
  UADDC1(r512[3], t[2]);
  UADDC1(r512[4], t[3]);
  UADD1(r512[5], t[4]);
  UMult(t, a, b[2]);
  UADDO1(r512[2], t[0]);
  UADDC1(r512[3], t[1]);
  UADDC1(r512[4], t[2]);
  UADDC1(r512[5], t[3]);
  UADD1(r512[6], t[4]);
  UMult(t, a, b[3]);
  UADDO1(r512[3], t[0]);
  UADDC1(r512[4], t[1]);
  UADDC1(r512[5], t[2]);
  UADDC1(r512[6], t[3]);
  UADD1(r512[7], t[4]);

  // Reduce from 512 to 320
  ReduceMult(t, (r512 + 4));
  UADDO1(r512[0], t[0]);
  UADDC1(r512[1], t[1]);
  UADDC1(r512[2], t[2]);
  UADDC1(r512[3], t[3]);

  // Reduce from 320 to 256
  UADD1(t[4], 0ULL);
  UMULLO(al, t[4], 0x1000003D1ULL);
  UMULHI(ah, t[4], 0x1000003D1ULL);
  UADDO(r[0], r512[0], al);
  UADDC(r[1], r512[1], ah);
  UADDC(r[2], r512[2], 0ULL);
  UADD(r[3], r512[3], 0ULL);

}




__device__ void _ModMult(uint64_t *r, uint64_t *a) {

  uint64_t r512[8];
  uint64_t t[NBBLOCK];
  uint64_t ah, al;
  r512[5] = 0;
  r512[6] = 0;
  r512[7] = 0;

  // 256*256 multiplier
  UMult(r512, a, r[0]);
  UMult(t, a, r[1]);
  UADDO1(r512[1], t[0]);
  UADDC1(r512[2], t[1]);
  UADDC1(r512[3], t[2]);
  UADDC1(r512[4], t[3]);
  UADD1(r512[5], t[4]);
  UMult(t, a, r[2]);
  UADDO1(r512[2], t[0]);
  UADDC1(r512[3], t[1]);
  UADDC1(r512[4], t[2]);
  UADDC1(r512[5], t[3]);
  UADD1(r512[6], t[4]);
  UMult(t, a, r[3]);
  UADDO1(r512[3], t[0]);
  UADDC1(r512[4], t[1]);
  UADDC1(r512[5], t[2]);
  UADDC1(r512[6], t[3]);
  UADD1(r512[7], t[4]);

  // Reduce from 512 to 320
  ReduceMult(t, (r512 + 4));
  UADDO1(r512[0], t[0]);
  UADDC1(r512[1], t[1]);
  UADDC1(r512[2], t[2]);
  UADDC1(r512[3], t[3]);

  // Reduce from 320 to 256
  UADD1(t[4], 0ULL);
  UMULLO(al, t[4], 0x1000003D1ULL);
  UMULHI(ah, t[4], 0x1000003D1ULL);
  UADDO(r[0], r512[0], al);
  UADDC(r[1], r512[1], ah);
  UADDC(r[2], r512[2], 0ULL);
  UADD(r[3], r512[3], 0ULL);

}






__device__ void _ModSqr(uint64_t *rp, const uint64_t *up) {

  uint64_t r512[8];

  uint64_t u10, u11;

  uint64_t r0;
  uint64_t r1;
  uint64_t r3;
  uint64_t r4;

  uint64_t t1;
  uint64_t t2;


  //k=0
  UMULLO(r512[0], up[0], up[0]);
  UMULHI(r1, up[0], up[0]);

  //k=1
  UMULLO(r3, up[0], up[1]);
  UMULHI(r4, up[0], up[1]);
  UADDO1(r3, r3);
  UADDC1(r4, r4);
  UADD(t1, 0x0ULL, 0x0ULL);
  UADDO1(r3, r1);
  UADDC1(r4, 0x0ULL);
  UADD1(t1, 0x0ULL);
  r512[1] = r3;

  //k=2
  UMULLO(r0, up[0], up[2]);
  UMULHI(r1, up[0], up[2]);
  UADDO1(r0, r0);
  UADDC1(r1, r1);
  UADD(t2, 0x0ULL, 0x0ULL);
  UMULLO(u10, up[1], up[1]);
  UMULHI(u11, up[1], up[1]);
  UADDO1(r0, u10);
  UADDC1(r1, u11);
  UADD1(t2, 0x0ULL);
  UADDO1(r0, r4);
  UADDC1(r1, t1);
  UADD1(t2, 0x0ULL);

  r512[2] = r0;

  //k=3
  UMULLO(r3, up[0], up[3]);
  UMULHI(r4, up[0], up[3]);
  UMULLO(u10, up[1], up[2]);
  UMULHI(u11, up[1], up[2]);
  UADDO1(r3, u10);
  UADDC1(r4, u11);
  UADD(t1, 0x0ULL, 0x0ULL);
  t1 += t1;
  UADDO1(r3, r3);
  UADDC1(r4, r4);
  UADD1(t1, 0x0ULL);
  UADDO1(r3, r1);
  UADDC1(r4, t2);
  UADD1(t1, 0x0ULL);

  r512[3] = r3;

  //k=4
  UMULLO(r0, up[1], up[3]);
  UMULHI(r1, up[1], up[3]);
  UADDO1(r0, r0);
  UADDC1(r1, r1);
  UADD(t2, 0x0ULL, 0x0ULL);
  UMULLO(u10, up[2], up[2]);
  UMULHI(u11, up[2], up[2]);
  UADDO1(r0, u10);
  UADDC1(r1, u11);
  UADD1(t2, 0x0ULL);
  UADDO1(r0, r4);
  UADDC1(r1, t1);
  UADD1(t2, 0x0ULL);

  r512[4] = r0;

  //k=5
  UMULLO(r3, up[2], up[3]);
  UMULHI(r4, up[2], up[3]);
  UADDO1(r3, r3);
  UADDC1(r4, r4);
  UADD(t1, 0x0ULL, 0x0ULL);
  UADDO1(r3, r1);
  UADDC1(r4, t2);
  UADD1(t1, 0x0ULL);

  r512[5] = r3;

  //k=6
  UMULLO(r0, up[3], up[3]);
  UMULHI(r1, up[3], up[3]);
  UADDO1(r0, r4);
  UADD1(r1, t1);
  r512[6] = r0;

  //k=7
  r512[7] = r1;


  // Reduce from 512 to 320
  UMULLO(r0, r512[4], 0x1000003D1ULL);
  UMULLO(r1, r512[5], 0x1000003D1ULL);
  MADDO(r1, r512[4], 0x1000003D1ULL, r1);
  UMULLO(t2, r512[6], 0x1000003D1ULL);
  MADDC(t2, r512[5], 0x1000003D1ULL, t2);
  UMULLO(r3, r512[7], 0x1000003D1ULL);
  MADDC(r3, r512[6], 0x1000003D1ULL, r3);
  MADD(r4, r512[7], 0x1000003D1ULL, 0ULL);

  UADDO1(r512[0], r0);
  UADDC1(r512[1], r1);
  UADDC1(r512[2], t2);
  UADDC1(r512[3], r3);

  // Reduce from 320 to 256
  UADD1(r4, 0ULL);
  UMULLO(u10, r4, 0x1000003D1ULL);
  UMULHI(u11, r4, 0x1000003D1ULL);
  UADDO(rp[0], r512[0], u10);
  UADDC(rp[1], r512[1], u11);
  UADDC(rp[2], r512[2], 0ULL);
  UADD(rp[3], r512[3], 0ULL);


}

// ---------------------------------------------------------------------------------------

#define MADDS(r,a,b,c) asm volatile ("madc.hi.s64 %0, %1, %2, %3;" : "=l"(r) : "l"(a), "l"(b), "l"(c));

#define __sright128(a,b,n) ((a)>>(n))|((b)<<(64-(n)))
#define __sleft128(a,b,n) ((b)<<(n))|((a)>>(64-(n)))

__device__ void ShiftR62(uint64_t dest[5], uint64_t r[5], uint64_t carry) {

    dest[0] = (r[1] << 2) | (r[0] >> 62);
    dest[1] = (r[2] << 2) | (r[1] >> 62);
    dest[2] = (r[3] << 2) | (r[2] >> 62);
    dest[3] = (r[4] << 2) | (r[3] >> 62);
    dest[4] = (carry << 2) | (r[4] >> 62);

}


__device__ uint64_t IMultC(uint64_t* r, uint64_t* a, int64_t b) {

    uint64_t t[NBBLOCK];
    uint64_t carry;

    // Make b positive
    if (b < 0) {
        b = -b;
        USUBO(t[0], 0ULL, a[0]);
        USUBC(t[1], 0ULL, a[1]);
        USUBC(t[2], 0ULL, a[2]);
        USUBC(t[3], 0ULL, a[3]);
        USUB(t[4], 0ULL, a[4]);
    }
    else {
        Load(t, a);
    }

    UMULLO(r[0], t[0], b);
    UMULLO(r[1], t[1], b);
    MADDO(r[1], t[0], b, r[1]);
    UMULLO(r[2], t[2], b);
    MADDC(r[2], t[1], b, r[2]);
    UMULLO(r[3], t[3], b);
    MADDC(r[3], t[2], b, r[3]);
    UMULLO(r[4], t[4], b);
    MADDC(r[4], t[3], b, r[4]);
    MADDS(carry, t[4], b, 0ULL);

    return carry;

}

__device__ __forceinline__ uint32_t ctz(uint64_t x) {
    uint32_t n;
    asm("{\n\t"
        " .reg .u64 tmp;\n\t"
        " brev.b64 tmp, %1;\n\t"
        " clz.b64 %0, tmp;\n\t"
        "}"
        : "=r"(n) : "l"(x));
    return n;
}

// ---------------------------------------------------------------------------------------
#define SWAP(tmp,x,y) tmp = x; x = y; y = tmp;


__device__ void _DivStep62(uint64_t u[5], uint64_t v[5],
    int32_t* pos,
    int64_t* uu, int64_t* uv,
    int64_t* vu, int64_t* vv) {


    // u' = (uu*u + uv*v) >> bitCount
    // v' = (vu*u + vv*v) >> bitCount
    // Do not maintain a matrix for r and s, the number of 
    // 'added P' can be easily calculated

    *uu = 1; *uv = 0;
    *vu = 0; *vv = 1;

    uint32_t bitCount = 62;
    uint32_t zeros;
    uint64_t u0 = u[0];
    uint64_t v0 = v[0];

    // Extract 64 MSB of u and v
    // u and v must be positive
    uint64_t uh, vh;
    int64_t w, x, y, z;
    bitCount = 62;

    while (*pos > 0 && (u[*pos] | v[*pos]) == 0) (*pos)--;
    if (*pos == 0) {

        uh = u[0];
        vh = v[0];

    }
    else {

        uint32_t s = __clzll(u[*pos] | v[*pos]);
        if (s == 0) {
            uh = u[*pos];
            vh = v[*pos];
        }
        else {
            uh = __sleft128(u[*pos - 1], u[*pos], s);
            vh = __sleft128(v[*pos - 1], v[*pos], s);
        }

    }


    while (true) {

        // Use a sentinel bit to count zeros only up to bitCount
        zeros = ctz(v0 | (1ULL << bitCount));

        v0 >>= zeros;
        vh >>= zeros;
        *uu <<= zeros;
        *uv <<= zeros;
        bitCount -= zeros;

        if (bitCount == 0)
            break;

        if (vh < uh) {
            SWAP(w, uh, vh);
            SWAP(x, u0, v0);
            SWAP(y, *uu, *vu);
            SWAP(z, *uv, *vv);
        }

        vh -= uh;
        v0 -= u0;
        *vv -= *uv;
        *vu -= *uu;

    }

}

__device__ void MatrixVecMulHalf(uint64_t dest[5], uint64_t u[5], uint64_t v[5], int64_t _11, int64_t _12, uint64_t* carry) {

    uint64_t t1[NBBLOCK];
    uint64_t t2[NBBLOCK];
    uint64_t c1, c2;

    c1 = IMultC(t1, u, _11);
    c2 = IMultC(t2, v, _12);

    UADDO(dest[0], t1[0], t2[0]);
    UADDC(dest[1], t1[1], t2[1]);
    UADDC(dest[2], t1[2], t2[2]);
    UADDC(dest[3], t1[3], t2[3]);
    UADDC(dest[4], t1[4], t2[4]);
    UADD(*carry, c1, c2);

}

__device__ void MatrixVecMul(uint64_t u[5], uint64_t v[5], int64_t _11, int64_t _12, int64_t _21, int64_t _22) {

    uint64_t t1[NBBLOCK];
    uint64_t t2[NBBLOCK];
    uint64_t t3[NBBLOCK];
    uint64_t t4[NBBLOCK];

    IMult(t1, u, _11);
    IMult(t2, v, _12);
    IMult(t3, u, _21);
    IMult(t4, v, _22);

    UADDO(u[0], t1[0], t2[0]);
    UADDC(u[1], t1[1], t2[1]);
    UADDC(u[2], t1[2], t2[2]);
    UADDC(u[3], t1[3], t2[3]);
    UADD(u[4], t1[4], t2[4]);

    UADDO(v[0], t3[0], t4[0]);
    UADDC(v[1], t3[1], t4[1]);
    UADDC(v[2], t3[2], t4[2]);
    UADDC(v[3], t3[3], t4[3]);
    UADD(v[4], t3[4], t4[4]);

}

__device__ uint64_t AddCh(uint64_t r[5], uint64_t a[5], uint64_t carry) {

    uint64_t carryOut;

    UADDO1(r[0], a[0]);
    UADDC1(r[1], a[1]);
    UADDC1(r[2], a[2]);
    UADDC1(r[3], a[3]);
    UADDC1(r[4], a[4]);
    UADD(carryOut, carry, 0ULL);

    return carryOut;

}

__device__ __noinline__ void _ModInv(uint64_t* R) {

    // Compute modular inverse of R mop P (using 320bits signed integer)
    // 0 < this < P  , P must be odd
    // Return 0 if no inverse
    // See IntMod.cpp for more info.

    int64_t  uu, uv, vu, vv;
    uint64_t mr0, ms0;
    int32_t  pos = NBBLOCK - 1;

    uint64_t u[NBBLOCK];
    uint64_t v[NBBLOCK];
    uint64_t r[NBBLOCK];
    uint64_t s[NBBLOCK];
    uint64_t tr[NBBLOCK];
    uint64_t ts[NBBLOCK];
    uint64_t r0[NBBLOCK];
    uint64_t s0[NBBLOCK];
    uint64_t carryR;
    uint64_t carryS;

    u[0] = 0xFFFFFFFEFFFFFC2F;
    u[1] = 0xFFFFFFFFFFFFFFFF;
    u[2] = 0xFFFFFFFFFFFFFFFF;
    u[3] = 0xFFFFFFFFFFFFFFFF;
    u[4] = 0;
    Load(v, R);
    r[0] = 0; s[0] = 1;
    r[1] = 0; s[1] = 0;
    r[2] = 0; s[2] = 0;
    r[3] = 0; s[3] = 0;
    r[4] = 0; s[4] = 0;

    // Delayed right shift 62bits

    // DivStep loop -------------------------------

    while (true) {

        _DivStep62(u, v, &pos, &uu, &uv, &vu, &vv);

        MatrixVecMul(u, v, uu, uv, vu, vv);

        if (_IsNegative(u)) {
            Neg(u);
            uu = -uu;
            uv = -uv;
        }
        if (_IsNegative(v)) {
            Neg(v);
            vu = -vu;
            vv = -vv;
        }

        ShiftR62(u);
        ShiftR62(v);

        // Update r
        MatrixVecMulHalf(tr, r, s, uu, uv, &carryR);
        mr0 = (tr[0] * MM64) & MSK62;
        MulP(r0, mr0);
        carryR = AddCh(tr, r0, carryR);

        if (_IsZero(v)) {

            ShiftR62(r, tr, carryR);
            break;

        }
        else {

            // Update s
            MatrixVecMulHalf(ts, r, s, vu, vv, &carryS);
            ms0 = (ts[0] * MM64) & MSK62;
            MulP(s0, ms0);
            carryS = AddCh(ts, s0, carryS);

        }

        ShiftR62(r, tr, carryR);
        ShiftR62(s, ts, carryS);

    }

    // u ends with gcd
    if (!_IsOne(u)) {
        // No inverse
        R[0] = 0ULL;
        R[1] = 0ULL;
        R[2] = 0ULL;
        R[3] = 0ULL;
        R[4] = 0ULL;
        return;
    }

    while (_IsNegative(r))
        AddP(r);
    while (!_IsNegative(r))
        SubP(r);
    AddP(r);

    Load(R, r);

}

// ---------------------------------------------------------------------------------------
// Native 32-bit limb secp256k1 field arithmetic tuned for NVIDIA Blackwell (sm_120)
// Eliminates duplicate IADD.64.X instructions, branch-divergent ModSub256, and IMAD.WIDE x1.
// ---------------------------------------------------------------------------------------

#define ADD32_CC(d, a, b)   asm volatile("add.cc.u32 %0, %1, %2;" : "=r"(d) : "r"(a), "r"(b))
#define ADDC32_CC(d, a, b)  asm volatile("addc.cc.u32 %0, %1, %2;" : "=r"(d) : "r"(a), "r"(b))
#define ADDC32(d, a, b)     asm volatile("addc.u32 %0, %1, %2;" : "=r"(d) : "r"(a), "r"(b))
#define ADDC0_32(d)         asm volatile("addc.u32 %0, 0, 0;" : "=r"(d))

#define COMBA_MAC(acc, c2, a, b) do { \
    asm volatile( \
        "{\n\t" \
        ".reg .u64 _p;\n\t" \
        "mul.wide.u32 _p, %2, %3;\n\t" \
        "add.cc.u64 %0, %0, _p;\n\t" \
        "addc.u32 %1, %1, 0;\n\t" \
        "}" \
        : "+l"(acc), "+r"(c2) : "r"(a), "r"(b)); \
} while (0)

#define COMBA_MAC_INIT(acc, c2, a, b) do { \
    asm volatile( \
        "{\n\t" \
        ".reg .u64 _p;\n\t" \
        "mul.wide.u32 _p, %2, %3;\n\t" \
        "add.cc.u64 %0, %0, _p;\n\t" \
        "addc.u32 %1, 0, 0;\n\t" \
        "}" \
        : "+l"(acc), "=r"(c2) : "r"(a), "r"(b)); \
} while (0)

#define COMBA_MAC0(acc, a, b) do { \
    asm volatile("mad.wide.u32 %0, %1, %2, %0;" : "+l"(acc) : "r"(a), "r"(b)); \
} while (0)

#define COMBA_STEP(out_limb, acc, c2) do { \
    out_limb = (uint32_t)(acc); \
    acc = ((acc) >> 32) | ((uint64_t)(c2) << 32); \
} while (0)

__device__ __forceinline__ void Load256_32(uint32_t dst[8], const uint32_t src[8]) {
#pragma unroll
    for (int k = 0; k < 8; k++) dst[k] = src[k];
}

__device__ __forceinline__ void LoadConst256_32(uint32_t dst[8], const uint64_t src[4]) {
    uint64_t v0 = src[0], v1 = src[1], v2 = src[2], v3 = src[3];
    dst[0] = (uint32_t)v0; dst[1] = (uint32_t)(v0 >> 32);
    dst[2] = (uint32_t)v1; dst[3] = (uint32_t)(v1 >> 32);
    dst[4] = (uint32_t)v2; dst[5] = (uint32_t)(v2 >> 32);
    dst[6] = (uint32_t)v3; dst[7] = (uint32_t)(v3 >> 32);
}

__device__ __forceinline__ void Load256A_32(uint32_t r[8], const uint64_t *a) {
    uint64_t v0 = a[IDX];
    uint64_t v1 = a[IDX + blockDim.x];
    uint64_t v2 = a[IDX + 2 * blockDim.x];
    uint64_t v3 = a[IDX + 3 * blockDim.x];
    r[0] = (uint32_t)v0; r[1] = (uint32_t)(v0 >> 32);
    r[2] = (uint32_t)v1; r[3] = (uint32_t)(v1 >> 32);
    r[4] = (uint32_t)v2; r[5] = (uint32_t)(v2 >> 32);
    r[6] = (uint32_t)v3; r[7] = (uint32_t)(v3 >> 32);
}

__device__ __forceinline__ void Store256A_32(uint64_t *a, const uint32_t r[8]) {
    a[IDX]                  = (uint64_t)r[0] | ((uint64_t)r[1] << 32);
    a[IDX + blockDim.x]     = (uint64_t)r[2] | ((uint64_t)r[3] << 32);
    a[IDX + 2 * blockDim.x] = (uint64_t)r[4] | ((uint64_t)r[5] << 32);
    a[IDX + 3 * blockDim.x] = (uint64_t)r[6] | ((uint64_t)r[7] << 32);
}

// Branchless r = a - b (mod p) using pure 32-bit ALU-lite adds + 1 IMAD (zero ALU-heavy, zero divergence)
__device__ __forceinline__ void ModSub256_32(uint32_t r[8], const uint32_t a[8], const uint32_t b[8]) {
    uint32_t r0 = a[0], r1 = a[1], r2 = a[2], r3 = a[3];
    uint32_t r4 = a[4], r5 = a[5], r6 = a[6], r7 = a[7];
    asm volatile(
        "{\n\t"
        ".reg .u32 _b, _c0, _c1;\n\t"
        "sub.cc.u32  %0, %0, %8;\n\t"
        "subc.cc.u32 %1, %1, %9;\n\t"
        "subc.cc.u32 %2, %2, %10;\n\t"
        "subc.cc.u32 %3, %3, %11;\n\t"
        "subc.cc.u32 %4, %4, %12;\n\t"
        "subc.cc.u32 %5, %5, %13;\n\t"
        "subc.cc.u32 %6, %6, %14;\n\t"
        "subc.cc.u32 %7, %7, %15;\n\t"
        "subc.u32    _b, 0, 0;\n\t"
        "mul.lo.u32  _c0, _b, 977;\n\t"
        "add.u32     _c1, _b, _b;\n\t"
        "add.cc.u32  %0, %0, _c0;\n\t"
        "addc.cc.u32 %1, %1, _c1;\n\t"
        "addc.cc.u32 %2, %2, _b;\n\t"
        "addc.cc.u32 %3, %3, _b;\n\t"
        "addc.cc.u32 %4, %4, _b;\n\t"
        "addc.cc.u32 %5, %5, _b;\n\t"
        "addc.cc.u32 %6, %6, _b;\n\t"
        "addc.u32    %7, %7, _b;\n\t"
        "}"
        : "+r"(r0), "+r"(r1), "+r"(r2), "+r"(r3),
          "+r"(r4), "+r"(r5), "+r"(r6), "+r"(r7)
        : "r"(b[0]), "r"(b[1]), "r"(b[2]), "r"(b[3]),
          "r"(b[4]), "r"(b[5]), "r"(b[6]), "r"(b[7]));
    r[0] = r0; r[1] = r1; r[2] = r2; r[3] = r3;
    r[4] = r4; r[5] = r5; r[6] = r6; r[7] = r7;
}

// Branchless r = a + b (mod p) for a, b < p: zero ALU-heavy, zero divergence
__device__ __forceinline__ void ModAdd256_32(uint32_t r[8], const uint32_t a[8], const uint32_t b[8]) {
    uint32_t r0 = a[0], r1 = a[1], r2 = a[2], r3 = a[3];
    uint32_t r4 = a[4], r5 = a[5], r6 = a[6], r7 = a[7];
    asm volatile(
        "{\n\t"
        ".reg .u32 _c, _c0;\n\t"
        "add.cc.u32  %0, %0, %8;\n\t"
        "addc.cc.u32 %1, %1, %9;\n\t"
        "addc.cc.u32 %2, %2, %10;\n\t"
        "addc.cc.u32 %3, %3, %11;\n\t"
        "addc.cc.u32 %4, %4, %12;\n\t"
        "addc.cc.u32 %5, %5, %13;\n\t"
        "addc.cc.u32 %6, %6, %14;\n\t"
        "addc.cc.u32 %7, %7, %15;\n\t"
        "addc.u32    _c, 0, 0;\n\t"
        "mul.lo.u32  _c0, _c, 977;\n\t"
        "add.cc.u32  %0, %0, _c0;\n\t"
        "addc.cc.u32 %1, %1, _c;\n\t"
        "addc.cc.u32 %2, %2, 0;\n\t"
        "addc.cc.u32 %3, %3, 0;\n\t"
        "addc.cc.u32 %4, %4, 0;\n\t"
        "addc.cc.u32 %5, %5, 0;\n\t"
        "addc.cc.u32 %6, %6, 0;\n\t"
        "addc.u32    %7, %7, 0;\n\t"
        "}"
        : "+r"(r0), "+r"(r1), "+r"(r2), "+r"(r3),
          "+r"(r4), "+r"(r5), "+r"(r6), "+r"(r7)
        : "r"(b[0]), "r"(b[1]), "r"(b[2]), "r"(b[3]),
          "r"(b[4]), "r"(b[5]), "r"(b[6]), "r"(b[7]));
    r[0] = r0; r[1] = r1; r[2] = r2; r[3] = r3;
    r[4] = r4; r[5] = r5; r[6] = r6; r[7] = r7;
}

// Parity of (a - b) mod p: only needs LSB of (a[0] - b[0]) XOR borrow(a < b)
__device__ __forceinline__ uint32_t ModSub256isOdd_32(const uint32_t a[8], const uint32_t b[8]) {
    uint32_t t, borrow;
    asm volatile(
        "sub.cc.u32  %0, %2, %10;\n\t"
        "subc.cc.u32 %1, %3, %11;\n\t"
        "subc.cc.u32 %1, %4, %12;\n\t"
        "subc.cc.u32 %1, %5, %13;\n\t"
        "subc.cc.u32 %1, %6, %14;\n\t"
        "subc.cc.u32 %1, %7, %15;\n\t"
        "subc.cc.u32 %1, %8, %16;\n\t"
        "subc.cc.u32 %1, %9, %17;\n\t"
        "subc.u32    %1, 0, 0;\n\t"
        : "=&r"(t), "=&r"(borrow)
        : "r"(a[0]), "r"(a[1]), "r"(a[2]), "r"(a[3]),
          "r"(a[4]), "r"(a[5]), "r"(a[6]), "r"(a[7]),
          "r"(b[0]), "r"(b[1]), "r"(b[2]), "r"(b[3]),
          "r"(b[4]), "r"(b[5]), "r"(b[6]), "r"(b[7]));
    return (t ^ borrow) & 1u;
}

// Direct 512->256 reduction: L + H*(2^32 + 977) using 8 independent mul.wide.u32 + 32-bit carry chains
// Zero IADD.64, zero HFMA2 zeroing, 8x ILP on FMA-heavy!
__device__ __forceinline__ void Reduce512_Wide32(uint32_t out[8], const uint32_t r[16]) {
    uint64_t m0 = (uint64_t)r[8]  * 977u;
    uint64_t m1 = (uint64_t)r[9]  * 977u;
    uint64_t m2 = (uint64_t)r[10] * 977u;
    uint64_t m3 = (uint64_t)r[11] * 977u;
    uint64_t m4 = (uint64_t)r[12] * 977u;
    uint64_t m5 = (uint64_t)r[13] * 977u;
    uint64_t m6 = (uint64_t)r[14] * 977u;
    uint64_t m7 = (uint64_t)r[15] * 977u;

    uint32_t t0, t1, t2, t3, t4, t5, t6, t7, ov_lo, ov_hi;
    // Chain 1: L[0..7] + lo(H[0..7] * 977)
    ADD32_CC(t0,    r[0], (uint32_t)m0);
    ADDC32_CC(t1,   r[1], (uint32_t)m1);
    ADDC32_CC(t2,   r[2], (uint32_t)m2);
    ADDC32_CC(t3,   r[3], (uint32_t)m3);
    ADDC32_CC(t4,   r[4], (uint32_t)m4);
    ADDC32_CC(t5,   r[5], (uint32_t)m5);
    ADDC32_CC(t6,   r[6], (uint32_t)m6);
    ADDC32_CC(t7,   r[7], (uint32_t)m7);
    ADDC0_32(ov_lo);

    // Chain 2: + (H[0..7] << 32)
    ADD32_CC(t1,    t1,    r[8]);
    ADDC32_CC(t2,   t2,    r[9]);
    ADDC32_CC(t3,   t3,    r[10]);
    ADDC32_CC(t4,   t4,    r[11]);
    ADDC32_CC(t5,   t5,    r[12]);
    ADDC32_CC(t6,   t6,    r[13]);
    ADDC32_CC(t7,   t7,    r[14]);
    ADDC32_CC(ov_lo, ov_lo, r[15]);
    ADDC0_32(ov_hi);

    // Chain 3: + (hi(H[0..7] * 977) << 32)
    ADD32_CC(t1,    t1,    (uint32_t)(m0 >> 32));
    ADDC32_CC(t2,   t2,    (uint32_t)(m1 >> 32));
    ADDC32_CC(t3,   t3,    (uint32_t)(m2 >> 32));
    ADDC32_CC(t4,   t4,    (uint32_t)(m3 >> 32));
    ADDC32_CC(t5,   t5,    (uint32_t)(m4 >> 32));
    ADDC32_CC(t6,   t6,    (uint32_t)(m5 >> 32));
    ADDC32_CC(t7,   t7,    (uint32_t)(m6 >> 32));
    ADDC32_CC(ov_lo, ov_lo, (uint32_t)(m7 >> 32));
    ADDC32(ov_hi,   ov_hi, 0u);

    // Step 3: reduce 33-bit overflow (ov_hi in {0,1}): ov * (2^32 + 977)
    uint64_t q = (uint64_t)ov_lo * 977u;
    uint32_t q0 = (uint32_t)q;
    uint32_t q1 = (uint32_t)(q >> 32);
    uint32_t q2;
    uint32_t oh_c = ov_hi * 977u;
    ADD32_CC(q1, q1, ov_lo);
    ADDC32(q2, ov_hi, 0u);
    ADD32_CC(q1, q1, oh_c);
    ADDC32(q2, q2, 0u);

    ADD32_CC(out[0],  t0, q0);
    ADDC32_CC(out[1], t1, q1);
    ADDC32_CC(out[2], t2, q2);
    ADDC32_CC(out[3], t3, 0u);
    ADDC32_CC(out[4], t4, 0u);
    ADDC32_CC(out[5], t5, 0u);
    ADDC32_CC(out[6], t6, 0u);
    ADDC32(out[7],    t7, 0u);
}

// Comba 256x256 -> 512 using IMAD.WIDE.U32 (-38% cycles, -19% insts, -12 regs on sm_120)
__device__ __forceinline__ void ModMult_Wide32(uint32_t out[8], const uint32_t a[8], const uint32_t b[8]) {
    uint32_t r[16];
    uint64_t acc;
    uint32_t c2;

    asm volatile("mul.wide.u32 %0, %1, %2;" : "=l"(acc) : "r"(a[0]), "r"(b[0]));
    r[0] = (uint32_t)acc; acc >>= 32;

    COMBA_MAC0(acc, a[0], b[1]); COMBA_MAC_INIT(acc, c2, a[1], b[0]);
    COMBA_STEP(r[1], acc, c2);

    COMBA_MAC0(acc, a[0], b[2]); COMBA_MAC_INIT(acc, c2, a[1], b[1]); COMBA_MAC(acc, c2, a[2], b[0]);
    COMBA_STEP(r[2], acc, c2);

    COMBA_MAC_INIT(acc, c2, a[0], b[3]); COMBA_MAC(acc, c2, a[1], b[2]); COMBA_MAC(acc, c2, a[2], b[1]); COMBA_MAC(acc, c2, a[3], b[0]);
    COMBA_STEP(r[3], acc, c2);

    COMBA_MAC_INIT(acc, c2, a[0], b[4]); COMBA_MAC(acc, c2, a[1], b[3]); COMBA_MAC(acc, c2, a[2], b[2]); COMBA_MAC(acc, c2, a[3], b[1]); COMBA_MAC(acc, c2, a[4], b[0]);
    COMBA_STEP(r[4], acc, c2);

    COMBA_MAC_INIT(acc, c2, a[0], b[5]); COMBA_MAC(acc, c2, a[1], b[4]); COMBA_MAC(acc, c2, a[2], b[3]); COMBA_MAC(acc, c2, a[3], b[2]); COMBA_MAC(acc, c2, a[4], b[1]); COMBA_MAC(acc, c2, a[5], b[0]);
    COMBA_STEP(r[5], acc, c2);

    COMBA_MAC_INIT(acc, c2, a[0], b[6]); COMBA_MAC(acc, c2, a[1], b[5]); COMBA_MAC(acc, c2, a[2], b[4]); COMBA_MAC(acc, c2, a[3], b[3]); COMBA_MAC(acc, c2, a[4], b[2]); COMBA_MAC(acc, c2, a[5], b[1]); COMBA_MAC(acc, c2, a[6], b[0]);
    COMBA_STEP(r[6], acc, c2);

    COMBA_MAC_INIT(acc, c2, a[0], b[7]); COMBA_MAC(acc, c2, a[1], b[6]); COMBA_MAC(acc, c2, a[2], b[5]); COMBA_MAC(acc, c2, a[3], b[4]); COMBA_MAC(acc, c2, a[4], b[3]); COMBA_MAC(acc, c2, a[5], b[2]); COMBA_MAC(acc, c2, a[6], b[1]); COMBA_MAC(acc, c2, a[7], b[0]);
    COMBA_STEP(r[7], acc, c2);

    COMBA_MAC_INIT(acc, c2, a[1], b[7]); COMBA_MAC(acc, c2, a[2], b[6]); COMBA_MAC(acc, c2, a[3], b[5]); COMBA_MAC(acc, c2, a[4], b[4]); COMBA_MAC(acc, c2, a[5], b[3]); COMBA_MAC(acc, c2, a[6], b[2]); COMBA_MAC(acc, c2, a[7], b[1]);
    COMBA_STEP(r[8], acc, c2);

    COMBA_MAC_INIT(acc, c2, a[2], b[7]); COMBA_MAC(acc, c2, a[3], b[6]); COMBA_MAC(acc, c2, a[4], b[5]); COMBA_MAC(acc, c2, a[5], b[4]); COMBA_MAC(acc, c2, a[6], b[3]); COMBA_MAC(acc, c2, a[7], b[2]);
    COMBA_STEP(r[9], acc, c2);

    COMBA_MAC_INIT(acc, c2, a[3], b[7]); COMBA_MAC(acc, c2, a[4], b[6]); COMBA_MAC(acc, c2, a[5], b[5]); COMBA_MAC(acc, c2, a[6], b[4]); COMBA_MAC(acc, c2, a[7], b[3]);
    COMBA_STEP(r[10], acc, c2);

    COMBA_MAC_INIT(acc, c2, a[4], b[7]); COMBA_MAC(acc, c2, a[5], b[6]); COMBA_MAC(acc, c2, a[6], b[5]); COMBA_MAC(acc, c2, a[7], b[4]);
    COMBA_STEP(r[11], acc, c2);

    COMBA_MAC_INIT(acc, c2, a[5], b[7]); COMBA_MAC(acc, c2, a[6], b[6]); COMBA_MAC(acc, c2, a[7], b[5]);
    COMBA_STEP(r[12], acc, c2);

    COMBA_MAC_INIT(acc, c2, a[6], b[7]); COMBA_MAC(acc, c2, a[7], b[6]);
    COMBA_STEP(r[13], acc, c2);

    COMBA_MAC0(acc, a[7], b[7]);
    r[14] = (uint32_t)acc;
    r[15] = (uint32_t)(acc >> 32);

    Reduce512_Wide32(out, r);
}

// Single-shift Comba squaring using IMAD.WIDE.U32 (-33% cycles, -13% insts on sm_120)
__device__ __forceinline__ void ModSqr_Wide32(uint32_t out[8], const uint32_t a[8]) {
    uint32_t r[16];
    uint64_t acc;
    uint32_t c2;

    asm volatile("mul.wide.u32 %0, %1, %2;" : "=l"(acc) : "r"(a[0]), "r"(a[1]));
    r[1] = (uint32_t)acc; acc >>= 32;

    COMBA_MAC0(acc, a[0], a[2]);
    r[2] = (uint32_t)acc; acc >>= 32;

    COMBA_MAC0(acc, a[0], a[3]); COMBA_MAC_INIT(acc, c2, a[1], a[2]);
    COMBA_STEP(r[3], acc, c2);

    COMBA_MAC0(acc, a[0], a[4]); COMBA_MAC_INIT(acc, c2, a[1], a[3]);
    COMBA_STEP(r[4], acc, c2);

    COMBA_MAC0(acc, a[0], a[5]); COMBA_MAC_INIT(acc, c2, a[1], a[4]); COMBA_MAC(acc, c2, a[2], a[3]);
    COMBA_STEP(r[5], acc, c2);

    COMBA_MAC_INIT(acc, c2, a[0], a[6]); COMBA_MAC(acc, c2, a[1], a[5]); COMBA_MAC(acc, c2, a[2], a[4]);
    COMBA_STEP(r[6], acc, c2);

    COMBA_MAC_INIT(acc, c2, a[0], a[7]); COMBA_MAC(acc, c2, a[1], a[6]); COMBA_MAC(acc, c2, a[2], a[5]); COMBA_MAC(acc, c2, a[3], a[4]);
    COMBA_STEP(r[7], acc, c2);

    COMBA_MAC_INIT(acc, c2, a[1], a[7]); COMBA_MAC(acc, c2, a[2], a[6]); COMBA_MAC(acc, c2, a[3], a[5]);
    COMBA_STEP(r[8], acc, c2);

    COMBA_MAC_INIT(acc, c2, a[2], a[7]); COMBA_MAC(acc, c2, a[3], a[6]); COMBA_MAC(acc, c2, a[4], a[5]);
    COMBA_STEP(r[9], acc, c2);

    COMBA_MAC_INIT(acc, c2, a[3], a[7]); COMBA_MAC(acc, c2, a[4], a[6]);
    COMBA_STEP(r[10], acc, c2);

    COMBA_MAC_INIT(acc, c2, a[4], a[7]); COMBA_MAC(acc, c2, a[5], a[6]);
    COMBA_STEP(r[11], acc, c2);

    COMBA_MAC_INIT(acc, c2, a[5], a[7]);
    COMBA_STEP(r[12], acc, c2);

    COMBA_MAC0(acc, a[6], a[7]);
    r[13] = (uint32_t)acc;
    r[14] = (uint32_t)(acc >> 32);
    r[15] = 0u;

    // Double off-diagonal terms in one 32-bit pass
    ADD32_CC(r[1], r[1], r[1]);
#pragma unroll
    for (int k = 2; k <= 14; k++) ADDC32_CC(r[k], r[k], r[k]);
    ADDC32(r[15], r[15], r[15]);

    // Add diagonal squares a[0..7]^2
    uint64_t d0 = (uint64_t)a[0] * a[0];
    uint64_t d1 = (uint64_t)a[1] * a[1];
    uint64_t d2 = (uint64_t)a[2] * a[2];
    uint64_t d3 = (uint64_t)a[3] * a[3];
    uint64_t d4 = (uint64_t)a[4] * a[4];
    uint64_t d5 = (uint64_t)a[5] * a[5];
    uint64_t d6 = (uint64_t)a[6] * a[6];
    uint64_t d7 = (uint64_t)a[7] * a[7];
    r[0] = (uint32_t)d0;
    ADD32_CC(r[1],  r[1],  (uint32_t)(d0 >> 32));
    ADDC32_CC(r[2], r[2],  (uint32_t)d1);
    ADDC32_CC(r[3], r[3],  (uint32_t)(d1 >> 32));
    ADDC32_CC(r[4], r[4],  (uint32_t)d2);
    ADDC32_CC(r[5], r[5],  (uint32_t)(d2 >> 32));
    ADDC32_CC(r[6], r[6],  (uint32_t)d3);
    ADDC32_CC(r[7], r[7],  (uint32_t)(d3 >> 32));
    ADDC32_CC(r[8], r[8],  (uint32_t)d4);
    ADDC32_CC(r[9], r[9],  (uint32_t)(d4 >> 32));
    ADDC32_CC(r[10], r[10], (uint32_t)d5);
    ADDC32_CC(r[11], r[11], (uint32_t)(d5 >> 32));
    ADDC32_CC(r[12], r[12], (uint32_t)d6);
    ADDC32_CC(r[13], r[13], (uint32_t)(d6 >> 32));
    ADDC32_CC(r[14], r[14], (uint32_t)d7);
    ADDC32(r[15],   r[15], (uint32_t)(d7 >> 32));

    Reduce512_Wide32(out, r);
}

// Warp-cooperative modular batch inversion via a 32-lane binary tree in shared memory.
// Amortizes ONE non-divergent _ModInv across all 32 lanes (4096 keys at ORION_SUBGRP=128)
// using only 10 ModMults, 8 KB SMEM/block (SoA, zero bank conflicts), and zero __syncthreads().
__device__ __forceinline__ void WarpModInv32(uint32_t val[8], uint32_t s_tree[8][64]) {
    const uint32_t lane = threadIdx.x & 31u;
    const uint32_t mask = 0xFFFFFFFFu;
    uint32_t a[8], b[8];

#pragma unroll
    for (int k = 0; k < 8; k++) s_tree[k][32u + lane] = val[k];
    __syncwarp(mask);

    // UP sweep: 5 tree levels (width = 16, 8, 4, 2, 1) — keep rolled to avoid register blowup!
#pragma unroll 1
    for (uint32_t w = 16u; w >= 1u; w >>= 1) {
        if (lane < w) {
            uint32_t c0 = (w << 1) + (lane << 1);
            uint32_t c1 = c0 + 1u;
#pragma unroll
            for (int k = 0; k < 8; k++) { a[k] = s_tree[k][c0]; b[k] = s_tree[k][c1]; }
            ModMult_Wide32(a, a, b);
#pragma unroll
            for (int k = 0; k < 8; k++) s_tree[k][w + lane] = a[k];
        }
        __syncwarp(mask);
    }

    // Root inversion on lane 0 only (eliminates worst-of-32 DivStep62 divergence!)
    if (lane == 0u) {
        uint64_t r64[5];
        r64[0] = (uint64_t)s_tree[0][1] | ((uint64_t)s_tree[1][1] << 32);
        r64[1] = (uint64_t)s_tree[2][1] | ((uint64_t)s_tree[3][1] << 32);
        r64[2] = (uint64_t)s_tree[4][1] | ((uint64_t)s_tree[5][1] << 32);
        r64[3] = (uint64_t)s_tree[6][1] | ((uint64_t)s_tree[7][1] << 32);
        r64[4] = 0ULL;
        _ModInv(r64);
        s_tree[0][1] = (uint32_t)r64[0]; s_tree[1][1] = (uint32_t)(r64[0] >> 32);
        s_tree[2][1] = (uint32_t)r64[1]; s_tree[3][1] = (uint32_t)(r64[1] >> 32);
        s_tree[4][1] = (uint32_t)r64[2]; s_tree[5][1] = (uint32_t)(r64[2] >> 32);
        s_tree[6][1] = (uint32_t)r64[3]; s_tree[7][1] = (uint32_t)(r64[3] >> 32);
    }
    __syncwarp(mask);

    // DOWN sweep: levels w = 2, 4, 8, 16 (keep rolled to avoid register blowup!)
#pragma unroll 1
    for (uint32_t w = 2u; w <= 16u; w <<= 1) {
        if (lane < w) {
            uint32_t parent = (w >> 1) + (lane >> 1);
            uint32_t sib    = (w + lane) ^ 1u;
#pragma unroll
            for (int k = 0; k < 8; k++) { a[k] = s_tree[k][parent]; b[k] = s_tree[k][sib]; }
        }
        __syncwarp(mask);
        if (lane < w) {
            ModMult_Wide32(a, a, b);
#pragma unroll
            for (int k = 0; k < 8; k++) s_tree[k][w + lane] = a[k];
        }
        __syncwarp(mask);
    }

    // Final DOWN level w = 32: all 32 lanes directly get their inverse into val[8]
    {
        uint32_t parent = 16u + (lane >> 1);
        uint32_t sib    = (32u + lane) ^ 1u;
#pragma unroll
        for (int k = 0; k < 8; k++) { a[k] = s_tree[k][parent]; b[k] = s_tree[k][sib]; }
        ModMult_Wide32(val, a, b);
    }
}












