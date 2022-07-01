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
#include "seahorse-gpgme-subkey.h"
#include "seahorse-gpgme-subkey-delete-operation.h"
#include "seahorse-gpgme-uid.h"

#include <string.h>

#include <glib/gi18n.h>

enum {
    PROP_0,
    PROP_SUBKEY,
    N_PROPS,
    /* overridden properties */
    PROP_DELETABLE,
};

struct _SeahorseGpgmeSubkey {
    SeahorsePgpSubkey parent_instance;

    gpgme_subkey_t subkey;
};

static void       seahorse_gpgme_subkey_deletable_iface       (SeahorseDeletableIface *iface);

G_DEFINE_TYPE_WITH_CODE (SeahorseGpgmeSubkey, seahorse_gpgme_subkey, SEAHORSE_PGP_TYPE_SUBKEY,
                         G_IMPLEMENT_INTERFACE (SEAHORSE_TYPE_DELETABLE, seahorse_gpgme_subkey_deletable_iface);
);

gpgme_subkey_t
seahorse_gpgme_subkey_get_subkey (SeahorseGpgmeSubkey *self)
{
    g_return_val_if_fail (SEAHORSE_GPGME_IS_SUBKEY (self), NULL);
    g_return_val_if_fail (self->subkey, NULL);
    return self->subkey;
}

void
seahorse_gpgme_subkey_set_subkey (SeahorseGpgmeSubkey *self, gpgme_subkey_t subkey)
{
    SeahorsePgpSubkey *base = SEAHORSE_PGP_SUBKEY (self);
    SeahorseGpgmeKey *parent;
    gpgme_key_t pubkey;
    g_autofree char *description = NULL, *fingerprint = NULL, *name = NULL;
    const char *algo_type;
    gpgme_subkey_t sub;
    int i, index;
    g_autoptr(GDateTime) created = NULL;
    g_autoptr(GDateTime) expires = NULL;
    unsigned int flags;

    g_return_if_fail (SEAHORSE_GPGME_IS_SUBKEY (self));
    g_return_if_fail (subkey);

    /* Make sure that this subkey is in the pubkey */
    parent = SEAHORSE_GPGME_KEY (seahorse_pgp_subkey_get_parent_key (base));
    pubkey = seahorse_gpgme_key_get_public (parent);
    index = -1;
    for (i = 0, sub = pubkey->subkeys; sub; ++i, sub = sub->next) {
        if (sub == subkey) {
            index = i;
            break;
        }
    }
    g_return_if_fail (index >= 0);

    /* Calculate the algorithm */
    algo_type = gpgme_pubkey_algo_name (subkey->pubkey_algo);
    if (algo_type == NULL)
        algo_type = C_("Algorithm", "Unknown");
    else if (g_str_equal ("Elg", algo_type) || g_str_equal("ELG-E", algo_type))
        algo_type = _("ElGamal");

    /* Additional properties */
    fingerprint = seahorse_pgp_subkey_calc_fingerprint (subkey->fpr);
    name = seahorse_gpgme_uid_calc_name (pubkey->uids);
    description = seahorse_pgp_subkey_calc_description (name, index);

    self->subkey = subkey;

    g_object_freeze_notify (G_OBJECT (self));

    seahorse_pgp_subkey_set_index (base, index);
    seahorse_pgp_subkey_set_keyid (base, subkey->keyid);
    seahorse_pgp_subkey_set_algorithm (base, algo_type);
    seahorse_pgp_subkey_set_length (base, subkey->length);
    seahorse_pgp_subkey_set_description (base, description);
    seahorse_pgp_subkey_set_fingerprint (base, fingerprint);

    if (subkey->timestamp > 0)
        created = g_date_time_new_from_unix_utc (subkey->timestamp);
    seahorse_pgp_subkey_set_created (base, created);
    if (subkey->expires > 0)
        expires = g_date_time_new_from_unix_utc (subkey->expires);
    seahorse_pgp_subkey_set_expires (base, expires);

    /* The order below is significant */
    flags = 0;
    if (subkey->revoked)
        flags |= SEAHORSE_FLAG_REVOKED;
    if (subkey->expired)
        flags |= SEAHORSE_FLAG_EXPIRED;
    if (subkey->disabled)
        flags |= SEAHORSE_FLAG_DISABLED;
    if (flags == 0 && !subkey->invalid)
        flags |= SEAHORSE_FLAG_IS_VALID;
    if (subkey->can_encrypt)
        flags |= SEAHORSE_FLAG_CAN_ENCRYPT;
    if (subkey->can_sign)
        flags |= SEAHORSE_FLAG_CAN_SIGN;
    if (subkey->can_certify)
        flags |= SEAHORSE_FLAG_CAN_CERTIFY;
    if (subkey->can_authenticate)
        flags |= SEAHORSE_FLAG_CAN_AUTHENTICATE;

    seahorse_pgp_subkey_set_flags (base, flags);

    g_object_notify (G_OBJECT (self), "subkey");
    g_object_thaw_notify (G_OBJECT (self));
}

