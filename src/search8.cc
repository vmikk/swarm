/*
    SWARM

    Copyright (C) 2012-2019 Torbjorn Rognes and Frederic Mahe

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as
    published by the Free Software Foundation, either version 3 of the
    License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

    Contact: Torbjorn Rognes <torognes@ifi.uio.no>,
    Department of Informatics, University of Oslo,
    PO Box 1080 Blindern, NO-0316 Oslo, Norway
*/

#include "swarm.h"

#define CHANNELS 16
#define CDEPTH 4

/* uses 16 unsigned 8-bit values */

#ifdef __aarch64__

typedef int8x16_t VECTORTYPE;

const VECTORTYPE T0_init = { -1, 0, 0, 0, 0, 0, 0, 0,
                              0, 0, 0, 0, 0, 0, 0, 0 };

const uint8x16_t neon_mask =
  { 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80,
    0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80 };

const uint16x8_t neon_shift = { 0, 0, 0, 0, 8, 8, 8, 8 };

#define v_load(a) vld1q_s8((const int8_t *)(a))
#define v_load_64(a) vld1q_dup_u64((const unsigned long long *)(a))
#define v_store(a, b) vst1q_s8((int8_t *)(a), (b))
#define v_merge_lo_8(a, b) vzip1q_s8((a),(b))
#define v_merge_hi_8(a, b) vzip2q_s8((a),(b))
#define v_merge_lo_16(a, b) vzip1q_s16((a),(b))
#define v_merge_hi_16(a, b) vzip2q_s16((a),(b))
#define v_merge_lo_32(a, b) vreinterpretq_s16_s32(vzip1q_s32(vreinterpretq_s32_s16(a), vreinterpretq_s32_s16(b)))
#define v_merge_hi_32(a, b) vreinterpretq_s16_s32(vzip2q_s32(vreinterpretq_s32_s16(a), vreinterpretq_s32_s16(b)))
#define v_merge_lo_64(a, b) vreinterpretq_s16_s64(vcombine_s64(vget_low_s64(vreinterpretq_s64_s16(a)), vget_low_s64(vreinterpretq_s64_s16(b))))
#define v_merge_hi_64(a, b) vreinterpretq_s16_s64(vcombine_s64(vget_high_s64(vreinterpretq_s64_s16(a)), vget_high_s64(vreinterpretq_s64_s16(b))))
#define v_min(a, b) vminq_u8((a), (b))
#define v_add(a, b) vqaddq_u8((a), (b))
#define v_sub(a, b) vqsubq_u8((a), (b))
#define v_dup(a) vdupq_n_u8(a)
#define v_zero v_dup(0)
#define v_and(a, b) vandq_u8((a), (b))
#define v_xor(a, b) veorq_u8((a), (b))
#define v_shift_left(a) vextq_u8((v_zero), (a), 15)
#define v_mask_eq(a, b) vaddvq_u16(vshlq_u16(vpaddlq_u8(vandq_u8((vceqq_s8((a), (b))), neon_mask)), neon_shift))

#elif __x86_64__

typedef __m128i VECTORTYPE;

const VECTORTYPE T0_init = _mm_set_epi8(0, 0, 0, 0, 0, 0, 0, 0,
                                        0, 0, 0, 0, 0, 0, 0, -1);

#define v_load(a) _mm_load_si128((VECTORTYPE *)(a))
#define v_load_64(a) _mm_loadl_epi64((VECTORTYPE *)(a))
#define v_store(a, b) _mm_store_si128((VECTORTYPE *)(a), (b))
#define v_merge_lo_8(a, b) _mm_unpacklo_epi8((a),(b))
#define v_merge_hi_8(a, b) _mm_unpackhi_epi8((a),(b))
#define v_merge_lo_16(a, b) _mm_unpacklo_epi16((a),(b))
#define v_merge_hi_16(a, b) _mm_unpackhi_epi16((a),(b))
#define v_merge_lo_32(a, b) _mm_unpacklo_epi32((a),(b))
#define v_merge_hi_32(a, b) _mm_unpackhi_epi32((a),(b))
#define v_merge_lo_64(a, b) _mm_unpacklo_epi64((a),(b))
#define v_merge_hi_64(a, b) _mm_unpackhi_epi64((a),(b))
#define v_min(a, b) _mm_min_epu8((a), (b))
#define v_add(a, b) _mm_adds_epu8((a), (b))
#define v_sub(a, b) _mm_subs_epu8((a), (b))
#define v_dup(a) _mm_set1_epi8(a)
#define v_zero v_dup(0)
#define v_and(a, b) _mm_and_si128((a), (b))
#define v_xor(a, b) _mm_xor_si128((a), (b))
#define v_shift_left(a) _mm_slli_si128((a), 1)
#define v_mask_eq(a, b) _mm_movemask_epi8(_mm_cmpeq_epi8((a), (b)))

#elif __PPC__

typedef vector unsigned char VECTORTYPE;

