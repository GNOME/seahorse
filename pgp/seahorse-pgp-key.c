/*
 * Seahorse
 *
 * Copyright (C) 2005 Stefan Walter
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

#include "seahorse-pgp-dialogs.h"
#include "seahorse-pgp-key.h"
#include "seahorse-pgp-uid.h"
#include "seahorse-pgp-subkey.h"

#include "libseahorse/seahorse-object-list.h"
#include "libseahorse/seahorse-util.h"

#include <gcr/gcr.h>

#include <glib/gi18n.h>

#include <string.h>

enum {
    PROP_0,
    PROP_PHOTOS,
    PROP_SUBKEYS,
    PROP_UIDS,
    PROP_FINGERPRINT,
    PROP_VALIDITY,
    PROP_TRUST,
    PROP_EXPIRES,
    PROP_LENGTH,
    PROP_ALGO,
    PROP_DESCRIPTION,
    N_PROPS
};
static GParamSpec *obj_props[N_PROPS] = { NULL, };

static void        seahorse_pgp_key_viewable_iface          (SeahorseViewableIface *iface);

typedef struct _SeahorsePgpKeyPrivate {
    gchar *keyid;
    GList *uids;            /* All the UID objects */
    GList *subkeys;         /* All the Subkey objects */
    GList *photos;          /* List of photos */
} SeahorsePgpKeyPrivate;

G_DEFINE_TYPE_WITH_CODE (SeahorsePgpKey, seahorse_pgp_key, SEAHORSE_TYPE_OBJECT,
                         G_ADD_PRIVATE (SeahorsePgpKey)
                         G_IMPLEMENT_INTERFACE (SEAHORSE_TYPE_VIEWABLE, seahorse_pgp_key_viewable_iface);
);

/*
 * PGP key ids can be of varying lengths. Shorter keyids are the last
 * characters of the longer ones. When hashing match on the last 8
 * characters.
 */

guint
seahorse_pgp_keyid_hash (gconstpointer v)
{
    const gchar *keyid = v;
    gsize len = strlen (keyid);
    if (len > 8)
        keyid += len - 8;
    return g_str_hash (keyid);
}

gboolean
seahorse_pgp_keyid_equal (gconstpointer v1,
                          gconstpointer v2)
{
    const gchar *keyid_1 = v1;
    const gchar *keyid_2 = v2;
    gsize len_1 = strlen (keyid_1);
    gsize len_2 = strlen (keyid_2);

    if (len_1 != len_2 && len_1 >= 8 && len_2 >= 8) {
        keyid_1 += len_1 - 8;
        keyid_2 += len_2 - 8;
    }
    return g_str_equal (keyid_1, keyid_2);
}

static const gchar*
calc_short_name (SeahorsePgpKey *self)
{
    GList *uids = seahorse_pgp_key_get_uids (self);
    return uids ? seahorse_pgp_uid_get_name (uids->data) : NULL;
}

static gchar*
calc_name (SeahorsePgpKey *self)
{
    GList *uids = seahorse_pgp_key_get_uids (self);
    return uids ? seahorse_pgp_uid_calc_label (seahorse_pgp_uid_get_name (uids->data),
                                               seahorse_pgp_uid_get_email (uids->data),
                                               seahorse_pgp_uid_get_comment (uids->data)) : g_strdup ("");
}

