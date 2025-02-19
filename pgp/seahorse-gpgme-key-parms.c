/*
 * Seahorse
 *
 * Copyright (C) 2025 Niels De Graef
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

#include "seahorse-gpgme-key-parms.h"
#include "seahorse-pgp-enums.h"

#include <glib/gi18n.h>

/**
 * SeahorseGpgmeKeyParms:
 *
 * A wrapper for the "<GnupgKeyParms>" set of parameters that GPG(ME) expects
 * for creating a key.
 */

struct _SeahorseGpgmeKeyParms {
    GObject parent_instance;

    char *name;
    char *email;
    char *comment;
    char *passphrase;
    SeahorseGpgmeKeyGenType type;
    unsigned int key_length;
    GDateTime *expires;
};

G_DEFINE_TYPE (SeahorseGpgmeKeyParms, seahorse_gpgme_key_parms, G_TYPE_OBJECT);

enum {
    PROP_NAME = 1,
    PROP_EMAIL,
    PROP_COMMENT,
    PROP_KEY_TYPE,
    PROP_KEY_LENGTH,
    PROP_EXPIRES,
    PROP_IS_VALID,
    N_PROPS
};
static GParamSpec *obj_props[N_PROPS] = { NULL, };

const char *
seahorse_gpgme_key_parms_get_name (SeahorseGpgmeKeyParms *self)
{
    g_return_val_if_fail (SEAHORSE_GPGME_IS_KEY_PARMS (self), NULL);
    return self->name;
}

void
seahorse_gpgme_key_parms_set_name (SeahorseGpgmeKeyParms *self,
                                   const char            *name)
{
    gboolean old_valid;

    g_return_if_fail (SEAHORSE_GPGME_IS_KEY_PARMS (self));

    if (self->name == name)
        return;

    old_valid = seahorse_gpgme_key_parms_is_valid (self);

    g_free (self->name);
    self->name = name? g_strdup (name) : NULL;

    g_object_notify_by_pspec (G_OBJECT (self), obj_props[PROP_NAME]);
    if (old_valid != seahorse_gpgme_key_parms_is_valid (self))
        g_object_notify_by_pspec (G_OBJECT (self), obj_props[PROP_IS_VALID]);
}

const char *
seahorse_gpgme_key_parms_get_email (SeahorseGpgmeKeyParms *self)
{
    g_return_val_if_fail (SEAHORSE_GPGME_IS_KEY_PARMS (self), NULL);
    return self->email;
}

void
seahorse_gpgme_key_parms_set_email (SeahorseGpgmeKeyParms *self,
                                    const char            *email)
{
    g_return_if_fail (SEAHORSE_GPGME_IS_KEY_PARMS (self));

    if (self->email == email)
        return;

    g_free (self->email);
    self->email = email? g_strdup (email) : NULL;
    g_object_notify_by_pspec (G_OBJECT (self), obj_props[PROP_EMAIL]);
}

const char *
seahorse_gpgme_key_parms_get_comment (SeahorseGpgmeKeyParms *self)
{
    g_return_val_if_fail (SEAHORSE_GPGME_IS_KEY_PARMS (self), NULL);
    return self->comment;
}

void
seahorse_gpgme_key_parms_set_comment (SeahorseGpgmeKeyParms *self,
                                      const char            *comment)
{
    g_return_if_fail (SEAHORSE_GPGME_IS_KEY_PARMS (self));

    if (self->comment == comment)
        return;

    g_free (self->comment);
    self->comment = comment? g_strdup (comment) : NULL;
    g_object_notify_by_pspec (G_OBJECT (self), obj_props[PROP_COMMENT]);
}

SeahorseGpgmeKeyGenType
seahorse_gpgme_key_parms_get_key_type (SeahorseGpgmeKeyParms *self)
{
    g_return_val_if_fail (SEAHORSE_GPGME_IS_KEY_PARMS (self), 0);
    return self->type;
}

