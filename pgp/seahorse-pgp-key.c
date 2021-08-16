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
#include "seahorse-pgp-photo.h"
#include "seahorse-pgp-uid.h"
#include "seahorse-pgp-subkey.h"

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
    char *keyid;
    GListModel *uids;            /* All the UID objects */
    GListModel *subkeys;         /* All the Subkey objects */
    GListModel *photos;          /* List of photos */
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
    const char *keyid = v;
    gsize len = strlen (keyid);
    if (len > 8)
        keyid += len - 8;
    return g_str_hash (keyid);
}

/**
 * seahorse_pgp_keyid_equal:
 * @v1: A fingerprint or key id
 * @v2: A fingerprint or key id
 *
 * Compares key IDs, regardless of casing.
 */
gboolean
seahorse_pgp_keyid_equal (const void *v1,
                          const void *v2)
{
    const char *keyid_1 = v1;
    const char *keyid_2 = v2;
    size_t len_1 = strlen (keyid_1);
    size_t len_2 = strlen (keyid_2);

    /* In case we have a key ids of different length, go to the shortest
     * reliable form: the long key id (16 characters) */
    if (len_1 != len_2 && len_1 >= 16 && len_2 >= 16) {
        keyid_1 += len_1 - 16;
        keyid_2 += len_2 - 16;
    }

    return g_ascii_strcasecmp (keyid_1, keyid_2) == 0;
}

static char*
calc_name (SeahorsePgpKey *self)
{
    SeahorsePgpKeyPrivate *priv = seahorse_pgp_key_get_instance_private (self);
    g_autoptr(SeahorsePgpUid) uid = g_list_model_get_item (priv->uids, 0);

    if (!uid)
        return g_strdup ("");

    return seahorse_pgp_uid_calc_label (seahorse_pgp_uid_get_name (uid),
                                        seahorse_pgp_uid_get_email (uid),
                                        seahorse_pgp_uid_get_comment (uid));
}

static char *
calc_markup (SeahorsePgpKey *self)
{
    SeahorsePgpKeyPrivate *priv = seahorse_pgp_key_get_instance_private (self);
    guint flags = seahorse_object_get_flags (SEAHORSE_OBJECT (self));
    GString *result;
    g_autoptr(SeahorsePgpUid) uid = NULL;
    const char *name;
    g_autofree char *name_escaped = NULL;
    const char *email;
    const char *comment;
    g_autofree char *email_comment = NULL;
    const char *primary = NULL;
    guint n_uids;

    uid = g_list_model_get_item (priv->uids, 0);
    n_uids = g_list_model_get_n_items (priv->uids);

    result = g_string_new ("<span");
    if (flags & SEAHORSE_FLAG_EXPIRED || flags & SEAHORSE_FLAG_REVOKED ||
        flags & SEAHORSE_FLAG_DISABLED)
        g_string_append (result, " strikethrough='true'");
    if (!(flags & SEAHORSE_FLAG_TRUSTED))
        g_string_append (result, "  foreground='#555555'");
    g_string_append_c (result, '>');

    if (!uid)
        goto done;

    /* The first name is the key name */
    name = seahorse_pgp_uid_get_name (uid);
    name_escaped = g_markup_escape_text (name, -1);
    g_string_append (result, name_escaped);
    primary = name;

    g_string_append (result, "<span size='small' rise='0'>");

    email = seahorse_pgp_uid_get_email (uid);
    if (email && !email[0])
        email = NULL;
    comment = seahorse_pgp_uid_get_comment (uid);
    if (comment && !comment[0])
        comment = NULL;
    email_comment = g_markup_printf_escaped ("\n%s%s%s%s%s",
                                             email ? email : "",
                                             email ? " " : "",
                                             comment ? "'" : "",
                                             comment ? comment : "",
                                             comment ? "'" : "");
    g_string_append (result, email_comment);

    /* Now add the other keys */
    for (guint i = 1; i < n_uids; i++) {
        g_autoptr(SeahorsePgpUid) uid = NULL;
        g_autofree char *text = NULL;

        uid = g_list_model_get_item (priv->uids, i);
        g_string_append_c (result, '\n');

        /* If we have 5 UIDs or more, ellipsze the list.
         * Otherwise we get huge rows in the list of GPG keys */
        if (i == 4) {
            int n_others = n_uids - i;
            g_autofree char *others_str = NULL;

            g_string_append_printf (result,
                                    ngettext ("(and %d other)",
                                              "(and %d others)",
                                              n_others),
                                    n_others);

            break;
        }

        name = seahorse_pgp_uid_get_name (uid);
        if (name && !name[0])
            name = NULL;
        if (g_strcmp0 (name, primary) == 0)
            name = NULL;
        email = seahorse_pgp_uid_get_email (uid);
        if (email && !email[0])
            email = NULL;
        comment = seahorse_pgp_uid_get_comment (uid);
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
    }

    g_string_append (result, "</span>");

done:
    g_string_append (result, "</span>");
    return g_string_free (result, FALSE);
}

