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

#include "seahorse-key.h"

enum {
	PROP_0,
	PROP_KEY
};

enum {
	CHANGED,
	LAST_SIGNAL
};

static void		seahorse_key_class_init		(SeahorseKeyClass	*klass);
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
			0, NULL
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
		g_param_spec_pointer ("key", "Gpgme Key",
				      "Gpgme Key that this object represents",
				      G_PARAM_READWRITE));
	
	key_signals[CHANGED] = g_signal_new ("changed", G_OBJECT_CLASS_TYPE (gobject_class),
		G_SIGNAL_RUN_LAST,  G_STRUCT_OFFSET (SeahorseKeyClass, changed),
		NULL, NULL, g_cclosure_marshal_VOID__INT, G_TYPE_NONE, 1, G_TYPE_INT);
}

/* Unrefs gpgme key and frees data */
static void
seahorse_key_finalize (GObject *gobject)
{
	SeahorseKey *skey;
	
	skey = SEAHORSE_KEY (gobject);
	gpgme_key_unref (skey->key);
	
	G_OBJECT_CLASS (parent_class)->finalize (gobject);
}

static void
seahorse_key_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	SeahorseKey *skey;
	
	skey = SEAHORSE_KEY (object);
	
	switch (prop_id) {
		case PROP_KEY:
			skey->key = g_value_get_pointer (value);
			gpgme_key_ref (skey->key);
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

/**
 * seahorse_key_new:
 * @key: Key to wrap
 *
 * Creates a new #SeahorseKey wrapper for @key.
 *
 * Returns: A new #SeahorseKey
 **/
SeahorseKey*
seahorse_key_new (gpgme_key_t key)
{
	return g_object_new (SEAHORSE_TYPE_KEY, "key", key, NULL);
}

/**
 * seahorse_key_destroy:
 * @skey: #SeahorseKey to destroy
 *
 * Conveniance wrapper for gtk_object_destroy(). Emits destroy signal for @skey.
 **/
void
seahorse_key_destroy (SeahorseKey *skey)
{
	g_return_if_fail (skey != NULL && GTK_IS_OBJECT (skey));
	
	gtk_object_destroy (GTK_OBJECT (skey));
}

/**
 * seahorse_key_changed:
 * @skey: #SeahorseKey that changed
 * @change: #SeahorseKeyChange type
 *
 * Emits the changed signal for @skey with @change.
 **/
void
seahorse_key_changed (SeahorseKey *skey, SeahorseKeyChange change)
{
	g_return_if_fail (skey != NULL && SEAHORSE_IS_KEY (skey));
	
	g_signal_emit (G_OBJECT (skey), key_signals[CHANGED], 0, change);
}

/**
 * seahorse_key_get_num_uids:
 * @skey: #SeahorseKey
 *
 * Counts the number of user IDs for @skey.
 *
 * Returns: The number of user IDs for @skey, or -1 if error.
 **/
const gint
seahorse_key_get_num_uids (const SeahorseKey *skey)
{
	gint index = 0;
	gpgme_user_id_t uid;

	g_return_val_if_fail (skey != NULL && SEAHORSE_IS_KEY (skey), -1);
	g_return_val_if_fail (skey->key != NULL, -1);
		
	uid = skey->key->uids;
	while (uid) {
		uid = uid->next;
		index++;
	}
	
	return index;
}

/**
 * seahorse_key_get_num_subkeys:
 * @skey: #SeahorseKey
 *
 * Counts the number of subkeys for @skey.
 *
 * Returns: The number of subkeys for @skey, or -1 if error.
 **/
const gint
seahorse_key_get_num_subkeys (const SeahorseKey *skey)
{
	gint index = 0;
	gpgme_subkey_t subkey;

	g_return_val_if_fail (skey != NULL && SEAHORSE_IS_KEY (skey), -1);
	g_return_val_if_fail (skey->key != NULL, -1);

	subkey = skey->key->subkeys;
	while (subkey) {
		subkey = subkey->next;
		index++;
	}
	
	return index;
}

/**
 * seahorse_key_get_nth_subkey:
 * @skey: #SeahorseKey
 * @index: Which subkey
 *
 * Gets the the subkey at @index of @skey.
 *
 * Returns: subkey of @skey at @index, or NULL if @index is out of bounds
 */
gpgme_subkey_t
seahorse_key_get_nth_subkey (const SeahorseKey *skey, const guint index)
{
	gpgme_subkey_t subkey;
	guint n;

	g_return_val_if_fail (skey != NULL && SEAHORSE_IS_KEY (skey), NULL);
	g_return_val_if_fail (skey->key != NULL, NULL);

	subkey = skey->key->subkeys;
	for (n = index; subkey && n; n--)
		subkey = subkey->next;

	return subkey;
}


/**
 * seahorse_key_get_keyid:
 * @skey: #SeahorseKey
 * @index: Which keyid
 *
 * Gets the formatted keyid of @skey at @index.
 * Borrowed from gpa.
 *
 * Returns: Keyid of @skey at @index, or NULL if @index is out of bounds
 **/
const gchar*
seahorse_key_get_keyid (const SeahorseKey *skey, const guint index)
{
	gpgme_subkey_t subkey = seahorse_key_get_nth_subkey (skey, index);

	if (subkey)
		return subkey->keyid + 8;
	else
		return NULL;
}

/**
 * seahorse_key_get_userid:
 * @skey: #SeahorseKey
 * @index: Which user ID
 *
 * Gets the formatted user ID of @skey at @index.
 * Concept borrowed from gpa.
 *
 * Returns: UTF8 valid user ID of @skey at @index,
 * or NULL if @index is out of bounds.
 **/
gchar*
seahorse_key_get_userid (const SeahorseKey *skey, const guint index)
{
	gpgme_user_id_t uid;
	guint n;
	
	g_return_val_if_fail (skey != NULL && SEAHORSE_IS_KEY (skey), NULL);
	g_return_val_if_fail (skey->key != NULL, NULL);

	uid = skey->key->uids;
	for (n = index; uid && n; n--)
		uid = uid->next;
	
	if (!uid)
		return NULL;
	
	/* If not utf8 valid, assume latin 1 */
	if (!g_utf8_validate (uid->uid, -1, NULL))
		return g_convert (uid->uid, -1, "UTF-8", "ISO-8859-1", NULL, NULL, NULL);
	else
		return g_strdup (uid->uid);
}

gchar*
seahorse_key_get_fingerprint (const SeahorseKey *skey)
{
	const gchar *raw;
	GString *string;
	guint index, len;
	gchar *fpr;
	
	g_return_val_if_fail (skey != NULL && SEAHORSE_IS_KEY (skey), NULL);
	g_return_val_if_fail (skey->key != NULL && skey->key->subkeys != NULL, NULL);

	raw = skey->key->subkeys->fpr;
	g_return_val_if_fail (raw != NULL, NULL);

	string = g_string_new ("");
	len = strlen (raw);
	
	for (index = 0; index < len; index++) {
		if (index > 0 && index % 4 == 0)
			g_string_append (string, " ");
		g_string_append_c (string, raw[index]);
	}
	
	fpr = string->str;
	g_string_free (string, FALSE);
	
	return fpr;
}

const gchar*
seahorse_key_get_id (gpgme_key_t key)
{
	g_return_val_if_fail (key != NULL && key->subkeys != NULL, FALSE);
	
	return key->subkeys->fpr;
}

gboolean
seahorse_key_is_valid (const SeahorseKey *skey)
{
	g_return_val_if_fail (skey != NULL && SEAHORSE_IS_KEY (skey), FALSE);
	g_return_val_if_fail (skey->key != NULL, FALSE);
	
	return (!skey->key->disabled && !skey->key->expired && !skey->key->revoked &&
		!skey->key->invalid);
}

gboolean
seahorse_key_can_encrypt (const SeahorseKey *skey)
{
	return (seahorse_key_is_valid (skey) &&	skey->key->can_encrypt);
}

const SeahorseValidity
seahorse_key_get_validity (const SeahorseKey *skey)
{
	g_return_val_if_fail (skey != NULL && SEAHORSE_IS_KEY (skey), SEAHORSE_VALIDITY_UNKNOWN);
	g_return_val_if_fail (skey->key != NULL, SEAHORSE_VALIDITY_UNKNOWN);

	if (skey->key->revoked)
		return SEAHORSE_VALIDITY_REVOKED;
	if (skey->key->disabled)
		return SEAHORSE_VALIDITY_DISABLED;
	if (skey->key->uids->validity <= SEAHORSE_VALIDITY_UNKNOWN)
		return SEAHORSE_VALIDITY_UNKNOWN;
	return skey->key->uids->validity;
}

const SeahorseValidity
seahorse_key_get_trust (const SeahorseKey *skey)
{
	if (skey->key->owner_trust <= SEAHORSE_VALIDITY_UNKNOWN)
		return SEAHORSE_VALIDITY_UNKNOWN;
	return skey->key->owner_trust;
}
