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
#include <string.h>
#include <math.h>

Int _ONE((uint64_t)1);


// ------------------------------------------------

Int::Int() {
}

Int::Int(Int *a) {
  if(a) Set(a);
  else CLEAR();
}

Int::Int(int64_t i64) {

  if (i64 < 0) {
	  CLEARFF();
  } else {
	  CLEAR();
  }
  bits64[0] = i64;

}

Int::Int(uint64_t u64) {

  CLEAR();
  bits64[0] = u64;

}

// ------------------------------------------------

void Int::CLEAR() {

  memset(bits64,0, NB64BLOCK*8);

}

void Int::CLEARFF() {

  memset(bits64, 0xFF, NB64BLOCK * 8);

}

// ------------------------------------------------

void Int::Set(Int *a) {

  for (int i = 0; i<NB64BLOCK; i++)
  	bits64[i] = a->bits64[i];

}

// ------------------------------------------------

void Int::Add(Int *a) {

  unsigned char c = 0;
  c = _addcarry_u64(c, bits64[0], a->bits64[0], bits64 +0);
  c = _addcarry_u64(c, bits64[1], a->bits64[1], bits64 +1);
  c = _addcarry_u64(c, bits64[2], a->bits64[2], bits64 +2);
  c = _addcarry_u64(c, bits64[3], a->bits64[3], bits64 +3);
  c = _addcarry_u64(c, bits64[4], a->bits64[4], bits64 +4);
#if NB64BLOCK > 5
  c = _addcarry_u64(c, bits64[5], a->bits64[5], bits64 +5);
  c = _addcarry_u64(c, bits64[6], a->bits64[6], bits64 +6);
  c = _addcarry_u64(c, bits64[7], a->bits64[7], bits64 +7);
  c = _addcarry_u64(c, bits64[8], a->bits64[8], bits64 +8);
#endif

}

// ------------------------------------------------

void Int::Add(uint64_t a) {

	unsigned char c = 0;
	c = _addcarry_u64(c, bits64[0], a, bits64 + 0);
	c = _addcarry_u64(c, bits64[1], 0, bits64 + 1);
	c = _addcarry_u64(c, bits64[2], 0, bits64 + 2);
	c = _addcarry_u64(c, bits64[3], 0, bits64 + 3);
	c = _addcarry_u64(c, bits64[4], 0, bits64 + 4);
#if NB64BLOCK > 5
	c = _addcarry_u64(c, bits64[5], 0, bits64 + 5);
	c = _addcarry_u64(c, bits64[6], 0, bits64 + 6);
	c = _addcarry_u64(c, bits64[7], 0, bits64 + 7);
	c = _addcarry_u64(c, bits64[8], 0, bits64 + 8);
#endif
}

// ------------------------------------------------
void Int::AddOne() {

  unsigned char c = 0;
  c = _addcarry_u64(c, bits64[0],1, bits64 +0);
  c = _addcarry_u64(c, bits64[1],0, bits64 +1);
  c = _addcarry_u64(c, bits64[2],0, bits64 +2);
  c = _addcarry_u64(c, bits64[3],0, bits64 +3);
  c = _addcarry_u64(c, bits64[4],0, bits64 +4);
#if NB64BLOCK > 5
  c = _addcarry_u64(c, bits64[5],0, bits64 +5);
  c = _addcarry_u64(c, bits64[6],0, bits64 +6);
  c = _addcarry_u64(c, bits64[7],0, bits64 +7);
  c = _addcarry_u64(c, bits64[8],0, bits64 +8);
#endif

}

// ------------------------------------------------

