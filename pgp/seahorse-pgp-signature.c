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

#include "seahorse-gpgme-key.h"
#include "seahorse-gpgme-keyring.h"
#include "seahorse-pgp-backend.h"
#include "seahorse-pgp-key.h"
#include "seahorse-pgp-signature.h"

#include <string.h>

#include <glib/gi18n.h>

typedef struct _SeahorsePgpSignaturePrivate {
    guint flags;
    char *keyid;
} SeahorsePgpSignaturePrivate;

enum {
    PROP_0,
    PROP_KEYID,
    PROP_FLAGS,
    PROP_SIGTYPE,
    N_PROPS
};

G_DEFINE_TYPE_WITH_PRIVATE (SeahorsePgpSignature, seahorse_pgp_signature, G_TYPE_OBJECT);

guint
seahorse_pgp_signature_get_flags (SeahorsePgpSignature *self)
{
    SeahorsePgpSignaturePrivate *priv
        = seahorse_pgp_signature_get_instance_private (self);

    g_return_val_if_fail (SEAHORSE_PGP_IS_SIGNATURE (self), 0);
    return priv->flags;
}

void
seahorse_pgp_signature_set_flags (SeahorsePgpSignature *self, guint flags)
{
    SeahorsePgpSignaturePrivate *priv
        = seahorse_pgp_signature_get_instance_private (self);
    GObject *obj;

    g_return_if_fail (SEAHORSE_PGP_IS_SIGNATURE (self));
    priv->flags = flags;

    obj = G_OBJECT (self);
    g_object_freeze_notify (obj);
    g_object_notify (obj, "flags");
    g_object_notify (obj, "sigtype");
    g_object_thaw_notify (obj);
}

const gchar*
seahorse_pgp_signature_get_keyid (SeahorsePgpSignature *self)
{
    SeahorsePgpSignaturePrivate *priv
        = seahorse_pgp_signature_get_instance_private (self);

    g_return_val_if_fail (SEAHORSE_PGP_IS_SIGNATURE (self), NULL);
    return priv->keyid;
}

void
seahorse_pgp_signature_set_keyid (SeahorsePgpSignature *self, const gchar *keyid)
{
    SeahorsePgpSignaturePrivate *priv
        = seahorse_pgp_signature_get_instance_private (self);
    GObject *obj;

    g_return_if_fail (SEAHORSE_PGP_IS_SIGNATURE (self));
    g_free (priv->keyid);
    priv->keyid = g_strdup (keyid);

    obj = G_OBJECT (self);
    g_object_freeze_notify (obj);
    g_object_notify (obj, "keyid");
    g_object_notify (obj, "sigtype");
    g_object_thaw_notify (obj);
}

guint
seahorse_pgp_signature_get_sigtype (SeahorsePgpSignature *self)
{
    SeahorsePgpSignaturePrivate *priv
        = seahorse_pgp_signature_get_instance_private (self);
    SeahorseGpgmeKeyring *keyring;
    SeahorseGpgmeKey *key;
    SeahorseItem *item;

    g_return_val_if_fail (SEAHORSE_PGP_IS_SIGNATURE (self), 0);

    keyring = seahorse_pgp_backend_get_default_keyring (NULL);
    key = seahorse_gpgme_keyring_lookup (keyring, priv->keyid);

    if (key != NULL) {
        item = SEAHORSE_ITEM (key);
        if (seahorse_item_get_usage (item) == SEAHORSE_USAGE_PRIVATE_KEY)
            return SKEY_PGPSIG_TRUSTED | SKEY_PGPSIG_PERSONAL;
        if (seahorse_item_get_item_flags (item) & SEAHORSE_FLAG_TRUSTED)
            return SKEY_PGPSIG_TRUSTED;
    }

    return 0;
}

static void
seahorse_pgp_signature_get_property (GObject *object, guint prop_id,
                                  GValue *value, GParamSpec *pspec)
{
    SeahorsePgpSignature *self = SEAHORSE_PGP_SIGNATURE (object);

    switch (prop_id) {
    case PROP_KEYID:
        g_value_set_string (value, seahorse_pgp_signature_get_keyid (self));
        break;
    case PROP_FLAGS:
        g_value_set_uint (value, seahorse_pgp_signature_get_flags (self));
        break;
    case PROP_SIGTYPE:
        g_value_set_uint (value, seahorse_pgp_signature_get_sigtype (self));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
seahorse_pgp_signature_set_property (GObject *object, guint prop_id, const GValue *value,
                                  GParamSpec *pspec)
{
    SeahorsePgpSignature *self = SEAHORSE_PGP_SIGNATURE (object);

    switch (prop_id) {
    case PROP_KEYID:
        seahorse_pgp_signature_set_keyid (self, g_value_get_string (value));
        break;
    case PROP_FLAGS:
        seahorse_pgp_signature_set_flags (self, g_value_get_uint (value));
        break;
    }
}

static void
seahorse_pgp_signature_finalize (GObject *gobject)
{
    SeahorsePgpSignature *self = SEAHORSE_PGP_SIGNATURE (gobject);
    SeahorsePgpSignaturePrivate *priv
        = seahorse_pgp_signature_get_instance_private (self);

    g_clear_pointer (&priv->keyid, g_free);

    G_OBJECT_CLASS (seahorse_pgp_signature_parent_class)->finalize (gobject);
}

static void
seahorse_pgp_signature_init (SeahorsePgpSignature *self)
{
}

static void
seahorse_pgp_signature_class_init (SeahorsePgpSignatureClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

    gobject_class->finalize = seahorse_pgp_signature_finalize;
    gobject_class->set_property = seahorse_pgp_signature_set_property;
    gobject_class->get_property = seahorse_pgp_signature_get_property;

    g_object_class_install_property (gobject_class, PROP_KEYID,
        g_param_spec_string ("keyid", "Key ID", "GPG Key ID",
                             "",
                             G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property (gobject_class, PROP_FLAGS,
        g_param_spec_uint ("flags", "Flags", "PGP signature flags",
                           0, G_MAXUINT, 0,
                           G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property (gobject_class, PROP_SIGTYPE,
        g_param_spec_uint ("sigtype", "Sig Type", "PGP signature type",
                           0, G_MAXUINT, 0,
                           G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
}

SeahorsePgpSignature*
seahorse_pgp_signature_new (const gchar *keyid)
{
    return g_object_new (SEAHORSE_PGP_TYPE_SIGNATURE, "keyid", keyid, NULL);
}