static gchar *
calc_markup (SeahorsePgpKey *self)
{
    guint flags = seahorse_object_get_flags (SEAHORSE_OBJECT (self));
    GList *uids;
    GString *result;
    gchar *text;
    const gchar *name;
    const gchar *email;
    const gchar *comment;
    const gchar *primary = NULL;
    guint i = 0;

    uids = seahorse_pgp_key_get_uids (self);

    result = g_string_new ("<span");
    if (flags & SEAHORSE_FLAG_EXPIRED || flags & SEAHORSE_FLAG_REVOKED ||
        flags & SEAHORSE_FLAG_DISABLED)
        g_string_append (result, " strikethrough='true'");
    if (!(flags & SEAHORSE_FLAG_TRUSTED))
        g_string_append (result, "  foreground='#555555'");
    g_string_append_c (result, '>');

    /* The first name is the key name */
    if (uids != NULL) {
        name = seahorse_pgp_uid_get_name (uids->data);
        text = g_markup_escape_text (name, -1);
        g_string_append (result, text);
        g_free (text);
        primary = name;
    }

    g_string_append (result, "<span size='small' rise='0'>");
    if (uids != NULL) {
        email = seahorse_pgp_uid_get_email (uids->data);
        if (email && !email[0])
            email = NULL;
        comment = seahorse_pgp_uid_get_comment (uids->data);
        if (comment && !comment[0])
            comment = NULL;
        text = g_markup_printf_escaped ("\n%s%s%s%s%s",
                                        email ? email : "",
                                        email ? " " : "",
                                        comment ? "'" : "",
                                        comment ? comment : "",
                                        comment ? "'" : "");
        g_string_append (result, text);
        g_free (text);
        uids = uids->next;
    }

    for (i = 0; uids != NULL; i++, uids = g_list_next (uids)) {
        g_string_append_c (result, '\n');

        // If we already have more than 5 UIDs, ellipsze the list.
        // Otherwise we get huge rows in the list of GPG keys
        if (i == 4) {
            int n_others = g_list_length (uids);
            g_autofree char *others_str = NULL;

            g_string_append_printf (result,
                                    ngettext ("(and %d other)",
                                              "(and %d others)",
                                              n_others),
                                    n_others);

            break;
        }

        name = seahorse_pgp_uid_get_name (uids->data);
        if (name && !name[0])
            name = NULL;
        if (g_strcmp0 (name, primary) == 0)
            name = NULL;
        email = seahorse_pgp_uid_get_email (uids->data);
        if (email && !email[0])
            email = NULL;
        comment = seahorse_pgp_uid_get_comment (uids->data);
        if (comment && !comment[0])
            comment = NULL;
        text = g_markup_printf_escaped ("%s%s%s%s%s%s%s",
                                        name ? name : "",
                                        name ? ": " : "",
                                        email ? email : "",
                                        email ? " " : "",
                                        comment ? "'" : "",
                                        comment ? comment : "",
                                        comment ? "'" : "");
        g_string_append (result, text);
        g_free (text);
        uids = uids->next;
    }

    g_string_append (result, "</span></span>");

    return g_string_free (result, FALSE);
}

/* -----------------------------------------------------------------------------
 * OBJECT
 */

static GList*
_seahorse_pgp_key_get_uids (SeahorsePgpKey *self)
{
    SeahorsePgpKeyPrivate *priv = seahorse_pgp_key_get_instance_private (self);

    g_return_val_if_fail (SEAHORSE_PGP_IS_KEY (self), NULL);
    return priv->uids;
}

static void
_seahorse_pgp_key_set_uids (SeahorsePgpKey *self, GList *uids)
{
    SeahorsePgpKeyPrivate *priv = seahorse_pgp_key_get_instance_private (self);

    g_return_if_fail (SEAHORSE_PGP_IS_KEY (self));

    seahorse_object_list_free (priv->uids);
    priv->uids = seahorse_object_list_copy (uids);

    g_object_notify_by_pspec (G_OBJECT (self), obj_props[PROP_UIDS]);
}

static GList*
_seahorse_pgp_key_get_subkeys (SeahorsePgpKey *self)
{
    SeahorsePgpKeyPrivate *priv = seahorse_pgp_key_get_instance_private (self);

    g_return_val_if_fail (SEAHORSE_PGP_IS_KEY (self), NULL);
    return priv->subkeys;
}

