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
#include "seahorse-pgp-key.h"
#include "seahorse-pgp-uid.h"
#include "seahorse-pgp-signature.h"

#include "seahorse-object-list.h"

#include <string.h>

#include <glib/gi18n.h>

enum {
	PROP_0,
	PROP_SIGNATURES,
	PROP_VALIDITY,
	PROP_VALIDITY_STR,
	PROP_NAME,
	PROP_EMAIL,
	PROP_COMMENT
};

G_DEFINE_TYPE (SeahorsePgpUid, seahorse_pgp_uid, SEAHORSE_TYPE_OBJECT);

struct _SeahorsePgpUidPrivate {
	GList *signatures;
	SeahorseValidity validity;
	gboolean realized;
	gchar *name;
	gchar *email;
	gchar *comment;
};

/* -----------------------------------------------------------------------------
 * INTERNAL HELPERS
 */

static gchar*
convert_string (const gchar *str)
{
	if (!str)
  	      return NULL;
    
	/* If not utf8 valid, assume latin 1 */
 	if (!g_utf8_validate (str, -1, NULL))
 		return g_convert (str, -1, "UTF-8", "ISO-8859-1", NULL, NULL, NULL);

	return g_strdup (str);
}

#ifndef HAVE_STRSEP
/* code taken from glibc-2.2.1/sysdeps/generic/strsep.c */
char *
strsep (char **stringp, const char *delim)
{
    char *begin, *end;

    begin = *stringp;
    if (begin == NULL)
        return NULL;

      /* A frequent case is when the delimiter string contains only one
         character.  Here we don't need to call the expensive `strpbrk'
         function and instead work using `strchr'.  */
      if (delim[0] == '\0' || delim[1] == '\0') {
        char ch = delim[0];

        if (ch == '\0')
            end = NULL; 
        else {
            if (*begin == ch)
                end = begin;
            else if (*begin == '\0')
                end = NULL;
            else
                end = strchr (begin + 1, ch);
        }
    } else
        /* Find the end of the token.  */
        end = strpbrk (begin, delim);

    if (end) {
      /* Terminate the token and set *STRINGP past NUL character.  */
      *end++ = '\0';
      *stringp = end;
    } else
        /* No more delimiters; this is the last token.  */
        *stringp = NULL;

    return begin;
}
#endif /*HAVE_STRSEP*/

/* Copied from GPGME */
static void
parse_user_id (const gchar *uid, gchar **name, gchar **email, gchar **comment)
{
    gchar *src, *tail, *x;
    int in_name = 0;
    int in_email = 0;
    int in_comment = 0;

    x = tail = src = g_strdup (uid);
    
    while (*src) {
        if (in_email) {
	        if (*src == '<')
	            /* Not legal but anyway.  */
	            in_email++;
	        else if (*src == '>') {
	            if (!--in_email && !*email) {
		            *email = tail;
                    *src = 0;
                    tail = src + 1;
		        }
	        }
	    } else if (in_comment) {
	        if (*src == '(')
	            in_comment++;
	        else if (*src == ')') {
	            if (!--in_comment && !*comment) {
		            *comment = tail;
                    *src = 0;
                    tail = src + 1;
		        }
	        }
	    } else if (*src == '<') {
	        if (in_name) {
	            if (!*name) {
		            *name = tail;
                    *src = 0;
                    tail = src + 1;
		        }
	            in_name = 0;
	        } else
	            tail = src + 1;
	            
	        in_email = 1;
	    } else if (*src == '(') {
	        if (in_name) {
	            if (!*name) {
		            *name = tail;
                    *src = 0;
                    tail = src + 1;
		        }
	            in_name = 0;
	        }
	        in_comment = 1;
	    } else if (!in_name && *src != ' ' && *src != '\t') {
	        in_name = 1;
	    }    
        src++;
    }
 
    if (in_name) {
        if (!*name) {
	        *name = tail;
            *src = 0;
            tail = src + 1;
	    }
    }
 
    /* Let unused parts point to an EOS.  */
    *name = g_strdup (*name ? *name : "");
    *email = g_strdup (*email ? *email : "");
    *comment = g_strdup (*comment ? *comment : "");
    
    g_strstrip (*name);
    g_strstrip (*email);
    g_strstrip (*comment);
    
    g_free (x);
}


