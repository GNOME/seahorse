/*
 * Seahorse
 *
 * Copyright (C) 2003 Jacob Perkins
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

#include <gnome.h>

#include "seahorse-key-pair.h"

enum {
	PROP_0,
	PROP_SECRET
};

static void		seahorse_key_pair_class_init	(SeahorseKeyPairClass	*klass);
static void		seahorse_key_pair_finalize	(GObject		*gobject);
static void		seahorse_key_pair_set_property	(GObject		*object,
							 guint			prop_id,
							 const GValue		*value,
							 GParamSpec		*pspec);
static void		seahorse_key_pair_get_property	(GObject		*object,
							 guint			prop_id,
							 GValue			*value,
							 GParamSpec		*pspec);

static SeahorseKeyClass	*parent_class			= NULL;

GType
seahorse_key_pair_get_type (void)
{
	static GType key_pair_type = 0;
	
	if (!key_pair_type) {
		static const GTypeInfo key_pair_info =
		{
			sizeof (SeahorseKeyPairClass),
			NULL, NULL,
			(GClassInitFunc) seahorse_key_pair_class_init,
			NULL, NULL,
			sizeof (SeahorseKeyPair),
			0, NULL
		};
		
		key_pair_type = g_type_register_static (SEAHORSE_TYPE_KEY,
			"SeahorseKeyPair", &key_pair_info, 0);
	}
	
	return key_pair_type;
}

static void
seahorse_key_pair_class_init (SeahorseKeyPairClass *klass)
{
	GObjectClass *gobject_class;
	
	parent_class = g_type_class_peek_parent (klass);
	gobject_class = G_OBJECT_CLASS (klass);
	
	gobject_class->finalize = seahorse_key_pair_finalize;
	gobject_class->set_property = seahorse_key_pair_set_property;
	gobject_class->get_property = seahorse_key_pair_get_property;
	
	g_object_class_install_property (gobject_class, PROP_SECRET,
		g_param_spec_pointer ("secret", "Secret Gpgme Key",
				     "Secret Gpgme Key for the key pair",
				     G_PARAM_READWRITE));
}

/* Unrefs gpgme key and frees data */
static void
seahorse_key_pair_finalize (GObject *gobject)
{
	SeahorseKeyPair *skpair;
	
	skpair = SEAHORSE_KEY_PAIR (gobject);
	seahorse_util_key_unref (skpair->secret);
	
	G_OBJECT_CLASS (parent_class)->finalize (gobject);
}

static void
seahorse_key_pair_set_property (GObject *object, guint prop_id,
				const GValue *value, GParamSpec *pspec)
{
	SeahorseKeyPair *skpair;
	
	skpair = SEAHORSE_KEY_PAIR (object);
	
	switch (prop_id) {
		case PROP_SECRET:
            if (skpair->secret)
                seahorse_util_key_unref (skpair->secret);
            skpair->secret = g_value_get_pointer (value);
            if (skpair->secret) {
                seahorse_util_key_ref (skpair->secret);
                seahorse_key_changed (SEAHORSE_KEY (skpair), SKEY_CHANGE_ALL);
            }        
			break;
		default:
			break;
	}
}

static void
seahorse_key_pair_get_property (GObject *object, guint prop_id,
				  GValue *value, GParamSpec *pspec)
{
	SeahorseKeyPair *skpair;
	
	skpair = SEAHORSE_KEY_PAIR (object);
	
	switch (prop_id) {
		case PROP_SECRET:
			g_value_set_pointer (value, skpair->secret);
			break;
		default:
			break;
	}
}

SeahorseKey*
seahorse_key_pair_new (SeahorseKeySource *sksrc, gpgme_key_t key, gpgme_key_t secret)
{
	return g_object_new (SEAHORSE_TYPE_KEY_PAIR, "key", key, "secret", secret, 
                            "key-source", sksrc, NULL);
}

gboolean
seahorse_key_pair_can_sign (const SeahorseKeyPair *skpair)
{
	g_return_val_if_fail (skpair != NULL && SEAHORSE_IS_KEY_PAIR (skpair), FALSE);
	
	return (seahorse_key_is_valid (SEAHORSE_KEY (skpair)) &&
		SEAHORSE_KEY (skpair)->key->can_sign);
}
