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
 * along with this program; if not, see
 * <http://www.gnu.org/licenses/>.
 */
#include "config.h"

#include "seahorse-gpgme.h"
#include "seahorse-gpgme-key-op.h"
#include "seahorse-gpgme-key-delete-operation.h"
#include "seahorse-gpgme-key-export-operation.h"
#include "seahorse-gpgme-photo.h"
#include "seahorse-gpgme-keyring.h"
#include "seahorse-gpgme-uid.h"
#include "seahorse-pgp-backend.h"
#include "seahorse-pgp-key.h"

#include "seahorse-common.h"

#include "libseahorse/seahorse-util.h"

#include <glib/gi18n.h>

#include <string.h>

enum {
    PROP_0,
    PROP_PUBKEY,
    PROP_SECKEY,
    PROP_VALIDITY,
    PROP_TRUST,
    N_PROPS,
    /* override properties */
    PROP_EXPORTABLE,
    PROP_DELETABLE,
};

struct _SeahorseGpgmeKey {
    SeahorsePgpKey parent_instance;

    gpgme_key_t pubkey;          /* The public key */
    gpgme_key_t seckey;          /* The secret key */
    gboolean has_secret;         /* Whether we have a secret key or not */

    int list_mode;               /* What to load our public key as */
    gboolean photos_loaded;      /* Photos were loaded */

    int block_loading;           /* Loading is blocked while this flag is set */
};

static void       seahorse_gpgme_key_deletable_iface       (SeahorseDeletableIface *iface);
static void       seahorse_gpgme_key_exportable_iface      (SeahorseExportableIface *iface);

G_DEFINE_TYPE_WITH_CODE (SeahorseGpgmeKey, seahorse_gpgme_key, SEAHORSE_PGP_TYPE_KEY,
                         G_IMPLEMENT_INTERFACE (SEAHORSE_TYPE_EXPORTABLE, seahorse_gpgme_key_exportable_iface);
                         G_IMPLEMENT_INTERFACE (SEAHORSE_TYPE_DELETABLE, seahorse_gpgme_key_deletable_iface);
);

static gboolean
load_gpgme_key (const gchar *keyid,
                int mode,
                int secret,
                gpgme_key_t *key)
{
    GError *error = NULL;
    gpgme_ctx_t ctx;
    gpgme_error_t gerr;

    ctx = seahorse_gpgme_keyring_new_context (&gerr);
    if (gerr != 0)
        return FALSE;

    gpgme_set_keylist_mode (ctx, mode);
    gerr = gpgme_op_keylist_start (ctx, keyid, secret);
    if (GPG_IS_OK (gerr)) {
        gerr = gpgme_op_keylist_next (ctx, key);
        gpgme_op_keylist_end (ctx);
    }

    gpgme_release (ctx);

    if (seahorse_gpgme_propagate_error (gerr, &error)) {
        g_message ("couldn't load GPGME key: %s", error->message);
        g_clear_error (&error);
        return FALSE;
    }

    return TRUE;
}

static void
load_key_public (SeahorseGpgmeKey *self, int list_mode)
{
    gpgme_key_t key = NULL;
    const gchar *keyid;
    gboolean ret;

    if (self->block_loading)
        return;

    list_mode |= self->list_mode;

    keyid = seahorse_pgp_key_get_keyid (SEAHORSE_PGP_KEY (self));
    ret = load_gpgme_key (keyid, list_mode, FALSE, &key);
    if (ret) {
        self->list_mode = list_mode;
        seahorse_gpgme_key_set_public (self, key);
        gpgme_key_unref (key);
    }
}

static gboolean
require_key_public (SeahorseGpgmeKey *self, int list_mode)
{
    if (!self->pubkey || (self->list_mode & list_mode) != list_mode)
        load_key_public (self, list_mode);
    return self->pubkey && (self->list_mode & list_mode) == list_mode;
}