/* -----------------------------------------------------------------------------
 * OBJECT 
 */

static void
seahorse_pgp_uid_realize (SeahorseObject *obj)
{
	SeahorsePgpUid *self = SEAHORSE_PGP_UID (obj);
	gchar *markup;

	/* Don't realize if no name present */
	if (!self->pv->name)
		return;

	self->pv->realized = TRUE;
	SEAHORSE_OBJECT_CLASS (seahorse_pgp_uid_parent_class)->realize (obj);

	g_object_set (self, "label", self->pv->name ? self->pv->name : "", NULL);
	markup = seahorse_pgp_uid_calc_markup (self->pv->name, self->pv->email, self->pv->comment, 0);
	g_object_set (self, "markup", markup, NULL);
	g_free (markup);
}

static void
seahorse_pgp_uid_init (SeahorsePgpUid *self)
{
	self->pv = G_TYPE_INSTANCE_GET_PRIVATE (self, SEAHORSE_TYPE_PGP_UID, SeahorsePgpUidPrivate);
	g_object_set (self, "icon", "", "usage", SEAHORSE_USAGE_IDENTITY, NULL);
	g_object_set (self, "tag", SEAHORSE_PGP_TYPE, NULL);
}

static void
seahorse_pgp_uid_get_property (GObject *object, guint prop_id,
                               GValue *value, GParamSpec *pspec)
{
	SeahorsePgpUid *self = SEAHORSE_PGP_UID (object);
	
	switch (prop_id) {
	case PROP_SIGNATURES:
		g_value_set_boxed (value, seahorse_pgp_uid_get_signatures (self));
		break;
	case PROP_VALIDITY:
		g_value_set_uint (value, seahorse_pgp_uid_get_validity (self));
		break;
	case PROP_VALIDITY_STR:
		g_value_set_string (value, seahorse_pgp_uid_get_validity_str (self));
		break;
	case PROP_NAME:
		g_value_set_string (value, seahorse_pgp_uid_get_name (self));
		break;
	case PROP_EMAIL:
		g_value_set_string (value, seahorse_pgp_uid_get_email (self));
		break;
	case PROP_COMMENT:
		g_value_set_string (value, seahorse_pgp_uid_get_comment (self));
		break;
	}
}

static void
seahorse_pgp_uid_set_property (GObject *object, guint prop_id, const GValue *value, 
                               GParamSpec *pspec)
{
	SeahorsePgpUid *self = SEAHORSE_PGP_UID (object);

	switch (prop_id) {
	case PROP_SIGNATURES:
		seahorse_pgp_uid_set_signatures (self, g_value_get_boxed (value));
		break;
	case PROP_VALIDITY:
		seahorse_pgp_uid_set_validity (self, g_value_get_uint (value));
		break;
	case PROP_NAME:
		seahorse_pgp_uid_set_name (self, g_value_get_string (value));
		break;
	case PROP_EMAIL:
		seahorse_pgp_uid_set_email (self, g_value_get_string (value));
		break;
	case PROP_COMMENT:
		seahorse_pgp_uid_set_comment (self, g_value_get_string (value));
		break;
	}
}

static void
seahorse_pgp_uid_object_finalize (GObject *gobject)
{
	SeahorsePgpUid *self = SEAHORSE_PGP_UID (gobject);

	seahorse_object_list_free (self->pv->signatures);
	self->pv->signatures = NULL;
	
	g_free (self->pv->name);
	self->pv->name = NULL;
	
	g_free (self->pv->email);
	self->pv->email = NULL;
	
	g_free (self->pv->comment);
	self->pv->comment = NULL;
    
	G_OBJECT_CLASS (seahorse_pgp_uid_parent_class)->finalize (gobject);
}