void Int::Add(Int *a,Int *b) {

  unsigned char c = 0;
  c = _addcarry_u64(c, b->bits64[0], a->bits64[0], bits64 +0);
  c = _addcarry_u64(c, b->bits64[1], a->bits64[1], bits64 +1);
  c = _addcarry_u64(c, b->bits64[2], a->bits64[2], bits64 +2);
  c = _addcarry_u64(c, b->bits64[3], a->bits64[3], bits64 +3);
  c = _addcarry_u64(c, b->bits64[4], a->bits64[4], bits64 +4);
#if NB64BLOCK > 5
  c = _addcarry_u64(c, b->bits64[5], a->bits64[5], bits64 +5);
  c = _addcarry_u64(c, b->bits64[6], a->bits64[6], bits64 +6);
  c = _addcarry_u64(c, b->bits64[7], a->bits64[7], bits64 +7);
  c = _addcarry_u64(c, b->bits64[8], a->bits64[8], bits64 +8);
#endif

}

// ------------------------------------------------

uint64_t Int::AddCh(Int* a,uint64_t ca,Int* b,uint64_t cb) {

  uint64_t carry;
  unsigned char c = 0;
  c = _addcarry_u64(c,a->bits64[0],b->bits64[0],bits64 + 0);
  c = _addcarry_u64(c,a->bits64[1],b->bits64[1],bits64 + 1);
  c = _addcarry_u64(c,a->bits64[2],b->bits64[2],bits64 + 2);
  c = _addcarry_u64(c,a->bits64[3],b->bits64[3],bits64 + 3);
  c = _addcarry_u64(c,a->bits64[4],b->bits64[4],bits64 + 4);
#if NB64BLOCK > 5
  c = _addcarry_u64(c,a->bits64[5],b->bits64[5],bits64 + 5);
  c = _addcarry_u64(c,a->bits64[6],b->bits64[6],bits64 + 6);
  c = _addcarry_u64(c,a->bits64[7],b->bits64[7],bits64 + 7);
  c = _addcarry_u64(c,a->bits64[8],b->bits64[8],bits64 + 8);
#endif
  _addcarry_u64(c,ca,cb,&carry);
  return carry;

}

uint64_t Int::AddCh(Int* a,uint64_t ca) {

  uint64_t carry;
  unsigned char c = 0;
  c = _addcarry_u64(c,bits64[0],a->bits64[0],bits64 + 0);
  c = _addcarry_u64(c,bits64[1],a->bits64[1],bits64 + 1);
  c = _addcarry_u64(c,bits64[2],a->bits64[2],bits64 + 2);
  c = _addcarry_u64(c,bits64[3],a->bits64[3],bits64 + 3);
  c = _addcarry_u64(c,bits64[4],a->bits64[4],bits64 + 4);
#if NB64BLOCK > 5
  c = _addcarry_u64(c,bits64[5],a->bits64[5],bits64 + 5);
  c = _addcarry_u64(c,bits64[6],a->bits64[6],bits64 + 6);
  c = _addcarry_u64(c,bits64[7],a->bits64[7],bits64 + 7);
  c = _addcarry_u64(c,bits64[8],a->bits64[8],bits64 + 8);
#endif
  _addcarry_u64(c,ca,0,&carry);
  return carry;

}
// ------------------------------------------------

uint64_t Int::AddC(Int* a) {

  unsigned char c = 0;
  c = _addcarry_u64(c,bits64[0],a->bits64[0],bits64 + 0);
  c = _addcarry_u64(c,bits64[1],a->bits64[1],bits64 + 1);
  c = _addcarry_u64(c,bits64[2],a->bits64[2],bits64 + 2);
  c = _addcarry_u64(c,bits64[3],a->bits64[3],bits64 + 3);
  c = _addcarry_u64(c,bits64[4],a->bits64[4],bits64 + 4);
#if NB64BLOCK > 5
  c = _addcarry_u64(c,bits64[5],a->bits64[5],bits64 + 5);
  c = _addcarry_u64(c,bits64[6],a->bits64[6],bits64 + 6);
  c = _addcarry_u64(c,bits64[7],a->bits64[7],bits64 + 7);
  c = _addcarry_u64(c,bits64[8],a->bits64[8],bits64 + 8);
#endif

  return c;

}