static void
load_key_private (SeahorseGpgmeKey *self)
{
    gpgme_key_t key = NULL;
    const gchar *keyid;
    gboolean ret;

    if (!self->has_secret || self->block_loading)
        return;

    keyid = seahorse_pgp_key_get_keyid (SEAHORSE_PGP_KEY (self));
    ret = load_gpgme_key (keyid, GPGME_KEYLIST_MODE_LOCAL, TRUE, &key);
    if (ret) {
        seahorse_gpgme_key_set_private (self, key);
        gpgme_key_unref (key);
    }
}

static gboolean
require_key_private (SeahorseGpgmeKey *self)
{
    if (!self->seckey)
        load_key_private (self);
    return self->seckey != NULL;
}

static void
load_key_photos (SeahorseGpgmeKey *self)
{
    gpgme_error_t gerr;

    if (self->block_loading)
        return;

    gerr = seahorse_gpgme_key_op_photos_load (self);
    if (!GPG_IS_OK (gerr))
        g_message ("couldn't load key photos: %s", gpgme_strerror (gerr));
    else
        self->photos_loaded = TRUE;
}

static void
renumber_actual_uids (SeahorseGpgmeKey *self)
{
    GArray *index_map;
    GListModel *photos;
    GListModel *uids;

    g_assert (SEAHORSE_GPGME_IS_KEY (self));

    /*
     * This function is necessary because the uid stored in a gpgme_user_id_t
     * struct is only usable with gpgme functions.  Problems will be caused if
     * that uid is used with functions found in seahorse-gpgme-key-op.h.  This
     * function is only to be called with uids from gpgme_user_id_t structs.
     */

    ++self->block_loading;
    photos = seahorse_pgp_key_get_photos (SEAHORSE_PGP_KEY (self));
    uids = seahorse_pgp_key_get_uids (SEAHORSE_PGP_KEY (self));
    --self->block_loading;

    /* First we build a bitmap of where all the photo uid indexes are */
    index_map = g_array_new (FALSE, TRUE, sizeof (gboolean));
    for (guint i = 0; i < g_list_model_get_n_items (photos); i++) {
        g_autoptr(SeahorseGpgmePhoto) photo = g_list_model_get_item (photos, i);
        guint index;

        index = seahorse_gpgme_photo_get_index (photo);
        if (index >= index_map->len)
            g_array_set_size (index_map, index + 1);
        g_array_index (index_map, gboolean, index) = TRUE;
    }

    /* Now for each UID we add however many photo indexes are below the gpgme index */
    for (guint i = 0; i < g_list_model_get_n_items (uids); i++) {
        g_autoptr(SeahorseGpgmeUid) uid = g_list_model_get_item (uids, i);
        guint index;

        index = seahorse_gpgme_uid_get_gpgme_index (uid);
        for (guint j = 0; j < index_map->len && j < index; ++j) {
            if (g_array_index (index_map, gboolean, index))
                ++index;
        }
        seahorse_gpgme_uid_set_actual_index (uid, index + 1);
    }

    g_array_free (index_map, TRUE);
}

static void
realize_uids (SeahorseGpgmeKey *self)
{
    GListModel *uids;
    guint n_uids;
    gpgme_user_id_t guid;
    g_autoptr(GPtrArray) results = NULL;
    gboolean changed = FALSE;

    uids = seahorse_pgp_key_get_uids (SEAHORSE_PGP_KEY (self));
    n_uids = g_list_model_get_n_items (uids);
    guid = self->pubkey ? self->pubkey->uids : NULL;
    results = g_ptr_array_new_with_free_func (g_object_unref);

    /* Look for out of sync or missing UIDs */
    for (guint i = 0; i < n_uids; i++) {
        g_autoptr(SeahorseGpgmeUid) uid = g_list_model_get_item (uids, i);

        /* Bring this UID up to date */
        if (guid && seahorse_gpgme_uid_is_same (uid, guid)) {
            if (seahorse_gpgme_uid_get_userid (uid) != guid) {
                g_object_set (uid, "pubkey", self->pubkey, "userid", guid, NULL);
                changed = TRUE;
            }
            g_ptr_array_add (results, g_steal_pointer (&uid));
            guid = guid->next;
        }
    }

    /* Add new UIDs */
    while (guid != NULL) {
        g_autoptr(SeahorseGpgmeUid) uid = seahorse_gpgme_uid_new (self, guid);
        changed = TRUE;
        g_ptr_array_add (results, g_steal_pointer (&uid));
        guid = guid->next;
    }

    if (!changed)
        return;

    /* Don't use remove_uid() and add_uid(), or we can get into an inconsistent
     * state where we have no UIDs at all, but do it atomically by splicing */
    /* FIXME: we should do this cleanly, without casting to GListStore */
    g_list_store_splice (G_LIST_STORE (uids), 0, n_uids, results->pdata, results->len);
}