static void
seahorse_pgp_uid_class_init (SeahorsePgpUidClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);    

	seahorse_pgp_uid_parent_class = g_type_class_peek_parent (klass);
	g_type_class_add_private (klass, sizeof (SeahorsePgpUidPrivate));

	gobject_class->finalize = seahorse_pgp_uid_object_finalize;
	gobject_class->set_property = seahorse_pgp_uid_set_property;
	gobject_class->get_property = seahorse_pgp_uid_get_property;
    
	SEAHORSE_OBJECT_CLASS (klass)->realize = seahorse_pgp_uid_realize;
	
	g_object_class_install_property (gobject_class, PROP_VALIDITY,
	        g_param_spec_uint ("validity", "Validity", "Validity of this identity",
	                           0, G_MAXUINT, 0, G_PARAM_READWRITE));

        g_object_class_install_property (gobject_class, PROP_VALIDITY_STR,
                g_param_spec_string ("validity-str", "Validity String", "Validity of this identity as a string",
                                     "", G_PARAM_READABLE));
        
        g_object_class_install_property (gobject_class, PROP_NAME,
                g_param_spec_string ("name", "Name", "User ID name",
                                     "", G_PARAM_READWRITE));

        g_object_class_install_property (gobject_class, PROP_EMAIL,
                g_param_spec_string ("email", "Email", "User ID email",
                                     "", G_PARAM_READWRITE));

        g_object_class_install_property (gobject_class, PROP_COMMENT,
                g_param_spec_string ("comment", "Comment", "User ID comment",
                                     "", G_PARAM_READWRITE));
        
        g_object_class_install_property (gobject_class, PROP_SIGNATURES,
                g_param_spec_boxed ("signatures", "Signatures", "Signatures on this UID",
                                    SEAHORSE_BOXED_OBJECT_LIST, G_PARAM_READWRITE));
}

/* -----------------------------------------------------------------------------
 * PUBLIC 
 */

SeahorsePgpUid*
seahorse_pgp_uid_new (const gchar *uid_string)
{
	SeahorsePgpUid *uid;
	gchar *name = NULL;
	gchar *email = NULL;
	gchar *comment = NULL;
	
	if (uid_string)
		parse_user_id (uid_string, &name, &email, &comment);
	
	uid = g_object_new (SEAHORSE_TYPE_PGP_UID, "name", name, "email", email, "comment", comment, NULL);
	
	g_free (name);
	g_free (comment);
	g_free (email);
	
	return uid;
}

GList*
seahorse_pgp_uid_get_signatures (SeahorsePgpUid *self)
{
	g_return_val_if_fail (SEAHORSE_IS_PGP_UID (self), NULL);
	return self->pv->signatures;
}

void
seahorse_pgp_uid_set_signatures (SeahorsePgpUid *self, GList *signatures)
{
	g_return_if_fail (SEAHORSE_IS_PGP_UID (self));
	
	seahorse_object_list_free (self->pv->signatures);
	self->pv->signatures = seahorse_object_list_copy (signatures);
	
	g_object_notify (G_OBJECT (self), "signatures");	
}

SeahorseValidity
seahorse_pgp_uid_get_validity (SeahorsePgpUid *self)
{
	g_return_val_if_fail (SEAHORSE_IS_PGP_UID (self), SEAHORSE_VALIDITY_UNKNOWN);
	return self->pv->validity;
}

void
seahorse_pgp_uid_set_validity (SeahorsePgpUid *self, SeahorseValidity validity)
{
	g_return_if_fail (SEAHORSE_IS_PGP_UID (self));
	self->pv->validity = validity;
	g_object_notify (G_OBJECT (self), "validity");
	g_object_notify (G_OBJECT (self), "validity-str");
}

const gchar*
seahorse_pgp_uid_get_validity_str (SeahorsePgpUid *self)
{
	return seahorse_validity_get_string (seahorse_pgp_uid_get_validity (self));
}

const gchar*
seahorse_pgp_uid_get_name (SeahorsePgpUid *self)
{
	g_return_val_if_fail (SEAHORSE_IS_PGP_UID (self), NULL);
	if (!self->pv->name)
		self->pv->name = g_strdup ("");
	return self->pv->name;
}

void
seahorse_pgp_uid_set_name (SeahorsePgpUid *self, const gchar *name)
{
	GObject *obj;
	
	g_return_if_fail (SEAHORSE_IS_PGP_UID (self));
	
	g_free (self->pv->name);
	self->pv->name = convert_string (name);
	
	obj = G_OBJECT (self);
	g_object_freeze_notify (obj);
	if (self->pv->realized)
		seahorse_pgp_uid_realize (SEAHORSE_OBJECT (self));
	g_object_notify (obj, "name");
	g_object_thaw_notify (obj);
}