// ------------------------------------------------

void Int::AddAndShift(Int* a,Int* b,uint64_t cH) {

  unsigned char c = 0;
  c = _addcarry_u64(c,b->bits64[0],a->bits64[0],bits64 + 0);
  c = _addcarry_u64(c,b->bits64[1],a->bits64[1],bits64 + 0);
  c = _addcarry_u64(c,b->bits64[2],a->bits64[2],bits64 + 1);
  c = _addcarry_u64(c,b->bits64[3],a->bits64[3],bits64 + 2);
  c = _addcarry_u64(c,b->bits64[4],a->bits64[4],bits64 + 3);
#if NB64BLOCK > 5
  c = _addcarry_u64(c,b->bits64[5],a->bits64[5],bits64 + 4);
  c = _addcarry_u64(c,b->bits64[6],a->bits64[6],bits64 + 5);
  c = _addcarry_u64(c,b->bits64[7],a->bits64[7],bits64 + 6);
  c = _addcarry_u64(c,b->bits64[8],a->bits64[8],bits64 + 7);
#endif

  bits64[NB64BLOCK - 1] = c + cH;

}

// ------------------------------------------------

void Int::MatrixVecMul(Int* u,Int* v,int64_t _11,int64_t _12,int64_t _21,int64_t _22,uint64_t* cu,uint64_t* cv) {

  Int t1,t2,t3,t4;
  uint64_t c1,c2,c3,c4;
  c1 = t1.IMult(u,_11);
  c2 = t2.IMult(v,_12);
  c3 = t3.IMult(u,_21);
  c4 = t4.IMult(v,_22);
  *cu = u->AddCh(&t1,c1,&t2,c2);
  *cv = v->AddCh(&t3,c3,&t4,c4);

}

void Int::MatrixVecMul(Int* u,Int* v,int64_t _11,int64_t _12,int64_t _21,int64_t _22) {

  Int t1,t2,t3,t4;
  t1.IMult(u,_11);
  t2.IMult(v,_12);
  t3.IMult(u,_21);
  t4.IMult(v,_22);
  u->Add(&t1,&t2);
  v->Add(&t3,&t4);

}

// ------------------------------------------------

bool Int::IsGreater(Int *a) {

  int i;

  for(i=NB64BLOCK-1;i>=0;) {
    if( a->bits64[i]!= bits64[i] )
		break;
    i--;
  }

  if(i>=0) {
    return bits64[i]>a->bits64[i];
  } else {
    return false;
  }

}

// ------------------------------------------------

bool Int::IsLower(Int *a) {

  int i;

  for (i = NB64BLOCK - 1; i >= 0;) {
    if (a->bits64[i] != bits64[i])
      break;
    i--;
  }

  if (i >= 0) {
    return bits64[i]<a->bits64[i];
  } else {
    return false;
  }

}

// ------------------------------------------------

bool Int::IsGreaterOrEqual(Int *a) {

  Int p;
  p.Sub(this,a);
  return p.IsPositive();

}

// ------------------------------------------------

bool Int::IsEqual(Int *a) {

return

#if NB64BLOCK > 5
  (bits64[8] == a->bits64[8]) &&
  (bits64[7] == a->bits64[7]) &&
  (bits64[6] == a->bits64[6]) &&
  (bits64[5] == a->bits64[5]) &&
#endif
  (bits64[4] == a->bits64[4]) &&
  (bits64[3] == a->bits64[3]) &&
  (bits64[2] == a->bits64[2]) &&
  (bits64[1] == a->bits64[1]) &&
  (bits64[0] == a->bits64[0]);

}

bool Int::IsOne() {
  return IsEqual(&_ONE);
}

bool Int::IsZero() {

#if NB64BLOCK > 5
  return (bits64[8] | bits64[7] | bits64[6] | bits64[5] | bits64[4] | bits64[3] | bits64[2] | bits64[1] | bits64[0]) == 0;
#else
  return (bits64[4] | bits64[3] | bits64[2] | bits64[1] | bits64[0]) == 0;
#endif

}


