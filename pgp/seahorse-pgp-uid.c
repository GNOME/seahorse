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
 * along with this program; if not, write to the
 * Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include "seahorse-gpgmex.h"
#include "seahorse-pgp-key.h"
#include "seahorse-pgp-uid.h"

#include <string.h>

#include <glib/gi18n.h>

enum {
	PROP_0,
	PROP_PUBKEY,
	PROP_USERID,
	PROP_INDEX,
	PROP_VALIDITY,
	PROP_VALIDITY_STR
};

G_DEFINE_TYPE (SeahorsePGPUid, seahorse_pgp_uid, SEAHORSE_TYPE_OBJECT);

/* -----------------------------------------------------------------------------
 * INTERNAL HELPERS
 */

static gchar*
convert_string (const gchar *str, gboolean escape)
{
	gchar *t, *ret;
    
	if (!str)
  	      return NULL;
    
	/* If not utf8 valid, assume latin 1 */
 	if (!g_utf8_validate (str, -1, NULL)) {
 		ret = g_convert (str, -1, "UTF-8", "ISO-8859-1", NULL, NULL, NULL);
 		if (escape) {
 			t = ret;
 			ret = g_markup_escape_text (t, -1);
 			g_free (t);
 		}
        
 		return ret;
 	}

 	if (escape)
 		return g_markup_escape_text (str, -1);
 	else
 		return g_strdup (str);
}

static void
changed_uid (SeahorsePGPUid *self)
{
	SeahorseObject *obj = SEAHORSE_OBJECT (self);
	SeahorseLocation loc;
	GQuark id = 0;
	gchar *name, *markup;
	
	if (!self->userid || !self->pubkey) {
        
		g_object_set (self,
		              "id", id,
		              "label", "",
		              "usage", SEAHORSE_USAGE_NONE,
		              "markup", "",
		              "location", SEAHORSE_LOCATION_INVALID,
		              "flags", SEAHORSE_FLAG_DISABLED,
		              NULL);
		return;
		
	} 
		
	/* The key id */
	if (self->pubkey->subkeys)
		id = seahorse_pgp_key_get_cannonical_id (self->pubkey->subkeys->keyid);

	/* The location */
	loc = seahorse_object_get_location (obj);
	if (self->pubkey->keylist_mode & GPGME_KEYLIST_MODE_EXTERN && 
	    loc <= SEAHORSE_LOCATION_REMOTE)
		loc = SEAHORSE_LOCATION_REMOTE;

	else if (loc <= SEAHORSE_LOCATION_LOCAL)
		loc = SEAHORSE_LOCATION_LOCAL;

	name = seahorse_pgp_uid_get_display_name (self);
	markup = seahorse_pgp_uid_get_markup (self, 0);
	
	g_object_set (self,
		      "id", id,
		      "label", name,
		      "markup", markup,
		      "usage", SEAHORSE_USAGE_IDENTITY,
		      "flags", 0,
		      NULL);
	
	g_free (name);
	g_free (markup);
}

/* -----------------------------------------------------------------------------
 * OBJECT 
 */

static void
seahorse_pgp_uid_init (SeahorsePGPUid *uid)
{
	g_object_set (uid, 
	              "icon", "",
	              NULL);
}

static void
seahorse_pgp_uid_get_property (GObject *object, guint prop_id,
                               GValue *value, GParamSpec *pspec)
{
	SeahorsePGPUid *uid = SEAHORSE_PGP_UID (object);
	
	switch (prop_id) {
	case PROP_PUBKEY:
		g_value_set_pointer (value, uid->pubkey);
		break;
	case PROP_USERID:
		g_value_set_pointer (value, uid->userid);
		break;
	case PROP_INDEX:
		g_value_set_uint (value, uid->index);
		break;
	case PROP_VALIDITY:
		g_value_set_uint (value, seahorse_pgp_uid_get_validity (uid));
		break;
	case PROP_VALIDITY_STR:
		g_value_set_string (value, seahorse_validity_get_string (seahorse_pgp_uid_get_validity (uid)));
		break;
	}
}

static void
seahorse_pgp_uid_set_property (GObject *object, guint prop_id, const GValue *value, 
                               GParamSpec *pspec)
{
	SeahorsePGPUid *uid = SEAHORSE_PGP_UID (object);

	switch (prop_id) {
	case PROP_PUBKEY:
		g_return_if_fail (!uid->pubkey);
		uid->pubkey = g_value_get_pointer (value);
		if (uid->pubkey)
			gpgmex_key_ref (uid->pubkey);
		break;
	case PROP_INDEX:
		uid->index = g_value_get_uint (value);
		break;
	case PROP_USERID:
		uid->userid = g_value_get_pointer (value);
		changed_uid (uid);
		break;
	}
}

static void
seahorse_pgp_uid_object_finalize (GObject *gobject)
{
	SeahorsePGPUid *uid = SEAHORSE_PGP_UID (gobject);

	/* Unref the key */
	if (uid->pubkey)
		gpgmex_key_unref (uid->pubkey);
	uid->pubkey = NULL;
	uid->userid = NULL;
    
	G_OBJECT_CLASS (seahorse_pgp_uid_parent_class)->finalize (gobject);
}

