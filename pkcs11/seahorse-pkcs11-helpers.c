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

#include "config.h"
#include "seahorse-common.h"

#include "seahorse-pkcs11.h"
#include "seahorse-pkcs11-helpers.h"

#include <stdlib.h>
#include <string.h>

guint
seahorse_pkcs11_ulong_hash (gconstpointer v)
{
	const signed char *p = v;
	guint32 i, h = *p;

	for(i = 0; i < sizeof (gulong); ++i)
		h = (h << 5) - h + *(p++);

	return h;
}

gboolean
seahorse_pkcs11_ulong_equal (gconstpointer v1,
                             gconstpointer v2)
{
	return *((const gulong*)v1) == *((const gulong*)v2);
}