void
seahorse_gpgme_key_parms_set_key_type (SeahorseGpgmeKeyParms   *self,
                                       SeahorseGpgmeKeyGenType  key_type)
{
    SeahorsePgpKeyAlgorithm algo;
    unsigned int default_val;

    g_return_if_fail (SEAHORSE_GPGME_IS_KEY_PARMS (self));

    if (self->type == key_type)
        return;

    self->type = key_type;
    g_object_notify_by_pspec (G_OBJECT (self), obj_props[PROP_KEY_TYPE]);

    algo = seahorse_gpgme_key_gen_type_get_key_algo (key_type);
    if (seahorse_pgp_key_algorithm_get_length_values (algo, &default_val, NULL, NULL))
        seahorse_gpgme_key_parms_set_key_length (self, default_val);
}

unsigned int
seahorse_gpgme_key_parms_get_key_length (SeahorseGpgmeKeyParms *self)
{
    g_return_val_if_fail (SEAHORSE_GPGME_IS_KEY_PARMS (self), 0);
    return self->key_length;
}

void
seahorse_gpgme_key_parms_set_key_length (SeahorseGpgmeKeyParms   *self,
                                         unsigned int             key_length)
{
    SeahorsePgpKeyAlgorithm algo;
    unsigned int default_val, lower, upper;

    g_return_if_fail (SEAHORSE_GPGME_IS_KEY_PARMS (self));

    algo = seahorse_gpgme_key_gen_type_get_key_algo (self->type);
    if (seahorse_pgp_key_algorithm_get_length_values (algo, &default_val, &lower, &upper)) {
        g_return_if_fail (key_length >= lower);
        g_return_if_fail (key_length <= upper);
    }

    if (self->key_length == key_length)
        return;

    self->key_length = key_length;
    g_object_notify_by_pspec (G_OBJECT (self), obj_props[PROP_KEY_LENGTH]);
}

GDateTime *
seahorse_gpgme_key_parms_get_expires (SeahorseGpgmeKeyParms *self)
{
    g_return_val_if_fail (SEAHORSE_GPGME_IS_KEY_PARMS (self), NULL);
    return self->expires;
}

void
seahorse_gpgme_key_parms_set_expires (SeahorseGpgmeKeyParms *self,
                                      GDateTime             *expires)
{
    g_return_if_fail (SEAHORSE_GPGME_IS_KEY_PARMS (self));

    if (self->expires == expires)
        return;

    g_clear_pointer (&self->expires, g_date_time_unref);
    self->expires = expires? g_date_time_ref (expires) : NULL;
    g_object_notify_by_pspec (G_OBJECT (self), obj_props[PROP_EXPIRES]);
}

void
seahorse_gpgme_key_parms_set_passphrase (SeahorseGpgmeKeyParms *self,
                                         const char            *passphrase)
{
    g_return_if_fail (SEAHORSE_GPGME_IS_KEY_PARMS (self));

    self->passphrase = gcr_secure_memory_strdup (passphrase);
}

gboolean
seahorse_gpgme_key_parms_has_subkey (SeahorseGpgmeKeyParms *self)
{
    g_return_val_if_fail (SEAHORSE_GPGME_IS_KEY_PARMS (self), FALSE);

    return self->type == SEAHORSE_GPGME_KEY_GEN_TYPE_DSA_ELGAMAL ||
        self->type == SEAHORSE_GPGME_KEY_GEN_TYPE_RSA_RSA;
}

gboolean
seahorse_gpgme_key_parms_has_valid_name (SeahorseGpgmeKeyParms *self)
{
    g_return_val_if_fail (SEAHORSE_GPGME_IS_KEY_PARMS (self), FALSE);

    return (self->name != NULL) && (strlen (self->name) > 4);
}

gboolean
seahorse_gpgme_key_parms_is_valid (SeahorseGpgmeKeyParms *self)
{
    g_return_val_if_fail (SEAHORSE_GPGME_IS_KEY_PARMS (self), FALSE);

    return seahorse_gpgme_key_parms_has_valid_name (self);
}