static void
seahorse_pgp_uid_class_init (SeahorsePGPUidClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);    
	seahorse_pgp_uid_parent_class = g_type_class_peek_parent (klass);
    
	gobject_class->finalize = seahorse_pgp_uid_object_finalize;
	gobject_class->set_property = seahorse_pgp_uid_set_property;
	gobject_class->get_property = seahorse_pgp_uid_get_property;
    
	g_object_class_install_property (gobject_class, PROP_PUBKEY,
	        g_param_spec_pointer ("pubkey", "Gpgme Public Key", "Gpgme Public Key that this uid is on",
	                              G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property (gobject_class, PROP_USERID,
	        g_param_spec_pointer ("userid", "Gpgme User ID", "Gpgme User ID",
	                              G_PARAM_READWRITE));
                      
	g_object_class_install_property (gobject_class, PROP_INDEX,
	        g_param_spec_uint ("index", "Index", "Gpgme User ID Index",
	                           0, G_MAXUINT, 0, G_PARAM_READWRITE));
         
	g_object_class_install_property (gobject_class, PROP_VALIDITY,
	        g_param_spec_uint ("validity", "Validity", "Validity of this identity",
	                           0, G_MAXUINT, 0, G_PARAM_READABLE));

        g_object_class_install_property (gobject_class, PROP_VALIDITY_STR,
                g_param_spec_string ("validity-str", "Validity String", "Validity of this identity as a string",
                                     "", G_PARAM_READABLE));
}

/* -----------------------------------------------------------------------------
 * PUBLIC 
 */

SeahorsePGPUid* 
seahorse_pgp_uid_new (gpgme_key_t pubkey, gpgme_user_id_t userid) 
{
	return g_object_new (SEAHORSE_TYPE_PGP_UID, "pubkey", pubkey, "userid", userid, NULL);
}

gchar*
seahorse_pgp_uid_get_display_name (SeahorsePGPUid *uid)
{
	g_return_val_if_fail (SEAHORSE_IS_PGP_UID (uid), NULL);
	g_return_val_if_fail (uid->userid, NULL);
	return convert_string (uid->userid->uid, FALSE);
}

gchar*
seahorse_pgp_uid_get_markup (SeahorsePGPUid *uid, guint flags)
{
	gchar *email, *name, *comment, *ret;
	const gchar *format;
	gboolean strike = FALSE;

	g_return_val_if_fail (SEAHORSE_IS_PGP_UID (uid), NULL);
	g_return_val_if_fail (uid->userid, NULL);
	
	name = convert_string (uid->userid->name, TRUE);
	email = convert_string (uid->userid->email, TRUE);
	comment = convert_string (uid->userid->comment, TRUE);

	if (uid->userid->revoked || flags & CRYPTUI_FLAG_EXPIRED || 
	    flags & CRYPTUI_FLAG_REVOKED || flags & CRYPTUI_FLAG_DISABLED)
		strike = TRUE;
	    
	if (strike)
		format = "<span strikethrough='true'>%s<span foreground='#555555' size='small' rise='0'>%s%s%s%s%s</span></span>";
	else
		format = "%s<span foreground='#555555' size='small' rise='0'>%s%s%s%s%s</span>";
		
	ret = g_markup_printf_escaped (format, name,
	 	          email && email[0] ? "  " : "",
	 	          email && email[0] ? email : "",
	 	          comment && comment[0] ? "  '" : "",
	 	          comment && comment[0] ? comment : "",
	 	          comment && comment[0] ? "'" : "");
	    
	g_free (name);
	g_free (comment);
	g_free (email);
	    
	return ret;
}

gchar*
seahorse_pgp_uid_get_name (SeahorsePGPUid *uid)
{
	g_return_val_if_fail (SEAHORSE_IS_PGP_UID (uid), NULL);
	g_return_val_if_fail (uid->userid, NULL);
	return convert_string (uid->userid->name, FALSE);
}


gchar*
seahorse_pgp_uid_get_email (SeahorsePGPUid *uid)
{
	g_return_val_if_fail (SEAHORSE_IS_PGP_UID (uid), NULL);
	g_return_val_if_fail (uid->userid, NULL);
	return convert_string (uid->userid->email, FALSE);
}

gchar*
seahorse_pgp_uid_get_comment (SeahorsePGPUid *uid)
{
	g_return_val_if_fail (SEAHORSE_IS_PGP_UID (uid), NULL);
	g_return_val_if_fail (uid->userid, NULL);
	return convert_string (uid->userid->comment, FALSE);
}

SeahorseValidity
seahorse_pgp_uid_get_validity (SeahorsePGPUid *uid)
{
	g_return_val_if_fail (SEAHORSE_IS_PGP_UID (uid), SEAHORSE_VALIDITY_UNKNOWN);
	g_return_val_if_fail (uid->userid, SEAHORSE_VALIDITY_UNKNOWN);
	return gpgmex_validity_to_seahorse (uid->userid->validity);
}

guint
seahorse_pgp_uid_get_index (SeahorsePGPUid *uid)
{
	g_return_val_if_fail (SEAHORSE_IS_PGP_UID (uid), 0);
	return uid->index;
}
