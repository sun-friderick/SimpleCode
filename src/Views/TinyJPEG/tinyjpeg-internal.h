/*
 * Small jpeg decoder library (Internal header)
 *
 * Copyright (c) 2006, Luc Saillard <luc@saillard.org>
 * All rights reserved.
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * - Redistributions of source code must retain the above copyright notice,
 *  this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright notice,
 *  this list of conditions and the following disclaimer in the documentation
 *  and/or other materials provided with the distribution.
 *
 * - Neither the name of the author nor the names of its contributors may be
 *  used to endorse or promote products derived from this software without
 *  specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */
#ifndef __TINYJPEG_INTERNAL_H_
#define __TINYJPEG_INTERNAL_H_

struct jdec_private;

#define HUFFMAN_HASH_NBITS 9
#define HUFFMAN_HASH_SIZE  (1UL<<HUFFMAN_HASH_NBITS)
#define HUFFMAN_HASH_MASK  (HUFFMAN_HASH_SIZE-1)

#define HUFFMAN_TABLES	   4
#define COMPONENTS	   4

struct huffman_table
{
  /* Fast look up table, using HUFFMAN_HASH_NBITS bits we can have directly the symbol,
   * if the symbol is <0, then we need to look into the tree table */
  short int lookup[HUFFMAN_HASH_SIZE];
  /* code size: give the number of bits of a symbol is encoded */
  unsigned char code_size[HUFFMAN_HASH_SIZE];
  /* some place to store value that is not encoded in the lookup table
   * FIXME: Calculate if 256 value is enough to store all values
   */
  uint16_t slowtable[16-HUFFMAN_HASH_NBITS][256];
};
struct component
{
  unsigned int Hfactor;
  unsigned int Vfactor;
  float *Q_table;	/* Pointer to the quantisation table to use */
  struct huffman_table *AC_table;
  struct huffman_table *DC_table;
  short int previous_DC;	/* Previous DC coefficient */
  short int DCT[64];	/* DCT coef */
};
typedef void (*decode_MCU_fct) (struct jdec_private *priv);
typedef void (*convert_colorspace_fct) (struct jdec_private *priv);
struct jdec_private
{
  /* Public variables */
  uint8_t *components[COMPONENTS];
  unsigned int bytes_per_row[COMPONENTS];
  unsigned int width, height;	/* Size of the image */
  unsigned int flags;
  /* Private variables */
  const unsigned char *stream_begin;
  unsigned int stream_length;
  const unsigned char *stream;	/* Pointer to the current stream */
  unsigned int reservoir, nbits_in_reservoir;
  struct component component_infos[COMPONENTS];
  float Q_tables[COMPONENTS][64];	/* quantization tables */
  struct huffman_table HTDC[HUFFMAN_TABLES];	/* DC huffman tables   */
  struct huffman_table HTAC[HUFFMAN_TABLES];	/* AC huffman tables   */
  int default_huffman_table_initialized;
  /* Temp space used after the IDCT to store each components */
  uint8_t Y[64*4], Cr[64], Cb[64];
  /* Internal Pointer use for colorspace conversion, do not modify it !!! */
  uint8_t *plane[COMPONENTS];
};
struct tinyjpeg_colorspace {
  convert_colorspace_fct convert_colorspace[4];
  const decode_MCU_fct *decode_mcu_table;
  int (*initialize)(struct jdec_private *, unsigned int *, unsigned int *);
};
#define IDCT jpeg_idct_float
void jpeg_idct_float (struct component *compptr, uint8_t *output_buf, int stride);
void tinyjpeg_process_Huffman_data_unit(struct jdec_private *priv, int component);
extern const decode_MCU_fct tinyjpeg_decode_mcu_3comp_table[4];
extern const decode_MCU_fct tinyjpeg_decode_mcu_1comp_table[4];
enum std_markers {
   DQT  = 0xDB, /* Define Quantization Table */
   SOF  = 0xC0, /* Start of Frame (size information) */
   DHT  = 0xC4, /* Huffman Table */
   SOI  = 0xD8, /* Start of Image */
   SOS  = 0xDA, /* Start of Scan */
   EOI  = 0xD9, /* End of Image */
   APP0 = 0xE0,
};


#define cY	1
#define cCb	2
#define cCr	3
#define BLACK_Y 0
#define BLACK_U 127
#define BLACK_V 127
#define SANITY_CHECK 1

#if DEBUG
#define error(fmt, args...) do { 
   snprintf(error_string, sizeof(error_string), fmt, ## args); 
   return -1; 
} while(0)
#define trace(fmt, args...) do { 
   fprintf(stderr, fmt, ## args); 
   fflush(stderr); 
} while(0)
#else
#define error(fmt, args...) do { return -1; } while(0)
#define trace(fmt, args...) do { } while (0)
#endif

#if 0
static char *print_bits(unsigned int value, char *bitstr)
{
  int i, j;
  i=31;
  while (i>0)
   {
     if (value & (1UL<<i))
       break;
     i--;
   }
  j=0;
  while (i>=0)
   {
     bitstr[j++] = (value & (1UL<<i))?'1':'0';
     i--;
   }
  bitstr[j] = 0;
  return bitstr;
}
static void print_next_16bytes(int offset, const unsigned char *stream)
{
  trace("%4.4x: %2.2x %2.2x %2.2x %2.2x %2.2x %2.2x %2.2x %2.2x %2.2x %2.2x %2.2x %2.2x %2.2x %2.2x %2.2x %2.2xn",
offset,
stream[0], stream[1], stream[2], stream[3],
stream[4], stream[5], stream[6], stream[7],
stream[8], stream[9], stream[10], stream[11],
stream[12], stream[13], stream[14], stream[15]);
}
#endif

#endif  //__TINYJPEG_INTERNAL_H_

