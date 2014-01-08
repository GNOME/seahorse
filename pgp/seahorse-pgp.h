/* 
 * Seahorse
 * 
 * Copyright (C) 2008 Stefan Walter
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2 of the licence or (at
 * your option) any later version.
 *
 * See the included COPYING file for more information.
 */

#ifndef __SEAHORSE_PGP_H__
#define __SEAHORSE_PGP_H__

#include <glib.h>
#include <glib-object.h>

#include <gcr/gcr.h>

G_BEGIN_DECLS

#define SEAHORSE_PGP_NAME "openpgp"
#define SEAHORSE_PGP_TYPE_STR SEAHORSE_PGP_NAME
#define SEAHORSE_PGP g_quark_from_string (SEAHORSE_PGP_NAME)

#ifdef WITH_PGP

void       seahorse_pgp_backend_initialize    (void);

#endif

G_END_DECLS

#endif
