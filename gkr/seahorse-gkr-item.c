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

#include <string.h>

#include <glib/gi18n.h>

#include "seahorse-icons.h"
#include "seahorse-source.h"
#include "seahorse-util.h"
#include "seahorse-secure-memory.h"

#include "seahorse-gkr.h"
#include "seahorse-gkr-item.h"
#include "seahorse-gkr-keyring.h"
#include "seahorse-gkr-operation.h"

/* For gnome-keyring secret type ids */
#ifdef WITH_PGP
#include "pgp/seahorse-pgp.h"
#endif
#ifdef WITH_SSH
#include "ssh/seahorse-ssh.h"
#endif 

/* XXX Copied from libgnomeui */
#define GNOME_STOCK_AUTHENTICATION      "gnome-stock-authentication"
#define GNOME_STOCK_BOOK_OPEN           "gnome-stock-book-open"
#define GNOME_STOCK_BLANK               "gnome-stock-blank"

enum {
    PROP_0,
    PROP_KEYRING_NAME,
    PROP_ITEM_ID,
    PROP_ITEM_INFO,
    PROP_ITEM_ATTRIBUTES,
    PROP_HAS_SECRET,
    PROP_USE
};

struct _SeahorseGkrItemPrivate {
	gchar *keyring_name;
	guint32 item_id;
	
	gpointer req_info;
	GnomeKeyringItemInfo *item_info;
	
	gpointer req_attrs;
	GnomeKeyringAttributeList *item_attrs;

	gpointer req_secret;
	gchar *item_secret;
};

G_DEFINE_TYPE (SeahorseGkrItem, seahorse_gkr_item, SEAHORSE_TYPE_OBJECT);

/* -----------------------------------------------------------------------------
 * INTERNAL HELPERS
 */

static GType
boxed_item_info_type (void)
{
	static GType type = 0;
	if (!type)
		type = g_boxed_type_register_static ("GnomeKeyringItemInfo", 
		                                     (GBoxedCopyFunc)gnome_keyring_item_info_copy,
		                                     (GBoxedFreeFunc)gnome_keyring_item_info_free);
	return type;
}

static GType
boxed_attributes_type (void)
{
	static GType type = 0;
	if (!type)
		type = g_boxed_type_register_static ("GnomeKeyringAttributeList", 
		                                     (GBoxedCopyFunc)gnome_keyring_attribute_list_copy,
		                                     (GBoxedFreeFunc)gnome_keyring_attribute_list_free);
	return type;
}

static gboolean 
received_result (SeahorseGkrItem *self, GnomeKeyringResult result)
{
	if (result == GNOME_KEYRING_RESULT_CANCELLED)
		return FALSE;
	
	if (result == GNOME_KEYRING_RESULT_OK)
		return TRUE;

	/* TODO: Implement so that we can display an error icon, along with some text */
	g_message ("failed to retrieve item %d from keyring %s: %s",
	           self->pv->item_id, self->pv->keyring_name, 
	           gnome_keyring_result_to_message (result));
	return FALSE;
}

static void
received_item_info (GnomeKeyringResult result, GnomeKeyringItemInfo *info, gpointer data)
{
	SeahorseGkrItem *self = SEAHORSE_GKR_ITEM (data);
	self->pv->req_info = NULL;
	if (received_result (self, result))
		seahorse_gkr_item_set_info (self, info);
}

static void
load_item_info (SeahorseGkrItem *self)
{
	/* Already in progress */
	if (!self->pv->req_info) {
		g_object_ref (self);
		self->pv->req_info = gnome_keyring_item_get_info_full (self->pv->keyring_name,
		                                                       self->pv->item_id,
		                                                       GNOME_KEYRING_ITEM_INFO_BASICS,
		                                                       received_item_info,
		                                                       self, g_object_unref);
	}
}

static gboolean
require_item_info (SeahorseGkrItem *self)
{
	if (!self->pv->item_info)
		load_item_info (self);
	return self->pv->item_info != NULL;
}

static void
received_item_secret (GnomeKeyringResult result, GnomeKeyringItemInfo *info, gpointer data)
{
	SeahorseGkrItem *self = SEAHORSE_GKR_ITEM (data);
	self->pv->req_secret = NULL;
	if (received_result (self, result)) {
		g_free (self->pv->item_secret);
		WITH_SECURE_MEM (self->pv->item_secret = gnome_keyring_item_info_get_secret (info));
		g_object_notify (G_OBJECT (self), "has-secret");
	}
}