const VECTORTYPE T0_init =
  { (unsigned char)(-1), 0, 0, 0, 0, 0, 0, 0,
                      0, 0, 0, 0, 0, 0, 0, 0 };

const vector unsigned char perm_merge_long_low =
  {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
   0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17};

const vector unsigned char perm_merge_long_high =
  {0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
   0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f};

const vector unsigned char perm_bits =
  { 0x78, 0x70, 0x68, 0x60, 0x58, 0x50, 0x48, 0x40,
    0x38, 0x30, 0x28, 0x20, 0x18, 0x10, 0x08, 0x00 };

#define v_load(a) *((VECTORTYPE *)(a))
#define v_load_64(a) (VECTORTYPE)vec_splats(*((uint64_t *)(a)))
#define v_store(a, b) vec_st((VECTORTYPE)(b), 0, (VECTORTYPE*)(a))
#define v_merge_lo_8(a, b) vec_mergeh((a), (b))
#define v_merge_hi_8(a, b) vec_mergel((a), (b))
#define v_merge_lo_16(a, b) (VECTORTYPE)vec_mergeh((vector short)(a),\
                                                   (vector short)(b))
#define v_merge_hi_16(a, b) (VECTORTYPE)vec_mergel((vector short)(a),\
                                                   (vector short)(b))
#define v_merge_lo_32(a, b) (VECTORTYPE)vec_mergeh((vector int)(a), \
                                                   (vector int)(b))
#define v_merge_hi_32(a, b) (VECTORTYPE)vec_mergel((vector int)(a), \
                                                   (vector int)(b))
#define v_merge_lo_64(a, b) (VECTORTYPE)vec_perm((vector long long)(a), \
                                                 (vector long long)(b), \
                                                 perm_merge_long_low)
#define v_merge_hi_64(a, b) (VECTORTYPE)vec_perm((vector long long)(a), \
                                                 (vector long long)(b), \
                                                 perm_merge_long_high)
#define v_min(a, b) vec_min((a), (b))
#define v_add(a, b) vec_adds((a), (b))
#define v_sub(a, b) vec_subs((a), (b))
#define v_dup(a) vec_splats((unsigned char)(a));
#define v_zero vec_splat_u8(0)
#define v_and(a, b) vec_and((a), (b))
#define v_xor(a, b) vec_xor((a), (b))
#define v_shift_left(a) vec_sld((a), v_zero, 1)
#define v_mask_eq(a, b) ((vector unsigned short) \
                         vec_vbpermq((vector unsigned char)             \
                                     vec_cmpeq((a), (b)), perm_bits))[4]

#else

#error Unknown Architecture

#endif

void dprofile_dump8(BYTE * dprofile)
{
  printf("\ndprofile:\n");
  for(int k=0; k<4; k++)
    {
      printf("k=%d 0 1 2 3 4 5 6 7 8 9 a b c d e f\n", k);
      for(int i=0; i<16; i++)
        {
          printf("%c: ", sym_nt[i]);
          for(int j = 0; j < 16; j++)
            printf("%2d", (char) dprofile[i*64+16*k + j]);
          printf("\n");
        }
    }
  printf("\n");
  exit(1);
}