const char *
key_algo_str (SeahorsePgpKeyAlgorithm algo)
{
    switch (algo) {
        case SEAHORSE_PGP_KEY_ALGORITHM_RSA:
            return "RSA";
        case SEAHORSE_PGP_KEY_ALGORITHM_DSA:
            return "DSA";
        case SEAHORSE_PGP_KEY_ALGORITHM_ELGAMAL:
            return "ELG-E";
        default: /* Others are not supported */
            return NULL;
    }
}

char *
seahorse_gpgme_key_parms_to_string (SeahorseGpgmeKeyParms *self)
{
    SeahorsePgpKeyAlgorithm algo;
    g_autoptr(GString) str = NULL;
    g_autofree char *expires_date = NULL;

    g_return_val_if_fail (SEAHORSE_GPGME_IS_KEY_PARMS (self), NULL);
    g_return_val_if_fail (seahorse_gpgme_key_parms_is_valid (self), NULL);

    /* Start */
    str = g_string_new ("<GnupgKeyParms format=\"internal\">");

    /* Key */
    algo = seahorse_gpgme_key_gen_type_get_key_algo (self->type);
    g_string_append_printf (str, "\nKey-Type: %s", key_algo_str (algo));
    g_string_append (str, "\nKey-Usage: sign");
    g_string_append_printf (str, "\nKey-Length: %u", self->key_length);

    /* Subkey */
    if (seahorse_gpgme_key_gen_type_get_subkey_algo (self->type, &algo)) {
        g_string_append_printf (str, "\nSubkey-Type: %s", key_algo_str (algo));
        g_string_append (str, "\nSubkey-Usage: encrypt");
        g_string_append_printf (str, "\nSubkey-Length: %d", self->key_length);
    }

    /* Name */
    g_string_append_printf (str, "\nName-Real: %s", self->name);
    if (self->comment != NULL && *self->comment != '\0')
        g_string_append_printf (str, "\nName-Comment: %s", self->comment);
    if (self->email != NULL && *self->email != '\0')
        g_string_append_printf (str, "\nName-Email: %s", self->email);

    /* Expires */
    if (self->expires != NULL)
        expires_date = g_date_time_format (self->expires, "%Y-%m-%d");
    else
        expires_date = g_strdup ("0");
    g_string_append_printf (str, "\nExpire-Date: %s", expires_date);

    /* Passphrase */
    if (self->passphrase != NULL && *self->passphrase != '\0')
        g_string_append_printf (str, "\nPassphrase: %s", self->passphrase);
    else
        g_string_append_printf (str, "\n%%no-protection");

    /* End */
    g_string_append (str, "\n</GnupgKeyParms>");

    return g_string_free_and_steal (g_steal_pointer (&str));
}

