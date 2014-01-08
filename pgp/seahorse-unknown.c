/*
 * Seahorse
 *
 * Copyright (C) 2006 Stefan Walter
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2 of the licence or (at
 * your option) any later version.
 *
 * See the included COPYING file for more information.
 */

#include "config.h"

#include "seahorse-pgp-key.h"
#include "seahorse-unknown.h"
#include "seahorse-unknown-source.h"

#include <glib/gi18n.h>

G_DEFINE_TYPE (SeahorseUnknown, seahorse_unknown, SEAHORSE_TYPE_OBJECT);

/* -----------------------------------------------------------------------------
 * OBJECT
 */


static void
seahorse_unknown_init (SeahorseUnknown *self)
{

}

static void
seahorse_unknown_class_init (SeahorseUnknownClass *klass)
{

}

/* -----------------------------------------------------------------------------
 * PUBLIC METHODS
 */

SeahorseUnknown*
seahorse_unknown_new (SeahorseUnknownSource *source,
                      const gchar *keyid,
                      const gchar *display)
{
	gchar *identifier;

	if (!display)
		display = _("Unavailable");
	identifier = seahorse_pgp_key_calc_identifier (keyid);

	return g_object_new (SEAHORSE_TYPE_UNKNOWN,
	                     "place", source,
	                     "label", display,
	                     "identifier", identifier,
	                     NULL);
}
