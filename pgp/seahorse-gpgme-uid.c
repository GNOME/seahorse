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
 * along with this program; if not, see
 * <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "seahorse-gpgme.h"
#include "seahorse-gpgme-uid.h"
#include "seahorse-gpgme-uid-delete-operation.h"
#include "seahorse-pgp-key.h"
#include "seahorse-pgp-signature.h"

#include <string.h>

#include <glib/gi18n.h>

enum {
    PROP_0,
    PROP_PUBKEY,
    PROP_USERID,
    PROP_GPGME_INDEX,
    PROP_ACTUAL_INDEX,
    /* override properties */
    PROP_DELETABLE,
};

struct _SeahorseGpgmeUid {
    SeahorsePgpUid parent_instance;

    gpgme_key_t pubkey;         /* The public key that this uid is part of */
    gpgme_user_id_t userid;     /* The userid referred to */
    guint gpgme_index;          /* The GPGME index of the UID */
    int actual_index;           /* The actual index of this UID */
};

static void seahorse_gpgme_uid_deletable_iface (SeahorseDeletableIface *iface);

G_DEFINE_TYPE_WITH_CODE (SeahorseGpgmeUid, seahorse_gpgme_uid, SEAHORSE_PGP_TYPE_UID,
                         G_IMPLEMENT_INTERFACE (SEAHORSE_TYPE_DELETABLE, seahorse_gpgme_uid_deletable_iface));

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

static void
realize_signatures (SeahorseGpgmeUid *self)
{
    gpgme_key_sig_t gsig;
    guint flags;

    g_return_if_fail (self->pubkey);
    g_return_if_fail (self->userid);

    /* If this key was loaded without signatures, then leave them as is */
    if ((self->pubkey->keylist_mode & GPGME_KEYLIST_MODE_SIGS) == 0)
        return;

    for (gsig = self->userid->signatures; gsig; gsig = gsig->next) {
        g_autoptr(SeahorsePgpSignature) sig = NULL;

        sig = seahorse_pgp_signature_new (gsig->keyid);

        /* Order of parsing these flags is important */
        flags = 0;
        if (gsig->revoked)
            flags |= SEAHORSE_FLAG_REVOKED;
        if (gsig->expired)
            flags |= SEAHORSE_FLAG_EXPIRED;
        if (flags == 0 && !gsig->invalid)
            flags = SEAHORSE_FLAG_IS_VALID;
        /* we don't use gsig->exportable, since we don't provide that option */

        seahorse_pgp_signature_set_flags (sig, flags);
        seahorse_pgp_uid_add_signature (SEAHORSE_PGP_UID (self), sig);
    }
}

static gboolean
compare_pubkeys (gpgme_key_t a, gpgme_key_t b)
{
    g_assert (a);
    g_assert (b);

    g_return_val_if_fail (a->subkeys, FALSE);
    g_return_val_if_fail (b->subkeys, FALSE);

    return g_strcmp0 (a->subkeys->keyid, b->subkeys->keyid) == 0;
}

/* -----------------------------------------------------------------------------
 * OBJECT
 */

gpgme_key_t
seahorse_gpgme_uid_get_pubkey (SeahorseGpgmeUid *self)
{
    g_return_val_if_fail (SEAHORSE_GPGME_IS_UID (self), NULL);
    g_return_val_if_fail (self->pubkey, NULL);
    return self->pubkey;
}

gpgme_user_id_t
seahorse_gpgme_uid_get_userid (SeahorseGpgmeUid *self)
{
    g_return_val_if_fail (SEAHORSE_GPGME_IS_UID (self), NULL);
    g_return_val_if_fail (self->userid, NULL);
    return self->userid;
}