// ------------------------------------------------

void Int::SetInt32(uint32_t value) {

  CLEAR();
  bits[0]=value;

}

// ------------------------------------------------

unsigned char Int::GetByte(int n) {
  
  unsigned char *bbPtr = (unsigned char *)bits;
  return bbPtr[n];

}

void Int::Get32Bytes(unsigned char *buff) {

  uint64_t *ptr = (uint64_t *)buff;
  ptr[3] = _byteswap_uint64(bits64[0]);
  ptr[2] = _byteswap_uint64(bits64[1]);
  ptr[1] = _byteswap_uint64(bits64[2]);
  ptr[0] = _byteswap_uint64(bits64[3]);

}

// ------------------------------------------------

void Int::Sub(Int *a) {

  unsigned char c = 0;
  c = _subborrow_u64(c, bits64[0], a->bits64[0], bits64 +0);
  c = _subborrow_u64(c, bits64[1], a->bits64[1], bits64 +1);
  c = _subborrow_u64(c, bits64[2], a->bits64[2], bits64 +2);
  c = _subborrow_u64(c, bits64[3], a->bits64[3], bits64 +3);
  c = _subborrow_u64(c, bits64[4], a->bits64[4], bits64 +4);
#if NB64BLOCK > 5
  c = _subborrow_u64(c, bits64[5], a->bits64[5], bits64 +5);
  c = _subborrow_u64(c, bits64[6], a->bits64[6], bits64 +6);
  c = _subborrow_u64(c, bits64[7], a->bits64[7], bits64 +7);
  c = _subborrow_u64(c, bits64[8], a->bits64[8], bits64 +8);
#endif

}

// ------------------------------------------------

void Int::Sub(Int *a,Int *b) {

  unsigned char c = 0;
  c = _subborrow_u64(c, a->bits64[0], b->bits64[0], bits64 + 0);
  c = _subborrow_u64(c, a->bits64[1], b->bits64[1], bits64 + 1);
  c = _subborrow_u64(c, a->bits64[2], b->bits64[2], bits64 + 2);
  c = _subborrow_u64(c, a->bits64[3], b->bits64[3], bits64 + 3);
  c = _subborrow_u64(c, a->bits64[4], b->bits64[4], bits64 + 4);
#if NB64BLOCK > 5
  c = _subborrow_u64(c, a->bits64[5], b->bits64[5], bits64 + 5);
  c = _subborrow_u64(c, a->bits64[6], b->bits64[6], bits64 + 6);
  c = _subborrow_u64(c, a->bits64[7], b->bits64[7], bits64 + 7);
  c = _subborrow_u64(c, a->bits64[8], b->bits64[8], bits64 + 8);
#endif

}

void Int::Sub(uint64_t a) {

  unsigned char c = 0;
  c = _subborrow_u64(c, bits64[0], a, bits64 + 0);
  c = _subborrow_u64(c, bits64[1], 0, bits64 + 1);
  c = _subborrow_u64(c, bits64[2], 0, bits64 + 2);
  c = _subborrow_u64(c, bits64[3], 0, bits64 + 3);
  c = _subborrow_u64(c, bits64[4], 0, bits64 + 4);
#if NB64BLOCK > 5
  c = _subborrow_u64(c, bits64[5], 0, bits64 + 5);
  c = _subborrow_u64(c, bits64[6], 0, bits64 + 6);
  c = _subborrow_u64(c, bits64[7], 0, bits64 + 7);
  c = _subborrow_u64(c, bits64[8], 0, bits64 + 8);
#endif

}

