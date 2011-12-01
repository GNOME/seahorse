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

#include "seahorse-gkr.h"
#include "seahorse-gkr-actions.h"
#include "seahorse-gkr-item.h"
#include "seahorse-gkr-item-deleter.h"
#include "seahorse-gkr-keyring.h"
#include "seahorse-gkr-operation.h"

#include "seahorse-deletable.h"
#include "seahorse-icons.h"
#include "seahorse-place.h"
#include "seahorse-util.h"
#include "seahorse-secure-memory.h"

#define GENERIC_SECRET "org.freedesktop.Secret.Generic"
#define NETWORK_PASSWORD "org.gnome.keyring.NetworkPassword"
#define KEYRING_NOTE "org.gnome.keyring.Note"
#define CHAINED_KEYRING "org.gnome.keyring.ChainedKeyring"
#define ENCRYPTION_KEY "org.gnome.keyring.EncryptionKey"
#define PK_STORAGE "org.gnome.keyring.PkStorage"
#define CHROME_PASSWORD "x.internal.Chrome"
#define GOA_PASSWORD "org.gnome.OnlineAccounts"
#define TELEPATHY_PASSWORD "org.freedesktop.Telepathy"
#define EMPATHY_PASSWORD "org.freedesktop.Empathy"
#define NETWORK_MANAGER_SECRET "org.freedesktop.NetworkManager"

typedef struct _DisplayEntry DisplayEntry;
typedef struct _DisplayInfo DisplayInfo;

typedef void      (*DisplayCustom) (const DisplayEntry *entry,
                                    GnomeKeyringItemInfo *item_info,
                                    GnomeKeyringAttributeList *item_attrs,
                                    DisplayInfo *info);

struct _DisplayEntry {
	const gchar *item_type;
	const gchar *description;
	DisplayCustom custom_func;
};

struct _DisplayInfo {
	const gchar *item_type;
	gchar *label;
	gchar *details;
	const gchar *description;
};

typedef struct {
	const gchar *item_type;
	const gchar *mapped_type;
	const gchar *match_attribute;
	const gchar *match_pattern;
} MappingEntry;

typedef struct {
	GnomeKeyringItemType old;
	const gchar *item_type;
} OldItemTypes;

static guint32
get_attribute_int (GnomeKeyringAttributeList *attrs,
                   const gchar *name)
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

static const gchar *
get_attribute_string (GnomeKeyringAttributeList *attrs,
                      const gchar *name)
{
	guint i;

	if (!attrs)
		return NULL;

	for (i = 0; i < attrs->len; i++) {
		GnomeKeyringAttribute *attr = &(gnome_keyring_attribute_list_index (attrs, i));
		if (g_ascii_strcasecmp (name, attr->name) == 0 &&
		    attr->type == GNOME_KEYRING_ATTRIBUTE_TYPE_STRING)
			return attr->value.string;
	}

	return NULL;
}

static gboolean
is_network_item (GnomeKeyringAttributeList *attrs,
                 const gchar *match)
{
	const gchar *protocol;

	g_assert (match);

	protocol = get_attribute_string (attrs, "protocol");
	return protocol && strcasecmp (protocol, match) == 0;
}

