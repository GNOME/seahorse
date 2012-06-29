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

#include <stdlib.h>
#include <string.h>

#include <glib/gi18n.h>

#include "seahorse-gkr.h"
#include "seahorse-gkr-actions.h"
#include "seahorse-gkr-dialogs.h"
#include "seahorse-gkr-item.h"
#include "seahorse-gkr-item-deleter.h"
#include "seahorse-gkr-keyring.h"

#include "seahorse-deletable.h"
#include "seahorse-icons.h"
#include "seahorse-place.h"
#include "seahorse-util.h"
#include "seahorse-viewable.h"

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
                                    const gchar *label,
                                    GHashTable *item_attrs,
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

static guint32
get_attribute_int (GHashTable *attrs,
                   const gchar *name)
{
	const gchar *value;
	gchar *end;
	guint32 num;

	if (!attrs)
		return 0;

	value = g_hash_table_lookup (attrs, name);
	if (value != NULL) {
		num = strtoul (value, &end, 10);
		if (end && *end == '\0')
			return num;
	}

	return 0;
}

static const gchar *
get_attribute_string (GHashTable *attrs,
                      const gchar *name)
{
	if (!attrs)
		return NULL;

	return g_hash_table_lookup (attrs, name);
}

static gboolean
is_network_item (GHashTable *attrs,
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
calc_network_label (GHashTable *attrs,
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
                const gchar *display,
                GHashTable *attrs,
                DisplayInfo *info)
{
	const gchar *object;
	const gchar *user;
	const gchar *protocol;
	const gchar *server;
	guint32 port;

	server = get_attribute_string (attrs, "server");
	protocol = get_attribute_string (attrs, "protocol");
	object = get_attribute_string (attrs, "object");
	user = get_attribute_string (attrs, "user");
	port = get_attribute_int (attrs, "port");

	if (!protocol)
		return;

	/* If it's customized by the application or user then display that */
	if (!is_custom_network_label (server, user, object, port, display))
		info->label = calc_network_label (attrs, TRUE);
	if (info->label == NULL)
		info->label = g_strdup (display);

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
               const gchar *display,
               GHashTable *attrs,
               DisplayInfo *info)
{
	const gchar *origin;
	GRegex *regex;
	GMatchInfo *match;

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
		info->label = g_strdup (display);

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
                const gchar *display,
                GHashTable *attrs,
                DisplayInfo *info)
{
	const gchar *account;
	GMatchInfo *match;
	GRegex *regex;
	gchar *identifier;
	const gchar *prefix;
	gchar *pos;
	gsize len;

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
		info->label = g_strdup (display);

	if (info->details == NULL)
		info->details = g_markup_escape_text (account, -1);
}

static void
telepathy_custom (const DisplayEntry *entry,
                  const gchar *display,
                  GHashTable *attrs,
                  DisplayInfo *info)
{
	const gchar *account;
	GMatchInfo *match;
	GRegex *regex;
	gchar *identifier;

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
		info->label = g_strdup (display);

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
                           GHashTable *attrs)
{
	const gchar *value;
	guint i;

	if (!item_type)
		return GENERIC_SECRET;
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
	PROP_HAS_SECRET,
	PROP_DESCRIPTION,
	PROP_USE,
	PROP_PLACE,
	PROP_FLAGS,
	PROP_DELETABLE,
	PROP_ICON,
	PROP_LABEL,
	PROP_MARKUP,
	PROP_USAGE,
	PROP_ACTIONS
};

struct _SeahorseGkrItemPrivate {
	SecretValue *item_secret;
	GCancellable *req_secret;
	DisplayInfo *display_info;
	GObject *place;
};

static void      seahorse_gkr_item_deletable_iface   (SeahorseDeletableIface *iface);

static void      seahorse_gkr_item_viewable_iface    (SeahorseViewableIface *iface);

G_DEFINE_TYPE_WITH_CODE (SeahorseGkrItem, seahorse_gkr_item, SECRET_TYPE_ITEM,
                         G_IMPLEMENT_INTERFACE (SEAHORSE_TYPE_DELETABLE, seahorse_gkr_item_deletable_iface);
                         G_IMPLEMENT_INTERFACE (SEAHORSE_TYPE_VIEWABLE, seahorse_gkr_item_viewable_iface);
);

/* -----------------------------------------------------------------------------
 * INTERNAL HELPERS
 */

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
	GHashTable *attrs;
	DisplayInfo *info;
	gchar *label;
	guint i;

	if (self->pv->display_info)
		return self->pv->display_info;

	info = g_new0 (DisplayInfo, 1);

	attrs = secret_item_get_attributes (SECRET_ITEM (self));
	item_type = get_attribute_string (attrs, "xdg:schema");
	item_type = map_item_type_to_specific (item_type, attrs);
	g_assert (item_type != NULL);
	info->item_type = item_type;

	label = secret_item_get_label (SECRET_ITEM (self));
	for (i = 0; i < G_N_ELEMENTS (DISPLAY_ENTRIES); i++) {
		if (g_str_equal (DISPLAY_ENTRIES[i].item_type, item_type)) {
			if (DISPLAY_ENTRIES[i].custom_func != NULL)
				(DISPLAY_ENTRIES[i].custom_func) (&DISPLAY_ENTRIES[i],
				                                  label, attrs,
				                                  info);
			if (!info->label) {
				info->label = label;
				label = NULL;
			}
			if (!info->description)
				info->description = DISPLAY_ENTRIES[i].description;
			if (!info->details)
				info->details = g_strdup ("");
			break;
		}
	}

	self->pv->display_info = info;
	g_hash_table_unref (attrs);
	g_free (label);
	return info;
}