const gchar*
seahorse_pgp_uid_get_email (SeahorsePgpUid *self)
{
	g_return_val_if_fail (SEAHORSE_IS_PGP_UID (self), NULL);
	if (!self->pv->email)
		self->pv->email = g_strdup ("");
	return self->pv->email;
}

void
seahorse_pgp_uid_set_email (SeahorsePgpUid *self, const gchar *email)
{
	GObject *obj;
	
	g_return_if_fail (SEAHORSE_IS_PGP_UID (self));
	
	g_free (self->pv->email);
	self->pv->email = convert_string (email);
	
	obj = G_OBJECT (self);
	g_object_freeze_notify (obj);
	if (self->pv->realized)
		seahorse_pgp_uid_realize (SEAHORSE_OBJECT (self));
	g_object_notify (obj, "email");
	g_object_thaw_notify (obj);
}

const gchar*
seahorse_pgp_uid_get_comment (SeahorsePgpUid *self)
{
	g_return_val_if_fail (SEAHORSE_IS_PGP_UID (self), NULL);
	if (!self->pv->comment)
		self->pv->comment = g_strdup ("");
	return self->pv->comment;
}

void
seahorse_pgp_uid_set_comment (SeahorsePgpUid *self, const gchar *comment)
{
	GObject *obj;
	
	g_return_if_fail (SEAHORSE_IS_PGP_UID (self));
	
	g_free (self->pv->comment);
	self->pv->comment = convert_string (comment);
	
	obj = G_OBJECT (self);
	g_object_freeze_notify (obj);
	if (self->pv->realized)
		seahorse_pgp_uid_realize (SEAHORSE_OBJECT (self));
	g_object_notify (obj, "comment");
	g_object_thaw_notify (obj);
}

gchar*
seahorse_pgp_uid_calc_label (const gchar *name, const gchar *email, 
                             const gchar *comment)
{
	GString *string;

	g_return_val_if_fail (name, NULL);

	string = g_string_new ("");
	g_string_append (string, name);
	
	if (email && email[0]) {
		g_string_append (string, " <");
		g_string_append (string, email);
		g_string_append (string, ">");
	}
	
	if (comment && comment[0]) {
		g_string_append (string, " (");
		g_string_append (string, comment);
		g_string_append (string, ")");
	}
	
	return g_string_free (string, FALSE);
}

gchar*
seahorse_pgp_uid_calc_markup (const gchar *name, const gchar *email, 
                              const gchar *comment, guint flags)
{
	const gchar *format;
	gboolean strike = FALSE;
	gboolean grayed = FALSE;

	g_return_val_if_fail (name, NULL);
	
	if (flags & SEAHORSE_FLAG_EXPIRED || flags & SEAHORSE_FLAG_REVOKED || 
	    flags & SEAHORSE_FLAG_DISABLED)
		strike = TRUE;
	if (!(flags & SEAHORSE_FLAG_TRUSTED))
		grayed = TRUE;
	    
	if (strike && grayed)
		format = "<span strikethrough='true' foreground='#555555'>%s<span size='small' rise='0'>%s%s%s%s%s</span></span>";
	else if (grayed)
		format = "<span foreground='#555555'>%s<span size='small' rise='0'>%s%s%s%s%s</span></span>";
	else if (strike)
		format = "<span strikethrough='true'>%s<span foreground='#555555' size='small' rise='0'>%s%s%s%s%s</span></span>";
	else
		format = "%s<span foreground='#555555' size='small' rise='0'>%s%s%s%s%s</span>";
		
	return g_markup_printf_escaped (format, name,
	 	          email && email[0] ? "  " : "",
	 	          email && email[0] ? email : "",
	 	          comment && comment[0] ? "  '" : "",
	 	          comment && comment[0] ? comment : "",
	 	          comment && comment[0] ? "'" : "");
}

GQuark
seahorse_pgp_uid_calc_id (GQuark key_id, guint index)
{
	gchar *str = NULL;
	GQuark id = 0;
	
	str = g_strdup_printf ("%s:%u", g_quark_to_string (key_id), index + 1);
	id = g_quark_from_string (str);
	g_free (str);

	return id;
}
