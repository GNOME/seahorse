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
 * Modifications cleanup and integration with glib by Nate Nielsen 
 */

#include "config.h"
#include <glib.h>
#include <stdint.h>
#include <errno.h>
#include <unistd.h>
#include <string.h> 
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <assert.h>

#if defined(HAVE_MLOCK) || defined(HAVE_MMAP)
#include <sys/mman.h>
#include <sys/types.h>
#include <fcntl.h>
#ifdef USE_CAPABILITIES
#include <sys/capability.h>
#endif
#endif

#include "seahorse-secure-memory.h"

/* extern declared in seahorse-secure-memory.h */
gboolean seahorse_use_secure_mem = FALSE;
static int have_secure = 0;

#ifndef DEBUG_SECMEM_ENABLE
#if _DEBUG
#define DEBUG_SECMEM_ENABLE 1
#else
#define DEBUG_SECMEM_ENABLE 0
#endif
#endif

#if DEBUG_SECMEM_ENABLE
#define DEBUG_SECMEM(x) printerr x
#else
#define DEBUG_SECMEM(x) 
#endif

/* 
 * Use our own message printing function so that 
 * any glib allocations during their message printing
 * won't cause a loop.
 */
static void 
printerr (const gchar* msg, ...)
{
    va_list va;
    va_start (va, msg);
    vfprintf (stderr, msg, va);
    va_end (va);
}

static gpointer
switch_malloc (gsize size)
{
    gpointer p;

    if (size == 0)
        return NULL;
    if (seahorse_use_secure_mem && have_secure)
        p = (gpointer) seahorse_secure_memory_malloc (size);
    else
        p = (gpointer) malloc (size);
    return p;
}

static gpointer
switch_calloc (gsize num, gsize size)
{
    gpointer p;

    if (size == 0 || num == 0)
        return NULL;
    if (seahorse_use_secure_mem && have_secure) {
        p = (gpointer) seahorse_secure_memory_malloc (size * num);
        if (p)
            memset (p, 0, size * num);
    } else
        p = (gpointer) calloc (num, size);
    return p;
}

static gpointer
switch_realloc (gpointer mem, gsize size)
{
    gpointer p;

    if (size == 0) {
        free (mem);
        return NULL;
    }

    if (!mem) {

        if (seahorse_use_secure_mem && have_secure)
            p = (gpointer) seahorse_secure_memory_malloc (size);
        else
            p = (gpointer) malloc (size);

    } else if (seahorse_secure_memory_check (mem))
        p = (gpointer) seahorse_secure_memory_realloc (mem, size);

    else
        p = (gpointer) realloc(mem, size);
    return p;
}

static void
switch_free (gpointer mem)
{
    if (mem) {
        if (seahorse_secure_memory_check (mem))
            seahorse_secure_memory_free (mem);
        else
            free (mem);
    }
}

/* -------------------------------------------------------------------------- */

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
#define MEMBLOCK_SIG 0xAABBCCDD

typedef struct memblock_struct MEMBLOCK;
    
struct memblock_struct {
    unsigned sig;
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
            printerr ("can't lock memory: %s", strerror (err));
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

    if( err ) {
        if(errno != EPERM && errno != EAGAIN)
            printerr ("can't lock memory: %s", strerror (err));
    } else {
        have_secure = 1;
    }
    
#else /* defined(HAVE_MLOCK) */
    
    printerr ("Please note that you don't have secure memory on this system");
#endif
}


static void
init_pool( size_t n)
{
    size_t pgsize;

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
            printerr ("can't open /dev/zero: %s", strerror(errno));
            pool = (void*)-1;
        } else {
            pool = mmap (0, poolsize, PROT_READ | PROT_WRITE,
                         MAP_PRIVATE, fd, 0);
        }
    }
# endif /* MAP_ANONYMOUS */
    
    if (pool == (void*)-1)
        printerr ("can't mmap pool of %u bytes: %s - using malloc\n",
                   (unsigned)poolsize, strerror (errno));
    else {
        pool_is_mmapped = 1;
        pool_okay = 1;
    }

#endif /* HAVE_MMAP */
    
    if( !pool_okay ) {
        pool = malloc (poolsize);
        assert (pool);
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
    GMemVTable vtable;
    
    memset (&vtable, 0, sizeof (vtable));
    vtable.malloc = switch_malloc;
    vtable.realloc = switch_realloc;
    vtable.free = switch_free;
    vtable.calloc = switch_calloc;
    g_mem_set_vtable (&vtable);
    
    if(!n) {
#ifdef USE_CAPABILITIES
        /* drop all capabilities */
        cap_set_proc( cap_from_text("all-eip") );
#elif !defined(HAVE_DOSISH_SYSTEM)
        have_secure = 0;
#endif
    } else {
        if (n < DEFAULT_POOLSIZE)
            n = DEFAULT_POOLSIZE;
        assert (!pool_okay);
        init_pool (n);
    }
    
    if (!have_secure)
        printerr ("WARNING: not using secure memory for passwords\n");
}

void*
seahorse_secure_memory_malloc (size_t size)
{
    MEMBLOCK *mb, *mb2;
    int compressed = 0;

    assert (pool_okay);

    /* blocks are always a multiple of 32 */
    size += sizeof (MEMBLOCK);
    size = ((size + 31) / 32) * 32;

    DEBUG_SECMEM(("SECMEM: allocating %0x bytes\n", size));

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
        mb->sig = MEMBLOCK_SIG;
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

    assert (mb->sig == MEMBLOCK_SIG);
    
    if( newsize <= size )
        return p; /* it is easier not to shrink the memory */
    
    DEBUG_SECMEM(("SECMEM: reallocating %x bytes to %x\n", size, newsize));
    
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

    assert (mb->sig == MEMBLOCK_SIG);
    
    DEBUG_SECMEM(("SECMEM: freeing %x bytes\n", size));
    
    /* This does not make much sense: probably this memory is held in the
     * cache. We do it anyway: */
    WIPE_MEMORY2 (mb, 0xff, size );
    WIPE_MEMORY2 (mb, 0xaa, size );
    WIPE_MEMORY2 (mb, 0x55, size );
    WIPE_MEMORY2 (mb, 0x00, size );
    mb->size = size;
    mb->sig = MEMBLOCK_SIG;
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
    if (!have_secure)
        return;
    
    printerr ("secmem usage: %u/%u bytes in %u/%u blocks of pool %lu/%lu\n",
              cur_alloced, max_alloced, cur_blocks, max_blocks, 
              (unsigned long)poollen, (unsigned long)poolsize);
}
