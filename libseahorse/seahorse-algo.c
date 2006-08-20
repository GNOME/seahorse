/* 
 * A bunch of necessary algorithms that should (or will be) in Glib.
 * We keep them here (with function names prefixed) until such a 
 * time as we can depend on them either in GTK or OS
 */

#include "seahorse-algo.h"

/*
 * This code implements the MD5 message-digest algorithm.
 * The algorithm is due to Ron Rivest.  This code was
 * written by Colin Plumb in 1993, no copyright is claimed.
 * This code is in the public domain; do with it what you wish.
 *
 * Equivalent code is available from RSA Data Security, Inc.
 * This code has been tested against that, and is equivalent,
 * except that you don't need to include two pages of legalese
 * with every copy.
 *
 * To compute the message digest of a chunk of bytes, declare an
 * MD5Context structure, pass it to MD5Init, call MD5Update as
 * needed on buffers full of bytes, and then call MD5Final, which
 * will fill a supplied 16-byte array with the digest.
 *
 * Minor modifications by Nate Nielsen <nielsen@memberwebs.com>
 */

#include <string.h>

void seahorse_md5_transform(unsigned int buf[4], unsigned int const in[16]);

#ifdef BIG_ENDIAN
void
byteSwap(unsigned int *buf, unsigned words)
{
    unsigned char *p = (unsigned char *)buf;

    do {
        *buf++ = (unsigned int)((unsigned)p[3] << 8 | p[2]) << 16 |
            ((unsigned)p[1] << 8 | p[0]);
        p += 4;
    } while (--words);
}
#else
#define byteSwap(buf,words)
#endif

/*
 * Start MD5 accumulation.  Set bit count to 0 and buffer to mysterious
 * initialization constants.
 */
void
seahorse_md5_init(md5_ctx_t* ctx)
{
    ctx->buf[0] = 0x67452301;
    ctx->buf[1] = 0xefcdab89;
    ctx->buf[2] = 0x98badcfe;
    ctx->buf[3] = 0x10325476;

    ctx->bytes[0] = 0;
    ctx->bytes[1] = 0;
}

/*
 * Update context to reflect the concatenation of another buffer full
 * of bytes.
 */
void
seahorse_md5_update(md5_ctx_t* ctx, const void* b, unsigned len)
{
    unsigned int t;
    const unsigned char* buf = (const unsigned char*)b;

    /* Update byte count */

    t = ctx->bytes[0];
    if ((ctx->bytes[0] = t + len) < t)
        ctx->bytes[1]++;    /* Carry from low to high */

    t = 64 - (t & 0x3f);    /* Space available in ctx->in (at least 1) */
    if (t > len) {
        memcpy((unsigned char *)ctx->in + 64 - t, buf, len);
        return;
    }
    /* First chunk is an odd size */
    memcpy((unsigned char *)ctx->in + 64 - t, buf, t);
    byteSwap(ctx->in, 16);
    seahorse_md5_transform(ctx->buf, ctx->in);
    buf += t;
    len -= t;

    /* Process data in 64-byte chunks */
    while (len >= 64) {
        memcpy(ctx->in, buf, 64);
        byteSwap(ctx->in, 16);
        seahorse_md5_transform(ctx->buf, ctx->in);
        buf += 64;
        len -= 64;
    }

    /* Handle any remaining bytes of data. */
    memcpy(ctx->in, buf, len);
}

/*
 * Final wrapup - pad to 64-byte boundary with the bit pattern 
 * 1 0* (64-bit count of bits processed, MSB-first)
 */
void
seahorse_md5_final(unsigned char digest[16], md5_ctx_t* ctx)
{
    int count = ctx->bytes[0] & 0x3f;       /* Number of bytes in ctx->in */
    unsigned char *p = (unsigned char *)ctx->in + count;

    /* Set the first char of padding to 0x80.  There is always room. */
    *p++ = 0x80;

    /* Bytes of padding needed to make 56 bytes (-8..55) */
    count = 56 - 1 - count;

    if (count < 0) {    /* Padding forces an extra block */
        memset(p, 0, (unsigned int)count + 8);
        byteSwap(ctx->in, 16);
        seahorse_md5_transform(ctx->buf, ctx->in);
        p = (unsigned char *)ctx->in;
        count = 56;
    }
    memset(p, 0, (unsigned int)count);
    byteSwap(ctx->in, 14);

    /* Append length in bits and transform */
    ctx->in[14] = ctx->bytes[0] << 3;
    ctx->in[15] = ctx->bytes[1] << 3 | ctx->bytes[0] >> 29;
    seahorse_md5_transform(ctx->buf, ctx->in);

    byteSwap(ctx->buf, 4);
    memcpy(digest, ctx->buf, 16);
    memset(ctx, 0, sizeof(ctx));    /* In case it's sensitive */
}