static void
realize_subkeys (SeahorseGpgmeKey *self)
{
    g_autoptr(GPtrArray) results = NULL;
    GListModel *subkeys;
    guint n_subkeys;
    gpgme_subkey_t gsubkey;

    results = g_ptr_array_new_with_free_func (g_object_unref);
    subkeys = seahorse_pgp_key_get_subkeys (SEAHORSE_PGP_KEY (self));
    n_subkeys = g_list_model_get_n_items (subkeys);

    if (self->pubkey) {
        for (gsubkey = self->pubkey->subkeys; gsubkey; gsubkey = gsubkey->next) {
            g_autoptr(SeahorseGpgmeSubkey) subkey = NULL;

            subkey = seahorse_gpgme_subkey_new (self, gsubkey);
            g_ptr_array_add (results, g_steal_pointer (&subkey));
        }
    }

    /* Don't use remove/add_subkey(), or we can get into an inconsistent state
     * where we have no subkeys at all, but do it atomically by splicing */
    /* FIXME: we should do this cleanly, without casting to GListStore */
    g_list_store_splice (G_LIST_STORE (subkeys),
                         0, n_subkeys,
                         results->pdata, results->len);
}

void
seahorse_gpgme_key_realize (SeahorseGpgmeKey *self)
{
    SeahorseUsage usage;
    guint flags = 0;

    if (!self->pubkey)
        return;

    if (!require_key_public (self, GPGME_KEYLIST_MODE_LOCAL))
        g_return_if_reached ();

    g_return_if_fail (self->pubkey);
    g_return_if_fail (self->pubkey->subkeys);

    /* Update the sub UIDs */
    realize_uids (self);
    realize_subkeys (self);

    if (!self->pubkey->disabled && !self->pubkey->expired &&
        !self->pubkey->revoked && !self->pubkey->invalid) {
        if (seahorse_gpgme_key_get_validity (self) >= SEAHORSE_VALIDITY_MARGINAL)
            flags |= SEAHORSE_FLAG_IS_VALID;
        if (self->pubkey->can_encrypt)
            flags |= SEAHORSE_FLAG_CAN_ENCRYPT;
        if (self->seckey && self->pubkey->can_sign)
            flags |= SEAHORSE_FLAG_CAN_SIGN;
    }

    if (self->pubkey->expired)
        flags |= SEAHORSE_FLAG_EXPIRED;

    if (self->pubkey->revoked)
        flags |= SEAHORSE_FLAG_REVOKED;

    if (self->pubkey->disabled)
        flags |= SEAHORSE_FLAG_DISABLED;

    if (seahorse_gpgme_key_get_trust (self) >= SEAHORSE_VALIDITY_MARGINAL &&
        !self->pubkey->revoked && !self->pubkey->disabled &&
        !self->pubkey->expired)
        flags |= SEAHORSE_FLAG_TRUSTED;

    /* The type */
    if (self->seckey) {
        usage = SEAHORSE_USAGE_PRIVATE_KEY;
        flags |= SEAHORSE_FLAG_PERSONAL;
    } else {
        usage = SEAHORSE_USAGE_PUBLIC_KEY;
    }

    seahorse_pgp_key_set_usage (SEAHORSE_PGP_KEY (self), usage);
    seahorse_pgp_key_set_item_flags (SEAHORSE_PGP_KEY (self), flags);
}

