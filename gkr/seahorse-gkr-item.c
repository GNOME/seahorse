/*
 * Seahorse
 *
 * Copyright (C) 2006 Stefan Walter
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

#include "seahorse-context.h"
#include "seahorse-source.h"
#include "seahorse-gkr-item.h"
#include "seahorse-gtkstock.h"
#include "seahorse-util.h"
#include "seahorse-secure-memory.h"

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
    PROP_ITEM_ID,
    PROP_ITEM_INFO,
    PROP_ITEM_ATTRIBUTES,
    PROP_ITEM_ACL,
    PROP_USE
};

G_DEFINE_TYPE (SeahorseGkrItem, seahorse_gkr_item, SEAHORSE_TYPE_OBJECT);

/* -----------------------------------------------------------------------------
 * INTERNAL HELPERS
 */

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
is_network_item (SeahorseGkrItem *git, const gchar *match)
{
    const gchar *protocol;
    if(gnome_keyring_item_info_get_type (git->info) != GNOME_KEYRING_ITEM_NETWORK_PASSWORD)
        return FALSE;
    
    if (!match)
        return TRUE;
    
    protocol = seahorse_gkr_item_get_attribute (git, "protocol");
    return protocol && g_ascii_strncasecmp (protocol, match, strlen (match)) == 0;
}