/* The four core functions - F1 is optimized somewhat */

/* #define F1(x, y, z) (x & y | ~x & z) */
#define F1(x, y, z) (z ^ (x & (y ^ z)))
#define F2(x, y, z) F1(z, x, y)
#define F3(x, y, z) (x ^ y ^ z)
#define F4(x, y, z) (y ^ (x | ~z))

/* This is the central step in the MD5 algorithm. */
#define MD5STEP(f,w,x,y,z,in,s) \
     (w += f(x,y,z) + in, w = (w<<s | w>>(32-s)) + x)

/*
 * The core of the MD5 algorithm, this alters an existing MD5 hash to
 * reflect the addition of 16 longwords of new data.  MD5Update blocks
 * the data and converts bytes into longwords for this routine.
 */
void
seahorse_md5_transform(unsigned int buf[4], unsigned int const in[16])
{
    register unsigned int a, b, c, d;

    a = buf[0];
    b = buf[1];
    c = buf[2];
    d = buf[3];

    MD5STEP(F1, a, b, c, d, in[0] + 0xd76aa478, 7);
    MD5STEP(F1, d, a, b, c, in[1] + 0xe8c7b756, 12);
    MD5STEP(F1, c, d, a, b, in[2] + 0x242070db, 17);
    MD5STEP(F1, b, c, d, a, in[3] + 0xc1bdceee, 22);
    MD5STEP(F1, a, b, c, d, in[4] + 0xf57c0faf, 7);
    MD5STEP(F1, d, a, b, c, in[5] + 0x4787c62a, 12);
    MD5STEP(F1, c, d, a, b, in[6] + 0xa8304613, 17);
    MD5STEP(F1, b, c, d, a, in[7] + 0xfd469501, 22);
    MD5STEP(F1, a, b, c, d, in[8] + 0x698098d8, 7);
    MD5STEP(F1, d, a, b, c, in[9] + 0x8b44f7af, 12);
    MD5STEP(F1, c, d, a, b, in[10] + 0xffff5bb1, 17);
    MD5STEP(F1, b, c, d, a, in[11] + 0x895cd7be, 22);
    MD5STEP(F1, a, b, c, d, in[12] + 0x6b901122, 7);
    MD5STEP(F1, d, a, b, c, in[13] + 0xfd987193, 12);
    MD5STEP(F1, c, d, a, b, in[14] + 0xa679438e, 17);
    MD5STEP(F1, b, c, d, a, in[15] + 0x49b40821, 22);

    MD5STEP(F2, a, b, c, d, in[1] + 0xf61e2562, 5);
    MD5STEP(F2, d, a, b, c, in[6] + 0xc040b340, 9);
    MD5STEP(F2, c, d, a, b, in[11] + 0x265e5a51, 14);
    MD5STEP(F2, b, c, d, a, in[0] + 0xe9b6c7aa, 20);
    MD5STEP(F2, a, b, c, d, in[5] + 0xd62f105d, 5);
    MD5STEP(F2, d, a, b, c, in[10] + 0x02441453, 9);
    MD5STEP(F2, c, d, a, b, in[15] + 0xd8a1e681, 14);
    MD5STEP(F2, b, c, d, a, in[4] + 0xe7d3fbc8, 20);
    MD5STEP(F2, a, b, c, d, in[9] + 0x21e1cde6, 5);
    MD5STEP(F2, d, a, b, c, in[14] + 0xc33707d6, 9);
    MD5STEP(F2, c, d, a, b, in[3] + 0xf4d50d87, 14);
    MD5STEP(F2, b, c, d, a, in[8] + 0x455a14ed, 20);
    MD5STEP(F2, a, b, c, d, in[13] + 0xa9e3e905, 5);
    MD5STEP(F2, d, a, b, c, in[2] + 0xfcefa3f8, 9);
    MD5STEP(F2, c, d, a, b, in[7] + 0x676f02d9, 14);
    MD5STEP(F2, b, c, d, a, in[12] + 0x8d2a4c8a, 20);

    MD5STEP(F3, a, b, c, d, in[5] + 0xfffa3942, 4);
    MD5STEP(F3, d, a, b, c, in[8] + 0x8771f681, 11);
    MD5STEP(F3, c, d, a, b, in[11] + 0x6d9d6122, 16);
    MD5STEP(F3, b, c, d, a, in[14] + 0xfde5380c, 23);
    MD5STEP(F3, a, b, c, d, in[1] + 0xa4beea44, 4);
    MD5STEP(F3, d, a, b, c, in[4] + 0x4bdecfa9, 11);
    MD5STEP(F3, c, d, a, b, in[7] + 0xf6bb4b60, 16);
    MD5STEP(F3, b, c, d, a, in[10] + 0xbebfbc70, 23);
    MD5STEP(F3, a, b, c, d, in[13] + 0x289b7ec6, 4);
    MD5STEP(F3, d, a, b, c, in[0] + 0xeaa127fa, 11);
    MD5STEP(F3, c, d, a, b, in[3] + 0xd4ef3085, 16);
    MD5STEP(F3, b, c, d, a, in[6] + 0x04881d05, 23);
    MD5STEP(F3, a, b, c, d, in[9] + 0xd9d4d039, 4);
    MD5STEP(F3, d, a, b, c, in[12] + 0xe6db99e5, 11);
    MD5STEP(F3, c, d, a, b, in[15] + 0x1fa27cf8, 16);
    MD5STEP(F3, b, c, d, a, in[2] + 0xc4ac5665, 23);

    MD5STEP(F4, a, b, c, d, in[0] + 0xf4292244, 6);
    MD5STEP(F4, d, a, b, c, in[7] + 0x432aff97, 10);
    MD5STEP(F4, c, d, a, b, in[14] + 0xab9423a7, 15);
    MD5STEP(F4, b, c, d, a, in[5] + 0xfc93a039, 21);
    MD5STEP(F4, a, b, c, d, in[12] + 0x655b59c3, 6);
    MD5STEP(F4, d, a, b, c, in[3] + 0x8f0ccc92, 10);
    MD5STEP(F4, c, d, a, b, in[10] + 0xffeff47d, 15);
    MD5STEP(F4, b, c, d, a, in[1] + 0x85845dd1, 21);
    MD5STEP(F4, a, b, c, d, in[8] + 0x6fa87e4f, 6);
    MD5STEP(F4, d, a, b, c, in[15] + 0xfe2ce6e0, 10);
    MD5STEP(F4, c, d, a, b, in[6] + 0xa3014314, 15);
    MD5STEP(F4, b, c, d, a, in[13] + 0x4e0811a1, 21);
    MD5STEP(F4, a, b, c, d, in[4] + 0xf7537e82, 6);
    MD5STEP(F4, d, a, b, c, in[11] + 0xbd3af235, 10);
    MD5STEP(F4, c, d, a, b, in[2] + 0x2ad7d2bb, 15);
    MD5STEP(F4, b, c, d, a, in[9] + 0xeb86d391, 21);

    buf[0] += a;
    buf[1] += b;
    buf[2] += c;
    buf[3] += d;
}