void
seahorse_gpgme_key_ensure_signatures (SeahorseGpgmeKey *self)
{
    require_key_public (self, GPGME_KEYLIST_MODE_LOCAL | GPGME_KEYLIST_MODE_SIGS);
}

void
seahorse_gpgme_key_refresh (SeahorseGpgmeKey *self)
{
    if (self->pubkey)
        load_key_public (self, self->list_mode);
    if (self->seckey)
        load_key_private (self);
    if (self->photos_loaded)
        load_key_photos (self);
}

static SeahorseDeleteOperation *
seahorse_gpgme_key_create_delete_operation (SeahorseDeletable *deletable)
{
    SeahorseGpgmeKey *self = SEAHORSE_GPGME_KEY (deletable);
    g_autoptr(SeahorseGpgmeKeyDeleteOperation) delete_op = NULL;

    delete_op = seahorse_gpgme_key_delete_operation_new (self);
    return SEAHORSE_DELETE_OPERATION (g_steal_pointer (&delete_op));
}

static gboolean
seahorse_gpgme_key_get_deletable (SeahorseDeletable *deletable)
{
    return TRUE;
}

static void
seahorse_gpgme_key_deletable_iface (SeahorseDeletableIface *iface)
{
    iface->create_delete_operation = seahorse_gpgme_key_create_delete_operation;
    iface->get_deletable = seahorse_gpgme_key_get_deletable;
}

static SeahorseExportOperation *
seahorse_gpgme_key_create_export_operation (SeahorseExportable *exportable)
{
    SeahorseGpgmeKey *self = SEAHORSE_GPGME_KEY (exportable);

    return seahorse_gpgme_key_export_operation_new (self, TRUE, FALSE);
}

static gboolean
seahorse_gpgme_key_get_exportable (SeahorseExportable *exportable)
{
    return TRUE;
}

static void
seahorse_gpgme_key_exportable_iface (SeahorseExportableIface *iface)
{
    iface->create_export_operation = seahorse_gpgme_key_create_export_operation;
    iface->get_exportable = seahorse_gpgme_key_get_exportable;
}

gpgme_key_t
seahorse_gpgme_key_get_public (SeahorseGpgmeKey *self)
{
    g_return_val_if_fail (SEAHORSE_GPGME_IS_KEY (self), NULL);
    if (require_key_public (self, GPGME_KEYLIST_MODE_LOCAL))
        return self->pubkey;
    return NULL;
}

void
seahorse_gpgme_key_set_public (SeahorseGpgmeKey *self, gpgme_key_t key)
{
    GObject *obj;

    g_return_if_fail (SEAHORSE_GPGME_IS_KEY (self));

    if (key == self->pubkey)
        return;

    if (self->pubkey)
        gpgme_key_unref (self->pubkey);
    self->pubkey = key;
    if (self->pubkey) {
        gpgme_key_ref (self->pubkey);
        self->list_mode |= self->pubkey->keylist_mode;
    }

    obj = G_OBJECT (self);
    g_object_freeze_notify (obj);
    seahorse_gpgme_key_realize (self);
    g_object_notify (obj, "fingerprint");
    g_object_notify (obj, "validity");
    g_object_notify (obj, "trust");
    g_object_notify (obj, "expires");
    g_object_notify (obj, "length");
    g_object_notify (obj, "algo");
    g_object_thaw_notify (obj);
}

gpgme_key_t
seahorse_gpgme_key_get_private (SeahorseGpgmeKey *self)
{
    g_return_val_if_fail (SEAHORSE_GPGME_IS_KEY (self), NULL);
    if (require_key_private (self))
        return self->seckey;
    return NULL;
}

