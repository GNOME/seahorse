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
#include "seahorse-pgp-subkey.h"
#include "seahorse-pgp-uid.h"

#include <string.h>

#include <glib/gi18n.h>

enum {
    PROP_0,
    PROP_PARENT_KEY,
    PROP_INDEX,
    PROP_KEYID,
    PROP_FLAGS,
    PROP_LENGTH,
    PROP_ALGORITHM,
    PROP_CREATED,
    PROP_EXPIRES,
    PROP_DESCRIPTION,
    PROP_FINGERPRINT,
    N_PROPS
};
static GParamSpec *obj_props[N_PROPS] = { NULL, };

typedef struct _SeahorsePgpSubkeyPrivate {
    SeahorsePgpKey *parent_key;
    unsigned int index;
    char *keyid;
    unsigned int flags;
    unsigned int length;
    char *algorithm;
    GDateTime *created;
    GDateTime *expires;
    char *description;
    char *fingerprint;
} SeahorsePgpSubkeyPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (SeahorsePgpSubkey, seahorse_pgp_subkey, G_TYPE_OBJECT);

SeahorsePgpKey *
seahorse_pgp_subkey_get_parent_key (SeahorsePgpSubkey *self)
{
    SeahorsePgpSubkeyPrivate *priv = seahorse_pgp_subkey_get_instance_private (self);

    g_return_val_if_fail (SEAHORSE_PGP_IS_SUBKEY (self), NULL);
    return priv->parent_key;
}

void
seahorse_pgp_subkey_set_parent_key (SeahorsePgpSubkey *self,
                                    SeahorsePgpKey    *parent_key)
{
    SeahorsePgpSubkeyPrivate *priv = seahorse_pgp_subkey_get_instance_private (self);

    g_return_if_fail (SEAHORSE_PGP_IS_SUBKEY (self));
    g_return_if_fail (SEAHORSE_PGP_IS_KEY (parent_key));

    priv->parent_key = parent_key;
    g_object_notify_by_pspec (G_OBJECT (self), obj_props[PROP_PARENT_KEY]);
}

unsigned int
seahorse_pgp_subkey_get_index (SeahorsePgpSubkey *self)
{
    SeahorsePgpSubkeyPrivate *priv = seahorse_pgp_subkey_get_instance_private (self);

    g_return_val_if_fail (SEAHORSE_PGP_IS_SUBKEY (self), 0);
    return priv->index;
}

void
seahorse_pgp_subkey_set_index (SeahorsePgpSubkey *self, unsigned int index)
{
    SeahorsePgpSubkeyPrivate *priv = seahorse_pgp_subkey_get_instance_private (self);

    g_return_if_fail (SEAHORSE_PGP_IS_SUBKEY (self));

    priv->index = index;
    g_object_notify_by_pspec (G_OBJECT (self), obj_props[PROP_INDEX]);
}

const char *
seahorse_pgp_subkey_get_keyid (SeahorsePgpSubkey *self)
{
    SeahorsePgpSubkeyPrivate *priv = seahorse_pgp_subkey_get_instance_private (self);

    g_return_val_if_fail (SEAHORSE_PGP_IS_SUBKEY (self), NULL);
    return priv->keyid;
}

void
seahorse_pgp_subkey_set_keyid (SeahorsePgpSubkey *self, const char *keyid)
{
    SeahorsePgpSubkeyPrivate *priv = seahorse_pgp_subkey_get_instance_private (self);

    g_return_if_fail (SEAHORSE_PGP_IS_SUBKEY (self));
    g_return_if_fail (keyid);

    g_free (priv->keyid);
    priv->keyid = g_strdup (keyid);
    g_object_notify_by_pspec (G_OBJECT (self), obj_props[PROP_KEYID]);
}

unsigned int
seahorse_pgp_subkey_get_flags (SeahorsePgpSubkey *self)
{
    SeahorsePgpSubkeyPrivate *priv = seahorse_pgp_subkey_get_instance_private (self);

    g_return_val_if_fail (SEAHORSE_PGP_IS_SUBKEY (self), 0);
    return priv->flags;
}

void
seahorse_pgp_subkey_set_flags (SeahorsePgpSubkey *self, unsigned int flags)
{
    SeahorsePgpSubkeyPrivate *priv = seahorse_pgp_subkey_get_instance_private (self);

    g_return_if_fail (SEAHORSE_PGP_IS_SUBKEY (self));

    priv->flags = flags;
    g_object_notify_by_pspec (G_OBJECT (self), obj_props[PROP_FLAGS]);
}