void Int::SubOne() {

  unsigned char c = 0;
  c = _subborrow_u64(c, bits64[0], 1, bits64 + 0);
  c = _subborrow_u64(c, bits64[1], 0, bits64 + 1);
  c = _subborrow_u64(c, bits64[2], 0, bits64 + 2);
  c = _subborrow_u64(c, bits64[3], 0, bits64 + 3);
  c = _subborrow_u64(c, bits64[4], 0, bits64 + 4);
#if NB64BLOCK > 5
  c = _subborrow_u64(c, bits64[5], 0, bits64 + 5);
  c = _subborrow_u64(c, bits64[6], 0, bits64 + 6);
  c = _subborrow_u64(c, bits64[7], 0, bits64 + 7);
  c = _subborrow_u64(c, bits64[8], 0, bits64 + 8);
#endif

}

// ------------------------------------------------

bool Int::IsPositive() {
  return (int64_t)(bits64[NB64BLOCK - 1])>=0;
}

// ------------------------------------------------

bool Int::IsNegative() {
  return (int64_t)(bits64[NB64BLOCK - 1])<0;
}

// ------------------------------------------------

bool Int::IsEven() {
  return (bits[0] & 0x1) == 0;
}

// ------------------------------------------------

void Int::Neg() {

	unsigned char c=0;
	c = _subborrow_u64(c, 0, bits64[0], bits64 + 0);
	c = _subborrow_u64(c, 0, bits64[1], bits64 + 1);
	c = _subborrow_u64(c, 0, bits64[2], bits64 + 2);
	c = _subborrow_u64(c, 0, bits64[3], bits64 + 3);
	c = _subborrow_u64(c, 0, bits64[4], bits64 + 4);
#if NB64BLOCK > 5
	c = _subborrow_u64(c, 0, bits64[5], bits64 + 5);
	c = _subborrow_u64(c, 0, bits64[6], bits64 + 6);
	c = _subborrow_u64(c, 0, bits64[7], bits64 + 7);
	c = _subborrow_u64(c, 0, bits64[8], bits64 + 8);
#endif

}

// ------------------------------------------------

void Int::ShiftL64Bit() {

	for (int i = NB64BLOCK-1 ; i>0; i--) {
		bits64[i] = bits64[i - 1];
	}
	bits64[0] = 0;

}

// ------------------------------------------------

void Int::ShiftL64BitAndSub(Int *a,int n) {

  Int b;
  int i=NB64BLOCK-1;

  for(;i>=n;i--)
    b.bits64[i] = ~a->bits64[i-n];
  for(;i>=0;i--)
    b.bits64[i] = 0xFFFFFFFFFFFFFFFFULL;

  Add(&b);
  AddOne();

}

// ------------------------------------------------

void Int::ShiftL(uint32_t n) {

  if(n==0)
    return;
    
  if( n<64 ) {
	  shiftL((unsigned char)n, bits64);
  } else {
    uint32_t nb64 = n/64;
    uint32_t nb   = n%64;
    for(uint32_t i=0;i<nb64;i++) ShiftL64Bit();
	  shiftL((unsigned char)nb, bits64);
  }
  
}

// ------------------------------------------------

void Int::ShiftR64Bit() {

	for (int i = 0; i<NB64BLOCK - 1; i++) {
		bits64[i] = bits64[i + 1];
	}
	if (((int64_t)bits64[NB64BLOCK - 2])<0)
		bits64[NB64BLOCK - 1] = 0xFFFFFFFFFFFFFFFF;
	else
		bits64[NB64BLOCK - 1] = 0;

}

// ---------------------------------D---------------

void Int::ShiftR(uint32_t n) {

  if(n==0)
    return;
    
  if( n<64 ) {
    shiftR((unsigned char)n, bits64);
  } else {
    uint32_t nb64 = n/64;
    uint32_t nb   = n%64;
    for(uint32_t i=0;i<nb64;i++) ShiftR64Bit();
	  shiftR((unsigned char)nb, bits64);
  }
  
}

// ------------------------------------------------

void Int::Mult(Int *a) {

  Int b(this);
  Mult(a,&b);

}

