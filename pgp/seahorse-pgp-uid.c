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
	PROP_DISPLAY_NAME,
	PROP_MARKUP,
	PROP_SIMPLE_NAME,
	PROP_VALIDITY,
	PROP_VALIDITY_STR,
	PROP_STOCK_ID
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
changed_uid (SeahorsePGPUid *uid)
{
	SeahorseObject *obj = SEAHORSE_OBJECT (uid);
    
	obj->_id = 0;
	obj->_tag = SEAHORSE_PGP;
    
	if (!uid->userid || !uid->pubkey) {
        
		obj->_usage = SEAHORSE_USAGE_NONE;
		obj->_flags = SKEY_FLAG_DISABLED;
        
	} else {
		
		/* The key id */
		if (uid->pubkey->subkeys)
			obj->_id = seahorse_pgp_key_get_cannonical_id (uid->pubkey->subkeys->keyid);
        
		/* The location */
		if (uid->pubkey->keylist_mode & GPGME_KEYLIST_MODE_EXTERN && 
		    obj->_location <= SEAHORSE_LOCATION_REMOTE)
			obj->_location = SEAHORSE_LOCATION_REMOTE;
        
		else if (obj->_location <= SEAHORSE_LOCATION_LOCAL)
			obj->_location = SEAHORSE_LOCATION_LOCAL;
        
		/* The type */
		obj->_usage = SEAHORSE_USAGE_IDENTITY;

		/* The flags */
		obj->_flags = 0;
	}
    
	if (!obj->_id)
		obj->_id = g_quark_from_string (SEAHORSE_PGP_STR ":UNKNOWN UNKNOWN ");
    
	seahorse_object_fire_changed (obj, SEAHORSE_OBJECT_CHANGE_ALL);
}

/* -----------------------------------------------------------------------------
 * OBJECT 
 */

static void
seahorse_pgp_uid_init (SeahorsePGPUid *uid)
{
    
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
	case PROP_DISPLAY_NAME:
		g_value_take_string (value, seahorse_pgp_uid_get_display_name (uid));
		break;
	case PROP_MARKUP:
		g_value_take_string (value, seahorse_pgp_uid_get_markup (uid, 0));
		break;
	case PROP_SIMPLE_NAME:        
		g_value_take_string (value, seahorse_pgp_uid_get_name (uid));
		break;
	case PROP_VALIDITY:
		g_value_set_uint (value, seahorse_pgp_uid_get_validity (uid));
		break;
	case PROP_VALIDITY_STR:
		g_value_set_string (value, seahorse_validity_get_string (seahorse_pgp_uid_get_validity (uid)));
		break;
	case PROP_STOCK_ID:
		g_value_set_string (value, ""); 
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

	g_object_class_install_property (gobject_class, PROP_DISPLAY_NAME,
	        g_param_spec_string ("display-name", "Display Name", "User Displayable name for this uid",
	                             "", G_PARAM_READABLE));

	g_object_class_install_property (gobject_class, PROP_MARKUP,
	        g_param_spec_string ("markup", "Display Markup", "GLib Markup",
	                             "", G_PARAM_READABLE));
                      
	g_object_class_install_property (gobject_class, PROP_SIMPLE_NAME,
	        g_param_spec_string ("simple-name", "Simple Name", "Simple name for this user id",
	                             "", G_PARAM_READABLE));
         
	g_object_class_install_property (gobject_class, PROP_VALIDITY,
	        g_param_spec_uint ("validity", "Validity", "Validity of this identity",
	                           0, G_MAXUINT, 0, G_PARAM_READABLE));

        g_object_class_install_property (gobject_class, PROP_VALIDITY_STR,
                g_param_spec_string ("validity-str", "Validity String", "Validity of this identity as a string",
                                     "", G_PARAM_READABLE));

        g_object_class_install_property (gobject_class, PROP_STOCK_ID,
                g_param_spec_string ("stock-id", "The stock icon", "The stock icon id",
                                     NULL, G_PARAM_READABLE));
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