inline void dprofile_fill8(BYTE * dprofile,
                           BYTE * score_matrix,
                           BYTE * dseq)
{
  VECTORTYPE reg0,  reg1, reg2,  reg3,  reg4,  reg5,  reg6,  reg7;
  VECTORTYPE reg8,  reg9, reg10, reg11, reg12, reg13, reg14, reg15;

  for(int j=0; j<CDEPTH; j++)
    {
      unsigned d[CHANNELS];
      for(int i=0; i<CHANNELS; i++)
        d[i] = dseq[j*CHANNELS+i] << 5;

      reg0  = v_load_64(score_matrix + d[ 0]);
      reg2  = v_load_64(score_matrix + d[ 2]);
      reg4  = v_load_64(score_matrix + d[ 4]);
      reg6  = v_load_64(score_matrix + d[ 6]);
      reg8  = v_load_64(score_matrix + d[ 8]);
      reg10 = v_load_64(score_matrix + d[10]);
      reg12 = v_load_64(score_matrix + d[12]);
      reg14 = v_load_64(score_matrix + d[14]);

      reg0  = v_merge_lo_8(reg0,  *(VECTORTYPE*)(score_matrix + d[ 1]));
      reg2  = v_merge_lo_8(reg2,  *(VECTORTYPE*)(score_matrix + d[ 3]));
      reg4  = v_merge_lo_8(reg4,  *(VECTORTYPE*)(score_matrix + d[ 5]));
      reg6  = v_merge_lo_8(reg6,  *(VECTORTYPE*)(score_matrix + d[ 7]));
      reg8  = v_merge_lo_8(reg8,  *(VECTORTYPE*)(score_matrix + d[ 9]));
      reg10 = v_merge_lo_8(reg10, *(VECTORTYPE*)(score_matrix + d[11]));
      reg12 = v_merge_lo_8(reg12, *(VECTORTYPE*)(score_matrix + d[13]));
      reg14 = v_merge_lo_8(reg14, *(VECTORTYPE*)(score_matrix + d[15]));

      reg1 = reg0;
      reg0 = v_merge_lo_16(reg0, reg2);
      reg1 = v_merge_hi_16(reg1, reg2);
      reg5 = reg4;
      reg4 = v_merge_lo_16(reg4, reg6);
      reg5 = v_merge_hi_16(reg5, reg6);
      reg9 = reg8;
      reg8 = v_merge_lo_16(reg8, reg10);
      reg9 = v_merge_hi_16(reg9, reg10);
      reg13 = reg12;
      reg12 = v_merge_lo_16(reg12, reg14);
      reg13 = v_merge_hi_16(reg13, reg14);

      reg2  = reg0;
      reg0  = v_merge_lo_32(reg0, reg4);
      reg2  = v_merge_hi_32(reg2, reg4);
      reg6  = reg1;
      reg1  = v_merge_lo_32(reg1, reg5);
      reg6  = v_merge_hi_32(reg6, reg5);
      reg10 = reg8;
      reg8  = v_merge_lo_32(reg8, reg12);
      reg10 = v_merge_hi_32(reg10, reg12);
      reg14 = reg9;
      reg9  = v_merge_lo_32(reg9, reg13);
      reg14 = v_merge_hi_32(reg14, reg13);

      reg3  = reg0;
      reg0  = v_merge_lo_64(reg0, reg8);
      reg3  = v_merge_hi_64(reg3, reg8);
      reg7  = reg2;
      reg2  = v_merge_lo_64(reg2, reg10);
      reg7  = v_merge_hi_64(reg7, reg10);
      reg11 = reg1;
      reg1  = v_merge_lo_64(reg1, reg9);
      reg11 = v_merge_hi_64(reg11, reg9);
      reg15 = reg6;
      reg6  = v_merge_lo_64(reg6, reg14);
      reg15 = v_merge_hi_64(reg15, reg14);

      v_store(dprofile+16*j+  0, reg0);
      v_store(dprofile+16*j+ 64, reg3);
      v_store(dprofile+16*j+128, reg2);
      v_store(dprofile+16*j+192, reg7);
      v_store(dprofile+16*j+256, reg1);
      v_store(dprofile+16*j+320, reg11);
      v_store(dprofile+16*j+384, reg6);
      v_store(dprofile+16*j+448, reg15);


      // loads not aligned on 16 byte boundary, cannot load and unpack in one instr.

      reg0  = v_load_64(score_matrix + 8 + d[0 ]);
      reg1  = v_load_64(score_matrix + 8 + d[1 ]);
      reg2  = v_load_64(score_matrix + 8 + d[2 ]);
      reg3  = v_load_64(score_matrix + 8 + d[3 ]);
      reg4  = v_load_64(score_matrix + 8 + d[4 ]);
      reg5  = v_load_64(score_matrix + 8 + d[5 ]);
      reg6  = v_load_64(score_matrix + 8 + d[6 ]);
      reg7  = v_load_64(score_matrix + 8 + d[7 ]);
      reg8  = v_load_64(score_matrix + 8 + d[8 ]);
      reg9  = v_load_64(score_matrix + 8 + d[9 ]);
      reg10 = v_load_64(score_matrix + 8 + d[10]);
      reg11 = v_load_64(score_matrix + 8 + d[11]);
      reg12 = v_load_64(score_matrix + 8 + d[12]);
      reg13 = v_load_64(score_matrix + 8 + d[13]);
      reg14 = v_load_64(score_matrix + 8 + d[14]);
      reg15 = v_load_64(score_matrix + 8 + d[15]);

      reg0  = v_merge_lo_8(reg0,  reg1);
      reg2  = v_merge_lo_8(reg2,  reg3);
      reg4  = v_merge_lo_8(reg4,  reg5);
      reg6  = v_merge_lo_8(reg6,  reg7);
      reg8  = v_merge_lo_8(reg8,  reg9);
      reg10 = v_merge_lo_8(reg10, reg11);
      reg12 = v_merge_lo_8(reg12, reg13);
      reg14 = v_merge_lo_8(reg14, reg15);

      reg1 = reg0;
      reg0 = v_merge_lo_16(reg0, reg2);
      reg1 = v_merge_hi_16(reg1, reg2);
      reg5 = reg4;
      reg4 = v_merge_lo_16(reg4, reg6);
      reg5 = v_merge_hi_16(reg5, reg6);
      reg9 = reg8;
      reg8 = v_merge_lo_16(reg8, reg10);
      reg9 = v_merge_hi_16(reg9, reg10);
      reg13 = reg12;
      reg12 = v_merge_lo_16(reg12, reg14);
      reg13 = v_merge_hi_16(reg13, reg14);

      reg2  = reg0;
      reg0  = v_merge_lo_32(reg0, reg4);
      reg2  = v_merge_hi_32(reg2, reg4);
      reg6  = reg1;
      reg1  = v_merge_lo_32(reg1, reg5);
      reg6  = v_merge_hi_32(reg6, reg5);
      reg10 = reg8;
      reg8  = v_merge_lo_32(reg8, reg12);
      reg10 = v_merge_hi_32(reg10, reg12);
      reg14 = reg9;
      reg9  = v_merge_lo_32(reg9, reg13);
      reg14 = v_merge_hi_32(reg14, reg13);

      reg3  = reg0;
      reg0  = v_merge_lo_64(reg0, reg8);
      reg3  = v_merge_hi_64(reg3, reg8);
      reg7  = reg2;
      reg2  = v_merge_lo_64(reg2, reg10);
      reg7  = v_merge_hi_64(reg7, reg10);
      reg11 = reg1;
      reg1  = v_merge_lo_64(reg1, reg9);
      reg11 = v_merge_hi_64(reg11, reg9);
      reg15 = reg6;
      reg6  = v_merge_lo_64(reg6, reg14);
      reg15 = v_merge_hi_64(reg15, reg14);

      v_store(dprofile+16*j+512+  0, reg0);
      v_store(dprofile+16*j+512+ 64, reg3);
      v_store(dprofile+16*j+512+128, reg2);
      v_store(dprofile+16*j+512+192, reg7);
      v_store(dprofile+16*j+512+256, reg1);
      v_store(dprofile+16*j+512+320, reg11);
      v_store(dprofile+16*j+512+384, reg6);
      v_store(dprofile+16*j+512+448, reg15);


      reg0  = v_load_64(score_matrix + 16 + d[0 ]);
      reg2  = v_load_64(score_matrix + 16 + d[2 ]);
      reg4  = v_load_64(score_matrix + 16 + d[4 ]);
      reg6  = v_load_64(score_matrix + 16 + d[6 ]);
      reg8  = v_load_64(score_matrix + 16 + d[8 ]);
      reg10 = v_load_64(score_matrix + 16 + d[10]);
      reg12 = v_load_64(score_matrix + 16 + d[12]);
      reg14 = v_load_64(score_matrix + 16 + d[14]);

      reg0  = v_merge_lo_8(reg0,  *(VECTORTYPE*)(score_matrix + 16 + d[ 1]));
      reg2  = v_merge_lo_8(reg2,  *(VECTORTYPE*)(score_matrix + 16 + d[ 3]));
      reg4  = v_merge_lo_8(reg4,  *(VECTORTYPE*)(score_matrix + 16 + d[ 5]));
      reg6  = v_merge_lo_8(reg6,  *(VECTORTYPE*)(score_matrix + 16 + d[ 7]));
      reg8  = v_merge_lo_8(reg8,  *(VECTORTYPE*)(score_matrix + 16 + d[ 9]));
      reg10 = v_merge_lo_8(reg10, *(VECTORTYPE*)(score_matrix + 16 + d[11]));
      reg12 = v_merge_lo_8(reg12, *(VECTORTYPE*)(score_matrix + 16 + d[13]));
      reg14 = v_merge_lo_8(reg14, *(VECTORTYPE*)(score_matrix + 16 + d[15]));

      reg1 = reg0;
      reg0 = v_merge_lo_16(reg0, reg2);
      reg1 = v_merge_hi_16(reg1, reg2);
      reg5 = reg4;
      reg4 = v_merge_lo_16(reg4, reg6);
      reg5 = v_merge_hi_16(reg5, reg6);
      reg9 = reg8;
      reg8 = v_merge_lo_16(reg8, reg10);
      reg9 = v_merge_hi_16(reg9, reg10);
      reg13 = reg12;
      reg12 = v_merge_lo_16(reg12, reg14);
      reg13 = v_merge_hi_16(reg13, reg14);

      reg2  = reg0;
      reg0  = v_merge_lo_32(reg0, reg4);
      reg2  = v_merge_hi_32(reg2, reg4);
      reg6  = reg1;
      reg1  = v_merge_lo_32(reg1, reg5);
      reg6  = v_merge_hi_32(reg6, reg5);
      reg10 = reg8;
      reg8  = v_merge_lo_32(reg8, reg12);
      reg10 = v_merge_hi_32(reg10, reg12);
      reg14 = reg9;
      reg9  = v_merge_lo_32(reg9, reg13);
      reg14 = v_merge_hi_32(reg14, reg13);

      reg3  = reg0;
      reg0  = v_merge_lo_64(reg0, reg8);
      reg3  = v_merge_hi_64(reg3, reg8);
      reg7  = reg2;
      reg2  = v_merge_lo_64(reg2, reg10);
      reg7  = v_merge_hi_64(reg7, reg10);
      reg11 = reg1;
      reg1  = v_merge_lo_64(reg1, reg9);
      reg11 = v_merge_hi_64(reg11, reg9);
      reg15 = reg6;
      reg6  = v_merge_lo_64(reg6, reg14);
      reg15 = v_merge_hi_64(reg15, reg14);

      v_store(dprofile+16*j+1024+  0, reg0);
      v_store(dprofile+16*j+1024+ 64, reg3);
      v_store(dprofile+16*j+1024+128, reg2);
      v_store(dprofile+16*j+1024+192, reg7);
      v_store(dprofile+16*j+1024+256, reg1);
      v_store(dprofile+16*j+1024+320, reg11);
      v_store(dprofile+16*j+1024+384, reg6);
      v_store(dprofile+16*j+1024+448, reg15);


      // loads not aligned on 16 byte boundary, cannot load and unpack in one instr.

      reg0  = v_load_64(score_matrix + 24 + d[ 0]);
      reg1  = v_load_64(score_matrix + 24 + d[ 1]);
      reg2  = v_load_64(score_matrix + 24 + d[ 2]);
      reg3  = v_load_64(score_matrix + 24 + d[ 3]);
      reg4  = v_load_64(score_matrix + 24 + d[ 4]);
      reg5  = v_load_64(score_matrix + 24 + d[ 5]);
      reg6  = v_load_64(score_matrix + 24 + d[ 6]);
      reg7  = v_load_64(score_matrix + 24 + d[ 7]);
      reg8  = v_load_64(score_matrix + 24 + d[ 8]);
      reg9  = v_load_64(score_matrix + 24 + d[ 9]);
      reg10 = v_load_64(score_matrix + 24 + d[10]);
      reg11 = v_load_64(score_matrix + 24 + d[11]);
      reg12 = v_load_64(score_matrix + 24 + d[12]);
      reg13 = v_load_64(score_matrix + 24 + d[13]);
      reg14 = v_load_64(score_matrix + 24 + d[14]);
      reg15 = v_load_64(score_matrix + 24 + d[15]);

      reg0  = v_merge_lo_8(reg0,  reg1);
      reg2  = v_merge_lo_8(reg2,  reg3);
      reg4  = v_merge_lo_8(reg4,  reg5);
      reg6  = v_merge_lo_8(reg6,  reg7);
      reg8  = v_merge_lo_8(reg8,  reg9);
      reg10 = v_merge_lo_8(reg10, reg11);
      reg12 = v_merge_lo_8(reg12, reg13);
      reg14 = v_merge_lo_8(reg14, reg15);

      reg1 = reg0;
      reg0 = v_merge_lo_16(reg0, reg2);
      reg1 = v_merge_hi_16(reg1, reg2);
      reg5 = reg4;
      reg4 = v_merge_lo_16(reg4, reg6);
      reg5 = v_merge_hi_16(reg5, reg6);
      reg9 = reg8;
      reg8 = v_merge_lo_16(reg8, reg10);
      reg9 = v_merge_hi_16(reg9, reg10);
      reg13 = reg12;
      reg12 = v_merge_lo_16(reg12, reg14);
      reg13 = v_merge_hi_16(reg13, reg14);

      reg2  = reg0;
      reg0  = v_merge_lo_32(reg0, reg4);
      reg2  = v_merge_hi_32(reg2, reg4);
      reg6  = reg1;
      reg1  = v_merge_lo_32(reg1, reg5);
      reg6  = v_merge_hi_32(reg6, reg5);
      reg10 = reg8;
      reg8  = v_merge_lo_32(reg8, reg12);
      reg10 = v_merge_hi_32(reg10, reg12);
      reg14 = reg9;
      reg9  = v_merge_lo_32(reg9, reg13);
      reg14 = v_merge_hi_32(reg14, reg13);

      reg3  = reg0;
      reg0  = v_merge_lo_64(reg0, reg8);
      reg3  = v_merge_hi_64(reg3, reg8);
      reg7  = reg2;
      reg2  = v_merge_lo_64(reg2, reg10);
      reg7  = v_merge_hi_64(reg7, reg10);
      reg11 = reg1;
      reg1  = v_merge_lo_64(reg1, reg9);
      reg11 = v_merge_hi_64(reg11, reg9);
      reg15 = reg6;
      reg6  = v_merge_lo_64(reg6, reg14);
      reg15 = v_merge_hi_64(reg15, reg14);

      v_store(dprofile+16*j+1536+  0, reg0);
      v_store(dprofile+16*j+1536+ 64, reg3);
      v_store(dprofile+16*j+1536+128, reg2);
      v_store(dprofile+16*j+1536+192, reg7);
      v_store(dprofile+16*j+1536+256, reg1);
      v_store(dprofile+16*j+1536+320, reg11);
      v_store(dprofile+16*j+1536+384, reg6);
      v_store(dprofile+16*j+1536+448, reg15);
    }
#if 0
  dprofile_dump8(dprofile);
#endif
}