static gboolean 
is_custom_display_name (SeahorseGkrItem *git, const gchar *display)
{
    const gchar *user;
    const gchar *server;
    const gchar *object;
    guint32 port;
    GString *generated;
    gboolean ret;
    
    if (gnome_keyring_item_info_get_type (git->info) != GNOME_KEYRING_ITEM_NETWORK_PASSWORD)
        return TRUE;
    
    if (!git->attributes || !display)
        return TRUE;
    
    /* 
     * For network passwords gnome-keyring generates in a funky looking display 
     * name that's generated from login credentials. We simulate that generating 
     * here and return FALSE if it matches. This allows us to display user 
     * customized display names and ignore the generated ones.
     */
    
    user = seahorse_gkr_item_get_attribute (git, "user");
    server = seahorse_gkr_item_get_attribute (git, "server");
    object = seahorse_gkr_item_get_attribute (git, "object");
    port = find_attribute_int (git->attributes, "port");
    
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
calc_display_name (SeahorseGkrItem *git, gboolean always)
{
    const gchar *val;
    gchar *display;
    
    display = gnome_keyring_item_info_get_display_name (git->info);
    
    /* If it's customized by the application or user then display that */
    if (is_custom_display_name (git, display))
        return display;
    
    /* If it's a network item ... */
    if (gnome_keyring_item_info_get_type (git->info) == GNOME_KEYRING_ITEM_NETWORK_PASSWORD) {
        
        /* HTTP usually has a the realm as the "object" display that */
        if (is_network_item (git, "http") && git->attributes) {
            val = seahorse_gkr_item_get_attribute (git, "object");
            if (val && val[0]) {
                g_free (display);
                return g_strdup (val);
            }
        }
        
        /* Display the server name as a last resort */
        if (always) {
            val = seahorse_gkr_item_get_attribute (git, "server");
            if (val && val[0]) {
                g_free (display);
                return g_strdup (val);
            }
        }
    }
    
    return always ? display : NULL;
}

static gchar*
calc_network_item_markup (SeahorseGkrItem *git)
{
    const gchar *object;
    const gchar *user;
    const gchar *protocol;
    const gchar *server;
    
    gchar *uri = NULL;
    gchar *display = NULL;
    gchar *ret;
    
    server = seahorse_gkr_item_get_attribute (git, "server");
    protocol = seahorse_gkr_item_get_attribute (git, "protocol");
    object = seahorse_gkr_item_get_attribute (git, "object");
    user = seahorse_gkr_item_get_attribute (git, "user");

    if (!protocol)
        return NULL;
    
    /* The object in HTTP often isn't a path at all */
    if (is_network_item (git, "http"))
        object = NULL;
    
    display = calc_display_name (git, TRUE);
    
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
calc_name_markup (SeahorseGkrItem *git)
{
    gchar *t, *markup = NULL;
    
    g_return_val_if_fail (git->info != NULL, NULL);
    
    /* Only do our special markup for network passwords */
    if (is_network_item (git, NULL))
        markup = calc_network_item_markup (git);
    
    if (!markup) {
        t = calc_display_name (git, TRUE);
        markup = g_markup_escape_text (t, -1);
        g_free (t);
    }

    return markup;
}

static void
changed_key (SeahorseGkrItem *git)
{
	const gchar *description;
	gboolean loaded;
	gchar *secret;
	gchar *display;
	gchar *markup;
	gchar *identifier;
	const gchar *icon;

	if (!git->info) {
        
		g_object_set (git,
		              "id", 0,
		              "label", "",
		              "icon", NULL,
		              "markup", "",
		              "identifier", "",
		              "description", _("Invalid"),
		              "flags", SEAHORSE_FLAG_DISABLED,
		              NULL);
		return;
	}

	WITH_SECURE_MEM ((secret = gnome_keyring_item_info_get_secret (git->info)));
	loaded = (secret == NULL);
	g_free (secret);
	
	if (is_network_item (git, "http")) 
		description = _("Web Password");
	else if (is_network_item (git, NULL)) 
		description = _("Network Password");
	else
		description = _("Password");

	display = calc_display_name (git, TRUE);
	markup = calc_name_markup(git);
	identifier = g_strdup_printf ("%u", git->item_id);

	/* We use a pointer so we don't copy the string every time */
	switch (git->info ? gnome_keyring_item_info_get_type (git->info) : -1)
	{
	case GNOME_KEYRING_ITEM_GENERIC_SECRET:
		icon = GNOME_STOCK_AUTHENTICATION;
		break;
	case GNOME_KEYRING_ITEM_NETWORK_PASSWORD:
		icon = is_network_item (git, "http") ? SEAHORSE_THEMED_WEBBROWSER : GTK_STOCK_NETWORK;
		break;
	case GNOME_KEYRING_ITEM_NOTE:
		icon = GNOME_STOCK_BOOK_OPEN;
		break;
	default:
        	icon = GNOME_STOCK_BLANK;
        	break;
        }
	
	g_object_set (git,
	              "id", seahorse_gkr_item_get_cannonical (git->item_id),
	              "label", display,
	              "icon", icon,
	              "markup", markup,
	              "identifier", identifier,
	              "description", description,
	              "flags", 0,
	              NULL);
	
	g_free (display);
	g_free (markup);
	g_free (identifier);
}

/* -----------------------------------------------------------------------------
 * OBJECT 
 */

static void
seahorse_gkr_item_init (SeahorseGkrItem *git)
{
	g_object_set (git, 
	              "usage", SEAHORSE_USAGE_CREDENTIALS,
	              NULL);
}

static void
seahorse_gkr_item_get_property (GObject *object, guint prop_id,
                                GValue *value, GParamSpec *pspec)
{
    SeahorseGkrItem *git = SEAHORSE_GKR_ITEM (object);
    
    switch (prop_id) {
    case PROP_ITEM_ID:
        g_value_set_uint (value, git->item_id);
        break;
    case PROP_ITEM_INFO:
        g_value_set_pointer (value, git->info);
        break;
    case PROP_ITEM_ATTRIBUTES:
        g_value_set_pointer (value, git->attributes);
        break;
    case PROP_ITEM_ACL:
    	g_value_set_pointer (value, git->acl);
    	break;
    case PROP_USE:
	g_value_set_uint (value, seahorse_gkr_item_get_use (git));
	break;
    }
}

static void
seahorse_gkr_item_set_property (GObject *object, guint prop_id, const GValue *value, 
                                GParamSpec *pspec)
{
    SeahorseGkrItem *git = SEAHORSE_GKR_ITEM (object);
    
    switch (prop_id) {
    case PROP_ITEM_ID:
        git->item_id = g_value_get_uint (value);
        break;
    case PROP_ITEM_INFO:
        if (git->info != g_value_get_pointer (value)) {
            if (git->info)
                gnome_keyring_item_info_free (git->info);
            git->info = g_value_get_pointer (value);
            changed_key (git);
        }
        break;
    case PROP_ITEM_ATTRIBUTES:
        if (git->attributes != g_value_get_pointer (value)) {
            if (git->attributes)
                gnome_keyring_attribute_list_free (git->attributes);
            git->attributes = g_value_get_pointer (value);
            changed_key (git);
        }
        break;
    case PROP_ITEM_ACL:
        if (git->acl != g_value_get_pointer (value)) {
            gnome_keyring_acl_free (git->acl);
            git->acl = g_value_get_pointer (value);
            changed_key (git);
        }
        break;
    }
}

static void
seahorse_gkr_item_object_finalize (GObject *gobject)
{
    SeahorseGkrItem *git = SEAHORSE_GKR_ITEM (gobject);
    
    if (git->info)
        gnome_keyring_item_info_free (git->info);
    git->info = NULL;
    
    if (git->attributes)
        gnome_keyring_attribute_list_free (git->attributes);
    git->attributes = NULL;
    
    gnome_keyring_acl_free (git->acl);
    git->acl = NULL;
    
    G_OBJECT_CLASS (seahorse_gkr_item_parent_class)->finalize (gobject);
}

static void
seahorse_gkr_item_class_init (SeahorseGkrItemClass *klass)
{
    GObjectClass *gobject_class;
    
    seahorse_gkr_item_parent_class = g_type_class_peek_parent (klass);
    gobject_class = G_OBJECT_CLASS (klass);
    
    gobject_class->finalize = seahorse_gkr_item_object_finalize;
    gobject_class->set_property = seahorse_gkr_item_set_property;
    gobject_class->get_property = seahorse_gkr_item_get_property;
    
    g_object_class_install_property (gobject_class, PROP_ITEM_ID,
        g_param_spec_uint ("item-id", "Item ID", "GNOME Keyring Item ID", 
                           0, G_MAXUINT, 0, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

    g_object_class_install_property (gobject_class, PROP_ITEM_INFO,
        g_param_spec_pointer ("item-info", "Item Info", "GNOME Keyring Item Info",
                              G_PARAM_READWRITE));
                              
    g_object_class_install_property (gobject_class, PROP_ITEM_ATTRIBUTES,
        g_param_spec_pointer ("item-attributes", "Item Attributes", "GNOME Keyring Item Attributes",
                              G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class, PROP_ITEM_ACL,
        g_param_spec_pointer ("item-acl", "Item ACL", "GNOME Keyring Item ACL",
                              G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class, PROP_USE,
        g_param_spec_uint ("use", "Use", "Item is used for", 
                           0, G_MAXUINT, 0, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
}

/* -----------------------------------------------------------------------------
 * PUBLIC 
 */

SeahorseGkrItem* 
seahorse_gkr_item_new (SeahorseSource *sksrc, guint32 item_id, GnomeKeyringItemInfo *info, 
                       GnomeKeyringAttributeList *attributes, GList *acl)
{
    SeahorseGkrItem *git;
    git = g_object_new (SEAHORSE_TYPE_GKR_ITEM, "source", sksrc, 
                        "item-id", item_id, "item-info", info, 
                        "item-attributes", attributes, "item-acl", acl, NULL);
    return git;
}

gchar*
seahorse_gkr_item_get_description  (SeahorseGkrItem *git)
{
	g_return_val_if_fail (SEAHORSE_IS_GKR_ITEM (git), NULL);
	return calc_display_name (git, FALSE);
}

const gchar*
seahorse_gkr_item_get_attribute (SeahorseGkrItem *git, const gchar *name)
{
    g_return_val_if_fail (SEAHORSE_IS_GKR_ITEM (git), NULL);
    if (!git->attributes)
        return NULL;
    return seahorse_gkr_find_string_attribute (git->attributes, name);
}

SeahorseGkrUse
seahorse_gkr_item_get_use (SeahorseGkrItem *git)
{
    const gchar *val;
    
    /* Network passwords */
    if (gnome_keyring_item_info_get_type (git->info) == GNOME_KEYRING_ITEM_NETWORK_PASSWORD) {
        if (is_network_item (git, "http"))
            return SEAHORSE_GKR_USE_WEB;
        return SEAHORSE_GKR_USE_NETWORK;
    }
    
    if (git->attributes) {
        val = seahorse_gkr_item_get_attribute (git, "seahorse-key-type");
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

GQuark
seahorse_gkr_item_get_cannonical (guint32 item_id)
{
    #define BUF_LEN (G_N_ELEMENTS (SEAHORSE_GKR_STR) + 16)
    gchar buf[BUF_LEN];
    
    snprintf(buf, BUF_LEN - 1, "%s:%08X", SEAHORSE_GKR_STR, item_id);
    buf[BUF_LEN - 1] = 0;
    return g_quark_from_string (buf);
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