void
seahorse_gpgme_key_set_private (SeahorseGpgmeKey *self, gpgme_key_t key)
{
    GObject *obj = G_OBJECT (self);;

    g_return_if_fail (SEAHORSE_GPGME_IS_KEY (self));

    if (key == self->seckey)
        return;

    if (self->seckey)
        gpgme_key_unref (self->seckey);
    self->seckey = key;
    if (self->seckey)
        gpgme_key_ref (self->seckey);

    g_object_freeze_notify (obj);
    seahorse_gpgme_key_realize (self);
    g_object_thaw_notify (obj);
}

SeahorseValidity
seahorse_gpgme_key_get_validity (SeahorseGpgmeKey *self)
{
    g_return_val_if_fail (SEAHORSE_GPGME_IS_KEY (self), SEAHORSE_VALIDITY_UNKNOWN);

    if (!require_key_public (self, GPGME_KEYLIST_MODE_LOCAL))
        return SEAHORSE_VALIDITY_UNKNOWN;

    g_return_val_if_fail (self->pubkey, SEAHORSE_VALIDITY_UNKNOWN);
    g_return_val_if_fail (self->pubkey->uids, SEAHORSE_VALIDITY_UNKNOWN);

    if (self->pubkey->revoked)
        return SEAHORSE_VALIDITY_REVOKED;
    if (self->pubkey->disabled)
        return SEAHORSE_VALIDITY_DISABLED;
    return seahorse_gpgme_convert_validity (self->pubkey->uids->validity);
}

SeahorseValidity
seahorse_gpgme_key_get_trust (SeahorseGpgmeKey *self)
{
    g_return_val_if_fail (SEAHORSE_GPGME_IS_KEY (self), SEAHORSE_VALIDITY_UNKNOWN);
    if (!require_key_public (self, GPGME_KEYLIST_MODE_LOCAL))
        return SEAHORSE_VALIDITY_UNKNOWN;

    return seahorse_gpgme_convert_validity (self->pubkey->owner_trust);
}

void
seahorse_gpgme_key_refresh_matching (gpgme_key_t key)
{
    SeahorseGpgmeKey *gkey;

    g_return_if_fail (key->subkeys->keyid);

    gkey = seahorse_gpgme_keyring_lookup (seahorse_pgp_backend_get_default_keyring (NULL),
                                          key->subkeys->keyid);
    if (gkey != NULL)
        seahorse_gpgme_key_refresh (gkey);
}

static void
seahorse_gpgme_key_init (SeahorseGpgmeKey *self)
{
}

static void
on_photos_changed (GListModel *list,
                   guint       position,
                   guint       removed,
                   guint       added,
                   gpointer    user_data)
{
    SeahorseGpgmeKey *self = SEAHORSE_GPGME_KEY (user_data);

    renumber_actual_uids (self);
}

static void
on_uids_changed (GListModel *list,
                 guint       position,
                 guint       removed,
                 guint       added,
                 gpointer    user_data)
{
    SeahorseGpgmeKey *self = SEAHORSE_GPGME_KEY (user_data);

    renumber_actual_uids (self);
}

static void
seahorse_gpgme_key_object_constructed (GObject *object)
{
    SeahorseGpgmeKey *self = SEAHORSE_GPGME_KEY (object);
    GListModel *uids;
    GListModel *photos;

    G_OBJECT_CLASS (seahorse_gpgme_key_parent_class)->constructed (object);

    uids = seahorse_pgp_key_get_uids (SEAHORSE_PGP_KEY (self));
    g_signal_connect (uids, "items-changed", G_CALLBACK (on_uids_changed), self);
    photos = seahorse_pgp_key_get_photos (SEAHORSE_PGP_KEY (self));
    g_signal_connect (photos, "items-changed", G_CALLBACK (on_photos_changed), self);
    load_key_photos (self);

    seahorse_gpgme_key_realize (self);
}