static void
load_item_secret (SeahorseGkrItem *self)
{
	/* Already in progress */
	if (!self->pv->req_secret) {
		g_object_ref (self);
		self->pv->req_secret = gnome_keyring_item_get_info_full (self->pv->keyring_name,
		                                                         self->pv->item_id,
		                                                         GNOME_KEYRING_ITEM_INFO_SECRET,
		                                                         received_item_secret,
		                                                         self, g_object_unref);
	}	
}

static gboolean
require_item_secret (SeahorseGkrItem *self)
{
	if (!self->pv->item_secret)
		load_item_secret (self);
	return self->pv->item_secret != NULL;
}

static void
received_item_attrs (GnomeKeyringResult result, GnomeKeyringAttributeList *attrs, gpointer data)
{
	SeahorseGkrItem *self = SEAHORSE_GKR_ITEM (data);
	self->pv->req_attrs = NULL;
	if (received_result (self, result))
		seahorse_gkr_item_set_attributes (self, attrs);
}

static void
load_item_attrs (SeahorseGkrItem *self)
{
	/* Already in progress */
	if (!self->pv->req_attrs) {
		g_object_ref (self);
		self->pv->req_attrs = gnome_keyring_item_get_attributes (self->pv->keyring_name,
		                                                         self->pv->item_id,
		                                                         received_item_attrs,
		                                                         self, g_object_unref);
	}	
}

static gboolean
require_item_attrs (SeahorseGkrItem *self)
{
	if (!self->pv->item_attrs)
		load_item_attrs (self);
	return self->pv->item_attrs != NULL;
}

static guint32
find_attribute_int (GnomeKeyringAttributeList *attrs, const gchar *name)
{
    guint i;
    
    if (!attrs)
        return 0;
    
    for (i = 0; i < attrs->len; i++) {
        GnomeKeyringAttribute *attr = &(gnome_keyring_attribute_list_index (attrs, i));
        if (g_ascii_strcasecmp (name, attr->name) == 0 && 
            attr->type == GNOME_KEYRING_ATTRIBUTE_TYPE_UINT32)
            return attr->value.integer;
    }
    
    return 0;
}


static gboolean
is_network_item (SeahorseGkrItem *self, const gchar *match)
{
	const gchar *protocol;
	
	if (!require_item_info (self))
		return FALSE;
	
	if (gnome_keyring_item_info_get_type (self->pv->item_info) != GNOME_KEYRING_ITEM_NETWORK_PASSWORD)
		return FALSE;
    
	if (!match)
		return TRUE;
    
	protocol = seahorse_gkr_item_get_attribute (self, "protocol");
	return protocol && g_ascii_strncasecmp (protocol, match, strlen (match)) == 0;
}

static gboolean 
is_custom_display_name (SeahorseGkrItem *self, const gchar *display)
{
	const gchar *user;
	const gchar *server;
	const gchar *object;
	guint32 port;
	GString *generated;
	gboolean ret;
	
	if (require_item_info (self) && gnome_keyring_item_info_get_type (self->pv->item_info) != 
					    GNOME_KEYRING_ITEM_NETWORK_PASSWORD)
		return TRUE;
    
	if (!require_item_attrs (self) || !display)
		return TRUE;
    
	/* 
	 * For network passwords gnome-keyring generates in a funky looking display 
	 * name that's generated from login credentials. We simulate that generating 
	 * here and return FALSE if it matches. This allows us to display user 
	 * customized display names and ignore the generated ones.
	 */
    
	user = seahorse_gkr_item_get_attribute (self, "user");
	server = seahorse_gkr_item_get_attribute (self, "server");
	object = seahorse_gkr_item_get_attribute (self, "object");
	port = find_attribute_int (self->pv->item_attrs, "port");
    
	if (!server)
		return TRUE;
    
	generated = g_string_new (NULL);
	if (user != NULL)
		g_string_append_printf (generated, "%s@", user);
	g_string_append (generated, server);
	if (port != 0)
		g_string_append_printf (generated, ":%d", port);
	if (object != NULL)
		g_string_append_printf (generated, "/%s", object);

	ret = strcmp (display, generated->str) != 0;
	g_string_free (generated, TRUE);
    
	return ret;
}