static void
_seahorse_pgp_key_set_subkeys (SeahorsePgpKey *self, GList *subkeys)
{
    SeahorsePgpKeyPrivate *priv = seahorse_pgp_key_get_instance_private (self);
    const gchar *keyid = NULL;

    g_return_if_fail (SEAHORSE_PGP_IS_KEY (self));
    g_return_if_fail (subkeys);

    keyid = seahorse_pgp_subkey_get_keyid (subkeys->data);
    g_return_if_fail (keyid);

    /* The keyid can't change */
    if (priv->keyid) {
        if (g_strcmp0 (priv->keyid, keyid) != 0) {
            g_warning ("The keyid of a SeahorsePgpKey changed by "
                       "setting a different subkey on it: %s != %s",
                       priv->keyid, keyid);
            return;
        }

    } else {
        priv->keyid = g_strdup (keyid);
    }

    seahorse_object_list_free (priv->subkeys);
    priv->subkeys = seahorse_object_list_copy (subkeys);

    g_object_notify_by_pspec (G_OBJECT (self), obj_props[PROP_SUBKEYS]);
}

static GList*
_seahorse_pgp_key_get_photos (SeahorsePgpKey *self)
{
    SeahorsePgpKeyPrivate *priv = seahorse_pgp_key_get_instance_private (self);

    g_return_val_if_fail (SEAHORSE_PGP_IS_KEY (self), NULL);
    return priv->photos;
}

static void
_seahorse_pgp_key_set_photos (SeahorsePgpKey *self, GList *photos)
{
    SeahorsePgpKeyPrivate *priv = seahorse_pgp_key_get_instance_private (self);

    g_return_if_fail (SEAHORSE_PGP_IS_KEY (self));

    seahorse_object_list_free (priv->photos);
    priv->photos = seahorse_object_list_copy (photos);

    g_object_notify_by_pspec (G_OBJECT (self), obj_props[PROP_PHOTOS]);
}

void
seahorse_pgp_key_realize (SeahorsePgpKey *self)
{
    const gchar *nickname, *keyid;
    const gchar *icon_name;
    gchar *markup, *name;
    const char *identifier;
    SeahorseUsage usage;
    GList *subkeys;
    GIcon *icon;

    subkeys = seahorse_pgp_key_get_subkeys (self);
    if (subkeys) {
        keyid = seahorse_pgp_subkey_get_keyid (subkeys->data);
        identifier = seahorse_pgp_key_calc_identifier (keyid);
    } else {
        identifier = "";
    }

    name = calc_name (self);
    markup = calc_markup (self);
    nickname = calc_short_name (self);

    g_object_get (self, "usage", &usage, NULL);

    /* The type */
    if (usage == SEAHORSE_USAGE_PRIVATE_KEY) {
        icon_name = GCR_ICON_KEY_PAIR;
    } else {
        icon_name = GCR_ICON_KEY;
        if (usage == SEAHORSE_USAGE_NONE)
            g_object_set (self, "usage", SEAHORSE_USAGE_PUBLIC_KEY, NULL);
    }

    icon = g_themed_icon_new (icon_name);
    g_object_set (self,
              "label", name,
              "markup", markup,
              "nickname", nickname,
              "identifier", identifier,
              "icon", icon,
              NULL);

    g_object_unref (icon);
    g_free (markup);
    g_free (name);
}

static GtkWindow *
seahorse_pgp_key_create_viewer (SeahorseViewable *viewable,
                                GtkWindow *parent)
{
    SeahorsePgpKey *self = SEAHORSE_PGP_KEY (viewable);
    g_autoptr(GtkWindow) viewer = NULL;

    viewer = seahorse_pgp_key_properties_new (self, parent);
    g_object_ref_sink (viewer);
    return GTK_WINDOW (g_steal_pointer (&viewer));
}

static void
seahorse_pgp_key_viewable_iface (SeahorseViewableIface *iface)
{
    iface->create_viewer = seahorse_pgp_key_create_viewer;
}

const char*
seahorse_pgp_key_calc_identifier (const char *keyid)
{
    guint len;

    g_return_val_if_fail (keyid, NULL);

    len = strlen (keyid);
    if (len > 16)
        keyid += len - 16;

    return keyid;
}

GList*
seahorse_pgp_key_get_uids (SeahorsePgpKey *self)
{
    g_return_val_if_fail (SEAHORSE_PGP_IS_KEY (self), NULL);
    if (!SEAHORSE_PGP_KEY_GET_CLASS (self)->get_uids)
        return NULL;
    return SEAHORSE_PGP_KEY_GET_CLASS (self)->get_uids (self);
}

