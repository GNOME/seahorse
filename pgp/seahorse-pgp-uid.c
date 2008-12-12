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

#include "seahorse-pgp.h"
#include "seahorse-gpgmex.h"
#include "seahorse-pgp-key.h"
#include "seahorse-pgp-uid.h"

#include <string.h>

#include <glib/gi18n.h>

enum {
	PROP_0,
	PROP_PUBKEY,
	PROP_USERID,
	PROP_GPGME_INDEX,
	PROP_ACTUAL_INDEX,
	PROP_VALIDITY,
	PROP_VALIDITY_STR,
	PROP_NAME,
	PROP_EMAIL,
	PROP_COMMENT
};

G_DEFINE_TYPE (SeahorsePgpUid, seahorse_pgp_uid, SEAHORSE_TYPE_OBJECT);

struct _SeahorsePgpUidPrivate {
	gpgme_key_t pubkey;         /* The public key that this uid is part of */
	gpgme_user_id_t userid;     /* The userid referred to */
	guint gpgme_index;          /* The GPGME index of the UID */
	gint actual_index;          /* The actual index of this UID */
};

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
changed_uid (SeahorsePgpUid *self)
{
	SeahorseObject *obj = SEAHORSE_OBJECT (self);
	SeahorseLocation loc;
	gchar *name, *markup;
	
	g_return_if_fail (self->pv->pubkey);
	
	if (!self->pv->userid) {
        
		g_object_set (self,
		              "label", "",
		              "usage", SEAHORSE_USAGE_NONE,
		              "markup", "",
		              "location", SEAHORSE_LOCATION_INVALID,
		              "flags", SEAHORSE_FLAG_DISABLED,
		              NULL);
		return;
		
	} 
		
	/* The location */
	loc = seahorse_object_get_location (obj);
	if (self->pv->pubkey->keylist_mode & GPGME_KEYLIST_MODE_EXTERN && 
	    loc <= SEAHORSE_LOCATION_REMOTE)
		loc = SEAHORSE_LOCATION_REMOTE;

	else if (loc <= SEAHORSE_LOCATION_LOCAL)
		loc = SEAHORSE_LOCATION_LOCAL;

	name = seahorse_pgp_uid_calc_label (self->pv->userid);
	markup = seahorse_pgp_uid_calc_markup (self->pv->userid, 0);
	
	g_object_set (self,
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
seahorse_pgp_uid_init (SeahorsePgpUid *self)
{
	self->pv = G_TYPE_INSTANCE_GET_PRIVATE (self, SEAHORSE_TYPE_PGP_UID, SeahorsePgpUidPrivate);
	self->pv->gpgme_index = 0;
	self->pv->actual_index = -1;
	g_object_set (self, "icon", "", NULL);
}

static GObject*
seahorse_pgp_uid_constructor (GType type, guint n_props, GObjectConstructParam *props)
{
	GObject *obj = G_OBJECT_CLASS (seahorse_pgp_uid_parent_class)->constructor (type, n_props, props);
	SeahorsePgpUid *self = NULL;
	
	if (obj) {
		self = SEAHORSE_PGP_UID (obj);
		g_return_val_if_fail (self->pv->pubkey, NULL);
	}
	
	return obj;
}

static void
seahorse_pgp_uid_get_property (GObject *object, guint prop_id,
                               GValue *value, GParamSpec *pspec)
{
	SeahorsePgpUid *self = SEAHORSE_PGP_UID (object);
	
	switch (prop_id) {
	case PROP_PUBKEY:
		g_value_set_boxed (value, seahorse_pgp_uid_get_pubkey (self));
		break;
	case PROP_USERID:
		g_value_set_pointer (value, seahorse_pgp_uid_get_userid (self));
		break;
	case PROP_GPGME_INDEX:
		g_value_set_uint (value, seahorse_pgp_uid_get_gpgme_index (self));
		break;
	case PROP_ACTUAL_INDEX:
		g_value_set_uint (value, seahorse_pgp_uid_get_actual_index (self));
		break;
	case PROP_VALIDITY:
		g_value_set_uint (value, seahorse_pgp_uid_get_validity (self));
		break;
	case PROP_VALIDITY_STR:
		g_value_set_string (value, seahorse_pgp_uid_get_validity_str (self));
		break;
	case PROP_NAME:
		g_value_take_string (value, seahorse_pgp_uid_get_name (self));
		break;
	case PROP_EMAIL:
		g_value_take_string (value, seahorse_pgp_uid_get_email (self));
		break;
	case PROP_COMMENT:
		g_value_take_string (value, seahorse_pgp_uid_get_comment (self));
		break;
	}
}

static void
seahorse_pgp_uid_set_property (GObject *object, guint prop_id, const GValue *value, 
                               GParamSpec *pspec)
{
	SeahorsePgpUid *self = SEAHORSE_PGP_UID (object);

	switch (prop_id) {
	case PROP_PUBKEY:
		g_return_if_fail (!self->pv->pubkey);
		self->pv->pubkey = g_value_get_boxed (value);
		if (self->pv->pubkey)
			gpgmex_key_ref (self->pv->pubkey);
		break;
	case PROP_ACTUAL_INDEX:
		seahorse_pgp_uid_set_actual_index (self, g_value_get_uint (value));
		break;
	case PROP_USERID:
		seahorse_pgp_uid_set_userid (self, g_value_get_pointer (value));
		break;
	}
}

static void
seahorse_pgp_uid_object_finalize (GObject *gobject)
{
	SeahorsePgpUid *self = SEAHORSE_PGP_UID (gobject);

	/* Unref the key */
	if (self->pv->pubkey)
		gpgmex_key_unref (self->pv->pubkey);
	self->pv->pubkey = NULL;
	self->pv->userid = NULL;
    
	G_OBJECT_CLASS (seahorse_pgp_uid_parent_class)->finalize (gobject);
}

static void
seahorse_pgp_uid_class_init (SeahorsePgpUidClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);    

	seahorse_pgp_uid_parent_class = g_type_class_peek_parent (klass);
	g_type_class_add_private (klass, sizeof (SeahorsePgpUidPrivate));

	gobject_class->constructor = seahorse_pgp_uid_constructor;
	gobject_class->finalize = seahorse_pgp_uid_object_finalize;
	gobject_class->set_property = seahorse_pgp_uid_set_property;
	gobject_class->get_property = seahorse_pgp_uid_get_property;
    
	g_object_class_install_property (gobject_class, PROP_PUBKEY,
	        g_param_spec_boxed ("pubkey", "Public Key", "GPGME Public Key that this uid is on",
	                            SEAHORSE_PGP_BOXED_KEY, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property (gobject_class, PROP_USERID,
	        g_param_spec_pointer ("userid", "User ID", "GPGME User ID",
	                              G_PARAM_READWRITE));
                      
	g_object_class_install_property (gobject_class, PROP_GPGME_INDEX,
	        g_param_spec_uint ("gpgme-index", "GPGME Index", "GPGME User ID Index",
	                           0, G_MAXUINT, 0, G_PARAM_READWRITE));
	
	g_object_class_install_property (gobject_class, PROP_ACTUAL_INDEX,
	        g_param_spec_uint ("actual-index", "Actual Index", "Actual GPG Index",
	                           0, G_MAXUINT, 0, G_PARAM_READWRITE));
         
	g_object_class_install_property (gobject_class, PROP_VALIDITY,
	        g_param_spec_uint ("validity", "Validity", "Validity of this identity",
	                           0, G_MAXUINT, 0, G_PARAM_READABLE));

        g_object_class_install_property (gobject_class, PROP_VALIDITY_STR,
                g_param_spec_string ("validity-str", "Validity String", "Validity of this identity as a string",
                                     "", G_PARAM_READABLE));
        
        g_object_class_install_property (gobject_class, PROP_NAME,
                g_param_spec_string ("name", "Name", "User ID name",
                                     "", G_PARAM_READABLE));

        g_object_class_install_property (gobject_class, PROP_EMAIL,
                g_param_spec_string ("email", "Email", "User ID email",
                                     "", G_PARAM_READABLE));

        g_object_class_install_property (gobject_class, PROP_COMMENT,
                g_param_spec_string ("comment", "Comment", "User ID comment",
                                     "", G_PARAM_READABLE));
}

/* -----------------------------------------------------------------------------
 * PUBLIC 
 */

SeahorsePgpUid* 
seahorse_pgp_uid_new (gpgme_key_t pubkey, gpgme_user_id_t userid) 
{
	return g_object_new (SEAHORSE_TYPE_PGP_UID, 
	                     "pubkey", pubkey, 
	                     "userid", userid, NULL);
}


gpgme_key_t
seahorse_pgp_uid_get_pubkey (SeahorsePgpUid *self)
{
	g_return_val_if_fail (SEAHORSE_IS_PGP_UID (self), NULL);
	g_return_val_if_fail (self->pv->pubkey, NULL);
	return self->pv->pubkey;
}

gpgme_user_id_t
seahorse_pgp_uid_get_userid (SeahorsePgpUid *self)
{
	g_return_val_if_fail (SEAHORSE_IS_PGP_UID (self), NULL);
	g_return_val_if_fail (self->pv->userid, NULL);
	return self->pv->userid;
}

void
seahorse_pgp_uid_set_userid (SeahorsePgpUid *self, gpgme_user_id_t userid)
{
	GObject *obj;
	gpgme_user_id_t uid;
	gint index, i;
	
	g_return_if_fail (SEAHORSE_IS_PGP_UID (self));
	g_return_if_fail (userid);
	
	/* Make sure that this userid is in the pubkey */
	index = -1;
	for (i = 0, uid = self->pv->pubkey->uids; uid; ++i, uid = uid->next) {
		if(userid == uid) {
			index = i;
			break;
		}
	}
	
	g_return_if_fail (index >= 0);
	
	self->pv->userid = userid;
	self->pv->gpgme_index = index;
	
	obj = G_OBJECT (self);
	g_object_freeze_notify (obj);
	changed_uid (self);
	g_object_notify (obj, "userid");
	g_object_notify (obj, "gpgme_index");
	g_object_notify (obj, "name");
	g_object_notify (obj, "email");
	g_object_notify (obj, "comment");
	g_object_notify (obj, "validity");
	g_object_notify (obj, "validity-str");
	g_object_thaw_notify (obj);
}

guint
seahorse_pgp_uid_get_gpgme_index (SeahorsePgpUid *self)
{
	g_return_val_if_fail (SEAHORSE_IS_PGP_UID (self), 0);
	return self->pv->gpgme_index;
}

guint
seahorse_pgp_uid_get_actual_index (SeahorsePgpUid *self)
{
	g_return_val_if_fail (SEAHORSE_IS_PGP_UID (self), 0);
	if(self->pv->actual_index < 0)
		return self->pv->gpgme_index;
	return self->pv->actual_index;
}

void
seahorse_pgp_uid_set_actual_index (SeahorsePgpUid *self, guint actual_index)
{
	g_return_if_fail (SEAHORSE_IS_PGP_UID (self));
	self->pv->actual_index = actual_index;
	g_object_notify (G_OBJECT (self), "actual-index");
}

SeahorseValidity
seahorse_pgp_uid_get_validity (SeahorsePgpUid *self)
{
	g_return_val_if_fail (SEAHORSE_IS_PGP_UID (self), SEAHORSE_VALIDITY_UNKNOWN);
	g_return_val_if_fail (self->pv->userid, SEAHORSE_VALIDITY_UNKNOWN);
	return gpgmex_validity_to_seahorse (self->pv->userid->validity);
}

const gchar*
seahorse_pgp_uid_get_validity_str (SeahorsePgpUid *self)
{
	return seahorse_validity_get_string (seahorse_pgp_uid_get_validity (self));
}

gchar*
seahorse_pgp_uid_get_name (SeahorsePgpUid *self)
{
	g_return_val_if_fail (SEAHORSE_IS_PGP_UID (self), NULL);
	g_return_val_if_fail (self->pv->userid, NULL);
	return convert_string (self->pv->userid->name, FALSE);
}

gchar*
seahorse_pgp_uid_get_email (SeahorsePgpUid *self)
{
	g_return_val_if_fail (SEAHORSE_IS_PGP_UID (self), NULL);
	g_return_val_if_fail (self->pv->userid, NULL);
	return convert_string (self->pv->userid->email, FALSE);
}

gchar*
seahorse_pgp_uid_get_comment (SeahorsePgpUid *self)
{
	g_return_val_if_fail (SEAHORSE_IS_PGP_UID (self), NULL);
	g_return_val_if_fail (self->pv->userid, NULL);
	return convert_string (self->pv->userid->comment, FALSE);
}

gchar*
seahorse_pgp_uid_calc_label (gpgme_user_id_t userid)
{
	g_return_val_if_fail (userid, NULL);
	return convert_string (userid->uid, FALSE);
}

gchar*
seahorse_pgp_uid_calc_name (gpgme_user_id_t userid)
{
	g_return_val_if_fail (userid, NULL);
	return convert_string (userid->name, FALSE);
}

gchar*
seahorse_pgp_uid_calc_markup (gpgme_user_id_t userid, guint flags)
{
	gchar *email, *name, *comment, *ret;
	const gchar *format;
	gboolean strike = FALSE;

	g_return_val_if_fail (userid, NULL);
	
	name = convert_string (userid->name, TRUE);
	email = convert_string (userid->email, TRUE);
	comment = convert_string (userid->comment, TRUE);

	if (userid->revoked || flags & CRYPTUI_FLAG_EXPIRED || 
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

void
seahorse_pgp_uid_signature_get_text (gpgme_key_sig_t signature,
                                     gchar **name, gchar **email, gchar **comment)
{
	g_return_if_fail (signature != NULL);
    
	if (name)
		*name = signature->name ? convert_string (signature->name, FALSE) : NULL;
	if (email)
		*email = signature->email ? convert_string (signature->email, FALSE) : NULL;
	if (comment)
		*comment = signature->comment ? convert_string (signature->comment, FALSE) : NULL;
}

guint         
seahorse_pgp_uid_signature_get_type (gpgme_key_sig_t signature)
{
	SeahorseObject *sobj;
	GQuark id;

	g_return_val_if_fail (signature != NULL, 0);
    
	id = seahorse_pgp_key_get_cannonical_id (signature->keyid);
	sobj = seahorse_context_find_object (SCTX_APP (), id, SEAHORSE_LOCATION_LOCAL);
    
	if (sobj) {
		if (seahorse_object_get_usage (sobj) == SEAHORSE_USAGE_PRIVATE_KEY) 
			return SKEY_PGPSIG_TRUSTED | SKEY_PGPSIG_PERSONAL;
		if (seahorse_object_get_flags (sobj) & SEAHORSE_FLAG_TRUSTED)
			return SKEY_PGPSIG_TRUSTED;
	}

	return 0;
}
