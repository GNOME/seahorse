/*
 * Seahorse
 *
 * Copyright (C) 2008 Stefan Walter
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
 * along with this program; if not, see
 * <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "seahorse-pgp.h"
#include "seahorse-gpgme.h"
#include "seahorse-pgp-subkey.h"
#include "seahorse-pgp-uid.h"

#include <string.h>

#include <glib/gi18n.h>

enum {
	PROP_0,
	PROP_INDEX,
	PROP_KEYID,
	PROP_FLAGS,
	PROP_LENGTH,
	PROP_ALGORITHM,
	PROP_CREATED,
	PROP_EXPIRES,
	PROP_DESCRIPTION,
	PROP_FINGERPRINT
};

G_DEFINE_TYPE (SeahorsePgpSubkey, seahorse_pgp_subkey, G_TYPE_OBJECT);

struct _SeahorsePgpSubkeyPrivate {
	guint index;
	gchar *keyid;
	guint flags;
	guint length;
	gchar *algorithm;
	gulong created;
	gulong expires;
	gchar *description;
	gchar *fingerprint;
};

/* -----------------------------------------------------------------------------
 * OBJECT 
 */

static void
seahorse_pgp_subkey_init (SeahorsePgpSubkey *self)
{
	self->pv = G_TYPE_INSTANCE_GET_PRIVATE (self, SEAHORSE_TYPE_PGP_SUBKEY, SeahorsePgpSubkeyPrivate);
}

static void
seahorse_pgp_subkey_get_property (GObject *object, guint prop_id,
                                  GValue *value, GParamSpec *pspec)
{
	SeahorsePgpSubkey *self = SEAHORSE_PGP_SUBKEY (object);
	
	switch (prop_id) {
	case PROP_INDEX:
		g_value_set_uint (value, seahorse_pgp_subkey_get_index (self));
		break;
	case PROP_KEYID:
		g_value_set_string (value, seahorse_pgp_subkey_get_keyid (self));
		break;
	case PROP_FLAGS:
		g_value_set_uint (value, seahorse_pgp_subkey_get_flags (self));
		break;
	case PROP_LENGTH:
		g_value_set_uint (value, seahorse_pgp_subkey_get_length (self));
		break;
	case PROP_ALGORITHM:
		g_value_set_string (value, seahorse_pgp_subkey_get_algorithm (self));
		break;
	case PROP_CREATED:
		g_value_set_ulong (value, seahorse_pgp_subkey_get_created (self));
		break;
	case PROP_EXPIRES:
		g_value_set_ulong (value, seahorse_pgp_subkey_get_expires (self));
		break;
	case PROP_DESCRIPTION:
		g_value_set_string (value, seahorse_pgp_subkey_get_description (self));
		break;
	case PROP_FINGERPRINT:
		g_value_set_string (value, seahorse_pgp_subkey_get_fingerprint (self));
		break;
	}
}

static void
seahorse_pgp_subkey_set_property (GObject *object, guint prop_id, const GValue *value, 
                                  GParamSpec *pspec)
{
	SeahorsePgpSubkey *self = SEAHORSE_PGP_SUBKEY (object);
	
	switch (prop_id) {
	case PROP_INDEX:
		seahorse_pgp_subkey_set_index (self, g_value_get_uint (value));
		break;
	case PROP_KEYID:
		seahorse_pgp_subkey_set_keyid (self, g_value_get_string (value));
		break;
	case PROP_FLAGS:
		seahorse_pgp_subkey_set_flags (self, g_value_get_uint (value));
		break;
	case PROP_LENGTH:
		seahorse_pgp_subkey_set_length (self, g_value_get_uint (value));
		break;
	case PROP_ALGORITHM:
		seahorse_pgp_subkey_set_algorithm (self, g_value_get_string (value));
		break;
	case PROP_CREATED:
		seahorse_pgp_subkey_set_created (self, g_value_get_ulong (value));
		break;
	case PROP_EXPIRES:
		seahorse_pgp_subkey_set_expires (self, g_value_get_ulong (value));
		break;
	case PROP_FINGERPRINT:
		seahorse_pgp_subkey_set_fingerprint (self, g_value_get_string (value));
		break;
	case PROP_DESCRIPTION:
		seahorse_pgp_subkey_set_description (self, g_value_get_string (value));
		break;
	}
}

static void
seahorse_pgp_subkey_finalize (GObject *gobject)
{
	SeahorsePgpSubkey *self = SEAHORSE_PGP_SUBKEY (gobject);

	g_free (self->pv->algorithm);
	self->pv->algorithm = NULL;
	
	g_free (self->pv->fingerprint);
	self->pv->fingerprint = NULL;

	g_free (self->pv->description);
	self->pv->description = NULL;
	
	g_free (self->pv->keyid);
	self->pv->keyid = NULL;
    
	G_OBJECT_CLASS (seahorse_pgp_subkey_parent_class)->finalize (gobject);
}