static void
seahorse_gkr_item_g_properties_changed (GDBusProxy *proxy,
                                        GVariant *changed_properties,
                                        const gchar* const *invalidated_properties)
{
	SeahorseGkrItem *self = SEAHORSE_GKR_ITEM (proxy);
	GObject *obj = G_OBJECT (self);

	if (self->pv->display_info) {
		free_display_info (self->pv->display_info);
		self->pv->display_info = NULL;
	}

	g_object_freeze_notify (obj);
	g_object_notify (obj, "has-secret");
	g_object_notify (obj, "use");
	g_object_notify (obj, "label");
	g_object_notify (obj, "icon");
	g_object_notify (obj, "markup");
	g_object_notify (obj, "description");
	g_object_notify (obj, "flags");
	g_object_thaw_notify (obj);
}

static void
received_item_secret (GObject *source,
                      GAsyncResult *result,
                      gpointer user_data)
{
	SeahorseGkrItem *self = SEAHORSE_GKR_ITEM (source);
	SecretValue *value;
	GError *error = NULL;

	g_clear_object (&self->pv->req_secret);

	value = secret_item_get_secret_finish (SECRET_ITEM (source), result, &error);
	if (error == NULL) {
		if (self->pv->item_secret)
			secret_value_unref (self->pv->item_secret);
		self->pv->item_secret = value;
		g_object_notify (G_OBJECT (self), "has-secret");
	} else {
		g_message ("Couldn't retrieve secret: %s", error->message);
		g_error_free (error);
	}

	g_object_unref (self);
}

static void
load_item_secret (SeahorseGkrItem *self)
{
	if (self->pv->req_secret)
		return;

	self->pv->req_secret = g_cancellable_new ();
	secret_item_get_secret (SECRET_ITEM (self), self->pv->req_secret,
	                        received_item_secret, g_object_ref (self));
}