/**
 * seahorse_pgp_subkey_get_length:
 * @self: A #SeahorsePgpSubkey
 *
 * Returns: the subkey's length
 */
unsigned int
seahorse_pgp_subkey_get_length (SeahorsePgpSubkey *self)
{
    SeahorsePgpSubkeyPrivate *priv = seahorse_pgp_subkey_get_instance_private (self);

    g_return_val_if_fail (SEAHORSE_PGP_IS_SUBKEY (self), 0);
    return priv->length;
}

/**
 * seahorse_pgp_subkey_set_length:
 * @self: A #SeahorsePgpSubkey
 * @length: The new key length
 */
void
seahorse_pgp_subkey_set_length (SeahorsePgpSubkey *self, unsigned int length)
{
    SeahorsePgpSubkeyPrivate *priv = seahorse_pgp_subkey_get_instance_private (self);

    g_return_if_fail (SEAHORSE_PGP_IS_SUBKEY (self));

    priv->length = length;
    g_object_notify_by_pspec (G_OBJECT (self), obj_props[PROP_LENGTH]);
}

/**
 * seahorse_pgp_subkey_get_usage:
 * @self: A #SeahorsePgpSubkey
 *
 * Returns: (transfer full): the subkey's usage, e.g. "Encrypt, Sign, Certify"
 */
char *
seahorse_pgp_subkey_get_usage (SeahorsePgpSubkey *self)
{
    g_auto(GStrv) usages = NULL;

    g_return_val_if_fail (SEAHORSE_PGP_IS_SUBKEY (self), NULL);

    usages = seahorse_pgp_subkey_get_usages (self, NULL);
    return g_strjoinv (", ", usages);
}

/**
 * seahorse_pgp_subkey_get_usages:
 * @self: A #SeahorsePgpSubkey
 * @decriptions: (transfer full) (array zero-terminated=1) (optional):
 *               The descriptions of the subkey's usages
 *
 * Returns: (transfer full) (array zero-terminated=1): the subkey's usages,
 * for example: { "Encrypt" , "Sign" , "Certify", %NULL }
 */
char **
seahorse_pgp_subkey_get_usages (SeahorsePgpSubkey *self, char ***descriptions)
{
    typedef struct {
        unsigned int flag;
        const char *name;
        const char *description;
    } FlagNames;

    const FlagNames flag_names[] = {
        { SEAHORSE_FLAG_CAN_ENCRYPT, N_("Encrypt"), N_("This subkey can be used for encryption") },
        { SEAHORSE_FLAG_CAN_SIGN, N_("Sign"), N_("This subkey can be used to create data signatures") },
        { SEAHORSE_FLAG_CAN_CERTIFY, N_("Certify"), N_("This subkey can be used to create certificates") },
        { SEAHORSE_FLAG_CAN_AUTHENTICATE, N_("Authenticate"), N_("This subkey can be used for authentication") }
    };

    SeahorsePgpSubkeyPrivate *priv = seahorse_pgp_subkey_get_instance_private (self);
    g_autoptr(GPtrArray) names = NULL, descs = NULL;

    g_return_val_if_fail (SEAHORSE_PGP_IS_SUBKEY (self), NULL);

    names = g_ptr_array_new_with_free_func (g_free);
    descs = g_ptr_array_new_with_free_func (g_free);

    for (unsigned i = 0; i < G_N_ELEMENTS (flag_names); i++) {
        if (priv->flags & flag_names[i].flag) {
            g_ptr_array_add (names, g_strdup (_(flag_names[i].name)));
            g_ptr_array_add (descs, g_strdup (_(flag_names[i].description)));
        }
    }

    g_ptr_array_add (names, NULL);
    g_ptr_array_add (descs, NULL);

    if (descriptions)
        *descriptions = (char**) g_ptr_array_free (g_steal_pointer (&descs), FALSE);

    return (char **) g_ptr_array_free (g_steal_pointer (&names), FALSE);
}

/**
 * seahorse_pgp_subkey_get_algorithm:
 * @self: A #SeahorsePgpSubkey
 *
 * Returns: (transfer none): the subkey's algorithm
 */
const char *
seahorse_pgp_subkey_get_algorithm (SeahorsePgpSubkey *self)
{
    SeahorsePgpSubkeyPrivate *priv = seahorse_pgp_subkey_get_instance_private (self);

    g_return_val_if_fail (SEAHORSE_PGP_IS_SUBKEY (self), NULL);
    return priv->algorithm;
}