static void
seahorse_pgp_subkey_class_init (SeahorsePgpSubkeyClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);    

	seahorse_pgp_subkey_parent_class = g_type_class_peek_parent (klass);

	gobject_class->finalize = seahorse_pgp_subkey_finalize;
	gobject_class->set_property = seahorse_pgp_subkey_set_property;
	gobject_class->get_property = seahorse_pgp_subkey_get_property;
	g_type_class_add_private (gobject_class, sizeof (SeahorsePgpSubkeyPrivate));

	g_object_class_install_property (gobject_class, PROP_INDEX,
	        g_param_spec_uint ("index", "Index", "PGP subkey index",
	                           0, G_MAXUINT, 0, G_PARAM_READWRITE));

        g_object_class_install_property (gobject_class, PROP_KEYID,
                g_param_spec_string ("keyid", "Key ID", "GPG Key ID",
                                     "", G_PARAM_READWRITE));

        g_object_class_install_property (gobject_class, PROP_FLAGS,
	        g_param_spec_uint ("flags", "Flags", "PGP subkey flags",
	                           0, G_MAXUINT, 0, G_PARAM_READWRITE));

	g_object_class_install_property (gobject_class, PROP_LENGTH,
	        g_param_spec_uint ("length", "Length", "PGP key length",
	                           0, G_MAXUINT, 0, G_PARAM_READWRITE));

        g_object_class_install_property (gobject_class, PROP_ALGORITHM,
                g_param_spec_string ("algorithm", "Algorithm", "GPG Algorithm",
                                     "", G_PARAM_READWRITE));
        
        g_object_class_install_property (gobject_class, PROP_CREATED,
                g_param_spec_ulong ("created", "Created On", "Date this key was created on",
                                    0, G_MAXULONG, 0, G_PARAM_READWRITE));
        
        g_object_class_install_property (gobject_class, PROP_EXPIRES,
                g_param_spec_ulong ("expires", "Expires On", "Date this key expires on",
                                    0, G_MAXULONG, 0, G_PARAM_READWRITE));

        g_object_class_install_property (gobject_class, PROP_DESCRIPTION,
                g_param_spec_string ("description", "Description", "Key Description",
                                     "", G_PARAM_READWRITE));

        g_object_class_install_property (gobject_class, PROP_FINGERPRINT,
                g_param_spec_string ("fingerprint", "Fingerprint", "PGP Key Fingerprint",
                                     "", G_PARAM_READWRITE));
}

/* -----------------------------------------------------------------------------
 * PUBLIC 
 */

SeahorsePgpSubkey*
seahorse_pgp_subkey_new (void)
{
	return g_object_new (SEAHORSE_TYPE_PGP_SUBKEY, NULL);
}

guint
seahorse_pgp_subkey_get_index (SeahorsePgpSubkey *self)
{
	g_return_val_if_fail (SEAHORSE_IS_PGP_SUBKEY (self), 0);
	return self->pv->index;
}

void
seahorse_pgp_subkey_set_index (SeahorsePgpSubkey *self, guint index)
{
	g_return_if_fail (SEAHORSE_IS_PGP_SUBKEY (self));
	self->pv->index = index;
	g_object_notify (G_OBJECT (self), "index");
}

const gchar*
seahorse_pgp_subkey_get_keyid (SeahorsePgpSubkey *self)
{
	g_return_val_if_fail (SEAHORSE_IS_PGP_SUBKEY (self), NULL);
	return self->pv->keyid;
}

void
seahorse_pgp_subkey_set_keyid (SeahorsePgpSubkey *self, const gchar *keyid)
{
	g_return_if_fail (SEAHORSE_IS_PGP_SUBKEY (self));
	g_free (self->pv->keyid);
	self->pv->keyid = g_strdup (keyid);
	g_object_notify (G_OBJECT (self), "keyid");
}

guint
seahorse_pgp_subkey_get_flags (SeahorsePgpSubkey *self)
{
	g_return_val_if_fail (SEAHORSE_IS_PGP_SUBKEY (self), 0);
	return self->pv->flags;
}

void
seahorse_pgp_subkey_set_flags (SeahorsePgpSubkey *self, guint flags)
{
	g_return_if_fail (SEAHORSE_IS_PGP_SUBKEY (self));
	self->pv->flags = flags;
	g_object_notify (G_OBJECT (self), "flags");
}

guint
seahorse_pgp_subkey_get_length (SeahorsePgpSubkey *self)
{
	g_return_val_if_fail (SEAHORSE_IS_PGP_SUBKEY (self), 0);
	return self->pv->length;
}

void
seahorse_pgp_subkey_set_length (SeahorsePgpSubkey *self, guint length)
{
	g_return_if_fail (SEAHORSE_IS_PGP_SUBKEY (self));
	self->pv->length = length;
	g_object_notify (G_OBJECT (self), "length");
}