static gboolean
is_custom_network_label (const gchar *server,
                         const gchar *user,
                         const gchar *object,
                         guint32 port,
                         const gchar *display)
{
	GString *generated;
	gboolean ret;

	/*
	 * For network passwords gnome-keyring generates in a funky looking display
	 * name that's generated from login credentials. We simulate that generating
	 * here and return FALSE if it matches. This allows us to display user
	 * customized display names and ignore the generated ones.
	 */

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

static gchar *
calc_network_label (GnomeKeyringAttributeList *attrs,
                    gboolean always)
{
	const gchar *val;

	/* HTTP usually has a the realm as the "object" display that */
	if (is_network_item (attrs, "http") && attrs != NULL) {
		val = get_attribute_string (attrs, "object");
		if (val && val[0])
			return g_strdup (val);

		/* Display the server name as a last resort */
		if (always) {
			val = get_attribute_string (attrs, "server");
			if (val && val[0])
				return g_strdup (val);
		}
	}

	return NULL;
}

static void
network_custom (const DisplayEntry *entry,
                GnomeKeyringItemInfo *item,
                GnomeKeyringAttributeList *attrs,
                DisplayInfo *info)
{
	const gchar *object;
	const gchar *user;
	const gchar *protocol;
	const gchar *server;
	guint32 port;
	gchar *display = NULL;

	server = get_attribute_string (attrs, "server");
	protocol = get_attribute_string (attrs, "protocol");
	object = get_attribute_string (attrs, "object");
	user = get_attribute_string (attrs, "user");
	port = get_attribute_int (attrs, "port");

	if (!protocol)
		return;

	display = gnome_keyring_item_info_get_display_name (item);

	/* If it's customized by the application or user then display that */
	if (!is_custom_network_label (server, user, object, port, display))
		info->label = calc_network_label (attrs, TRUE);
	if (info->label == NULL)
		info->label = display;
	else
		g_free (display);

	if (server && protocol) {
		info->details = g_markup_printf_escaped ("%s://%s%s%s/%s",
		                                         protocol,
		                                         user ? user : "",
		                                         user ? "@" : "",
		                                         server,
		                                         object ? object : "");
	}
}

static void
chrome_custom (const DisplayEntry *entry,
               GnomeKeyringItemInfo *item,
               GnomeKeyringAttributeList *attrs,
               DisplayInfo *info)
{
	gchar *display;
	const gchar *origin;
	GRegex *regex;
	GMatchInfo *match;

	display = gnome_keyring_item_info_get_display_name (item);
	origin = get_attribute_string (attrs, "origin_url");

	/* A brain dead url, parse */
	if (g_strcmp0 (display, origin) == 0) {
		regex = g_regex_new ("[a-z]+://([^/]+)/", G_REGEX_CASELESS, 0, NULL);
		if (g_regex_match (regex, display, G_REGEX_MATCH_ANCHORED, &match)) {
			if (g_match_info_matches (match))
				info->label = g_match_info_fetch (match, 1);
			g_match_info_free (match);
		}
		g_regex_unref (regex);
	}

	if (info->label == NULL)
		info->label = (display);
	else
		g_free (display);

	info->details = g_markup_escape_text (origin ? origin : "", -1);
}

static gchar *
decode_telepathy_id (const gchar *account)
{
	gchar *value;
	gchar *result;
	gchar *pos;
	gsize len;

	value = g_strdup (account);
	while ((pos = strchr (value, '_')) != NULL)
		*pos = '%';

	len = strlen (value);

	if (len > 0 && value[len - 1] == '0')
		value[len - 1] = '\0';

	result = g_uri_unescape_string (value, "");
	g_free (value);

	if (result != NULL)
		return result;

	return NULL;
}

static void
empathy_custom (const DisplayEntry *entry,
                GnomeKeyringItemInfo *item,
                GnomeKeyringAttributeList *attrs,
                DisplayInfo *info)
{
	const gchar *account;
	GMatchInfo *match;
	GRegex *regex;
	gchar *display;
	gchar *identifier;
	const gchar *prefix;
	gchar *pos;
	gsize len;

	display = gnome_keyring_item_info_get_display_name (item);
	account = get_attribute_string (attrs, "account-id");

	/* Translators: This should be the same as the string in empathy */
	prefix = _("IM account password for ");

	if (g_str_has_prefix (display, prefix)) {
		len = strlen (prefix);
		pos = strchr (display + len, '(');
		if (pos != NULL)
			info->label = g_strndup (display + len, pos - (display + len));

		regex = g_regex_new ("^.+/.+/(.+)$", G_REGEX_CASELESS, 0, NULL);
		if (g_regex_match (regex, account, G_REGEX_MATCH_ANCHORED, &match)) {
			if (g_match_info_matches (match)) {
				identifier = g_match_info_fetch (match, 1);
				info->details = decode_telepathy_id (identifier);
				g_free (identifier);
			}
			g_match_info_free (match);
		}
		g_regex_unref (regex);
	}

	if (info->label == NULL)
		info->label = (display);
	else
		g_free (display);

	if (info->details == NULL)
		info->details = g_markup_escape_text (account, -1);
}

static void
telepathy_custom (const DisplayEntry *entry,
                  GnomeKeyringItemInfo *item,
                  GnomeKeyringAttributeList *attrs,
                  DisplayInfo *info)
{
	const gchar *account;
	GMatchInfo *match;
	GRegex *regex;
	gchar *display;
	gchar *identifier;

	display = gnome_keyring_item_info_get_display_name (item);
	account = get_attribute_string (attrs, "account");

	if (strstr (display, account)) {
		regex = g_regex_new ("^.+/.+/(.+)$", G_REGEX_CASELESS, 0, NULL);
		if (g_regex_match (regex, account, G_REGEX_MATCH_ANCHORED, &match)) {
			if (g_match_info_matches (match)) {
				identifier = g_match_info_fetch (match, 1);
				info->label = decode_telepathy_id (identifier);
				g_free (identifier);
			}
			g_match_info_free (match);
		}
		g_regex_unref (regex);
	}

	if (info->label == NULL)
		info->label = (display);
	else
		g_free (display);

	info->details = g_markup_escape_text (account, -1);
}

static const DisplayEntry DISPLAY_ENTRIES[] = {
	{ GENERIC_SECRET, N_("Password or secret"), NULL },
	{ NETWORK_PASSWORD, N_("Network password"), network_custom },
	{ KEYRING_NOTE, N_("Stored note"), NULL },
	{ CHAINED_KEYRING, N_("Keyring password"), NULL },
	{ ENCRYPTION_KEY, N_("Encryption key password"), NULL },
	{ PK_STORAGE, N_("Key storage password"), NULL },
	{ CHROME_PASSWORD, N_("Google Chrome password"), chrome_custom },
	{ GOA_PASSWORD, N_("Gnome Online Accounts password"), NULL },
	{ TELEPATHY_PASSWORD, N_("Telepathy password"), telepathy_custom },
	{ EMPATHY_PASSWORD, N_("Instant messaging password"), empathy_custom },
	{ NETWORK_MANAGER_SECRET, N_("Network Manager secret"), NULL },
};

static const OldItemTypes OLD_ITEM_TYPES[] = {
	{ GNOME_KEYRING_ITEM_GENERIC_SECRET, GENERIC_SECRET },
	{ GNOME_KEYRING_ITEM_NETWORK_PASSWORD, NETWORK_PASSWORD },
	{ GNOME_KEYRING_ITEM_NOTE, KEYRING_NOTE },
	{ GNOME_KEYRING_ITEM_CHAINED_KEYRING_PASSWORD, CHAINED_KEYRING },
	{ GNOME_KEYRING_ITEM_ENCRYPTION_KEY_PASSWORD, ENCRYPTION_KEY },
	{ GNOME_KEYRING_ITEM_PK_STORAGE, PK_STORAGE },
};

static const gchar *
map_item_type_to_string (GnomeKeyringItemType old)
{
	guint i;

	for (i = 0; i < G_N_ELEMENTS (OLD_ITEM_TYPES); i++) {
		if (old == OLD_ITEM_TYPES[i].old)
			return OLD_ITEM_TYPES[i].item_type;
	}

	return GENERIC_SECRET;
}

static const MappingEntry MAPPING_ENTRIES[] = {
	{ GENERIC_SECRET, CHROME_PASSWORD, "application", "chrome*" },
	{ GENERIC_SECRET, GOA_PASSWORD, "goa-identity", NULL },
	{ GENERIC_SECRET, TELEPATHY_PASSWORD, "account", "*/*/*" },
	{ GENERIC_SECRET, EMPATHY_PASSWORD, "account-id", "*/*/*" },
	/* Network secret for Auto anna/802-11-wireless-security/psk */
	{ GENERIC_SECRET, NETWORK_MANAGER_SECRET, "connection-uuid", NULL },
};

static const gchar *
map_item_type_to_specific (const gchar *item_type,
                           GnomeKeyringAttributeList *attrs)
{
	const gchar *value;
	guint i;

	if (!attrs)
		return item_type;

	for (i = 0; i < G_N_ELEMENTS (MAPPING_ENTRIES); i++) {
		if (g_str_equal (item_type, MAPPING_ENTRIES[i].item_type)) {
			value = get_attribute_string (attrs, MAPPING_ENTRIES[i].match_attribute);
			if (value && MAPPING_ENTRIES[i].match_pattern != NULL) {
				if (g_pattern_match_simple (MAPPING_ENTRIES[i].match_pattern, value))
					return MAPPING_ENTRIES[i].mapped_type;
			} else if (value) {
				return MAPPING_ENTRIES[i].mapped_type;
			}
		}
	}

	return item_type;
}

enum {
	PROP_0,
	PROP_KEYRING_NAME,
	PROP_ITEM_ID,
	PROP_ITEM_INFO,
	PROP_ITEM_ATTRIBUTES,
	PROP_HAS_SECRET,
	PROP_DESCRIPTION,
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

	DisplayInfo *display_info;
};

static gboolean  require_item_attrs                  (SeahorseGkrItem *self);

static void      seahorse_gkr_item_deletable_iface   (SeahorseDeletableIface *iface);

G_DEFINE_TYPE_WITH_CODE (SeahorseGkrItem, seahorse_gkr_item, SEAHORSE_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (SEAHORSE_TYPE_DELETABLE, seahorse_gkr_item_deletable_iface);
);

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

static void
free_display_info (DisplayInfo *info)
{
	g_free (info->label);
	g_free (info->details);
	g_free (info);
}

static DisplayInfo *
ensure_display_info (SeahorseGkrItem *self)
{
	const gchar *item_type;
	DisplayInfo *info;
	guint i;

	if (self->pv->display_info)
		return self->pv->display_info;

	require_item_attrs (self);

	info = g_new0 (DisplayInfo, 1);

	item_type = map_item_type_to_string (gnome_keyring_item_info_get_type (self->pv->item_info));
	item_type = map_item_type_to_specific (item_type, self->pv->item_attrs);
	g_assert (item_type != NULL);
	info->item_type = item_type;

	for (i = 0; i < G_N_ELEMENTS (DISPLAY_ENTRIES); i++) {
		if (g_str_equal (DISPLAY_ENTRIES[i].item_type, item_type)) {
			if (DISPLAY_ENTRIES[i].custom_func != NULL)
				(DISPLAY_ENTRIES[i].custom_func) (&DISPLAY_ENTRIES[i],
				                                  self->pv->item_info,
				                                  self->pv->item_attrs,
				                                  info);
			if (!info->label && self->pv->item_info)
				info->label = gnome_keyring_item_info_get_display_name (self->pv->item_info);
			if (!info->description)
				info->description = DISPLAY_ENTRIES[i].description;
			if (!info->details)
				info->details = g_strdup ("");
			break;
		}
	}

	self->pv->display_info = info;
	return info;
}

static void
clear_display_info (SeahorseGkrItem *self)
{
	if (self->pv->display_info) {
		free_display_info (self->pv->display_info);
		self->pv->display_info = NULL;
	}
}

static gboolean 
received_result (SeahorseGkrItem *self, GnomeKeyringResult result)
{
	if (result == GNOME_KEYRING_RESULT_CANCELLED)
		return FALSE;

	if (result == GNOME_KEYRING_RESULT_OK) {
		clear_display_info (self);
		return TRUE;
	}

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

static gchar*
calc_name_markup (SeahorseGkrItem *self)
{
	GString *result;
	DisplayInfo *info;
	gchar *value;

	info = ensure_display_info (self);
	result = g_string_new ("");
	value = g_markup_escape_text (info->label, -1);
	g_string_append (result, value);
	g_string_append (result, "<span size='small' rise='0' foreground='#555555'>\n");
	g_string_append (result, info->details);
	g_string_append (result, "</span>");

	return g_string_free (result, FALSE);
}

/* -----------------------------------------------------------------------------
 * OBJECT 
 */

void
seahorse_gkr_item_realize (SeahorseGkrItem *self)
{
	DisplayInfo *info;
	gchar *markup;
	GIcon *icon;

	if (!require_item_info (self))
		return;

	info = ensure_display_info (self);
	markup = calc_name_markup (self);

	icon = g_themed_icon_new (GCR_ICON_PASSWORD);
	g_object_set (self,
	              "label", info->label,
	              "icon", icon,
	              "markup", markup,
	              "flags", SEAHORSE_FLAG_DELETABLE | SEAHORSE_FLAG_PERSONAL,
	              NULL);
	g_object_unref (icon);
	g_free (markup);

	g_object_notify (G_OBJECT (self), "has-secret");
	g_object_notify (G_OBJECT (self), "use");
	g_object_notify (G_OBJECT (self), "description");
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
	GtkActionGroup *actions;

	self->pv = G_TYPE_INSTANCE_GET_PRIVATE (self, SEAHORSE_TYPE_GKR_ITEM, SeahorseGkrItemPrivate);

	actions = seahorse_gkr_item_actions_instance ();
	g_object_set (self,
	              "usage", SEAHORSE_USAGE_CREDENTIALS,
	              "actions", actions,
	              NULL);
	g_object_unref (actions);
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
	DisplayInfo *info;

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
	case PROP_DESCRIPTION:
		if (!require_item_info (self))
			break;
		info = ensure_display_info (self);
		g_value_set_string (value, info->description);
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
	SeahorsePlace *place;

	place = seahorse_object_get_place (SEAHORSE_OBJECT (self));
	if (place != NULL)
		seahorse_gkr_keyring_remove_item (SEAHORSE_GKR_KEYRING (place),
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

	g_object_class_install_property (gobject_class, PROP_DESCRIPTION,
	        g_param_spec_string("description", "Description", "Item description",
	                            "", G_PARAM_READABLE));

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

static SeahorseDeleter *
seahorse_gkr_item_create_deleter (SeahorseDeletable *deletable)
{
	SeahorseGkrItem *self = SEAHORSE_GKR_ITEM (deletable);
	return seahorse_gkr_item_deleter_new (self);
}

static void
seahorse_gkr_item_deletable_iface (SeahorseDeletableIface *iface)
{
	iface->create_deleter = seahorse_gkr_item_create_deleter;
}

SeahorseGkrItem *
seahorse_gkr_item_new (SeahorseGkrKeyring *keyring,
                       const gchar *keyring_name,
                       guint32 item_id)
{
	return g_object_new (SEAHORSE_TYPE_GKR_ITEM,
	                     "place", keyring,
	                     "item-id", item_id,
	                     "keyring-name", keyring_name,
	                     NULL);
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
	g_object_notify (obj, "description");

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
	g_object_notify (obj, "description");
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
	/* Network passwords */
	if (require_item_info (self)) {
		if (gnome_keyring_item_info_get_type (self->pv->item_info) == GNOME_KEYRING_ITEM_NETWORK_PASSWORD) {
			return SEAHORSE_GKR_USE_NETWORK;
		}
	}

	return SEAHORSE_GKR_USE_OTHER;
}