/* gbase64.c - Base64 encoding/decoding
 *
 *  Copyright (C) 2006 Alexander Larsson <alexl@redhat.com>
 *  Copyright (C) 2000-2003 Ximian Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * This is based on code in camel, written by:
 *    Michael Zucchi <notzed@ximian.com>
 *    Jeffrey Stedfast <fejj@ximian.com>
 */

#include "config.h"

#include <string.h>
#include <glib.h>

static const char base64_alphabet[] =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/**
 * g_base64_encode_step:
 * @in: the binary data to encode.
 * @len: the length of @in.
 * @break_lines: whether to break long lines
 * @out: pointer to destination buffer
 * @state: Saved state between steps, initialize to 0
 * @save: Saved state between steps, initialize to 0
 *
 * Incrementally encode a sequence of binary data into it's Base-64 stringified
 * representation. By calling this functions multiple times you can convert data
 * in chunks to avoid having to have the full encoded data in memory.
 *
 * When all the data has been converted you must call g_base64_encode_close()
 * to flush the saved state.
 *
 * The output buffer must be large enough to fit all the data that will
 * be written to it. Due to the way base64 encodes you will need
 * at least: @len * 4 / 3 + 6 bytes. If you enable line-breaking you will
 * need at least: @len * 4 / 3 + @len * 4 / (3 * 72) + 7 bytes.
 *
 * @break_lines is typically used when putting base64-encoded data in emails.
 * It breaks the lines at 72 columns instead of putting all text on the same
 * line. This avoids problems with long lines in the email system.
 *
 * Return value: The number of bytes of output that was written
 *
 * Since: 2.12
 */