gchar *
seahorse_pgp_subkey_get_usage (SeahorsePgpSubkey *self)
{
	typedef struct {
		unsigned int flag;
		const char *name;
	} FlagNames;

	const FlagNames const flag_names[] = {
		{ SEAHORSE_FLAG_CAN_ENCRYPT,      N_("Encrypt") },
		{ SEAHORSE_FLAG_CAN_SIGN,         N_("Sign") },
		{ SEAHORSE_FLAG_CAN_CERTIFY,      N_("Certify") },
		{ SEAHORSE_FLAG_CAN_AUTHENTICATE, N_("Authenticate") }
	};

	GString *str;
	gboolean previous;
	int i;

	g_return_val_if_fail (SEAHORSE_IS_PGP_SUBKEY (self), NULL);

	str = g_string_new (NULL);

	previous = FALSE;

	for (i = 0; i < G_N_ELEMENTS (flag_names); i++) {
		if (self->pv->flags & flag_names[i].flag) {
			if (previous)
				g_string_append (str, ", ");

			previous = TRUE;
			g_string_append (str, _(flag_names[i].name));
		}
	}

	return g_string_free (str, FALSE);
}

const gchar*
seahorse_pgp_subkey_get_algorithm (SeahorsePgpSubkey *self)
{
	g_return_val_if_fail (SEAHORSE_IS_PGP_SUBKEY (self), NULL);
	return self->pv->algorithm;
}

void
seahorse_pgp_subkey_set_algorithm (SeahorsePgpSubkey *self, const gchar *algorithm)
{
	g_return_if_fail (SEAHORSE_IS_PGP_SUBKEY (self));
	g_free (self->pv->algorithm);
	self->pv->algorithm = g_strdup (algorithm);
	g_object_notify (G_OBJECT (self), "algorithm");
}

gulong
seahorse_pgp_subkey_get_created (SeahorsePgpSubkey *self)
{
	g_return_val_if_fail (SEAHORSE_IS_PGP_SUBKEY (self), 0);
	return self->pv->created;
}

void
seahorse_pgp_subkey_set_created (SeahorsePgpSubkey *self, gulong created)
{
	g_return_if_fail (SEAHORSE_IS_PGP_SUBKEY (self));
	self->pv->created = created;
	g_object_notify (G_OBJECT (self), "created");
}

gulong
seahorse_pgp_subkey_get_expires (SeahorsePgpSubkey *self)
{
	g_return_val_if_fail (SEAHORSE_IS_PGP_SUBKEY (self), 0);
	return self->pv->expires;
}

void
seahorse_pgp_subkey_set_expires (SeahorsePgpSubkey *self, gulong expires)
{
	g_return_if_fail (SEAHORSE_IS_PGP_SUBKEY (self));
	self->pv->expires = expires;
	g_object_notify (G_OBJECT (self), "expires");
}

const gchar*
seahorse_pgp_subkey_get_fingerprint (SeahorsePgpSubkey *self)
{
	g_return_val_if_fail (SEAHORSE_IS_PGP_SUBKEY (self), NULL);
	return self->pv->fingerprint;
}

void
seahorse_pgp_subkey_set_fingerprint (SeahorsePgpSubkey *self, const gchar *fingerprint)
{
	g_return_if_fail (SEAHORSE_IS_PGP_SUBKEY (self));
	g_free (self->pv->fingerprint);
	self->pv->fingerprint = g_strdup (fingerprint);
	g_object_notify (G_OBJECT (self), "fingerprint");
}

const gchar*
seahorse_pgp_subkey_get_description (SeahorsePgpSubkey *self)
{
	g_return_val_if_fail (SEAHORSE_IS_PGP_SUBKEY (self), NULL);
	return self->pv->description;
}

void
seahorse_pgp_subkey_set_description (SeahorsePgpSubkey *self, const gchar *description)
{
	g_return_if_fail (SEAHORSE_IS_PGP_SUBKEY (self));
	g_free (self->pv->description);
	self->pv->description = g_strdup (description);
	g_object_notify (G_OBJECT (self), "description");
}

gchar*
seahorse_pgp_subkey_calc_description (const gchar *name, guint index)
{
	if (name == NULL)
		name = _("Key");

	if (index == 0)
		return g_strdup (name);

	return g_strdup_printf (_("Subkey %d of %s"), index, name);
}

/* Takes runs of hexadecimal digits, possibly with whitespace among them, and
 * formats them nicely in groups of four digits.
 */
gchar*
seahorse_pgp_subkey_calc_fingerprint (const gchar *raw_fingerprint)
{
	const gchar *raw;
	GString *string;
	guint i, len;
	guint num_digits;
	gchar *fpr;
	    
	raw = raw_fingerprint;
	g_return_val_if_fail (raw != NULL, NULL);

	string = g_string_new ("");
	len = strlen (raw);

	num_digits = 0;
	for (i = 0; i < len; i++) {
		if (g_ascii_isxdigit (raw[i])) {
			g_string_append_c (string, g_ascii_toupper (raw[i]));
			num_digits++;

			if (num_digits > 0 && num_digits % 4 == 0)
				g_string_append (string, " ");
		}
	}

	fpr = string->str;
	g_string_free (string, FALSE);

	g_strchomp (fpr);

	return fpr;
}