void
seahorse_gpgme_uid_set_userid (SeahorseGpgmeUid *self, gpgme_user_id_t userid)
{
    SeahorsePgpUid *base;
    GObject *obj;
    gpgme_user_id_t uid;
    gchar *string;
    int index, i;

    g_return_if_fail (SEAHORSE_GPGME_IS_UID (self));
    g_return_if_fail (userid);

    if (self->userid)
        g_return_if_fail (seahorse_gpgme_uid_is_same (self, userid));

    /* Make sure that this userid is in the pubkey */
    index = -1;
    for (i = 0, uid = self->pubkey->uids; uid; ++i, uid = uid->next) {
        if(userid == uid) {
            index = i;
            break;
        }
    }

    g_return_if_fail (index >= 0);

    self->userid = userid;
    self->gpgme_index = index;

    obj = G_OBJECT (self);
    g_object_freeze_notify (obj);
    g_object_notify (obj, "userid");
    g_object_notify (obj, "gpgme_index");

    base = SEAHORSE_PGP_UID (self);

    string = convert_string (userid->comment);
    seahorse_pgp_uid_set_comment (base, string);
    g_free (string);

    string = convert_string (userid->email);
    seahorse_pgp_uid_set_email (base, string);
    g_free (string);

    string = convert_string (userid->name);
    seahorse_pgp_uid_set_name (base, string);
    g_free (string);

    realize_signatures (self);

    seahorse_pgp_uid_set_validity (base, seahorse_gpgme_convert_validity (userid->validity));

    g_object_thaw_notify (obj);
}

guint
seahorse_gpgme_uid_get_gpgme_index (SeahorseGpgmeUid *self)
{
    g_return_val_if_fail (SEAHORSE_GPGME_IS_UID (self), 0);
    return self->gpgme_index;
}

guint
seahorse_gpgme_uid_get_actual_index (SeahorseGpgmeUid *self)
{
    g_return_val_if_fail (SEAHORSE_GPGME_IS_UID (self), 0);
    if(self->actual_index < 0)
        return self->gpgme_index + 1;
    return self->actual_index;
}

void
seahorse_gpgme_uid_set_actual_index (SeahorseGpgmeUid *self, guint actual_index)
{
    g_return_if_fail (SEAHORSE_GPGME_IS_UID (self));
    self->actual_index = actual_index;
    g_object_notify (G_OBJECT (self), "actual-index");
}

char *
seahorse_gpgme_uid_calc_label (gpgme_user_id_t userid)
{
    g_return_val_if_fail (userid, NULL);
    return convert_string (userid->uid);
}

gchar*
seahorse_gpgme_uid_calc_name (gpgme_user_id_t userid)
{
    g_return_val_if_fail (userid, NULL);
    return convert_string (userid->name);
}

gboolean
seahorse_gpgme_uid_is_same (SeahorseGpgmeUid *self, gpgme_user_id_t userid)
{
    g_return_val_if_fail (SEAHORSE_GPGME_IS_UID (self), FALSE);
    g_return_val_if_fail (userid, FALSE);

    return g_strcmp0 (self->userid->uid, userid->uid) == 0;
}

static SeahorseDeleteOperation *
seahorse_gpgme_uid_create_delete_operation (SeahorseDeletable *deletable)
{
    SeahorseGpgmeUid *self = SEAHORSE_GPGME_UID (deletable);
    g_autoptr(SeahorseGpgmeUidDeleteOperation) delete_op = NULL;

    delete_op = seahorse_gpgme_uid_delete_operation_new (self);
    return SEAHORSE_DELETE_OPERATION (g_steal_pointer (&delete_op));
}

static gboolean
seahorse_gpgme_uid_get_deletable (SeahorseDeletable *deletable)
{
    return TRUE;
}

static void
seahorse_gpgme_uid_deletable_iface (SeahorseDeletableIface *iface)
{
    iface->create_delete_operation = seahorse_gpgme_uid_create_delete_operation;
    iface->get_deletable = seahorse_gpgme_uid_get_deletable;
}

static void
seahorse_gpgme_uid_init (SeahorseGpgmeUid *self)
{
    self->gpgme_index = 0;
    self->actual_index = -1;
}