static gboolean
require_item_secret (SeahorseGkrItem *self)
{
	if (!self->pv->item_secret)
		load_item_secret (self);
	return self->pv->item_secret != NULL;
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

void
seahorse_gkr_item_refresh (SeahorseGkrItem *self)
{
	if (self->pv->item_secret)
		load_item_secret (self);
}

static void
seahorse_gkr_item_init (SeahorseGkrItem *self)
{
	self->pv = G_TYPE_INSTANCE_GET_PRIVATE (self, SEAHORSE_TYPE_GKR_ITEM, SeahorseGkrItemPrivate);
}

static void
seahorse_gkr_item_get_property (GObject *object, guint prop_id,
                                GValue *value, GParamSpec *pspec)
{
	SeahorseGkrItem *self = SEAHORSE_GKR_ITEM (object);
	DisplayInfo *info;

	switch (prop_id) {
	case PROP_HAS_SECRET:
		g_value_set_boolean (value, self->pv->item_secret != NULL);
		break;
	case PROP_USE:
		g_value_set_uint (value, seahorse_gkr_item_get_use (self));
		break;
	case PROP_DESCRIPTION:
		info = ensure_display_info (self);
		g_value_set_string (value, info->description);
		break;
	case PROP_PLACE:
		g_value_set_object (value, self->pv->place);
		break;
	case PROP_FLAGS:
		g_value_set_flags (value, SEAHORSE_FLAG_DELETABLE | SEAHORSE_FLAG_PERSONAL);
		break;
	case PROP_LABEL:
		info = ensure_display_info (self);
		g_value_set_string (value, info->label);
		break;
	case PROP_MARKUP:
		info = ensure_display_info (self);
		g_value_take_string (value, calc_name_markup (self));
		break;
	case PROP_ICON:
		g_value_take_object (value, g_themed_icon_new (GCR_ICON_PASSWORD));
		break;
	case PROP_DELETABLE:
		g_value_set_boolean (value, TRUE);
		break;
	case PROP_USAGE:
		g_value_set_enum (value, SEAHORSE_USAGE_CREDENTIALS);
		break;
	case PROP_ACTIONS:
		g_value_set_object (value, NULL);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;		
	}
}

static void
seahorse_gkr_item_set_property (GObject *object,
                                guint prop_id,
                                const GValue *value,
                                GParamSpec *pspec)
{
	SeahorseGkrItem *self = SEAHORSE_GKR_ITEM (object);
	switch (prop_id) {
	case PROP_PLACE:
		if (self->pv->place)
			g_object_remove_weak_pointer (self->pv->place, (gpointer *)&self->pv->place);
		self->pv->place = g_value_get_object (value);
		if (self->pv->place)
			g_object_add_weak_pointer (self->pv->place, (gpointer *)&self->pv->place);
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

	if (self->pv->req_secret) {
		g_cancellable_cancel (self->pv->req_secret);
		g_object_unref (self->pv->req_secret);
		self->pv->req_secret = NULL;
	}

	if (self->pv->place) {
		g_object_remove_weak_pointer (self->pv->place, (gpointer *)&self->pv->place);
		self->pv->place = NULL;
	}

	G_OBJECT_CLASS (seahorse_gkr_item_parent_class)->dispose (obj);
}

static void
seahorse_gkr_item_finalize (GObject *gobject)
{
	SeahorseGkrItem *self = SEAHORSE_GKR_ITEM (gobject);

	if (self->pv->item_secret)
		secret_value_unref (self->pv->item_secret);
	self->pv->item_secret = NULL;
	g_assert (self->pv->req_secret == NULL);

	G_OBJECT_CLASS (seahorse_gkr_item_parent_class)->finalize (gobject);
}

static void
seahorse_gkr_item_class_init (SeahorseGkrItemClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	GDBusProxyClass *proxy_class = G_DBUS_PROXY_CLASS (klass);

	gobject_class->dispose = seahorse_gkr_item_dispose;
	gobject_class->finalize = seahorse_gkr_item_finalize;
	gobject_class->get_property = seahorse_gkr_item_get_property;
	gobject_class->set_property = seahorse_gkr_item_set_property;

	proxy_class->g_properties_changed = seahorse_gkr_item_g_properties_changed;

	g_type_class_add_private (klass, sizeof (SeahorseGkrItemPrivate));

	g_object_class_install_property (gobject_class, PROP_DESCRIPTION,
	        g_param_spec_string("description", "Description", "Item description",
	                            "", G_PARAM_READABLE));

	g_object_class_install_property (gobject_class, PROP_USE,
	        g_param_spec_uint ("use", "Use", "Item is used for", 
	                           0, G_MAXUINT, 0, G_PARAM_READABLE));

	g_object_class_install_property (gobject_class, PROP_HAS_SECRET,
	        g_param_spec_boolean ("has-secret", "Has Secret", "Secret has been loaded", 
	                              FALSE, G_PARAM_READABLE));

	g_object_class_install_property (gobject_class, PROP_PLACE,
	         g_param_spec_object ("place", "place", "place", SEAHORSE_TYPE_GKR_KEYRING,
	                              G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE));

	g_object_class_install_property (gobject_class, PROP_FLAGS,
	         g_param_spec_flags ("flags", "flags", "flags", SEAHORSE_TYPE_FLAGS, SEAHORSE_FLAG_NONE,
	                              G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_READABLE));

	g_object_class_install_property (gobject_class, PROP_ICON,
	            g_param_spec_object ("icon", "Icon", "Icon", G_TYPE_ICON,
	                                 G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_READABLE));

	g_object_class_install_property (gobject_class, PROP_LABEL,
	            g_param_spec_string ("label", "Label", "Label", "",
	                              G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_READABLE));

	g_object_class_install_property (gobject_class, PROP_MARKUP,
	            g_param_spec_string ("markup", "Markup", "Markup", "",
	                              G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_READABLE));

	g_object_class_install_property (gobject_class, PROP_USAGE,
	              g_param_spec_enum ("usage", "Object Usage", "How this object is used.",
	                                 SEAHORSE_TYPE_USAGE, SEAHORSE_USAGE_NONE, G_PARAM_READWRITE));

	g_object_class_install_property (gobject_class, PROP_ACTIONS,
	            g_param_spec_object ("actions", "Actions", "Actions", GTK_TYPE_ACTION_GROUP,
	                                 G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_READABLE));

	g_object_class_override_property (gobject_class, PROP_DELETABLE, "deletable");
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

static void
seahorse_gkr_item_show_viewer (SeahorseViewable *viewable,
                               GtkWindow *parent)
{
	seahorse_gkr_item_properties_show (SEAHORSE_GKR_ITEM (viewable),
	                                   parent);
}

static void
seahorse_gkr_item_viewable_iface (SeahorseViewableIface *iface)
{
	iface->show_viewer = seahorse_gkr_item_show_viewer;
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

gboolean
seahorse_gkr_item_has_secret (SeahorseGkrItem *self)
{
	g_return_val_if_fail (SEAHORSE_IS_GKR_ITEM (self), FALSE);
	return self->pv->item_secret != NULL;
}

SecretValue *
seahorse_gkr_item_get_secret (SeahorseGkrItem *self)
{
	g_return_val_if_fail (SEAHORSE_IS_GKR_ITEM (self), NULL);
	require_item_secret (self);
	return self->pv->item_secret;
}

const gchar*
seahorse_gkr_item_get_attribute (SeahorseGkrItem *self, const gchar *name)
{
	GHashTable *attributes;

	g_return_val_if_fail (SEAHORSE_IS_GKR_ITEM (self), NULL);

	attributes = secret_item_get_attributes (SECRET_ITEM (self));
	if (attributes == NULL)
		return NULL;
	return g_hash_table_lookup (attributes, name);
}


const gchar*
seahorse_gkr_find_string_attribute (GHashTable *attrs,
                                    const gchar *name)
{
	g_return_val_if_fail (attrs, NULL);
	g_return_val_if_fail (name, NULL);

	return g_hash_table_lookup (attrs, name);
}

SeahorseGkrUse
seahorse_gkr_item_get_use (SeahorseGkrItem *self)
{
	const gchar *schema_name;

	schema_name = seahorse_gkr_item_get_attribute (self, "xdg:schema");
	if (schema_name && g_str_equal (schema_name, "org.gnome.keyring.NetworkPassword"))
		return SEAHORSE_GKR_USE_NETWORK;

	return SEAHORSE_GKR_USE_OTHER;
}

static void
on_set_secret (GObject *source,
               GAsyncResult *result,
               gpointer user_data)
{
	GSimpleAsyncResult *async = G_SIMPLE_ASYNC_RESULT (user_data);
	SeahorseGkrItem *self = SEAHORSE_GKR_ITEM (source);
	GError *error = NULL;

	secret_item_set_secret_finish (SECRET_ITEM (source), result, &error);
	if (error == NULL) {
		if (self->pv->item_secret)
			secret_value_unref (self->pv->item_secret);
		self->pv->item_secret = g_simple_async_result_get_op_res_gpointer (async);
		secret_value_ref (self->pv->item_secret);
		g_object_notify (G_OBJECT (self), "has-secret");

	} else {
		g_simple_async_result_take_error (async, error);
	}

	g_simple_async_result_complete (async);
	g_object_unref (async);
}

void
seahorse_gkr_item_set_secret (SeahorseGkrItem *self,
                              SecretValue *value,
                              GCancellable *cancellable,
                              GAsyncReadyCallback callback,
                              gpointer user_data)
{
	GSimpleAsyncResult *async;

	async = g_simple_async_result_new (G_OBJECT (self), callback, user_data,
	                                   seahorse_gkr_item_set_secret);
	g_simple_async_result_set_op_res_gpointer (async, secret_value_ref (value),
	                                           secret_value_unref);

	secret_item_set_secret (SECRET_ITEM (self), value, cancellable,
	                        on_set_secret, g_object_ref (async));

	g_object_unref (async);
}

gboolean
seahorse_gkr_item_set_secret_finish (SeahorseGkrItem *self,
                                     GAsyncResult *result,
                                     GError **error)
{
	if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (result), error))
		return FALSE;

	return TRUE;
}