/**
 * seahorse_pgp_subkey_set_algorithm:
 * @self: A #SeahorsePgpSubkey
 * @algorithm: An algorithm, for example "RSA"
 *
 * Sets the algorithm of @self.
 */
void
seahorse_pgp_subkey_set_algorithm (SeahorsePgpSubkey *self,
                                   const char        *algorithm)
{
    SeahorsePgpSubkeyPrivate *priv = seahorse_pgp_subkey_get_instance_private (self);

    g_return_if_fail (SEAHORSE_PGP_IS_SUBKEY (self));

    g_free (priv->algorithm);
    priv->algorithm = g_strdup (algorithm);
    g_object_notify_by_pspec (G_OBJECT (self), obj_props[PROP_ALGORITHM]);
}

/**
 * seahorse_pgp_subkey_get_created:
 * @self: A #SeahorsePgpSubkey
 *
 * Returns the #GDateTime at which this key was created, or %NULL if unknown.
 *
 * Returns: (transfer none) (nullable): the creation datetime, or %NULL
 */
GDateTime *
seahorse_pgp_subkey_get_created (SeahorsePgpSubkey *self)
{
    SeahorsePgpSubkeyPrivate *priv = seahorse_pgp_subkey_get_instance_private (self);

    g_return_val_if_fail (SEAHORSE_PGP_IS_SUBKEY (self), 0);
    return priv->created;
}

/**
 * seahorse_pgp_subkey_set_created:
 * @self: A #SeahorsePgpSubkey
 * @expires: (nullable): The datetime @self was create or %NULL if unknown
 *
 * Sets the creation datetime of @self (can be %NULL, if unknown).
 */
void
seahorse_pgp_subkey_set_created (SeahorsePgpSubkey *self,
                                 GDateTime         *created)
{
    SeahorsePgpSubkeyPrivate *priv = seahorse_pgp_subkey_get_instance_private (self);

    g_return_if_fail (SEAHORSE_PGP_IS_SUBKEY (self));

    g_clear_pointer (&priv->created, g_date_time_unref);
    priv->created = created? g_date_time_ref (created) : NULL;
    g_object_notify_by_pspec (G_OBJECT (self), obj_props[PROP_CREATED]);
}

/**
 * seahorse_pgp_subkey_get_expires:
 * @self: A #SeahorsePgpSubkey
 *
 * Returns the #GDateTime at which this key will expire, or %NULL if not set.
 *
 * Returns: (transfer none) (nullable): the expiration datetime, or %NULL
 */
GDateTime *
seahorse_pgp_subkey_get_expires (SeahorsePgpSubkey *self)
{
    SeahorsePgpSubkeyPrivate *priv = seahorse_pgp_subkey_get_instance_private (self);

    g_return_val_if_fail (SEAHORSE_PGP_IS_SUBKEY (self), 0);

    return priv->expires;
}

/**
 * seahorse_pgp_subkey_set_expires:
 * @self: A #SeahorsePgpSubkey
 * @expires: (nullable): The expiration datetime or %NULL if none.
 *
 * Sets the expiration datetime of @self (or if %NULL, subkey doesn't expire).
 */
void
seahorse_pgp_subkey_set_expires (SeahorsePgpSubkey *self,
                                 GDateTime         *expires)
{
    SeahorsePgpSubkeyPrivate *priv = seahorse_pgp_subkey_get_instance_private (self);

    g_return_if_fail (SEAHORSE_PGP_IS_SUBKEY (self));

    g_clear_pointer (&priv->expires, g_date_time_unref);
    priv->expires = expires? g_date_time_ref (expires) : NULL;
    g_object_notify_by_pspec (G_OBJECT (self), obj_props[PROP_EXPIRES]);
}

const char *
seahorse_pgp_subkey_get_fingerprint (SeahorsePgpSubkey *self)
{
    SeahorsePgpSubkeyPrivate *priv = seahorse_pgp_subkey_get_instance_private (self);

    g_return_val_if_fail (SEAHORSE_PGP_IS_SUBKEY (self), NULL);
    return priv->fingerprint;
}

/**
 * seahorse_pgp_subkey_set_expires:
 * @self: A #SeahorsePgpSubkey
 * @fingerprint: The fingerprint of @key
 *
 * Sets the fingerprint of @self to @fingerprint
 */
