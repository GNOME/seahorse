/* 
 * Copyright (C) 1998, 1999, 2003 Free Software Foundation, Inc.
 *
 * This file is part of GnuPG.
 *
 * GnuPG is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * GnuPG is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */

/* 
 * Originally from pinentry in gnupg.
 * Modifications and cleanup by Nate Nielsen 
 */

#include "config.h"
#include <glib.h>
#include <stdint.h>
#include <errno.h>
#include <unistd.h>
#include <string.h> 

#if defined(HAVE_MLOCK) || defined(HAVE_MMAP)
#include <sys/mman.h>
#include <sys/types.h>
#include <fcntl.h>
#ifdef USE_CAPABILITIES
#include <sys/capability.h>
#endif
#endif

#include "seahorse-secure-memory.h"

/* 
 * To avoid that a compiler optimizes certain memset calls away, these
 * macros may be used instead. 
 */
#define WIPE_MEMORY2(_ptr, _set ,_len) \
    do { \
        volatile char *_vptr=(volatile char *)(_ptr); \
        size_t _vlen = (_len); \
        while (_vlen) { *_vptr=(_set); _vptr++; _vlen--; } \
    } while (0)
    
typedef union {
    int a;
    short b;
    char c[1];
    long d;
    uint64_t e;
    float f;
    double g;
} PROPERLY_ALIGNED_TYPE;


#if defined(MAP_ANON) && !defined(MAP_ANONYMOUS)
#define MAP_ANONYMOUS MAP_ANON
#endif

#define DEFAULT_POOLSIZE 16384

typedef struct memblock_struct MEMBLOCK;
    
struct memblock_struct {
    unsigned size;
    union {
    MEMBLOCK *next;
    PROPERLY_ALIGNED_TYPE aligned;
    } u;
};

static void  *pool;
static volatile int pool_okay; 
#ifdef HAVE_MMAP
static int pool_is_mmapped;
#endif
static size_t poolsize; /* allocated length */
static size_t poollen; /* used length */
static MEMBLOCK *unused_blocks;
static unsigned max_alloced;
static unsigned cur_alloced;
static unsigned max_blocks;
static unsigned cur_blocks;
static int disable_secmem;
static int have_secure = 0;

static void
lock_pool(void *p, size_t n)
{
#if defined(USE_CAPABILITIES) && defined(HAVE_MLOCK)
    int err;

    cap_set_proc (cap_from_text ("cap_ipc_lock+ep"));
    err = mlock (p, n);
    if(err && errno)
        err = errno;
    cap_set_proc (cap_from_text ("cap_ipc_lock+p"));

    if (err) {
        if (errno != EPERM && errno != EAGAIN)
            g_warning ("can't lock memory: %s", g_strerror (err));
    }

#elif defined(HAVE_MLOCK)
    uid_t uid;
    int err;

    uid = getuid ();

#ifdef HAVE_BROKEN_MLOCK
    if( uid ) {
        errno = EPERM;
        err = errno;
    } else {
        err = mlock (p, n);
        if(err && errno)
            err = errno;
    }
#else /* HAVE_BROKEN_MLOCK */
    err = mlock (p, n);
    if( err && errno )
        err = errno;
#endif /* HAVE_BROKEN_MLOCK */

    if (uid && !geteuid()) {
        if (setuid (uid) || getuid () != geteuid ())
            g_assert_not_reached ();
    }

    if( err ) {
        if(errno != EPERM && errno != EAGAIN)
            g_warning ("can't lock memory: %s", g_strerror (err));
    } else {
        have_secure = 1;
    }
    
#else /* defined(HAVE_MLOCK) */
    
    g_warning ("Please note that you don't have secure memory on this system");
#endif
}