inline void onestep_8(VECTORTYPE & H,
                      VECTORTYPE & N,
                      VECTORTYPE & F,
                      VECTORTYPE V,
                      unsigned short * DIR,
                      VECTORTYPE & E,
                      VECTORTYPE QR,
                      VECTORTYPE R)
{
  VECTORTYPE W;

  H = v_add(H, V);
  W = H;
  H = v_min(H, F);
  *((DIR) + 0) = v_mask_eq(W, H);
  H = v_min(H, E);
  *((DIR) + 1) = v_mask_eq(H, E);
  N = H;
  H = v_add(H, QR);
  F = v_add(F, R);
  E = v_add(E, R);
  F = v_min(H, F);
  *((DIR) + 2) = v_mask_eq(H, F);
  E = v_min(H, E);
  *((DIR) + 3) = v_mask_eq(H, E);
}

void align_cells_regular_8(VECTORTYPE * Sm,
                           VECTORTYPE * hep,
                           VECTORTYPE ** qp,
                           VECTORTYPE * Qm,
                           VECTORTYPE * Rm,
                           int64_t ql,
                           VECTORTYPE * F0,
                           uint64_t * dir_long,
                           VECTORTYPE * H0)
{
  VECTORTYPE Q, R, E;
  VECTORTYPE h0, h1, h2, h3, h4, h5, h6, h7, h8;
  VECTORTYPE f0, f1, f2, f3;
  VECTORTYPE * x;

  unsigned short * dir = (unsigned short *) dir_long;

  Q = *Qm;
  R = *Rm;

  f0 = *F0;
  f1 = v_add(f0, R);
  f2 = v_add(f1, R);
  f3 = v_add(f2, R);

  h0 = *H0;
  h1 = v_sub(f0, Q);
  h2 = v_add(h1, R);
  h3 = v_add(h2, R);
  h4 = v_zero;
  h5 = v_zero;
  h6 = v_zero;
  h7 = v_zero;
  h8 = v_zero;

  for(int64_t i = 0; i < ql; i++)
    {
      x = qp[i + 0];
      h4 = hep[2*i + 0];
      E  = hep[2*i + 1];
      onestep_8(h0, h5, f0, x[0], dir + 16*i +  0, E, Q, R);
      onestep_8(h1, h6, f1, x[1], dir + 16*i +  4, E, Q, R);
      onestep_8(h2, h7, f2, x[2], dir + 16*i +  8, E, Q, R);
      onestep_8(h3, h8, f3, x[3], dir + 16*i + 12, E, Q, R);
      hep[2*i + 0] = h8;
      hep[2*i + 1] = E;
      h0 = h4;
      h1 = h5;
      h2 = h6;
      h3 = h7;
    }

  Sm[0] = h5;
  Sm[1] = h6;
  Sm[2] = h7;
  Sm[3] = h8;
}

