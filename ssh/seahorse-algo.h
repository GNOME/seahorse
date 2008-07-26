/* 
 * A bunch of necessary algorithms that should (or will be) in Glib.
 * We keep them here (with function names prefixed) until such a 
 * time as we can depend on them either in GTK or OS
 */
 
/*
 * This is the header file for the MD5 message-digest algorithm.
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
 * Changed so as no longer to depend on Colin Plumb's `usual.h'
 * header definitions
 *  - Ian Jackson <ijackson@nyx.cs.du.edu>.
 * Still in the public domain.
 * 
 * Some minor additions by Stef Walter <stef@memberwebs.com>
 */
 
#ifndef __SEAHORSE_ALGO_H__
#define __SEAHORSE_ALGO_H__

#define MD5_LEN 16
#define md5byte unsigned char

typedef struct md5_ctx  {
    unsigned int buf[4];
    unsigned int bytes[2];
    unsigned int in[16];
} md5_ctx_t;

void seahorse_md5_init(md5_ctx_t* context);
void seahorse_md5_update(md5_ctx_t* context, const void* buf, unsigned len);
void seahorse_md5_final(unsigned char digest[MD5_LEN], md5_ctx_t* context);

#endif /* __SEAHORSE_ALGO_H__ */