// ------------------------------------------------

uint64_t Int::Mult(uint64_t a) {

  uint64_t carry;
  imm_mul(bits64, a, bits64, &carry);
  return carry;

}
// ------------------------------------------------

uint64_t Int::IMult(Int *a, int64_t b) {
  
  uint64_t carry;

  // Make b positive
  if (b < 0LL) {

    unsigned char c = 0;
    c = _subborrow_u64(c,0,a->bits64[0],bits64 + 0);
    c = _subborrow_u64(c,0,a->bits64[1],bits64 + 1);
    c = _subborrow_u64(c,0,a->bits64[2],bits64 + 2);
    c = _subborrow_u64(c,0,a->bits64[3],bits64 + 3);
    c = _subborrow_u64(c,0,a->bits64[4],bits64 + 4);
#if NB64BLOCK > 5
    c = _subborrow_u64(c,0,a->bits64[5],bits64 + 5);
    c = _subborrow_u64(c,0,a->bits64[6],bits64 + 6);
    c = _subborrow_u64(c,0,a->bits64[7],bits64 + 7);
    c = _subborrow_u64(c,0,a->bits64[8],bits64 + 8);
#endif

  	b = -b;

  } else {

    Set(a);

  }

  imm_imul(bits64, b, bits64, &carry);
  return carry;

}


// ------------------------------------------------

uint64_t Int::Mult(Int *a, uint64_t b) {

  uint64_t carry;
  imm_mul(a->bits64, b, bits64, &carry);
  return carry;

}

// ------------------------------------------------

void Int::Mult(Int *a,Int *b) {
  
  unsigned char c = 0;
  uint64_t h;
  uint64_t pr = 0;
  uint64_t carryh = 0;
  uint64_t carryl = 0;

  bits64[0] = _umul128(a->bits64[0], b->bits64[0], &pr);

  for (int i = 1; i < NB64BLOCK; i++) {
    for (int j = 0; j <= i; j++) {
      c = _addcarry_u64(c, _umul128(a->bits64[j], b->bits64[i - j], &h), pr, &pr);
      c = _addcarry_u64(c, carryl, h, &carryl);
      c = _addcarry_u64(c, carryh, 0, &carryh);
    }
    bits64[i] = pr;
    pr = carryl;
    carryl = carryh;
    carryh = 0;
  }

}

// ------------------------------------------------

double Int::ToDouble() {

  double base = 1.0;
  double sum = 0;
  double pw32 = pow(2.0,32.0);
  for(int i=0;i<NB32BLOCK;i++) {
    sum += (double)(bits[i]) * base;
    base *= pw32;
  }

  return sum;

}

// ------------------------------------------------

int Int::GetSize() {

  int i=NB32BLOCK-1;
  while(i>0 && bits[i]==0) i--;
  return i+1;

}

// ------------------------------------------------

int Int::GetSize64() {

  int i = NB64BLOCK - 1;
  while(i > 0 && bits64[i] == 0) i--;
  return i + 1;

}

// ------------------------------------------------

void Int::Mod(Int *n) {

  Int r;
  Div(n,&r);
  Set(&r);

}

// ------------------------------------------------