static gchar*
calc_display_name (SeahorseGkrItem *self, gboolean always)
{
	const gchar *val;
	gchar *display;
    
	if (!require_item_info (self))
		return NULL;
	
	display = gnome_keyring_item_info_get_display_name (self->pv->item_info);
    
	/* If it's customized by the application or user then display that */
	if (is_custom_display_name (self, display))
		return display;
    
	/* If it's a network item ... */
	if (gnome_keyring_item_info_get_type (self->pv->item_info) == GNOME_KEYRING_ITEM_NETWORK_PASSWORD) {
        
		/* HTTP usually has a the realm as the "object" display that */
		if (is_network_item (self, "http") && self->pv->item_attrs) {
			val = seahorse_gkr_item_get_attribute (self, "object");
			if (val && val[0]) {
				g_free (display);
				return g_strdup (val);
			}
		}
        
		/* Display the server name as a last resort */
		if (always) {
			val = seahorse_gkr_item_get_attribute (self, "server");
			if (val && val[0]) {
				g_free (display);
				return g_strdup (val);
			}
		}
	}
    
	return always ? display : NULL;
}

static gchar*
calc_network_item_markup (SeahorseGkrItem *self)
{
	const gchar *object;
	const gchar *user;
	const gchar *protocol;
	const gchar *server;
    
	gchar *uri = NULL;
	gchar *display = NULL;
	gchar *ret;
    
	server = seahorse_gkr_item_get_attribute (self, "server");
	protocol = seahorse_gkr_item_get_attribute (self, "protocol");
	object = seahorse_gkr_item_get_attribute (self, "object");
	user = seahorse_gkr_item_get_attribute (self, "user");

	if (!protocol)
		return NULL;
    
	/* The object in HTTP often isn't a path at all */
	if (is_network_item (self, "http"))
		object = NULL;
    
	display = calc_display_name (self, TRUE);
    
	if (server && protocol) {
		uri = g_strdup_printf ("  %s://%s%s%s/%s", 
		                       protocol, 
		                       user ? user : "",
		                       user ? "@" : "",
		                       server,
		                       object ? object : "");
	}
    
	ret = g_markup_printf_escaped ("%s<span foreground='#555555' size='small' rise='0'>%s</span>",
	                               display, uri ? uri : "");
	g_free (display);
	g_free (uri);
    
	return ret;
}

static gchar* 
calc_name_markup (SeahorseGkrItem *self)
{
	gchar *name, *markup = NULL;
	
	if (!require_item_info (self))
		return NULL;
    
	/* Only do our special markup for network passwords */
	if (is_network_item (self, NULL))
        markup = calc_network_item_markup (self);
    
	if (!markup) {
		name = calc_display_name (self, TRUE);
		markup = g_markup_escape_text (name, -1);
		g_free (name);
	}

	return markup;
}

static gint
calc_item_type (SeahorseGkrItem *self)
{
	if (!require_item_info (self))
		return -1;
	return gnome_keyring_item_info_get_type (self->pv->item_info);
}

/* -----------------------------------------------------------------------------
 * OBJECT 
 */

void
seahorse_gkr_item_realize (SeahorseGkrItem *self)
{
	gchar *display;
	gchar *markup;
	gchar *identifier;
	const gchar *icon_name;
	GIcon *icon;

	display = calc_display_name (self, TRUE);
	markup = calc_name_markup(self);
	identifier = g_strdup_printf ("%u", self->pv->item_id);

	/* We use a pointer so we don't copy the string every time */
	switch (calc_item_type (self))
	{
	case GNOME_KEYRING_ITEM_GENERIC_SECRET:
		icon_name = GNOME_STOCK_AUTHENTICATION;
		break;
	case GNOME_KEYRING_ITEM_NETWORK_PASSWORD:
		icon_name = is_network_item (self, "http") ? SEAHORSE_ICON_WEBBROWSER : GTK_STOCK_NETWORK;
		break;
	case GNOME_KEYRING_ITEM_NOTE:
		icon_name = GNOME_STOCK_BOOK_OPEN;
		break;
	default:
		icon_name = GNOME_STOCK_BLANK;
		break;
	}

	icon = g_themed_icon_new (icon_name);
	g_object_set (self,
		      "label", display,
		      "icon", icon,
		      "markup", markup,
		      "identifier", identifier,
		      "flags", SEAHORSE_FLAG_DELETABLE,
		      NULL);
	g_object_unref (icon);
	g_free (display);
	g_free (markup);
	g_free (identifier);
	
	g_object_notify (G_OBJECT (self), "has-secret");
	g_object_notify (G_OBJECT (self), "use");
}