inline void align_cells_masked_8(VECTORTYPE * Sm,
                                 VECTORTYPE * hep,
                                 VECTORTYPE ** qp,
                                 VECTORTYPE * Qm,
                                 VECTORTYPE * Rm,
                                 int64_t ql,
                                 VECTORTYPE * F0,
                                 uint64_t * dir_long,
                                 VECTORTYPE * H0,
                                 VECTORTYPE * Mm,
                                 VECTORTYPE * MQ,
                                 VECTORTYPE * MR,
                                 VECTORTYPE * MQ0)
{
  VECTORTYPE Q, R, E;
  VECTORTYPE h0, h1, h2, h3, h4, h5, h6, h7, h8;
  VECTORTYPE f0, f1, f2, f3;
  VECTORTYPE * x;

  unsigned short * dir = (unsigned short *) dir_long;

  Q = *Qm;
  R = *Rm;

  f0 = *F0;
  f1 = v_add(f0, R);
  f2 = v_add(f1, R);
  f3 = v_add(f2, R);

  h0 = *H0;
  h1 = v_sub(f0, Q);
  h2 = v_add(h1, R);
  h3 = v_add(h2, R);
  h4 = v_zero;
  h5 = v_zero;
  h6 = v_zero;
  h7 = v_zero;
  h8 = v_zero;

  for(int64_t i = 0; i < ql; i++)
    {
      h4 = hep[2*i + 0];
      E  = hep[2*i + 1];
      x = qp[i + 0];

      /* mask h4 and E */
      h4 = v_sub(h4, *Mm);
      E  = v_sub(E,  *Mm);

      /* init h4 and E */
      h4 = v_add(h4, *MQ);
      E  = v_add(E,  *MQ);
      E  = v_add(E,  *MQ0);

      /* update MQ */
      *MQ = v_add(*MQ,  *MR);

      onestep_8(h0, h5, f0, x[0], dir + 16*i +  0, E, Q, R);
      onestep_8(h1, h6, f1, x[1], dir + 16*i +  4, E, Q, R);
      onestep_8(h2, h7, f2, x[2], dir + 16*i +  8, E, Q, R);
      onestep_8(h3, h8, f3, x[3], dir + 16*i + 12, E, Q, R);
      hep[2*i + 0] = h8;
      hep[2*i + 1] = E;

      h0 = h4;
      h1 = h5;
      h2 = h6;
      h3 = h7;
    }

  Sm[0] = h5;
  Sm[1] = h6;
  Sm[2] = h7;
  Sm[3] = h8;
}