/* -----------------------------------------------------------------------------
 * OBJECT
 */

void
seahorse_pgp_key_realize (SeahorsePgpKey *self)
{
    SeahorsePgpKeyPrivate *priv = seahorse_pgp_key_get_instance_private (self);
    g_autoptr(SeahorsePgpSubkey) subkey = NULL;
    const char *nickname, *keyid;
    const char *icon_name;
    g_autofree char *name = NULL;
    g_autofree char *markup = NULL;
    const char *identifier;
    SeahorseUsage usage;
    g_autoptr(GIcon) icon = NULL;

    subkey = g_list_model_get_item (priv->subkeys, 0);
    if (subkey) {
        keyid = seahorse_pgp_subkey_get_keyid (subkey);
        identifier = seahorse_pgp_key_calc_identifier (keyid);
    } else {
        identifier = "";
    }

    name = calc_name (self);
    markup = calc_markup (self);
    nickname = seahorse_pgp_key_get_primary_name (self);

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

GListModel *
seahorse_pgp_key_get_uids (SeahorsePgpKey *self)
{
    SeahorsePgpKeyPrivate *priv = seahorse_pgp_key_get_instance_private (self);

    g_return_val_if_fail (SEAHORSE_PGP_IS_KEY (self), NULL);
    return priv->uids;
}

const char *
seahorse_pgp_key_get_primary_name (SeahorsePgpKey *self)
{
    SeahorsePgpKeyPrivate *priv = seahorse_pgp_key_get_instance_private (self);
    g_autoptr(SeahorsePgpUid) uid = NULL;

    g_return_val_if_fail (SEAHORSE_PGP_IS_KEY (self), NULL);

    uid = g_list_model_get_item (priv->uids, 0);
    return uid ? seahorse_pgp_uid_get_name (uid) : NULL;
}

void
seahorse_pgp_key_add_uid (SeahorsePgpKey *self,
                          SeahorsePgpUid *uid)
{
    SeahorsePgpKeyPrivate *priv = seahorse_pgp_key_get_instance_private (self);

    g_return_if_fail (SEAHORSE_PGP_IS_KEY (self));

    /* Don't try to add a key which already exists */
    for (guint i = 0; i < g_list_model_get_n_items (priv->uids); i++) {
        g_autoptr(SeahorsePgpUid) _uid = NULL;

        _uid = g_list_model_get_item (priv->uids, i);
        if (uid == _uid)
            return;
    }

    g_list_store_append (G_LIST_STORE (priv->uids), uid);
}

void
seahorse_pgp_key_remove_uid (SeahorsePgpKey *self,
                             SeahorsePgpUid *uid)
{
    SeahorsePgpKeyPrivate *priv = seahorse_pgp_key_get_instance_private (self);

    g_return_if_fail (SEAHORSE_PGP_IS_KEY (self));

    for (guint i = 0; i < g_list_model_get_n_items (priv->uids); i++) {
        g_autoptr(SeahorsePgpUid) _uid = NULL;

        _uid = g_list_model_get_item (priv->uids, i);
        if (uid == _uid) {
            if (g_list_model_get_n_items (priv->uids) == 1)
                g_warning ("Removing last UID");

            g_list_store_remove (G_LIST_STORE (priv->uids), i);
            break;
        }
    }
}

GListModel *
seahorse_pgp_key_get_subkeys (SeahorsePgpKey *self)
{
    SeahorsePgpKeyPrivate *priv = seahorse_pgp_key_get_instance_private (self);

    g_return_val_if_fail (SEAHORSE_PGP_IS_KEY (self), NULL);
    return priv->subkeys;
}

void
seahorse_pgp_key_add_subkey (SeahorsePgpKey    *self,
                             SeahorsePgpSubkey *subkey)
{
    SeahorsePgpKeyPrivate *priv = seahorse_pgp_key_get_instance_private (self);

    g_return_if_fail (SEAHORSE_PGP_IS_KEY (self));
    g_return_if_fail (SEAHORSE_PGP_IS_SUBKEY (subkey));

    /* Don't try to add a key which already exists */
    for (guint i = 0; i < g_list_model_get_n_items (priv->subkeys); i++) {
        g_autoptr(SeahorsePgpSubkey) _subkey = NULL;

        _subkey = g_list_model_get_item (priv->subkeys, i);
        if (subkey == _subkey)
            return;
    }

    seahorse_pgp_subkey_set_parent_key (subkey, self);
    g_list_store_append (G_LIST_STORE (priv->subkeys), subkey);
}

void
seahorse_pgp_key_remove_subkey (SeahorsePgpKey    *self,
                                SeahorsePgpSubkey *subkey)
{
    SeahorsePgpKeyPrivate *priv = seahorse_pgp_key_get_instance_private (self);

    g_return_if_fail (SEAHORSE_PGP_IS_KEY (self));

    for (guint i = 0; i < g_list_model_get_n_items (priv->subkeys); i++) {
        g_autoptr(SeahorsePgpSubkey) _subkey = NULL;

        _subkey = g_list_model_get_item (priv->subkeys, i);
        if (subkey == _subkey) {
            g_list_store_remove (G_LIST_STORE (priv->subkeys), i);
            break;
        }
    }
}

GListModel *
seahorse_pgp_key_get_photos (SeahorsePgpKey *self)
{
    SeahorsePgpKeyPrivate *priv = seahorse_pgp_key_get_instance_private (self);

    g_return_val_if_fail (SEAHORSE_PGP_IS_KEY (self), NULL);
    return priv->photos;
}

void
seahorse_pgp_key_add_photo (SeahorsePgpKey   *self,
                            SeahorsePgpPhoto *photo)
{
    SeahorsePgpKeyPrivate *priv = seahorse_pgp_key_get_instance_private (self);

    g_return_if_fail (SEAHORSE_PGP_IS_KEY (self));
    g_return_if_fail (SEAHORSE_PGP_IS_PHOTO (photo));

    /* Don't try to add a key which already exists */
    for (guint i = 0; i < g_list_model_get_n_items (priv->photos); i++) {
        g_autoptr(SeahorsePgpPhoto) _photo = NULL;

        _photo = g_list_model_get_item (priv->photos, i);
        if (photo == _photo)
            return;
    }

    g_list_store_append (G_LIST_STORE (priv->photos), photo);
}

void
seahorse_pgp_key_remove_photo (SeahorsePgpKey   *self,
                               SeahorsePgpPhoto *photo)
{
    SeahorsePgpKeyPrivate *priv = seahorse_pgp_key_get_instance_private (self);

    g_return_if_fail (SEAHORSE_PGP_IS_KEY (self));

    for (guint i = 0; i < g_list_model_get_n_items (priv->photos); i++) {
        g_autoptr(SeahorsePgpPhoto) _photo = NULL;

        _photo = g_list_model_get_item (priv->photos, i);
        if (photo == _photo) {
            if (g_list_model_get_n_items (priv->photos) == 1)
                g_warning ("Removing last PHOTO");

            g_list_store_remove (G_LIST_STORE (priv->photos), i);
            break;
        }
    }
}

const char *
seahorse_pgp_key_get_fingerprint (SeahorsePgpKey *self)
{
    SeahorsePgpKeyPrivate *priv = seahorse_pgp_key_get_instance_private (self);
    g_autoptr(SeahorsePgpSubkey) subkey = NULL;

    g_return_val_if_fail (SEAHORSE_PGP_IS_KEY (self), NULL);

    subkey = g_list_model_get_item (priv->subkeys, 0);
    return subkey? seahorse_pgp_subkey_get_fingerprint (subkey) : "";
}

SeahorseValidity
seahorse_pgp_key_get_validity (SeahorsePgpKey *self)
{
    guint validity = SEAHORSE_VALIDITY_UNKNOWN;
    g_object_get (self, "validity", &validity, NULL);
    return validity;
}

GDateTime *
seahorse_pgp_key_get_expires (SeahorsePgpKey *self)
{
    SeahorsePgpKeyPrivate *priv = seahorse_pgp_key_get_instance_private (self);
    g_autoptr(SeahorsePgpSubkey) subkey = NULL;

    g_return_val_if_fail (SEAHORSE_PGP_IS_KEY (self), 0);

    subkey = g_list_model_get_item (priv->subkeys, 0);
    return subkey? seahorse_pgp_subkey_get_expires (subkey) : 0;
}

GDateTime *
seahorse_pgp_key_get_created (SeahorsePgpKey *self)
{
    SeahorsePgpKeyPrivate *priv = seahorse_pgp_key_get_instance_private (self);
    g_autoptr(SeahorsePgpSubkey) subkey = NULL;

    g_return_val_if_fail (SEAHORSE_PGP_IS_KEY (self), 0);

    subkey = g_list_model_get_item (priv->subkeys, 0);
    return subkey? seahorse_pgp_subkey_get_created (subkey) : 0;
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
    SeahorsePgpKeyPrivate *priv = seahorse_pgp_key_get_instance_private (self);
    g_autoptr(SeahorsePgpSubkey) subkey = NULL;

    g_return_val_if_fail (SEAHORSE_PGP_IS_KEY (self), 0);

    subkey = g_list_model_get_item (priv->subkeys, 0);
    return subkey? seahorse_pgp_subkey_get_length (subkey) : 0;
}

const char *
seahorse_pgp_key_get_algo (SeahorsePgpKey *self)
{
    SeahorsePgpKeyPrivate *priv = seahorse_pgp_key_get_instance_private (self);
    g_autoptr(SeahorsePgpSubkey) subkey = NULL;

    g_return_val_if_fail (SEAHORSE_PGP_IS_KEY (self), NULL);

    subkey = g_list_model_get_item (priv->subkeys, 0);
    return subkey? seahorse_pgp_subkey_get_algorithm (subkey) : NULL;
}

const char *
seahorse_pgp_key_get_keyid (SeahorsePgpKey *self)
{
    SeahorsePgpKeyPrivate *priv = seahorse_pgp_key_get_instance_private (self);
    g_autoptr(SeahorsePgpSubkey) subkey = NULL;

    g_return_val_if_fail (SEAHORSE_PGP_IS_KEY (self), NULL);

    subkey = g_list_model_get_item (priv->subkeys, 0);
    return subkey? seahorse_pgp_subkey_get_keyid (subkey) : NULL;
}

gboolean
seahorse_pgp_key_has_keyid (SeahorsePgpKey *self, const char *match)
{
    SeahorsePgpKeyPrivate *priv = seahorse_pgp_key_get_instance_private (self);

    g_return_val_if_fail (SEAHORSE_PGP_IS_KEY (self), FALSE);
    g_return_val_if_fail (match && *match, FALSE);

    for (guint i = 0; i < g_list_model_get_n_items (priv->subkeys); i++) {
        g_autoptr(SeahorsePgpSubkey) subkey = NULL;
        const char *keyid;

        subkey = g_list_model_get_item (priv->subkeys, i);
        keyid = seahorse_pgp_subkey_get_keyid (subkey);
        g_return_val_if_fail (keyid, FALSE);
        if (seahorse_pgp_keyid_equal (keyid, match))
            return TRUE;
    }

    return FALSE;
}

gboolean
seahorse_pgp_key_is_private_key (SeahorsePgpKey *self)
{
    SeahorseUsage usage;

    g_return_val_if_fail (SEAHORSE_PGP_IS_KEY (self), FALSE);

    usage = seahorse_object_get_usage (SEAHORSE_OBJECT (self));
    return usage == SEAHORSE_USAGE_PRIVATE_KEY;
}

static void
seahorse_pgp_key_init (SeahorsePgpKey *self)
{
    SeahorsePgpKeyPrivate *priv = seahorse_pgp_key_get_instance_private (self);

    priv->uids = G_LIST_MODEL (g_list_store_new (SEAHORSE_PGP_TYPE_UID));
    priv->subkeys = G_LIST_MODEL (g_list_store_new (SEAHORSE_PGP_TYPE_SUBKEY));
    priv->photos = G_LIST_MODEL (g_list_store_new (SEAHORSE_PGP_TYPE_PHOTO));
}

static void
seahorse_pgp_key_get_property (GObject *object, guint prop_id,
                               GValue *value, GParamSpec *pspec)
{
    SeahorsePgpKey *self = SEAHORSE_PGP_KEY (object);
    SeahorseUsage usage;

    switch (prop_id) {
    case PROP_PHOTOS:
        g_value_set_object (value, seahorse_pgp_key_get_photos (self));
        break;
    case PROP_SUBKEYS:
        g_value_set_object (value, seahorse_pgp_key_get_subkeys (self));
        break;
    case PROP_UIDS:
        g_value_set_object (value, seahorse_pgp_key_get_uids (self));
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
        g_value_set_boxed (value, seahorse_pgp_key_get_expires (self));
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
seahorse_pgp_key_object_dispose (GObject *obj)
{
    SeahorsePgpKey *self = SEAHORSE_PGP_KEY (obj);
    SeahorsePgpKeyPrivate *priv = seahorse_pgp_key_get_instance_private (self);

    g_clear_object (&priv->uids);
    g_clear_object (&priv->subkeys);
    g_clear_object (&priv->photos);

    G_OBJECT_CLASS (seahorse_pgp_key_parent_class)->dispose (obj);
}

static void
seahorse_pgp_key_object_finalize (GObject *obj)
{
    SeahorsePgpKey *self = SEAHORSE_PGP_KEY (obj);
    SeahorsePgpKeyPrivate *priv = seahorse_pgp_key_get_instance_private (self);

    g_free (priv->keyid);

    G_OBJECT_CLASS (seahorse_pgp_key_parent_class)->finalize (obj);
}

static void
seahorse_pgp_key_class_init (SeahorsePgpKeyClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

    gobject_class->dispose = seahorse_pgp_key_object_dispose;
    gobject_class->finalize = seahorse_pgp_key_object_finalize;
    gobject_class->get_property = seahorse_pgp_key_get_property;

    obj_props[PROP_PHOTOS] =
        g_param_spec_object ("photos", "Key Photos", "Photos for the key",
                             G_TYPE_LIST_MODEL,
                             G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

    obj_props[PROP_SUBKEYS] =
        g_param_spec_object ("subkeys", "PGP subkeys", "PGP subkeys",
                             G_TYPE_LIST_MODEL,
                             G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

    obj_props[PROP_UIDS] =
        g_param_spec_object ("uids", "PGP User Ids", "PGP User Ids",
                             G_TYPE_LIST_MODEL,
                             G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

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
        g_param_spec_boxed ("expires", "Expires On", "Date this key expires on",
                            G_TYPE_DATE_TIME,
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
