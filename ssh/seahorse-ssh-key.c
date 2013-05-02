/*
 * Seahorse
 *
 * Copyright (C) 2005 Stefan Walter
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

#include "seahorse-ssh-actions.h"
#include "seahorse-ssh-deleter.h"
#include "seahorse-ssh-dialogs.h"
#include "seahorse-ssh-exporter.h"
#include "seahorse-ssh-key.h"
#include "seahorse-ssh-operation.h"
#include "seahorse-ssh-source.h"

#include "seahorse-common.h"
#include "seahorse-icons.h"
#include "seahorse-validity.h"

#include <gcr/gcr.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>

#include <errno.h>
#include <string.h>

enum {
    PROP_0,
    PROP_KEY_DATA,
    PROP_FINGERPRINT,
    PROP_DESCRIPTION,
    PROP_VALIDITY,
    PROP_TRUST,
    PROP_EXPIRES,
    PROP_LENGTH
};

static void       seahorse_ssh_key_deletable_iface       (SeahorseDeletableIface *iface);

static void       seahorse_ssh_key_exportable_iface      (SeahorseExportableIface *iface);

static void       seahorse_ssh_key_viewable_iface        (SeahorseViewableIface *iface);

G_DEFINE_TYPE_WITH_CODE (SeahorseSSHKey, seahorse_ssh_key, SEAHORSE_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (SEAHORSE_TYPE_EXPORTABLE, seahorse_ssh_key_exportable_iface);
                         G_IMPLEMENT_INTERFACE (SEAHORSE_TYPE_DELETABLE, seahorse_ssh_key_deletable_iface);
                         G_IMPLEMENT_INTERFACE (SEAHORSE_TYPE_VIEWABLE, seahorse_ssh_key_viewable_iface);
);

/* -----------------------------------------------------------------------------
 * INTERNAL 
 */

static gchar*
parse_first_word (const gchar *line) 
{
    int pos;
    
    #define PARSE_CHARS "\t \n@;,.\\?()[]{}+/"
    line += strspn (line, PARSE_CHARS);
    pos = strcspn (line, PARSE_CHARS);
    return pos == 0 ? NULL : g_strndup (line, pos);
}

static void
changed_key (SeahorseSSHKey *self)
{
	SeahorseObject *obj = SEAHORSE_OBJECT (self);
	SeahorseUsage usage;
	SeahorseFlags flags;
	const gchar *display = NULL;
	gchar *identifier;
	gchar *simple = NULL;
	GtkActionGroup *actions;
	GIcon *icon;
	gchar *filename;
	gchar *markup;

	if (self->keydata) {

  		 /* Try to make display and simple names */
		if (self->keydata->comment) {
			display = self->keydata->comment;
			simple = parse_first_word (self->keydata->comment);
            
		/* No names when not even the fingerpint loaded */
		} else if (!self->keydata->fingerprint) {
			display = _("(Unreadable Secure Shell Key)");

		/* No comment, but loaded */        
		} else {
			display = _("Secure Shell Key");
		}
    
		if (simple == NULL)
			simple = g_strdup (_("Secure Shell Key"));
	}
    
	if (!self->keydata || !self->keydata->fingerprint) {
		g_object_set (self,
		              "label", "",
		              "icon", NULL,
		              "usage", SEAHORSE_USAGE_NONE,
		              "nickname", "",
		              "object-flags", SEAHORSE_FLAG_DISABLED,
		              NULL);
		return;
	} 

	flags = SEAHORSE_FLAG_EXPORTABLE | SEAHORSE_FLAG_DELETABLE;

	if (self->keydata->privfile) {
		usage = SEAHORSE_USAGE_PRIVATE_KEY;
		flags |= SEAHORSE_FLAG_PERSONAL | SEAHORSE_FLAG_TRUSTED;
		icon = g_themed_icon_new (GCR_ICON_KEY_PAIR);
	} else {
		flags = 0;
		usage = SEAHORSE_USAGE_PUBLIC_KEY;
		icon = g_themed_icon_new (GCR_ICON_KEY);
	}

	if (self->keydata->privfile)
		filename = g_path_get_basename (self->keydata->privfile);
	else
		filename = g_path_get_basename (self->keydata->pubfile);

	markup = g_markup_printf_escaped ("%s<span size='small' rise='0' foreground='#555555'>\n%s</span>",
	                                  display, filename);

	identifier = seahorse_ssh_key_calc_identifier (self->keydata->fingerprint);
	actions = seahorse_ssh_actions_instance ();

	if (self->keydata->authorized)
		flags |= SEAHORSE_FLAG_TRUSTED;

	g_object_set (obj,
	              "actions", actions,
	              "markup", markup,
	              "label", display,
	              "icon", icon,
	              "usage", usage,
	              "nickname", simple,
	              "identifier", identifier,
	              "object-flags", flags,
	              NULL);

	g_object_unref (actions);
	g_object_unref (icon);
	g_free (identifier);
	g_free (markup);
}