inline uint64_t backtrack_8(char * qseq,
                            char * dseq,
                            uint64_t qlen,
                            uint64_t dlen,
                            uint64_t * dirbuffer,
                            uint64_t offset,
                            uint64_t dirbuffersize,
                            uint64_t channel,
                            uint64_t * alignmentlengthp)
{
  uint64_t maskup      = 1UL << (channel+ 0);
  uint64_t maskleft    = 1UL << (channel+16);
  uint64_t maskextup   = 1UL << (channel+32);
  uint64_t maskextleft = 1UL << (channel+48);

#if 0

  printf("Dumping backtracking array\n");

  for(uint64_t i=0; i<qlen; i++)
    {
      for(uint64_t j=0; j<dlen; j++)
        {
          uint64_t d = dirbuffer[(offset + longestdbsequence*4*(j/4)
                                  + 4*i + (j&3)) % dirbuffersize];
          if (d & maskleft)
            {
              printf("<");
            }
          else if (!(d & maskup))
            {
              printf("^");
            }
          else
            {
              printf("\\");
            }
        }
      printf("\n");
    }

  printf("Dumping gap extension array\n");

  for(uint64_t i=0; i<qlen; i++)
    {
      for(uint64_t j=0; j<dlen; j++)
        {
          uint64_t d = dirbuffer[(offset + longestdbsequence*4*(j/4)
                                  + 4*i + (j&3)) % dirbuffersize];
          if (!(d & maskextup))
            {
              if (!(d & maskextleft))
                printf("+");
              else
                printf("^");
            }
          else if (!(d & maskextleft))
            {
              printf("<");
            }
          else
            {
              printf("\\");
            }
        }
      printf("\n");
    }

#endif

  int64_t i = qlen - 1;
  int64_t j = dlen - 1;
  uint64_t aligned = 0;
  uint64_t matches = 0;
  char op = 0;

#undef SHOWALIGNMENT
#ifdef SHOWALIGNMENT
  printf("alignment, reversed: ");
#endif

  while ((i>=0) && (j>=0))
    {
      aligned++;

      uint64_t d = dirbuffer[(offset + longestdbsequence*4*(j/4)
                                   + 4*i + (j&3)) % dirbuffersize];

      if ((op == 'I') && (!(d & maskextleft)))
        {
          j--;
        }
      else if ((op == 'D') && (!(d & maskextup)))
        {
          i--;
        }
      else if (d & maskleft)
        {
          j--;
          op = 'I';
        }
      else if (!(d & maskup))
        {
          i--;
          op = 'D';
        }
      else
        {
          if (nt_extract(qseq, i) == nt_extract(dseq, j))
            matches++;
          i--;
          j--;
          op = 'M';
        }

#ifdef SHOWALIGNMENT
      printf("%c", op);
#endif

    }

  while (i>=0)
    {
      aligned++;
      i--;
#ifdef SHOWALIGNMENT
      printf("D");
#endif
    }

  while (j>=0)
    {
      aligned++;
      j--;
#ifdef SHOWALIGNMENT
      printf("I");
#endif
    }

#ifdef SHOWALIGNMENT
  printf("\n");
#endif

  * alignmentlengthp = aligned;
  return aligned - matches;
}