gsize
seahorse_base64_encode_step (const guchar *in, 
		      gsize         len, 
		      gboolean      break_lines, 
		      gchar        *out, 
		      gint         *state, 
		      gint         *save)
{
  char *outptr;
  const guchar *inptr;
  
  if (len <= 0)
    return 0;
  
  inptr = in;
  outptr = out;
  
  if (len + ((char *) save) [0] > 2)
    {
      const guchar *inend = in+len-2;
      int c1, c2, c3;
      int already;
      
      already = *state;
      
      switch (((char *) save) [0])
	{	
	case 1:	
	  c1 = ((unsigned char *) save) [1]; 
          goto skip1;
	case 2:	
          c1 = ((unsigned char *) save) [1];
	  c2 = ((unsigned char *) save) [2]; 
          goto skip2;
	}
      
      /* 
       * yes, we jump into the loop, no i'm not going to change it, 
       * it's beautiful! 
       */
      while (inptr < inend)
	{
	  c1 = *inptr++;
	skip1:
	  c2 = *inptr++;
	skip2:
	  c3 = *inptr++;
	  *outptr++ = base64_alphabet [ c1 >> 2 ];
	  *outptr++ = base64_alphabet [ c2 >> 4 | 
					((c1&0x3) << 4) ];
	  *outptr++ = base64_alphabet [ ((c2 &0x0f) << 2) | 
					(c3 >> 6) ];
	  *outptr++ = base64_alphabet [ c3 & 0x3f ];
	  /* this is a bit ugly ... */
	  if (break_lines && (++already) >= 19)
	    {
	      *outptr++ = '\n';
	      already = 0;
	    }
	}
      
      ((char *)save)[0] = 0;
      len = 2 - (inptr - inend);
      *state = already;
    }
  
  if (len>0)
    {
      char *saveout;
      
      /* points to the slot for the next char to save */
      saveout = & (((char *)save)[1]) + ((char *)save)[0];
      
      /* len can only be 0 1 or 2 */
      switch(len)
	{
	case 2:	*saveout++ = *inptr++;
	case 1:	*saveout++ = *inptr++;
	}
      ((char *)save)[0] += len;
    }
  
  return outptr - out;
}

/**
 * g_base64_encode_close:
 * @break_lines: whether to break long lines
 * @out: pointer to destination buffer
 * @state: Saved state from g_base64_encode_step()
 * @save: Saved state from g_base64_encode_step()
 *
 * Flush the status from a sequence of calls to g_base64_encode_step().
 *
 * Return value: The number of bytes of output that was written
 *
 * Since: 2.12
 */
gsize
seahorse_base64_encode_close (gboolean  break_lines,
		       gchar    *out, 
		       gint     *state, 
		       gint     *save)
{
  int c1, c2;
  char *outptr = out;

  c1 = ((unsigned char *) save) [1];
  c2 = ((unsigned char *) save) [2];
  
  switch (((char *) save) [0])
    {
    case 2:
      outptr [2] = base64_alphabet[ ( (c2 &0x0f) << 2 ) ];
      g_assert (outptr [2] != 0);
      goto skip;
    case 1:
      outptr[2] = '=';
    skip:
      outptr [0] = base64_alphabet [ c1 >> 2 ];
      outptr [1] = base64_alphabet [ c2 >> 4 | ( (c1&0x3) << 4 )];
      outptr [3] = '=';
      outptr += 4;
      break;
    }
  if (break_lines)
    *outptr++ = '\n';
  
  *save = 0;
  *state = 0;
  
  return outptr - out;
}

