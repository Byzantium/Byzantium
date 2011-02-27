/* sha.c
 *
 * Copyright (C) 2006-2009 Sawtooth Consulting Ltd.
 *
 * This file is part of CyaSSL.
 *
 * CyaSSL is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * CyaSSL is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */


#include "sha.h"
#ifdef NO_INLINE
    #include "misc.h"
#else
    #include "misc.c"
#endif
#include <string.h>
#include <assert.h>


#ifndef min

    static INLINE word32 min(word32 a, word32 b)
    {
        return a > b ? b : a;
    }

#endif /* min */


void InitSha(Sha* sha)
{
    sha->digest[0] = 0x67452301L;
    sha->digest[1] = 0xEFCDAB89L;
    sha->digest[2] = 0x98BADCFEL;
    sha->digest[3] = 0x10325476L;
    sha->digest[4] = 0xC3D2E1F0L;

    sha->buffLen = 0;
    sha->loLen   = 0;
    sha->hiLen   = 0;
}


#define f1(x,y,z) (z^(x &(y^z)))
#define f2(x,y,z) (x^y^z)
#define f3(x,y,z) ((x&y)|(z&(x|y)))
#define f4(x,y,z) (x^y^z)

/* (R0+R1), R2, R3, R4 are the different operations used in SHA1 */
#define R0(v,w,x,y,z,i) z+= f1(w,x,y) + W[i] + 0x5A827999+ \
                        rotlFixed(v,5); w = rotlFixed(w,30);
#define R1(v,w,x,y,z,i) z+= f1(w,x,y) + W[i] + 0x5A827999+ \
                        rotlFixed(v,5); w = rotlFixed(w,30);
#define R2(v,w,x,y,z,i) z+= f2(w,x,y) + W[i] + 0x6ED9EBA1+ \
                        rotlFixed(v,5); w = rotlFixed(w,30);
#define R3(v,w,x,y,z,i) z+= f3(w,x,y) + W[i] + 0x8F1BBCDC+ \
                        rotlFixed(v,5); w = rotlFixed(w,30);
#define R4(v,w,x,y,z,i) z+= f4(w,x,y) + W[i] + 0xCA62C1D6+ \
                        rotlFixed(v,5); w = rotlFixed(w,30);


static void Transform(Sha* sha)
{
    word32 W[80], i;

    /* Copy context->state[] to working vars */ 
    word32 a = sha->digest[0];
    word32 b = sha->digest[1];
    word32 c = sha->digest[2];
    word32 d = sha->digest[3];
    word32 e = sha->digest[4];

    for (i = 0; i < 16; i++)
        W[i] = sha->buffer[i];

    for (i = 16; i < 80; i++)
        W[i] = rotlFixed(W[i-3]^W[i-8]^W[i-14]^W[i-16],1);
    
    /* 4 rounds of 20 operations each.  */
    for (i = 0; i < 20; ) { 
        R0(a,b,c,d,e,i); i++;
        R0(e,a,b,c,d,i); i++;
        R0(d,e,a,b,c,i); i++;
        R0(c,d,e,a,b,i); i++;
        R0(b,c,d,e,a,i); i++;
    }

    for (i = 20; i < 40; ) {
        R2(a,b,c,d,e,i); i++;
        R2(e,a,b,c,d,i); i++;
        R2(d,e,a,b,c,i); i++;
        R2(c,d,e,a,b,i); i++;
        R2(b,c,d,e,a,i); i++;
    }

    for (i = 40; i < 60; ) {
        R3(a,b,c,d,e,i); i++;
        R3(e,a,b,c,d,i); i++;
        R3(d,e,a,b,c,i); i++;
        R3(c,d,e,a,b,i); i++;
        R3(b,c,d,e,a,i); i++;
    }

    for (i = 60; i < 80; ) {
        R4(a,b,c,d,e,i); i++;
        R4(e,a,b,c,d,i); i++;
        R4(d,e,a,b,c,i); i++;
        R4(c,d,e,a,b,i); i++;
        R4(b,c,d,e,a,i); i++;
    }

    /* Add the working vars back into digest state[] */
    sha->digest[0] += a;
    sha->digest[1] += b;
    sha->digest[2] += c;
    sha->digest[3] += d;
    sha->digest[4] += e;
}


static INLINE void AddLength(Sha* sha, word32 len)
{
    word32 tmp = sha->loLen;
    if ( (sha->loLen += len) < tmp)
        sha->hiLen++;                       /* carry low to high */
}


void ShaUpdate(Sha* sha, const byte* data, word32 len)
{
    /* do block size increments */
    byte* local = (byte*)sha->buffer;

    while (len) {
        word32 add = min(len, SHA_BLOCK_SIZE - sha->buffLen);
        memcpy(&local[sha->buffLen], data, add);

        sha->buffLen += add;
        data         += add;
        len          -= add;

        if (sha->buffLen == SHA_BLOCK_SIZE) {
            #ifdef LITTLE_ENDIAN_ORDER
                ByteReverseBytes(local, local, SHA_BLOCK_SIZE);
            #endif
            Transform(sha);
            AddLength(sha, SHA_BLOCK_SIZE);
            sha->buffLen = 0;
        }
    }
}


void ShaFinal(Sha* sha, byte* hash)
{
    byte* local = (byte*)sha->buffer;

    AddLength(sha, sha->buffLen);               /* before adding pads */

    local[sha->buffLen++] = 0x80;  /* add 1 */

    /* pad with zeros */
    if (sha->buffLen > SHA_PAD_SIZE) {
        memset(&local[sha->buffLen], 0, SHA_BLOCK_SIZE - sha->buffLen);
        sha->buffLen += SHA_BLOCK_SIZE - sha->buffLen;

        #ifdef LITTLE_ENDIAN_ORDER
            ByteReverseBytes(local, local, SHA_BLOCK_SIZE);
        #endif
        Transform(sha);
        sha->buffLen = 0;
    }
    memset(&local[sha->buffLen], 0, SHA_PAD_SIZE - sha->buffLen);
   
    /* put lengths in bits */
    sha->loLen = sha->loLen << 3;
    sha->hiLen = (sha->loLen >> (8*sizeof(sha->loLen) - 3)) + 
                 (sha->hiLen << 3);

    /* store lengths */
    #ifdef LITTLE_ENDIAN_ORDER
        ByteReverseBytes(local, local, SHA_BLOCK_SIZE);
    #endif
    memcpy(&local[SHA_PAD_SIZE], &sha->hiLen, sizeof(word32));
    memcpy(&local[SHA_PAD_SIZE + sizeof(word32)], &sha->loLen, sizeof(word32));

    Transform(sha);
    #ifdef LITTLE_ENDIAN_ORDER
        ByteReverseWords(sha->digest, sha->digest, SHA_DIGEST_SIZE);
    #endif
    memcpy(hash, sha->digest, SHA_DIGEST_SIZE);

    InitSha(sha);  /* reset state */
}