void search8(BYTE * * q_start,
             BYTE gap_open_penalty,
             BYTE gap_extend_penalty,
             BYTE * score_matrix,
             BYTE * dprofile,
             BYTE * hearray,
             uint64_t sequences,
             uint64_t * seqnos,
             uint64_t * scores,
             uint64_t * diffs,
             uint64_t * alignmentlengths,
             uint64_t qlen,
             uint64_t dirbuffersize,
             uint64_t * dirbuffer)
{
  VECTORTYPE Q, R, T, M, T0, MQ, MR, MQ0;
  VECTORTYPE *hep, **qp;

  uint64_t d_pos[CHANNELS];
  uint64_t d_offset[CHANNELS];
  char * d_address[CHANNELS];
  uint64_t d_length[CHANNELS];

  VECTORTYPE dseqalloc[CDEPTH];

  VECTORTYPE H0;
  VECTORTYPE F0;
  VECTORTYPE S[4];

  BYTE * dseq = (BYTE*) & dseqalloc;

  int64_t seq_id[CHANNELS];
  uint64_t next_id = 0;
  uint64_t done;

  T0 = T0_init;
  Q  = v_dup(gap_open_penalty+gap_extend_penalty);
  R  = v_dup(gap_extend_penalty);

  done = 0;

  hep = (VECTORTYPE*) hearray;
  qp = (VECTORTYPE**) q_start;

  for (int c=0; c<CHANNELS; c++)
    {
      d_address[c] = 0;
      d_pos[c] = 0;
      d_length[c] = 0;
      seq_id[c] = -1;
    }

  F0 = v_zero;
  H0 = v_zero;

  int easy = 0;

  uint64_t * dir = dirbuffer;

  while(1)
    {
      if (easy)
        {
          // fill all channels

          for(int c=0; c<CHANNELS; c++)
            {
              for(int j=0; j<CDEPTH; j++)
                {
                  if (d_pos[c] < d_length[c])
                    dseq[CHANNELS*j+c] = 1 + nt_extract(d_address[c], d_pos[c]++);
                  else
                    dseq[CHANNELS*j+c] = 0;
                }
              if (d_pos[c] == d_length[c])
                easy = 0;
            }

#ifdef __x86_64__
          if (ssse3_present)
            dprofile_shuffle8(dprofile, score_matrix, dseq);
          else
#endif
            dprofile_fill8(dprofile, score_matrix, dseq);

          align_cells_regular_8(S, hep, qp, &Q, &R, qlen, &F0, dir, &H0);
        }
      else
        {
          // One or more sequences ended in the previous block
          // We have to switch over to a new sequence

          easy = 1;

          M = v_zero;
          T = T0;
          for (int c=0; c<CHANNELS; c++)
            {
              if (d_pos[c] < d_length[c])
                {
                  // this channel has more sequence

                  for(int j=0; j<CDEPTH; j++)
                    {
                      if (d_pos[c] < d_length[c])
                        dseq[CHANNELS*j+c] = 1 + nt_extract(d_address[c], d_pos[c]++);
                      else
                        dseq[CHANNELS*j+c] = 0;
                    }
                  if (d_pos[c] == d_length[c])
                    easy = 0;
                }
              else
                {
                  // sequence in channel c ended
                  // change of sequence

                  M = v_xor(M, T);

                  int64_t cand_id = seq_id[c];

                  if (cand_id >= 0)
                    {
                      // save score

                      char * dbseq = (char*) d_address[c];
                      int64_t dbseqlen = d_length[c];
                      int64_t z = (dbseqlen+3) % 4;
                      int64_t score = ((BYTE*)S)[z*CHANNELS+c];
                      scores[cand_id] = score;

                      uint64_t diff;

                      if (score < 255)
                        {
                          int64_t offset = d_offset[c];
                          diff = backtrack_8(query.seq, dbseq, qlen, dbseqlen,
                                             dirbuffer,
                                             offset,
                                             dirbuffersize, c,
                                             alignmentlengths + cand_id);
                        }
                      else
                        {
                          diff = 255;
                        }

                      diffs[cand_id] = diff;

                      done++;
                    }

                  if (next_id < sequences)
                    {
                      // get next sequence
                      seq_id[c] = next_id;
                      int64_t seqno = seqnos[next_id];
                      char* address;
                      int64_t length;

                      db_getsequenceandlength(seqno, & address, & length);

                      d_address[c] = address;
                      d_length[c] = length;

                      d_pos[c] = 0;
                      d_offset[c] = dir - dirbuffer;
                      next_id++;

                      ((BYTE*)&H0)[c] = 0;
                      ((BYTE*)&F0)[c] = 2 * gap_open_penalty + 2 * gap_extend_penalty;

                      // fill channel
                      for(int j=0; j<CDEPTH; j++)
                        {
                          if (d_pos[c] < d_length[c])
                            dseq[CHANNELS*j+c] = 1 + nt_extract(d_address[c], d_pos[c]++);
                          else
                            dseq[CHANNELS*j+c] = 0;
                        }
                      if (d_pos[c] == d_length[c])
                        easy = 0;
                    }
                  else
                    {
                      // no more sequences, empty channel
                      seq_id[c] = -1;
                      d_address[c] = 0;
                      d_pos[c] = 0;
                      d_length[c] = 0;
                      for (int j=0; j<CDEPTH; j++)
                        dseq[CHANNELS*j+c] = 0;
                    }
                }

              T = v_shift_left(T);
            }

          if (done == sequences)
            break;

#ifdef __x86_64__
          if (ssse3_present)
            dprofile_shuffle8(dprofile, score_matrix, dseq);
          else
#endif
            dprofile_fill8(dprofile, score_matrix, dseq);

          MQ = v_and(M, Q);
          MR = v_and(M, R);
          MQ0 = MQ;

          align_cells_masked_8(S, hep, qp, &Q, &R, qlen, &F0, dir, &H0, &M, &MQ, &MR, &MQ0);
        }

      F0 = v_add(F0, R);
      F0 = v_add(F0, R);
      F0 = v_add(F0, R);
      H0 = v_sub(F0, Q);
      F0 = v_add(F0, R);

      dir += 4*longestdbsequence;
      if (dir >= dirbuffer + dirbuffersize)
        dir -= dirbuffersize;
    }
}