static void
seahorse_gpgme_key_parms_get_property (GObject      *object,
                                       unsigned int  prop_id,
                                       GValue       *value,
                                       GParamSpec   *pspec)
{
    SeahorseGpgmeKeyParms *self = SEAHORSE_GPGME_KEY_PARMS (object);

    switch (prop_id) {
    case PROP_NAME:
        g_value_set_string (value, seahorse_gpgme_key_parms_get_name (self));
        break;
    case PROP_EMAIL:
        g_value_set_string (value, seahorse_gpgme_key_parms_get_email (self));
        break;
    case PROP_COMMENT:
        g_value_set_string (value, seahorse_gpgme_key_parms_get_comment (self));
        break;
    case PROP_KEY_TYPE:
        g_value_set_enum (value, seahorse_gpgme_key_parms_get_key_type (self));
        break;
    case PROP_KEY_LENGTH:
        g_value_set_uint (value, seahorse_gpgme_key_parms_get_key_length (self));
        break;
    case PROP_EXPIRES:
        g_value_set_boxed (value, seahorse_gpgme_key_parms_get_expires (self));
        break;
    case PROP_IS_VALID:
        g_value_set_boolean (value, seahorse_gpgme_key_parms_is_valid (self));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
seahorse_gpgme_key_parms_set_property (GObject      *object,
                                       unsigned int  prop_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
    SeahorseGpgmeKeyParms *self = SEAHORSE_GPGME_KEY_PARMS (object);

    switch (prop_id) {
    case PROP_NAME:
        seahorse_gpgme_key_parms_set_name (self, g_value_get_string (value));
        break;
    case PROP_EMAIL:
        seahorse_gpgme_key_parms_set_email (self, g_value_get_string (value));
        break;
    case PROP_COMMENT:
        seahorse_gpgme_key_parms_set_comment (self, g_value_get_string (value));
        break;
    case PROP_KEY_TYPE:
        seahorse_gpgme_key_parms_set_key_type (self, g_value_get_enum (value));
        break;
    case PROP_KEY_LENGTH:
        seahorse_gpgme_key_parms_set_key_length (self, g_value_get_uint (value));
        break;
    case PROP_EXPIRES:
        seahorse_gpgme_key_parms_set_expires (self, g_value_get_boxed (value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
seahorse_gpgme_key_parms_init (SeahorseGpgmeKeyParms *self)
{
    self->name = g_strdup ("");
    self->email = g_strdup ("");
    self->comment = g_strdup ("");
    self->type = SEAHORSE_GPGME_KEY_GEN_TYPE_RSA_RSA;
    self->key_length = 4096;
}

static void
seahorse_gpgme_key_parms_finalize (GObject *obj)
{
    SeahorseGpgmeKeyParms *self = SEAHORSE_GPGME_KEY_PARMS (obj);

    g_clear_pointer (&self->name, g_free);
    g_clear_pointer (&self->email, g_free);
    g_clear_pointer (&self->comment, g_free);
    g_clear_pointer (&self->passphrase, gcr_secure_memory_free);
    g_clear_object (&self->expires);

    G_OBJECT_CLASS (seahorse_gpgme_key_parms_parent_class)->finalize (obj);
}

static void
seahorse_gpgme_key_parms_class_init (SeahorseGpgmeKeyParmsClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

    gobject_class->finalize = seahorse_gpgme_key_parms_finalize;
    gobject_class->get_property = seahorse_gpgme_key_parms_get_property;
    gobject_class->set_property = seahorse_gpgme_key_parms_set_property;

    obj_props[PROP_NAME] =
        g_param_spec_string ("name", NULL, NULL,
                             NULL,
                             G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
    obj_props[PROP_EMAIL] =
        g_param_spec_string ("email", NULL, NULL,
                             NULL,
                             G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
    obj_props[PROP_COMMENT] =
        g_param_spec_string ("comment", NULL, NULL,
                             NULL,
                             G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
    obj_props[PROP_KEY_TYPE] =
        g_param_spec_enum ("key-type", NULL, NULL,
                           SEAHORSE_TYPE_GPGME_KEY_GEN_TYPE,
                           SEAHORSE_GPGME_KEY_GEN_TYPE_RSA_RSA,
                           G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
    obj_props[PROP_KEY_LENGTH] =
        g_param_spec_uint ("key-length", NULL, NULL,
                           0, G_MAXUINT, 0,
                           G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
    obj_props[PROP_EXPIRES] =
        g_param_spec_boxed ("expires", NULL, NULL,
                            G_TYPE_DATE_TIME,
                            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
    obj_props[PROP_IS_VALID] =
        g_param_spec_boolean ("is-valid", NULL, NULL,
                              FALSE,
                              G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties (gobject_class, N_PROPS, obj_props);
}

SeahorseGpgmeKeyParms *
seahorse_gpgme_key_parms_new (void)
{
    return g_object_new (SEAHORSE_GPGME_TYPE_KEY_PARMS, NULL);
}