static void
seahorse_gpgme_key_get_property (GObject *object, guint prop_id,
                                 GValue *value, GParamSpec *pspec)
{
    SeahorseGpgmeKey *self = SEAHORSE_GPGME_KEY (object);
    SeahorseExportable *exportable = SEAHORSE_EXPORTABLE (self);
    SeahorseDeletable *deletable = SEAHORSE_DELETABLE (self);

    switch (prop_id) {
    case PROP_PUBKEY:
        g_value_set_boxed (value, seahorse_gpgme_key_get_public (self));
        break;
    case PROP_SECKEY:
        g_value_set_boxed (value, seahorse_gpgme_key_get_private (self));
        break;
    case PROP_VALIDITY:
        g_value_set_uint (value, seahorse_gpgme_key_get_validity (self));
        break;
    case PROP_TRUST:
        g_value_set_uint (value, seahorse_gpgme_key_get_trust (self));
        break;
    case PROP_EXPORTABLE:
        g_value_set_boolean (value, seahorse_gpgme_key_get_exportable (exportable));
        break;
    case PROP_DELETABLE:
        g_value_set_boolean (value, seahorse_gpgme_key_get_deletable (deletable));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
seahorse_gpgme_key_set_property (GObject *object, guint prop_id, const GValue *value,
                                 GParamSpec *pspec)
{
    SeahorseGpgmeKey *self = SEAHORSE_GPGME_KEY (object);

    switch (prop_id) {
    case PROP_PUBKEY:
        seahorse_gpgme_key_set_public (self, g_value_get_boxed (value));
        break;
    case PROP_SECKEY:
        seahorse_gpgme_key_set_private (self, g_value_get_boxed (value));
        break;
    }
}

static void
seahorse_gpgme_key_object_dispose (GObject *obj)
{
    SeahorseGpgmeKey *self = SEAHORSE_GPGME_KEY (obj);

    if (self->pubkey)
        gpgme_key_unref (self->pubkey);
    if (self->seckey)
        gpgme_key_unref (self->seckey);
    self->pubkey = self->seckey = NULL;

    G_OBJECT_CLASS (seahorse_gpgme_key_parent_class)->dispose (obj);
}

static void
seahorse_gpgme_key_object_finalize (GObject *obj)
{
    SeahorseGpgmeKey *self = SEAHORSE_GPGME_KEY (obj);

    g_assert (self->pubkey == NULL);
    g_assert (self->seckey == NULL);

    G_OBJECT_CLASS (seahorse_gpgme_key_parent_class)->finalize (G_OBJECT (self));
}

static void
seahorse_gpgme_key_class_init (SeahorseGpgmeKeyClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

    gobject_class->constructed = seahorse_gpgme_key_object_constructed;
    gobject_class->dispose = seahorse_gpgme_key_object_dispose;
    gobject_class->finalize = seahorse_gpgme_key_object_finalize;
    gobject_class->set_property = seahorse_gpgme_key_set_property;
    gobject_class->get_property = seahorse_gpgme_key_get_property;

    g_object_class_install_property (gobject_class, PROP_PUBKEY,
        g_param_spec_boxed ("pubkey", "GPGME Public Key", "GPGME Public Key that this object represents",
                            SEAHORSE_GPGME_BOXED_KEY,
                            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property (gobject_class, PROP_SECKEY,
        g_param_spec_boxed ("seckey", "GPGME Secret Key", "GPGME Secret Key that this object represents",
                            SEAHORSE_GPGME_BOXED_KEY,
                            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_override_property (gobject_class, PROP_VALIDITY, "validity");
    g_object_class_override_property (gobject_class, PROP_TRUST, "trust");
    g_object_class_override_property (gobject_class, PROP_EXPORTABLE, "exportable");
    g_object_class_override_property (gobject_class, PROP_DELETABLE, "deletable");
}

SeahorseGpgmeKey*
seahorse_gpgme_key_new (SeahorsePlace *sksrc,
                        gpgme_key_t pubkey,
                        gpgme_key_t seckey)
{
    g_return_val_if_fail (pubkey || seckey, NULL);

    return g_object_new (SEAHORSE_GPGME_TYPE_KEY,
                         "place", sksrc,
                         "pubkey", pubkey,
                         "seckey", seckey,
                         NULL);
}