void
seahorse_pgp_key_set_uids (SeahorsePgpKey *self, GList *uids)
{
    g_return_if_fail (SEAHORSE_PGP_IS_KEY (self));
    g_return_if_fail (SEAHORSE_PGP_KEY_GET_CLASS (self)->set_uids);
    SEAHORSE_PGP_KEY_GET_CLASS (self)->set_uids (self, uids);
}

GList*
seahorse_pgp_key_get_subkeys (SeahorsePgpKey *self)
{
    g_return_val_if_fail (SEAHORSE_PGP_IS_KEY (self), NULL);
    if (!SEAHORSE_PGP_KEY_GET_CLASS (self)->get_subkeys)
        return NULL;
    return SEAHORSE_PGP_KEY_GET_CLASS (self)->get_subkeys (self);
}

void
seahorse_pgp_key_set_subkeys (SeahorsePgpKey *self, GList *subkeys)
{
    g_return_if_fail (SEAHORSE_PGP_IS_KEY (self));
    g_return_if_fail (SEAHORSE_PGP_KEY_GET_CLASS (self)->set_subkeys);
    SEAHORSE_PGP_KEY_GET_CLASS (self)->set_subkeys (self, subkeys);
}

GList*
seahorse_pgp_key_get_photos (SeahorsePgpKey *self)
{
    g_return_val_if_fail (SEAHORSE_PGP_IS_KEY (self), NULL);
    if (!SEAHORSE_PGP_KEY_GET_CLASS (self)->get_photos)
        return NULL;
    return SEAHORSE_PGP_KEY_GET_CLASS (self)->get_photos (self);
}

void
seahorse_pgp_key_set_photos (SeahorsePgpKey *self, GList *photos)
{
    g_return_if_fail (SEAHORSE_PGP_IS_KEY (self));
    g_return_if_fail (SEAHORSE_PGP_KEY_GET_CLASS (self)->set_photos);
    SEAHORSE_PGP_KEY_GET_CLASS (self)->set_photos (self, photos);
}

const gchar*
seahorse_pgp_key_get_fingerprint (SeahorsePgpKey *self)
{
    GList *subkeys;

    g_return_val_if_fail (SEAHORSE_PGP_IS_KEY (self), NULL);

    subkeys = seahorse_pgp_key_get_subkeys (self);
    if (!subkeys)
        return "";

    return seahorse_pgp_subkey_get_fingerprint (subkeys->data);
}

SeahorseValidity
seahorse_pgp_key_get_validity (SeahorsePgpKey *self)
{
    guint validity = SEAHORSE_VALIDITY_UNKNOWN;
    g_object_get (self, "validity", &validity, NULL);
    return validity;
}

gulong
seahorse_pgp_key_get_expires (SeahorsePgpKey *self)
{
    GList *subkeys;

    g_return_val_if_fail (SEAHORSE_PGP_IS_KEY (self), 0);

    subkeys = seahorse_pgp_key_get_subkeys (self);
    if (!subkeys)
        return 0;

    return seahorse_pgp_subkey_get_expires (subkeys->data);
}

SeahorseValidity
seahorse_pgp_key_get_trust (SeahorsePgpKey *self)
{
    guint trust = SEAHORSE_VALIDITY_UNKNOWN;
    g_object_get (self, "trust", &trust, NULL);
    return trust;
}

guint
seahorse_pgp_key_get_length (SeahorsePgpKey *self)
{
    GList *subkeys;

    g_return_val_if_fail (SEAHORSE_PGP_IS_KEY (self), 0);

    subkeys = seahorse_pgp_key_get_subkeys (self);
    if (!subkeys)
        return 0;

    return seahorse_pgp_subkey_get_length (subkeys->data);
}