void
seahorse_pgp_subkey_set_fingerprint (SeahorsePgpSubkey *self,
                                     const char        *fingerprint)
{
    SeahorsePgpSubkeyPrivate *priv = seahorse_pgp_subkey_get_instance_private (self);

    g_return_if_fail (SEAHORSE_PGP_IS_SUBKEY (self));
    g_return_if_fail (fingerprint);

    g_free (priv->fingerprint);
    priv->fingerprint = g_strdup (fingerprint);
    g_object_notify_by_pspec (G_OBJECT (self), obj_props[PROP_FINGERPRINT]);
}

const char *
seahorse_pgp_subkey_get_description (SeahorsePgpSubkey *self)
{
    SeahorsePgpSubkeyPrivate *priv = seahorse_pgp_subkey_get_instance_private (self);

    g_return_val_if_fail (SEAHORSE_PGP_IS_SUBKEY (self), NULL);
    return priv->description;
}

void
seahorse_pgp_subkey_set_description (SeahorsePgpSubkey *self,
                                     const char        *description)
{
    SeahorsePgpSubkeyPrivate *priv = seahorse_pgp_subkey_get_instance_private (self);

    g_return_if_fail (SEAHORSE_PGP_IS_SUBKEY (self));
    g_return_if_fail (description);

    g_free (priv->description);
    priv->description = g_strdup (description);
    g_object_notify_by_pspec (G_OBJECT (self), obj_props[PROP_DESCRIPTION]);
}

char *
seahorse_pgp_subkey_calc_description (const char *name, unsigned int index)
{
    if (name == NULL)
        name = _("Key");

    if (index == 0)
        return g_strdup (name);

    return g_strdup_printf (_("Subkey %d of %s"), index, name);
}

/* Takes runs of hexadecimal digits, possibly with whitespace among them, and
 * formats them nicely in groups of four digits.
 */
char *
seahorse_pgp_subkey_calc_fingerprint (const char *raw_fingerprint)
{
    const char *raw;
    GString *string;
    unsigned int len, num_digits;
    char *fpr;

    raw = raw_fingerprint;
    g_return_val_if_fail (raw != NULL, NULL);

    string = g_string_new ("");
    len = strlen (raw);

    num_digits = 0;
    for (unsigned int i = 0; i < len; i++) {
        if (g_ascii_isxdigit (raw[i])) {
            g_string_append_c (string, g_ascii_toupper (raw[i]));
            num_digits++;

            if (num_digits > 0 && num_digits % 4 == 0)
                g_string_append (string, " ");
        }
    }

    fpr = g_string_free (string, FALSE);

    g_strchomp (fpr);

    return fpr;
}

static void
seahorse_pgp_subkey_get_property (GObject      *object,
                                  unsigned int  prop_id,
                                  GValue       *value,
                                  GParamSpec   *pspec)
{
    SeahorsePgpSubkey *self = SEAHORSE_PGP_SUBKEY (object);

    switch (prop_id) {
    case PROP_PARENT_KEY:
        g_value_set_object (value, seahorse_pgp_subkey_get_parent_key (self));
        break;
    case PROP_INDEX:
        g_value_set_uint (value, seahorse_pgp_subkey_get_index (self));
        break;
    case PROP_KEYID:
        g_value_set_string (value, seahorse_pgp_subkey_get_keyid (self));
        break;
    case PROP_FLAGS:
        g_value_set_uint (value, seahorse_pgp_subkey_get_flags (self));
        break;
    case PROP_LENGTH:
        g_value_set_uint (value, seahorse_pgp_subkey_get_length (self));
        break;
    case PROP_ALGORITHM:
        g_value_set_string (value, seahorse_pgp_subkey_get_algorithm (self));
        break;
    case PROP_CREATED:
        g_value_set_boxed (value, seahorse_pgp_subkey_get_created (self));
        break;
    case PROP_EXPIRES:
        g_value_set_boxed (value, seahorse_pgp_subkey_get_expires (self));
        break;
    case PROP_DESCRIPTION:
        g_value_set_string (value, seahorse_pgp_subkey_get_description (self));
        break;
    case PROP_FINGERPRINT:
        g_value_set_string (value, seahorse_pgp_subkey_get_fingerprint (self));
        break;
    }
}

