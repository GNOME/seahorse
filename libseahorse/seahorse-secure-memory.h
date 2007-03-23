/* 
 * Quintuple Agent secure memory allocation
 * Copyright (C) 1998,1999 Free Software Foundation, Inc.
 * Copyright (C) 1999,2000 Robert Bihlmeyer <robbe@orcus.priv.at>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef _SEAHORSE_SECURE_MEMORY_H_
#define _SEAHORSE_SECURE_MEMORY_H_

#include <sys/types.h>

extern gboolean seahorse_use_secure_mem;

#define WITH_SECURE_MEM(EXP) \
    do { \
        gboolean tmp = seahorse_use_secure_mem; \
        seahorse_use_secure_mem = TRUE; \
        EXP; \
        seahorse_use_secure_mem = tmp; \
    } while (0)

/* This must be called before any glib/gtk/gnome functions */
void    seahorse_secure_memory_init         (size_t npool);

void    seahorse_secure_memory_term         ();

int     seahorse_secure_memory_have         ();

void*   seahorse_secure_memory_malloc       (size_t size);

void*   seahorse_secure_memory_realloc      (void *a, size_t newsize);

void    seahorse_secure_memory_free         (void *a);

int     seahorse_secure_memory_check        (const void *p);

void    seahorse_secure_memory_dump_stats   (void);

#endif /* _SEAHORSE_SECURE_MEMORY_H_ */