const gchar*
seahorse_pgp_key_get_algo (SeahorsePgpKey *self)
{
    GList *subkeys;

    g_return_val_if_fail (SEAHORSE_PGP_IS_KEY (self), 0);

    subkeys = seahorse_pgp_key_get_subkeys (self);
    if (!subkeys)
        return 0;

    return seahorse_pgp_subkey_get_algorithm (subkeys->data);
}

const gchar*
seahorse_pgp_key_get_keyid (SeahorsePgpKey *self)
{
    GList *subkeys;

    g_return_val_if_fail (SEAHORSE_PGP_IS_KEY (self), 0);

    subkeys = seahorse_pgp_key_get_subkeys (self);
    if (!subkeys)
        return 0;

    return seahorse_pgp_subkey_get_keyid (subkeys->data);
}

gboolean
seahorse_pgp_key_has_keyid (SeahorsePgpKey *self, const gchar *match)
{
    GList *subkeys, *l;
    SeahorsePgpSubkey *subkey;
    const gchar *keyid;
    guint n_match, n_keyid;

    g_return_val_if_fail (SEAHORSE_PGP_IS_KEY (self), FALSE);
    g_return_val_if_fail (match, FALSE);

    subkeys = seahorse_pgp_key_get_subkeys (self);
    if (!subkeys)
        return FALSE;

    n_match = strlen (match);

    for (l = subkeys; l && (subkey = SEAHORSE_PGP_SUBKEY (l->data)); l = g_list_next (l)) {
        keyid = seahorse_pgp_subkey_get_keyid (subkey);
        g_return_val_if_fail (keyid, FALSE);
        n_keyid = strlen (keyid);
        if (n_match <= n_keyid) {
            keyid += (n_keyid - n_match);
            if (strncmp (keyid, match, n_match) == 0)
                return TRUE;
        }
    }

    return FALSE;
}

static void
seahorse_pgp_key_init (SeahorsePgpKey *self)
{
}

static void
seahorse_pgp_key_get_property (GObject *object, guint prop_id,
                               GValue *value, GParamSpec *pspec)
{
    SeahorsePgpKey *self = SEAHORSE_PGP_KEY (object);
    SeahorseUsage usage;

    switch (prop_id) {
    case PROP_PHOTOS:
        g_value_set_boxed (value, seahorse_pgp_key_get_photos (self));
        break;
    case PROP_SUBKEYS:
        g_value_set_boxed (value, seahorse_pgp_key_get_subkeys (self));
        break;
    case PROP_UIDS:
        g_value_set_boxed (value, seahorse_pgp_key_get_uids (self));
        break;
    case PROP_FINGERPRINT:
        g_value_set_string (value, seahorse_pgp_key_get_fingerprint (self));
        break;
    case PROP_DESCRIPTION:
        g_object_get (self, "usage", &usage, NULL);
        if (usage == SEAHORSE_USAGE_PRIVATE_KEY)
            g_value_set_string (value, _("Personal PGP key"));
        else
            g_value_set_string (value, _("PGP key"));
        break;
    case PROP_EXPIRES:
        g_value_set_ulong (value, seahorse_pgp_key_get_expires (self));
        break;
    case PROP_LENGTH:
        g_value_set_uint (value, seahorse_pgp_key_get_length (self));
        break;
    case PROP_ALGO:
        g_value_set_string (value, seahorse_pgp_key_get_algo (self));
        break;
    case PROP_VALIDITY:
        g_value_set_uint (value, SEAHORSE_VALIDITY_UNKNOWN);
        break;
    case PROP_TRUST:
        g_value_set_uint (value, SEAHORSE_VALIDITY_UNKNOWN);
        break;
    }
}

static void
seahorse_pgp_key_set_property (GObject *object, guint prop_id, const GValue *value,
                               GParamSpec *pspec)
{
    SeahorsePgpKey *self = SEAHORSE_PGP_KEY (object);

    switch (prop_id) {
    case PROP_UIDS:
        seahorse_pgp_key_set_uids (self, g_value_get_boxed (value));
        break;
    case PROP_SUBKEYS:
        seahorse_pgp_key_set_subkeys (self, g_value_get_boxed (value));
        break;
    case PROP_PHOTOS:
        seahorse_pgp_key_set_photos (self, g_value_get_boxed (value));
        break;
    }
}