static void
seahorse_pgp_subkey_set_property (GObject      *object,
                                  unsigned int  prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
    SeahorsePgpSubkey *self = SEAHORSE_PGP_SUBKEY (object);

    switch (prop_id) {
    case PROP_PARENT_KEY:
        seahorse_pgp_subkey_set_parent_key (self, g_value_get_object (value));
        break;
    case PROP_INDEX:
        seahorse_pgp_subkey_set_index (self, g_value_get_uint (value));
        break;
    case PROP_KEYID:
        seahorse_pgp_subkey_set_keyid (self, g_value_get_string (value));
        break;
    case PROP_FLAGS:
        seahorse_pgp_subkey_set_flags (self, g_value_get_uint (value));
        break;
    case PROP_LENGTH:
        seahorse_pgp_subkey_set_length (self, g_value_get_uint (value));
        break;
    case PROP_ALGORITHM:
        seahorse_pgp_subkey_set_algorithm (self, g_value_get_string (value));
        break;
    case PROP_CREATED:
        seahorse_pgp_subkey_set_created (self, g_value_get_boxed (value));
        break;
    case PROP_EXPIRES:
        seahorse_pgp_subkey_set_expires (self, g_value_get_boxed (value));
        break;
    case PROP_FINGERPRINT:
        seahorse_pgp_subkey_set_fingerprint (self, g_value_get_string (value));
        break;
    case PROP_DESCRIPTION:
        seahorse_pgp_subkey_set_description (self, g_value_get_string (value));
        break;
    }
}

static void
seahorse_pgp_subkey_finalize (GObject *gobject)
{
    SeahorsePgpSubkey *self = SEAHORSE_PGP_SUBKEY (gobject);
    SeahorsePgpSubkeyPrivate *priv
        = seahorse_pgp_subkey_get_instance_private (self);

    g_clear_pointer (&priv->created, g_date_time_unref);
    g_clear_pointer (&priv->expires, g_date_time_unref);
    g_clear_pointer (&priv->algorithm, g_free);
    g_clear_pointer (&priv->fingerprint, g_free);
    g_clear_pointer (&priv->description, g_free);
    g_clear_pointer (&priv->keyid, g_free);

    G_OBJECT_CLASS (seahorse_pgp_subkey_parent_class)->finalize (gobject);
}

static void
seahorse_pgp_subkey_init (SeahorsePgpSubkey *self)
{
}

static void
seahorse_pgp_subkey_class_init (SeahorsePgpSubkeyClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

    gobject_class->finalize = seahorse_pgp_subkey_finalize;
    gobject_class->set_property = seahorse_pgp_subkey_set_property;
    gobject_class->get_property = seahorse_pgp_subkey_get_property;

    obj_props[PROP_PARENT_KEY] =
        g_param_spec_object ("parent-key", "Parent Key", "Key this subkey belongs to",
                             SEAHORSE_PGP_TYPE_KEY,
                             G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    obj_props[PROP_INDEX] =
        g_param_spec_uint ("index", "Index", "PGP subkey index",
                           0, G_MAXUINT, 0,
                           G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    obj_props[PROP_KEYID] =
        g_param_spec_string ("keyid", "Key ID", "GPG Key ID",
                             "",
                             G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    obj_props[PROP_FLAGS] =
        g_param_spec_uint ("flags", "Flags", "PGP subkey flags",
                           0, G_MAXUINT, 0,
                           G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    obj_props[PROP_LENGTH] =
        g_param_spec_uint ("length", "Length", "PGP key length",
                           0, G_MAXUINT, 0,
                           G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    obj_props[PROP_ALGORITHM] =
        g_param_spec_string ("algorithm", "Algorithm", "GPG Algorithm",
                             "",
                             G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    obj_props[PROP_CREATED] =
        g_param_spec_boxed ("created", "Created On", "Date this key was created on",
                            G_TYPE_DATE_TIME,
                            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    obj_props[PROP_EXPIRES] =
        g_param_spec_boxed ("expires", "Expires On", "Date this key expires on",
                            G_TYPE_DATE_TIME,
                            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    obj_props[PROP_DESCRIPTION] =
        g_param_spec_string ("description", "Description", "Key Description",
                             "",
                             G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    obj_props[PROP_FINGERPRINT] =
        g_param_spec_string ("fingerprint", "Fingerprint", "PGP Key Fingerprint",
                             "",
                             G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties (gobject_class, N_PROPS, obj_props);
}

SeahorsePgpSubkey*
seahorse_pgp_subkey_new (void)
{
    return g_object_new (SEAHORSE_PGP_TYPE_SUBKEY, NULL);
}