static SeahorseDeleteOperation *
seahorse_gpgme_subkey_create_delete_operation (SeahorseDeletable *deletable)
{
    SeahorseGpgmeSubkey *self = SEAHORSE_GPGME_SUBKEY (deletable);

    return seahorse_gpgme_subkey_delete_operation_new (self);
}

static gboolean
seahorse_gpgme_subkey_get_deletable (SeahorseDeletable *deletable)
{
    return TRUE;
}

static void
seahorse_gpgme_subkey_deletable_iface (SeahorseDeletableIface *iface)
{
    iface->create_delete_operation = seahorse_gpgme_subkey_create_delete_operation;
    iface->get_deletable = seahorse_gpgme_subkey_get_deletable;
}

static void
seahorse_gpgme_subkey_init (SeahorseGpgmeSubkey *self)
{
}

static void
seahorse_gpgme_subkey_get_property (GObject      *object,
                                    unsigned int  prop_id,
                                    GValue       *value,
                                    GParamSpec   *pspec)
{
    SeahorseGpgmeSubkey *self = SEAHORSE_GPGME_SUBKEY (object);
    SeahorseDeletable *deletable = SEAHORSE_DELETABLE (self);

    switch (prop_id) {
    case PROP_SUBKEY:
        g_value_set_pointer (value, seahorse_gpgme_subkey_get_subkey (self));
        break;
    case PROP_DELETABLE:
        g_value_set_boolean (value, seahorse_gpgme_subkey_get_deletable (deletable));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
seahorse_gpgme_subkey_set_property (GObject      *object,
                                    unsigned int  prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
    SeahorseGpgmeSubkey *self = SEAHORSE_GPGME_SUBKEY (object);

    switch (prop_id) {
    case PROP_SUBKEY:
        seahorse_gpgme_subkey_set_subkey (self, g_value_get_pointer (value));
        break;
    }
}

static void
seahorse_gpgme_subkey_finalize (GObject *gobject)
{
    SeahorseGpgmeSubkey *self = SEAHORSE_GPGME_SUBKEY (gobject);

    self->subkey = NULL;

    G_OBJECT_CLASS (seahorse_gpgme_subkey_parent_class)->finalize (gobject);
}

static void
seahorse_gpgme_subkey_class_init (SeahorseGpgmeSubkeyClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

    gobject_class->finalize = seahorse_gpgme_subkey_finalize;
    gobject_class->set_property = seahorse_gpgme_subkey_set_property;
    gobject_class->get_property = seahorse_gpgme_subkey_get_property;

    g_object_class_install_property (gobject_class, PROP_SUBKEY,
        g_param_spec_pointer ("subkey", "Subkey", "GPGME Subkey",
                              G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_override_property (gobject_class, PROP_DELETABLE, "deletable");
}

SeahorseGpgmeSubkey*
seahorse_gpgme_subkey_new (SeahorseGpgmeKey *parent_key,
                           gpgme_subkey_t    subkey)
{
    g_return_val_if_fail (SEAHORSE_GPGME_IS_KEY (parent_key), NULL);
    g_return_val_if_fail (subkey, NULL);

    return g_object_new (SEAHORSE_GPGME_TYPE_SUBKEY,
                         "parent-key", parent_key,
                         "subkey", subkey, NULL);
}