static guint 
calc_validity (SeahorseSSHKey *skey)
{
	if (skey->keydata->privfile)
		return SEAHORSE_VALIDITY_ULTIMATE;
	return 0;
}

static guint
calc_trust (SeahorseSSHKey *skey)
{
	if (skey->keydata->authorized)
		return SEAHORSE_VALIDITY_FULL;
	return 0;
}

/* -----------------------------------------------------------------------------
 * OBJECT 
 */

void
seahorse_ssh_key_refresh (SeahorseSSHKey *self)
{
	/* TODO: Need to work on key refreshing */
}

static void
seahorse_ssh_key_get_property (GObject *object, guint prop_id,
                               GValue *value, GParamSpec *pspec)
{
    SeahorseSSHKey *skey = SEAHORSE_SSH_KEY (object);
    SeahorseUsage usage;

    switch (prop_id) {
    case PROP_KEY_DATA:
        g_value_set_pointer (value, skey->keydata);
        break;
    case PROP_FINGERPRINT:
        g_value_set_string (value, skey->keydata ? skey->keydata->fingerprint : NULL);
        break;
    case PROP_DESCRIPTION:
        g_object_get (skey, "usage", &usage, NULL);
        if (usage == SEAHORSE_USAGE_PRIVATE_KEY)
            g_value_set_string (value, _("Personal SSH key"));
        else
            g_value_set_string (value, _("SSH key"));
        break;
    case PROP_VALIDITY:
        g_value_set_uint (value, calc_validity (skey));
        break;
    case PROP_TRUST:
        g_value_set_uint (value, calc_trust (skey));
        break;
    case PROP_EXPIRES:
        g_value_set_ulong (value, 0);
        break;
    case PROP_LENGTH:
        g_value_set_uint (value, skey->keydata ? skey->keydata->length : 0);
        break;
    }
}

static void
seahorse_ssh_key_set_property (GObject *object, guint prop_id, 
                               const GValue *value, GParamSpec *pspec)
{
    SeahorseSSHKey *skey = SEAHORSE_SSH_KEY (object);
    SeahorseSSHKeyData *keydata;
    
    switch (prop_id) {
    case PROP_KEY_DATA:
        keydata = (SeahorseSSHKeyData*)g_value_get_pointer (value);
        if (skey->keydata != keydata) {
            seahorse_ssh_key_data_free (skey->keydata);
            skey->keydata = keydata;
        }
        changed_key (skey);
        break;
    }
}

/* Unrefs gpgme key and frees data */
static void
seahorse_ssh_key_finalize (GObject *gobject)
{
    SeahorseSSHKey *skey = SEAHORSE_SSH_KEY (gobject);
    
    seahorse_ssh_key_data_free (skey->keydata);
    
    G_OBJECT_CLASS (seahorse_ssh_key_parent_class)->finalize (gobject);
}

static void
seahorse_ssh_key_init (SeahorseSSHKey *self)
{
	
}