void Int::Div(Int *a,Int *mod) {

  if(a->IsGreater(this)) {
    if(mod) mod->Set(this);
    CLEAR();
    return;
  }

  if(a->IsZero()) {
    printf("Divide by 0!\n");
    return;
  }

  if(IsEqual(a)) {
    if(mod) mod->CLEAR();
    Set(&_ONE);
    return;
  }

  //Division algorithm D (Knuth section 4.3.1)

  Int rem(this);
  Int d(a);
  Int dq;
  CLEAR();

  // Size
  uint32_t dSize = d.GetSize64();
  uint32_t tSize = rem.GetSize64();
  uint32_t qSize = tSize - dSize + 1;

  // D1 normalize the divisor (d!=0)
  uint32_t shift = (uint32_t)LZC(d.bits64[dSize-1]);
  d.ShiftL(shift);
  rem.ShiftL(shift);

  uint64_t  _dh    = d.bits64[dSize-1];
  uint64_t  _dl    = (dSize>1)?d.bits64[dSize-2]:0;
  int sb = tSize-1;
        
  // D2 Initialize j
  for(int j=0; j<(int)qSize; j++) {

    // D3 Estimate qhat
    uint64_t qhat = 0;
    uint64_t qrem = 0;
    int skipCorrection = false;
    uint64_t nh = rem.bits64[sb-j+1];
    uint64_t nm = rem.bits64[sb-j];

    if (nh == _dh) {
      qhat = ~0;
      qrem = nh + nm;
      skipCorrection = qrem < nh;
    } else {
      qhat = _udiv128(nh,nm,_dh,&qrem);
    }

    if (qhat == 0)
      continue;

    if (!skipCorrection) { 

      // Correct qhat
      uint64_t nl = rem.bits64[sb-j-1];

      uint64_t estProH;
      uint64_t estProL = _umul128(_dl,qhat,&estProH);
      if( isStrictGreater128(estProH,estProL,qrem,nl) ) {
        qhat--;
        qrem += _dh;
        if (qrem >= _dh) {
          estProL = _umul128(_dl,qhat,&estProH);
          if(isStrictGreater128(estProH,estProL,qrem,nl))
            qhat--;
        }
      }

    }

    // D4 Multiply and subtract    
    dq.Mult(&d,qhat);
    rem.ShiftL64BitAndSub(&dq,qSize-j-1);
    if( rem.IsNegative() ) {
      // Overflow
      rem.Add(&d);
      qhat--;
    }

    bits64[qSize-j-1] = qhat;

 }

 if( mod ) {
   // Unnormalize remainder
   rem.ShiftR(shift);
   mod->Set(&rem);
 }

}

// ------------------------------------------------

void  Int::SetBase16(char *value) {  
  SetBaseN(16,"0123456789ABCDEF",value);
}

// ------------------------------------------------

std::string Int::GetBase16() {
  return GetBaseN(16,"0123456789ABCDEF");
}

// ------------------------------------------------

void  Int::SetBaseN(int n,char *charset,char *value) {

  CLEAR();

  Int pw((uint64_t)1);
  Int nb((uint64_t)n);
  Int c;

  int lgth = (int)strlen(value);
  for(int i=lgth-1;i>=0;i--) {
    char *p = strchr(charset,toupper(value[i]));
    if(!p) {
      printf("Invalid charset !!\n");
      return;
    }
    int id = (int)(p-charset);
    c.SetInt32(id);
    c.Mult(&pw);
    Add(&c);
    pw.Mult(&nb);

  }

}

// ------------------------------------------------

std::string Int::GetBaseN(int n,char *charset) {

  std::string ret;

  Int N(this);
  int isNegative = N.IsNegative();
  if (isNegative) N.Neg();

  // TODO: compute max digit
  unsigned char digits[1024];
  memset(digits, 0, sizeof(digits));

  int digitslen = 1;
  for (int i = 0; i < NB64BLOCK * 8; i++) {
    unsigned int carry = N.GetByte(NB64BLOCK*8 - i - 1);
    for (int j = 0; j < digitslen; j++) {
      carry += (unsigned int)(digits[j]) << 8;
      digits[j] = (unsigned char)(carry % n);
      carry /= n;
    }
    while (carry > 0) {
      digits[digitslen++] = (unsigned char)(carry % n);
      carry /= n;
    }
  }

  // reverse
  if (isNegative)
    ret.push_back('-');

  for (int i = 0; i < digitslen; i++)
    ret.push_back(charset[digits[digitslen - 1 - i]]);

  if (ret.length() == 0)
    ret.push_back('0');

  return ret;

}

// ------------------------------------------------
