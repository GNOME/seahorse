/*
 * Seahorse
 *
 * Copyright (C) 2002 Jacob Perkins
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

#include "seahorse-key.h"

struct _SeahorseKeyPrivate
{
	gint		num_uids;
	gint		num_subkeys;
};

enum {
	PROP_0,
	PROP_KEY
};

enum {
	CHANGED,
	LAST_SIGNAL
};

static void		seahorse_key_class_init		(SeahorseKeyClass	*klass);
static void		seahorse_key_init		(SeahorseKey		*skey);
static void		seahorse_key_finalize		(GObject		*gobject);
static void		seahorse_key_set_property	(GObject		*object,
							 guint			prop_id,
							 const GValue		*value,
							 GParamSpec		*pspec);
static void		seahorse_key_get_property	(GObject		*object,
							 guint			prop_id,
							 GValue			*value,
							 GParamSpec		*pspec);

static GtkObjectClass	*parent_class			= NULL;
static guint		key_signals[LAST_SIGNAL]	= { 0 };

GType
seahorse_key_get_type (void)
{
	static GType key_type = 0;
	
	if (!key_type) {
		static const GTypeInfo key_info =
		{
			sizeof (SeahorseKeyClass),
			NULL, NULL,
			(GClassInitFunc) seahorse_key_class_init,
			NULL, NULL,
			sizeof (SeahorseKey),
			0,
			(GInstanceInitFunc) seahorse_key_init
		};
		
		key_type = g_type_register_static (GTK_TYPE_OBJECT, "SeahorseKey", &key_info, 0);
	}
	
	return key_type;
}

static void
seahorse_key_class_init (SeahorseKeyClass *klass)
{
	GObjectClass *gobject_class;
	
	parent_class = g_type_class_peek_parent (klass);
	gobject_class = G_OBJECT_CLASS (klass);
	
	gobject_class->finalize = seahorse_key_finalize;
	gobject_class->set_property = seahorse_key_set_property;
	gobject_class->get_property = seahorse_key_get_property;
	
	klass->changed = NULL;
	
	g_object_class_install_property (gobject_class, PROP_KEY,
		g_param_spec_pointer ("key", _("Gpgme Key"),
				     _("Gpgme Key that this object represents"),
				     G_PARAM_READWRITE));
	
	key_signals[CHANGED] = g_signal_new ("changed", G_OBJECT_CLASS_TYPE (gobject_class),
		G_SIGNAL_RUN_LAST,  G_STRUCT_OFFSET (SeahorseKeyClass, changed),
		NULL, NULL, g_cclosure_marshal_VOID__INT, G_TYPE_NONE, 1, G_TYPE_INT);
}

static void
seahorse_key_init (SeahorseKey *skey)
{
	skey->priv = g_new0 (SeahorseKeyPrivate, 1);
}

/* Unrefs gpgme key */
static void
seahorse_key_finalize (GObject *gobject)
{
	SeahorseKey *skey;
	
	skey = SEAHORSE_KEY (gobject);
	gpgme_key_unref (skey->key);
	g_free (skey->priv);
	
	G_OBJECT_CLASS (parent_class)->finalize (gobject);
}

static void
seahorse_key_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	SeahorseKey *skey;
	gint id = 1;
	gchar *name;
	
	skey = SEAHORSE_KEY (object);
	
	switch (prop_id) {
		/* Sets the key and saves the primary name, # user ids, # subkeys */
		case PROP_KEY:
			skey->key = g_value_get_pointer (value);
			gpgme_key_ref (skey->key);
			
			while (gpgme_key_get_string_attr (skey->key, GPGME_ATTR_NAME, NULL, id))
				id++;
			
			skey->priv->num_uids = id;
			
			id = 0;
			while (gpgme_key_get_string_attr (skey->key, GPGME_ATTR_KEYID, NULL, id+1))
				id++;
			
			skey->priv->num_subkeys = id;
			break;
		default:
			break;
	}
}

static void
seahorse_key_get_property (GObject *object, guint prop_id,
				  GValue *value, GParamSpec *pspec)
{
	SeahorseKey *skey;
	
	skey = SEAHORSE_KEY (object);
	
	switch (prop_id) {
		case PROP_KEY:
			g_value_set_pointer (value, skey->key);
			break;
		default:
			break;
	}
}

SeahorseKey*
seahorse_key_new (GpgmeKey key)
{
	SeahorseKey *skey;
	
	skey = g_object_new (SEAHORSE_TYPE_KEY, "key", key, NULL);
	
	return skey;
}

void
seahorse_key_destroy (SeahorseKey *skey)
{
	g_return_if_fail (GTK_IS_OBJECT (skey));
	
	gtk_object_destroy (GTK_OBJECT (skey));
}

void
seahorse_key_changed (SeahorseKey *skey, SeahorseKeyChange change)
{
	g_return_if_fail (SEAHORSE_IS_KEY (skey));
	
	g_signal_emit (G_OBJECT (skey), key_signals[CHANGED], 0, change);
}

const gint
seahorse_key_get_num_uids (const SeahorseKey *skey)
{
	g_return_val_if_fail (SEAHORSE_IS_KEY (skey) && skey->priv != NULL, -1);
	
	return skey->priv->num_uids;
}
const gint
seahorse_key_get_num_subkeys (const SeahorseKey *skey)
{
	g_return_val_if_fail (SEAHORSE_IS_KEY (skey) && skey->priv != NULL, -1);
	
	return skey->priv->num_subkeys;
}

/* Borrowed from gpa */
const gchar*
seahorse_key_get_keyid (const SeahorseKey *skey, const guint index)
{
	const gchar *keyid;
	
	g_return_val_if_fail (SEAHORSE_IS_KEY (skey), NULL);
	
	keyid = gpgme_key_get_string_attr (skey->key, GPGME_ATTR_KEYID, NULL, index);
	
	return (keyid+8);
}

/* Concept borrowed from gpa */
const gchar*
seahorse_key_get_userid (const SeahorseKey *skey, const guint index)
{
	gchar *uid;
	
	uid = g_strdup_printf ("%s (%s) <%s>",
		gpgme_key_get_string_attr (skey->key, GPGME_ATTR_NAME, NULL, index),
		gpgme_key_get_string_attr (skey->key, GPGME_ATTR_COMMENT, NULL, index),
		gpgme_key_get_string_attr (skey->key, GPGME_ATTR_EMAIL, NULL, index));
	
	/* If not utf8 valid, assume latin 1 */
	if (!g_utf8_validate (uid, -1, NULL))
		uid = g_convert (uid, -1, "UTF-8", "ISO-8859-1", NULL, NULL, NULL);
	
	return uid;
}
