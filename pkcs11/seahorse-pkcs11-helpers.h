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

#ifndef __SEAHORSE_PKCS11_HELPERS_H__
#define __SEAHORSE_PKCS11_HELPERS_H__

#include <glib.h>
#include <gck/gck.h>

guint               seahorse_pkcs11_ulong_hash              (gconstpointer v);

gboolean            seahorse_pkcs11_ulong_equal             (gconstpointer v1,
                                                             gconstpointer v2);


#endif