void
seahorse_gkr_item_refresh (SeahorseGkrItem *self)
{
	if (self->pv->item_info)
		load_item_info (self);
	if (self->pv->item_attrs)
		load_item_attrs (self);
	if (self->pv->item_secret)
		load_item_secret (self);
}

static void
seahorse_gkr_item_init (SeahorseGkrItem *self)
{
	self->pv = G_TYPE_INSTANCE_GET_PRIVATE (self, SEAHORSE_TYPE_GKR_ITEM, SeahorseGkrItemPrivate);
	g_object_set (self, "usage", SEAHORSE_USAGE_CREDENTIALS, NULL);
}

static void
seahorse_gkr_item_constructed (GObject *object)
{
	SeahorseGkrItem *self = SEAHORSE_GKR_ITEM (object);

	G_OBJECT_CLASS (seahorse_gkr_item_parent_class)->constructed (object);

	seahorse_gkr_item_realize (self);
}

static void
seahorse_gkr_item_get_property (GObject *object, guint prop_id,
                                GValue *value, GParamSpec *pspec)
{
	SeahorseGkrItem *self = SEAHORSE_GKR_ITEM (object);
    
	switch (prop_id) {
	case PROP_KEYRING_NAME:
		g_value_set_string (value, seahorse_gkr_item_get_keyring_name (self));
		break;
	case PROP_ITEM_ID:
		g_value_set_uint (value, seahorse_gkr_item_get_item_id (self));
		break;
	case PROP_ITEM_INFO:
		g_value_set_boxed (value, seahorse_gkr_item_get_info (self));
		break;
	case PROP_ITEM_ATTRIBUTES:
		g_value_set_boxed (value, seahorse_gkr_item_get_attributes (self));
		break;
	case PROP_HAS_SECRET:
		g_value_set_boolean (value, self->pv->item_secret != NULL);
		break;
	case PROP_USE:
		g_value_set_uint (value, seahorse_gkr_item_get_use (self));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;		
	}
}