static void
seahorse_ssh_key_class_init (SeahorseSSHKeyClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

    seahorse_ssh_key_parent_class = g_type_class_peek_parent (klass);
    
    gobject_class->finalize = seahorse_ssh_key_finalize;
    gobject_class->set_property = seahorse_ssh_key_set_property;
    gobject_class->get_property = seahorse_ssh_key_get_property;

    g_object_class_install_property (gobject_class, PROP_KEY_DATA,
        g_param_spec_pointer ("key-data", "SSH Key Data", "SSH key data pointer",
                              G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class, PROP_FINGERPRINT,
        g_param_spec_string ("fingerprint", "Fingerprint", "Unique fingerprint for this key",
                             "", G_PARAM_READABLE));

    g_object_class_install_property (gobject_class, PROP_DESCRIPTION,
        g_param_spec_string ("description", "Description", "Description",
                             "", G_PARAM_READABLE));

    g_object_class_install_property (gobject_class, PROP_VALIDITY,
        g_param_spec_uint ("validity", "Validity", "Validity of this key",
                           0, G_MAXUINT, 0, G_PARAM_READABLE));

    g_object_class_install_property (gobject_class, PROP_TRUST,
        g_param_spec_uint ("trust", "Trust", "Trust in this key",
                           0, G_MAXUINT, 0, G_PARAM_READABLE));

    g_object_class_install_property (gobject_class, PROP_EXPIRES,
        g_param_spec_ulong ("expires", "Expires On", "Date this key expires on",
                           0, G_MAXULONG, 0, G_PARAM_READABLE));

    g_object_class_install_property (gobject_class, PROP_LENGTH,
        g_param_spec_uint ("length", "Length of", "The length of this key",
                           0, G_MAXUINT, 0, G_PARAM_READABLE));
}

static GList *
seahorse_ssh_key_create_exporters (SeahorseExportable *exportable,
                                   SeahorseExporterType type)
{
	return g_list_append (NULL, seahorse_ssh_exporter_new (G_OBJECT (exportable), FALSE));
}

static gboolean
seahorse_ssh_key_get_exportable (SeahorseExportable *exportable)
{
	gboolean can;
	g_object_get (exportable, "exportable", &can, NULL);
	return can;
}

static void
seahorse_ssh_key_exportable_iface (SeahorseExportableIface *iface)
{
	iface->create_exporters = seahorse_ssh_key_create_exporters;
	iface->get_exportable = seahorse_ssh_key_get_exportable;
}

static SeahorseDeleter *
seahorse_ssh_key_create_deleter (SeahorseDeletable *deletable)
{
	SeahorseSSHKey *self = SEAHORSE_SSH_KEY (deletable);
	return seahorse_ssh_deleter_new (self);
}

static gboolean
seahorse_ssh_key_get_deletable (SeahorseDeletable *deletable)
{
	gboolean can;
	g_object_get (deletable, "deletable", &can, NULL);
	return can;
}

static void
seahorse_ssh_key_deletable_iface (SeahorseDeletableIface *iface)
{
	iface->create_deleter = seahorse_ssh_key_create_deleter;
	iface->get_deletable = seahorse_ssh_key_get_deletable;
}

static void
seahorse_ssh_key_show_viewer (SeahorseViewable *viewable,
                              GtkWindow *parent)
{
	seahorse_ssh_key_properties_show (SEAHORSE_SSH_KEY (viewable), parent);
}

static void
seahorse_ssh_key_viewable_iface (SeahorseViewableIface *iface)
{
	iface->show_viewer = seahorse_ssh_key_show_viewer;
}

SeahorseSSHKey* 
seahorse_ssh_key_new (SeahorsePlace *place,
                      SeahorseSSHKeyData *data)
{
    SeahorseSSHKey *skey;
    skey = g_object_new (SEAHORSE_TYPE_SSH_KEY,
                         "place", place,
                         "key-data", data,
                         NULL);
    return skey;
}

SeahorseSSHKeyData *
seahorse_ssh_key_get_data (SeahorseSSHKey *self)
{
	g_return_val_if_fail (SEAHORSE_IS_SSH_KEY (self), NULL);
	return self->keydata;
}

guint 
seahorse_ssh_key_get_algo (SeahorseSSHKey *skey)
{
    g_return_val_if_fail (SEAHORSE_IS_SSH_KEY (skey), SSH_ALGO_UNK);
    return skey->keydata->algo;
}

const gchar*
seahorse_ssh_key_get_algo_str (SeahorseSSHKey *skey)
{
    g_return_val_if_fail (SEAHORSE_IS_SSH_KEY (skey), "");
    
    switch(skey->keydata->algo) {
    case SSH_ALGO_UNK:
        return "";
    case SSH_ALGO_RSA:
        return "RSA";
    case SSH_ALGO_DSA:
        return "DSA";
    default:
        g_assert_not_reached ();
        return NULL;
    };
}

guint           
seahorse_ssh_key_get_strength (SeahorseSSHKey *skey)
{
    g_return_val_if_fail (SEAHORSE_IS_SSH_KEY (skey), 0);
    return skey->keydata ? skey->keydata->length : 0;
}

const gchar*    
seahorse_ssh_key_get_location (SeahorseSSHKey *skey)
{
    g_return_val_if_fail (SEAHORSE_IS_SSH_KEY (skey), NULL);
    if (!skey->keydata)
        return NULL;
    return skey->keydata->privfile ? 
                    skey->keydata->privfile : skey->keydata->pubfile;
}

gchar*
seahorse_ssh_key_calc_identifier (const gchar *id)
{
	#define SSH_IDENTIFIER_SIZE 8
	gchar *canonical_id = g_malloc0 (SSH_IDENTIFIER_SIZE + 1);
	gint i, off, len = strlen (id);

	/* Strip out all non alpha numeric chars and limit length to SSH_ID_SIZE */
	for (i = len, off = SSH_IDENTIFIER_SIZE; i >= 0 && off > 0; --i) {
		if (g_ascii_isalnum (id[i]))
			canonical_id[--off] = g_ascii_toupper (id[i]);
	}
    
	/* Not enough characters */
	g_return_val_if_fail (off == 0, NULL);

	return canonical_id;
}

gchar*
seahorse_ssh_key_get_fingerprint (SeahorseSSHKey *self)
{
	gchar *fpr;
	g_object_get (self, "fingerprint", &fpr, NULL);
	return fpr;	
}

SeahorseValidity
seahorse_ssh_key_get_trust (SeahorseSSHKey *self)
{
    guint validity;
    g_object_get (self, "trust", &validity, NULL);
    return validity;
}