static void
seahorse_pgp_key_object_dispose (GObject *obj)
{
    SeahorsePgpKey *self = SEAHORSE_PGP_KEY (obj);
    SeahorsePgpKeyPrivate *priv = seahorse_pgp_key_get_instance_private (self);

    g_clear_pointer (&priv->uids, seahorse_object_list_free);
    g_clear_pointer (&priv->photos, seahorse_object_list_free);
    g_clear_pointer (&priv->subkeys, seahorse_object_list_free);

    G_OBJECT_CLASS (seahorse_pgp_key_parent_class)->dispose (obj);
}

static void
seahorse_pgp_key_object_finalize (GObject *obj)
{
    SeahorsePgpKey *self = SEAHORSE_PGP_KEY (obj);
    SeahorsePgpKeyPrivate *priv = seahorse_pgp_key_get_instance_private (self);

    g_free (priv->keyid);
    g_assert (priv->uids == NULL);
    g_assert (priv->photos == NULL);
    g_assert (priv->subkeys == NULL);

    G_OBJECT_CLASS (seahorse_pgp_key_parent_class)->finalize (obj);
}

static void
seahorse_pgp_key_class_init (SeahorsePgpKeyClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

    gobject_class->dispose = seahorse_pgp_key_object_dispose;
    gobject_class->finalize = seahorse_pgp_key_object_finalize;
    gobject_class->set_property = seahorse_pgp_key_set_property;
    gobject_class->get_property = seahorse_pgp_key_get_property;

    klass->get_uids = _seahorse_pgp_key_get_uids;
    klass->set_uids = _seahorse_pgp_key_set_uids;
    klass->get_subkeys = _seahorse_pgp_key_get_subkeys;
    klass->set_subkeys = _seahorse_pgp_key_set_subkeys;
    klass->get_photos = _seahorse_pgp_key_get_photos;
    klass->set_photos = _seahorse_pgp_key_set_photos;

    obj_props[PROP_PHOTOS] =
        g_param_spec_boxed ("photos", "Key Photos", "Photos for the key",
                            SEAHORSE_BOXED_OBJECT_LIST,
                            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    obj_props[PROP_SUBKEYS] =
        g_param_spec_boxed ("subkeys", "PGP subkeys", "PGP subkeys",
                            SEAHORSE_BOXED_OBJECT_LIST,
                            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    obj_props[PROP_UIDS] =
        g_param_spec_boxed ("uids", "PGP User Ids", "PGP User Ids",
                            SEAHORSE_BOXED_OBJECT_LIST,
                            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    obj_props[PROP_FINGERPRINT] =
        g_param_spec_string ("fingerprint", "Fingerprint", "Unique fingerprint for this key",
                             "",
                             G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

    obj_props[PROP_DESCRIPTION] =
        g_param_spec_string ("description", "Description", "Description for key",
                             "",
                             G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

    obj_props[PROP_VALIDITY] =
        g_param_spec_uint ("validity", "Validity", "Validity of this key",
                           0, G_MAXUINT, 0,
                           G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

    obj_props[PROP_TRUST] =
        g_param_spec_uint ("trust", "Trust", "Trust in this key",
                           0, G_MAXUINT, 0,
                           G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

    obj_props[PROP_EXPIRES] =
        g_param_spec_ulong ("expires", "Expires On", "Date this key expires on",
                            0, G_MAXULONG, 0,
                            G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

    obj_props[PROP_LENGTH] =
        g_param_spec_uint ("length", "Length", "The length of this key.",
                           0, G_MAXUINT, 0,
                           G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

    obj_props[PROP_ALGO] =
        g_param_spec_string ("algo", "Algorithm", "The algorithm of this key.",
                             "",
                             G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties (gobject_class, N_PROPS, obj_props);
}

SeahorsePgpKey*
seahorse_pgp_key_new (void)
{
    return g_object_new (SEAHORSE_PGP_TYPE_KEY, NULL);
}