static void
seahorse_gpgme_uid_get_property (GObject *object, guint prop_id,
                               GValue *value, GParamSpec *pspec)
{
    SeahorseGpgmeUid *self = SEAHORSE_GPGME_UID (object);
    SeahorseDeletable *deletable = SEAHORSE_DELETABLE (self);

    switch (prop_id) {
    case PROP_PUBKEY:
        g_value_set_boxed (value, seahorse_gpgme_uid_get_pubkey (self));
        break;
    case PROP_USERID:
        g_value_set_pointer (value, seahorse_gpgme_uid_get_userid (self));
        break;
    case PROP_GPGME_INDEX:
        g_value_set_uint (value, seahorse_gpgme_uid_get_gpgme_index (self));
        break;
    case PROP_ACTUAL_INDEX:
        g_value_set_uint (value, seahorse_gpgme_uid_get_actual_index (self));
        break;
    case PROP_DELETABLE:
        g_value_set_boolean (value, seahorse_gpgme_uid_get_deletable (deletable));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
seahorse_gpgme_uid_set_property (GObject *object, guint prop_id, const GValue *value,
                               GParamSpec *pspec)
{
    SeahorseGpgmeUid *self = SEAHORSE_GPGME_UID (object);
    gpgme_key_t pubkey;

    switch (prop_id) {
    case PROP_PUBKEY:
        pubkey = g_value_get_boxed (value);
        g_return_if_fail (pubkey);

        if (pubkey != self->pubkey) {

            if (self->pubkey) {
                /* Should always be set to the same actual key */
                g_return_if_fail (compare_pubkeys (pubkey, self->pubkey));
                gpgme_key_unref (self->pubkey);
            }

            self->pubkey = g_value_get_boxed (value);
            if (self->pubkey)
                gpgme_key_ref (self->pubkey);

            /* This is expected to be set shortly along with pubkey */
            self->userid = NULL;
        }
        break;
    case PROP_ACTUAL_INDEX:
        seahorse_gpgme_uid_set_actual_index (self, g_value_get_uint (value));
        break;
    case PROP_USERID:
        seahorse_gpgme_uid_set_userid (self, g_value_get_pointer (value));
        break;
    }
}

static void
seahorse_gpgme_uid_object_finalize (GObject *gobject)
{
    SeahorseGpgmeUid *self = SEAHORSE_GPGME_UID (gobject);

    g_clear_pointer (&self->pubkey, gpgme_key_unref);
    self->userid = NULL;

    G_OBJECT_CLASS (seahorse_gpgme_uid_parent_class)->finalize (gobject);
}

static void
seahorse_gpgme_uid_class_init (SeahorseGpgmeUidClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

    gobject_class->finalize = seahorse_gpgme_uid_object_finalize;
    gobject_class->set_property = seahorse_gpgme_uid_set_property;
    gobject_class->get_property = seahorse_gpgme_uid_get_property;

    g_object_class_install_property (gobject_class, PROP_PUBKEY,
        g_param_spec_boxed ("pubkey", "Public Key", "GPGME Public Key that this uid is on",
                            SEAHORSE_GPGME_BOXED_KEY,
                            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property (gobject_class, PROP_USERID,
        g_param_spec_pointer ("userid", "User ID", "GPGME User ID",
                              G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property (gobject_class, PROP_GPGME_INDEX,
        g_param_spec_uint ("gpgme-index", "GPGME Index", "GPGME User ID Index",
                           0, G_MAXUINT, 0,
                           G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property (gobject_class, PROP_ACTUAL_INDEX,
        g_param_spec_uint ("actual-index", "Actual Index", "Actual GPG Index",
                           0, G_MAXUINT, 0,
                           G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_override_property (gobject_class, PROP_DELETABLE, "deletable");
}

SeahorseGpgmeUid*
seahorse_gpgme_uid_new (SeahorseGpgmeKey *parent,
                        gpgme_user_id_t userid)
{
    return g_object_new (SEAHORSE_GPGME_TYPE_UID,
                         "parent", parent,
                         "pubkey", seahorse_gpgme_key_get_public (parent),
                         "userid", userid, NULL);
}