/**
 * g_base64_encode:
 * @data: the binary data to encode.
 * @len: the length of @data.
 *
 * Encode a sequence of binary data into it's Base-64 stringified
 * representation.
 *
 * Return value: a newly allocated, zero-terminated Base-64 encoded
 *               string representing @data.
 *
 * Since: 2.12
 */
gchar *
seahorse_base64_encode (const guchar *data, 
                 gsize         len)
{
  gchar *out;
  gint state = 0, outlen;
  gint save = 0;

  /* We can use a smaller limit here, since we know the saved state is 0 */
  out = g_malloc (len * 4 / 3 + 4);
  outlen = seahorse_base64_encode_step (data, len, FALSE, out, &state, &save);
  outlen += seahorse_base64_encode_close (FALSE,
				   out + outlen, 
				   &state, 
				   &save);
  out[outlen] = '\0';
  return (gchar *) out;
}

static const unsigned char mime_base64_rank[256] = {
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255, 62,255,255,255, 63,
   52, 53, 54, 55, 56, 57, 58, 59, 60, 61,255,255,255,  0,255,255,
  255,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
   15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25,255,255,255,255,255,
  255, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
   41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
};

/**
 * g_base64_decode_step: 
 * @in: binary input data
 * @len: max length of @in data to decode
 * @out: output buffer
 * @state: Saved state between steps, initialize to 0
 * @save: Saved state between steps, initialize to 0
 *
 * Incrementally decode a sequence of binary data from it's Base-64 stringified
 * representation. By calling this functions multiple times you can convert data
 * in chunks to avoid having to have the full encoded data in memory.
 *
 * The output buffer must be large enough to fit all the data that will
 * be written to it. Since base64 encodes 3 bytes in 4 chars you need
 * at least: @len * 3 / 4 bytes.
 * 
 * Return value: The number of bytes of output that was written
 *
 * Since: 2.12
 **/
gsize
seahorse_base64_decode_step (const gchar  *in, 
		      gsize         len, 
		      guchar       *out, 
		      gint         *state, 
		      guint        *save)
{
  const guchar *inptr;
  guchar *outptr;
  const guchar *inend;
  guchar c, rank;
  guchar last[2];
  unsigned int v;
  int i;
  
  inend = (const guchar *)in+len;
  outptr = out;
  
  /* convert 4 base64 bytes to 3 normal bytes */
  v=*save;
  i=*state;
  inptr = (const guchar *)in;
  last[0] = last[1] = 0;
  while (inptr < inend)
    {
      c = *inptr++;
      rank = mime_base64_rank [c];
      if (rank != 0xff)
	{
	  last[1] = last[0];
	  last[0] = c;
	  v = (v<<6) | rank;
	  i++;
	  if (i==4)
	    {
	      *outptr++ = v>>16;
	      if (last[1] != '=')
		*outptr++ = v>>8;
	      if (last[0] != '=')
		*outptr++ = v;
	      i=0;
	    }
	}
    }
  
  *save = v;
  *state = i;
  
  return outptr - out;
}

/**
 * g_base64_decode:
 * @text: zero-terminated string with base64 text to decode.
 * @out_len: The lenght of the decoded data is written here.
 *
 * Decode a sequence of Base-64 encoded text into binary data
 *
 * Return value: a newly allocated, buffer containing the binary data
 *               that @text represents
 *
 * Since: 2.12
 */
guchar *
seahorse_base64_decode (const gchar *text,
		 gsize       *out_len)
{
  guchar *ret;
  gint inlen, state = 0;
  guint save = 0;
  
  inlen = strlen (text);
  ret = g_malloc0 (inlen * 3 / 4);
  
  *out_len = seahorse_base64_decode_step (text, inlen, ret, &state, &save);
  
  return ret; 
}