static void
init_pool( size_t n)
{
    size_t pgsize;

    g_assert (!disable_secmem);

    poolsize = n;

#ifdef HAVE_GETPAGESIZE
    pgsize = getpagesize ();
#else
    pgsize = 4096;
#endif

#if HAVE_MMAP
    poolsize = (poolsize + pgsize -1) & ~(pgsize - 1);
    
#ifdef MAP_ANONYMOUS
    pool = mmap (0, poolsize, PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    
/* map /dev/zero instead */
# else /* MAP_ANONYMOUS */
    {
        int fd = open("/dev/zero", O_RDWR);
        if (fd == -1) {
            g_warning ("can't open /dev/zero: %s", g_strerror(errno));
            pool = (void*)-1;
        } else {
            pool = mmap (0, poolsize, PROT_READ | PROT_WRITE,
                         MAP_PRIVATE, fd, 0);
        }
    }
# endif /* MAP_ANONYMOUS */
    
    if (pool == (void*)-1)
        g_message ("can't mmap pool of %u bytes: %s - using malloc\n",
                   (unsigned)poolsize, g_strerror (errno));
    else {
        pool_is_mmapped = 1;
        pool_okay = 1;
    }

#endif /* HAVE_MMAP */
    
    if( !pool_okay ) {
        pool = g_malloc (poolsize);
        g_assert (pool);
        pool_okay = 1;
    }
    
    lock_pool (pool, poolsize);
    poollen = 0;
}

/* concatenate unused blocks */
static void
compress_pool(void)
{
    /* TODO: we really should do this */
}

int 
seahorse_secure_memory_have ()
{
    return have_secure;
}

void
seahorse_secure_memory_init (size_t n)
{
    if(!n) {
#ifdef USE_CAPABILITIES
        /* drop all capabilities */
        cap_set_proc( cap_from_text("all-eip") );

#elif !defined(HAVE_DOSISH_SYSTEM)
        uid_t uid;

        disable_secmem = 1;
        uid = getuid();
        
        if (uid && !geteuid()) {
            if (setuid (uid) || getuid () != geteuid ())
                g_assert_not_reached ();
        }
#endif
    } else {
        if (n < DEFAULT_POOLSIZE)
            n = DEFAULT_POOLSIZE;
        g_assert (!pool_okay);
        init_pool (n);
    }
}

void*
seahorse_secure_memory_malloc (size_t size)
{
    MEMBLOCK *mb, *mb2;
    int compressed = 0;

    g_assert (pool_okay);

    /* blocks are always a multiple of 32 */
    size += sizeof (MEMBLOCK);
    size = ((size + 31) / 32) * 32;

retry:
    /* try to get it from the used blocks */
    for (mb = unused_blocks, mb2 = NULL; mb; mb2 = mb, mb = mb->u.next) {
        if (mb->size >= size) {
            if (mb2)
                mb2->u.next = mb->u.next;
            else
                unused_blocks = mb->u.next;
            goto leave;
        }
    }
        
    /* allocate a new block */
    if ((poollen + size <= poolsize)) {
        mb = (void*)((char*)pool + poollen);
        poollen += size;
        mb->size = size;
    } else if(!compressed) {
        compressed = 1;
        compress_pool();
        goto retry;
    } else
        return NULL;

leave:
    cur_alloced += mb->size;
    cur_blocks++;
    if (cur_alloced > max_alloced)
        max_alloced = cur_alloced;
    if (cur_blocks > max_blocks)
        max_blocks = cur_blocks;

    return &mb->u.aligned.c;
}

void*
seahorse_secure_memory_realloc (void *p, size_t newsize)
{
    MEMBLOCK *mb;
    size_t size;
    void *a;

    mb = (MEMBLOCK*)((char*)p - ((size_t) &((MEMBLOCK*)0)->u.aligned.c));
    size = mb->size;
    if( newsize < size )
        return p; /* it is easier not to shrink the memory */
    
    a = seahorse_secure_memory_malloc (newsize);
    memcpy (a, p, size);
    memset ((char*)a+size, 0, newsize-size);
    seahorse_secure_memory_free (p);
    return a;
}

void
seahorse_secure_memory_free (void *a)
{
    MEMBLOCK *mb;
    size_t size;

    if(!a)
        return;

    mb = (MEMBLOCK*)((char*)a - ((size_t) &((MEMBLOCK*)0)->u.aligned.c));
    size = mb->size;
    
    /* This does not make much sense: probably this memory is held in the
     * cache. We do it anyway: */
    WIPE_MEMORY2 (mb, 0xff, size );
    WIPE_MEMORY2 (mb, 0xaa, size );
    WIPE_MEMORY2 (mb, 0x55, size );
    WIPE_MEMORY2 (mb, 0x00, size );
    mb->size = size;
    mb->u.next = unused_blocks;
    unused_blocks = mb;
    cur_blocks--;
    cur_alloced -= size;
}

gboolean
seahorse_secure_memory_check (const void *p)
{
    return p >= pool && p < (void*)((char*)pool + poolsize);
}

void
seahorse_secure_memory_term ()
{
    if(!pool_okay)
        return;

    WIPE_MEMORY2 (pool, 0xff, poolsize);
    WIPE_MEMORY2 (pool, 0xaa, poolsize);
    WIPE_MEMORY2 (pool, 0x55, poolsize);
    WIPE_MEMORY2 (pool, 0x00, poolsize);
    
#if HAVE_MMAP
    if (pool_is_mmapped)
        munmap (pool, poolsize);
#endif
    pool = NULL;
    pool_okay = 0;
    poolsize = 0;
    poollen = 0;
    unused_blocks = NULL;
}

void
seahorse_secure_memory_dump ()
{
    if (disable_secmem)
        return;
    
    g_printerr ("secmem usage: %u/%u bytes in %u/%u blocks of pool %lu/%lu\n",
                cur_alloced, max_alloced, cur_blocks, max_blocks, 
                (unsigned long)poollen, (unsigned long)poolsize);
}