static void
seahorse_gkr_item_set_property (GObject *object, guint prop_id, const GValue *value, 
                                GParamSpec *pspec)
{
	SeahorseGkrItem *self = SEAHORSE_GKR_ITEM (object);
    
	switch (prop_id) {
	case PROP_KEYRING_NAME:
		g_return_if_fail (self->pv->keyring_name == NULL);
		self->pv->keyring_name = g_value_dup_string (value);
		break;
	case PROP_ITEM_ID:
		g_return_if_fail (self->pv->item_id == 0);
		self->pv->item_id = g_value_get_uint (value);
		break;
	case PROP_ITEM_INFO:
		seahorse_gkr_item_set_info (self, g_value_get_boxed (value));
		break;
	case PROP_ITEM_ATTRIBUTES:
		seahorse_gkr_item_set_attributes (self, g_value_get_boxed (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
seahorse_gkr_item_dispose (GObject *obj)
{
	SeahorseGkrItem *self = SEAHORSE_GKR_ITEM (obj);
	SeahorseSource *source;

	source = seahorse_object_get_source (SEAHORSE_OBJECT (self));
	if (source != NULL)
		seahorse_gkr_keyring_remove_item (SEAHORSE_GKR_KEYRING (source),
		                                  self->pv->item_id);

	G_OBJECT_CLASS (seahorse_gkr_item_parent_class)->dispose (obj);
}

static void
seahorse_gkr_item_finalize (GObject *gobject)
{
	SeahorseGkrItem *self = SEAHORSE_GKR_ITEM (gobject);
	
	g_free (self->pv->keyring_name);
	self->pv->keyring_name = NULL;
    
	if (self->pv->item_info)
		gnome_keyring_item_info_free (self->pv->item_info);
	self->pv->item_info = NULL;
	g_assert (self->pv->req_info == NULL);
    
	if (self->pv->item_attrs)
		gnome_keyring_attribute_list_free (self->pv->item_attrs);
	self->pv->item_attrs = NULL;
	g_assert (self->pv->req_attrs == NULL);

	g_free (self->pv->item_secret);
	self->pv->item_secret = NULL;
	g_assert (self->pv->req_secret == NULL);
	
	G_OBJECT_CLASS (seahorse_gkr_item_parent_class)->finalize (gobject);
}

static void
seahorse_gkr_item_class_init (SeahorseGkrItemClass *klass)
{
	GObjectClass *gobject_class;

	seahorse_gkr_item_parent_class = g_type_class_peek_parent (klass);
	gobject_class = G_OBJECT_CLASS (klass);
	
	gobject_class->constructed = seahorse_gkr_item_constructed;
	gobject_class->dispose = seahorse_gkr_item_dispose;
	gobject_class->finalize = seahorse_gkr_item_finalize;
	gobject_class->set_property = seahorse_gkr_item_set_property;
	gobject_class->get_property = seahorse_gkr_item_get_property;

	g_type_class_add_private (klass, sizeof (SeahorseGkrItemPrivate));
    
	g_object_class_install_property (gobject_class, PROP_KEYRING_NAME,
	        g_param_spec_string("keyring-name", "Keyring Name", "Keyring this item is in", 
	                            "", G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property (gobject_class, PROP_ITEM_ID,
	        g_param_spec_uint ("item-id", "Item ID", "GNOME Keyring Item ID", 
	                           0, G_MAXUINT, 0, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property (gobject_class, PROP_ITEM_INFO,
                g_param_spec_boxed ("item-info", "Item Info", "GNOME Keyring Item Info",
                                    boxed_item_info_type (),  G_PARAM_READWRITE));
                              
	g_object_class_install_property (gobject_class, PROP_ITEM_ATTRIBUTES,
                g_param_spec_boxed ("item-attributes", "Item Attributes", "GNOME Keyring Item Attributes",
                                    boxed_attributes_type (), G_PARAM_READWRITE));

	g_object_class_install_property (gobject_class, PROP_USE,
	        g_param_spec_uint ("use", "Use", "Item is used for", 
	                           0, G_MAXUINT, 0, G_PARAM_READABLE));

	g_object_class_install_property (gobject_class, PROP_HAS_SECRET,
	        g_param_spec_boolean ("has-secret", "Has Secret", "Secret has been loaded", 
	                              FALSE, G_PARAM_READABLE));
}

/* -----------------------------------------------------------------------------
 * PUBLIC 
 */

SeahorseGkrItem *
seahorse_gkr_item_new (SeahorseGkrKeyring *keyring,
                       const gchar *keyring_name,
                       guint32 item_id)
{
	return g_object_new (SEAHORSE_TYPE_GKR_ITEM, "source", keyring,
	                     "item-id", item_id, "keyring-name", keyring_name, NULL);
}

guint32
seahorse_gkr_item_get_item_id (SeahorseGkrItem *self)
{
	g_return_val_if_fail (SEAHORSE_IS_GKR_ITEM (self), 0);
	return self->pv->item_id;
}

const gchar*
seahorse_gkr_item_get_keyring_name (SeahorseGkrItem *self)
{
	g_return_val_if_fail (SEAHORSE_IS_GKR_ITEM (self), NULL);
	return self->pv->keyring_name;
}

GnomeKeyringItemInfo*
seahorse_gkr_item_get_info (SeahorseGkrItem *self)
{
	g_return_val_if_fail (SEAHORSE_IS_GKR_ITEM (self), NULL);
	require_item_info (self);
	return self->pv->item_info;
}

void
seahorse_gkr_item_set_info (SeahorseGkrItem *self, GnomeKeyringItemInfo* info)
{
	GObject *obj;
	gchar *secret;

	g_return_if_fail (SEAHORSE_IS_GKR_ITEM (self));
	
	if (self->pv->item_info)
		gnome_keyring_item_info_free (self->pv->item_info);
	if (info)
		self->pv->item_info = gnome_keyring_item_info_copy (info);
	else
		self->pv->item_info = NULL;
	
	obj = G_OBJECT (self);
	g_object_freeze_notify (obj);
	seahorse_gkr_item_realize (self);
	g_object_notify (obj, "item-info");
	g_object_notify (obj, "use");
	
	/* Get the secret out of the item info, if not already loaded */
	WITH_SECURE_MEM (secret = gnome_keyring_item_info_get_secret (self->pv->item_info));
	if (secret != NULL) {
		gnome_keyring_free_password (self->pv->item_secret);
		self->pv->item_secret = secret;
		g_object_notify (obj, "has-secret");
	}

	g_object_thaw_notify (obj);
}

gboolean
seahorse_gkr_item_has_secret (SeahorseGkrItem *self)
{
	g_return_val_if_fail (SEAHORSE_IS_GKR_ITEM (self), FALSE);
	return self->pv->item_secret != NULL;
}

const gchar*
seahorse_gkr_item_get_secret (SeahorseGkrItem *self)
{
	g_return_val_if_fail (SEAHORSE_IS_GKR_ITEM (self), NULL);
	require_item_secret (self);
	return self->pv->item_secret;
}

GnomeKeyringAttributeList*
seahorse_gkr_item_get_attributes (SeahorseGkrItem *self)
{
	g_return_val_if_fail (SEAHORSE_IS_GKR_ITEM (self), NULL);
	require_item_attrs (self);
	return self->pv->item_attrs;
}

void
seahorse_gkr_item_set_attributes (SeahorseGkrItem *self, GnomeKeyringAttributeList* attrs)
{
	GObject *obj;
	
	g_return_if_fail (SEAHORSE_IS_GKR_ITEM (self));
	
	if (self->pv->item_attrs)
		gnome_keyring_attribute_list_free (self->pv->item_attrs);
	if (attrs)
		self->pv->item_attrs = gnome_keyring_attribute_list_copy (attrs);
	else
		self->pv->item_attrs = NULL;
	
	obj = G_OBJECT (self);
	g_object_freeze_notify (obj);
	seahorse_gkr_item_realize (self);
	g_object_notify (obj, "item-attributes");
	g_object_notify (obj, "use");
	g_object_thaw_notify (obj);
}

const gchar*
seahorse_gkr_item_get_attribute (SeahorseGkrItem *self, const gchar *name)
{
	g_return_val_if_fail (SEAHORSE_IS_GKR_ITEM (self), NULL);
	if (!require_item_attrs (self))
		return NULL;
	return seahorse_gkr_find_string_attribute (self->pv->item_attrs, name);
}

const gchar*
seahorse_gkr_find_string_attribute (GnomeKeyringAttributeList *attrs, const gchar *name)
{
	guint i;
	
	g_return_val_if_fail (attrs, NULL);
	g_return_val_if_fail (name, NULL);

	for (i = 0; i < attrs->len; i++) {
		GnomeKeyringAttribute *attr = &(gnome_keyring_attribute_list_index (attrs, i));
		if (g_ascii_strcasecmp (name, attr->name) == 0 && 
				attr->type == GNOME_KEYRING_ATTRIBUTE_TYPE_STRING)
			return attr->value.string;
	}
	    
	return NULL;	
}

SeahorseGkrUse
seahorse_gkr_item_get_use (SeahorseGkrItem *self)
{
	const gchar *val;
    
	/* Network passwords */
	if (require_item_info (self)) {
		if (gnome_keyring_item_info_get_type (self->pv->item_info) == GNOME_KEYRING_ITEM_NETWORK_PASSWORD) {
			if (is_network_item (self, "http"))
				return SEAHORSE_GKR_USE_WEB;
			return SEAHORSE_GKR_USE_NETWORK;
		}
	}
    
	if (require_item_attrs (self)) {
		val = seahorse_gkr_item_get_attribute (self, "seahorse-key-type");
		if (val) {
#ifdef WITH_PGP
			if (strcmp (val, SEAHORSE_PGP_TYPE_STR) == 0)
				return SEAHORSE_GKR_USE_PGP;
#endif
#ifdef WITH_SSH
			if (strcmp (val, SEAHORSE_SSH_TYPE_STR) == 0)
				return SEAHORSE_GKR_USE_SSH;
#endif
		}
	}
    
	return SEAHORSE_GKR_USE_OTHER;
}
