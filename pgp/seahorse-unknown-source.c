/*
 * Seahorse
 *
 * Copyright (C) 2006 Stefan Walter
 * Copyright (C) 2011 Collabora Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the
 * Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include <glib/gi18n.h>

#include "seahorse-pgp-key.h"
#include "seahorse-unknown-source.h"

#include "seahorse-registry.h"
#include "seahorse-unknown.h"

struct _SeahorseUnknownSource {
	GObject parent;
	GHashTable *keys;
};

struct _SeahorseUnknownSourceClass {
	GObjectClass parent_class;
};

G_DEFINE_TYPE (SeahorseUnknownSource, seahorse_unknown_source, G_TYPE_OBJECT);

/* -----------------------------------------------------------------------------
 * OBJECT
 */

static void
seahorse_unknown_source_init (SeahorseUnknownSource *self)
{
	self->keys = g_hash_table_new_full (seahorse_pgp_keyid_hash,
	                                    seahorse_pgp_keyid_equal,
	                                    g_free, g_object_unref);
}

static void
seahorse_unknown_source_finalize (GObject *obj)
{
	SeahorseUnknownSource *self = SEAHORSE_UNKNOWN_SOURCE (obj);

	g_hash_table_destroy (self->keys);

	G_OBJECT_CLASS (seahorse_unknown_source_parent_class)->finalize (obj);
}

static void
seahorse_unknown_source_class_init (SeahorseUnknownSourceClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	gobject_class->finalize = seahorse_unknown_source_finalize;
}

SeahorseUnknownSource*
seahorse_unknown_source_new (void)
{
	return g_object_new (SEAHORSE_TYPE_UNKNOWN_SOURCE, NULL);
}

static void
on_cancellable_gone (gpointer user_data,
                     GObject *where_the_object_was)
{
	/* TODO: Change the icon */
}

SeahorseObject *
seahorse_unknown_source_add_object (SeahorseUnknownSource *self,
                                    const gchar *keyid,
                                    GCancellable *cancellable)
{
	SeahorseObject *object;

	g_return_val_if_fail (keyid != NULL, NULL);
	g_return_val_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable), NULL);

	object = g_hash_table_lookup (self->keys, keyid);
	if (object == NULL) {
		object = SEAHORSE_OBJECT (seahorse_unknown_new (self, keyid, NULL));
		g_hash_table_insert (self->keys, g_strdup (keyid), object);
	}

	if (cancellable)
		g_object_weak_ref (G_OBJECT (cancellable), on_cancellable_gone, object);

	return object;
}
