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

struct _SeahorseGpgmeKeyParms {
    GObject parent_instance;

    char *name;
    char *email;
    char *comment;
    char *passphrase;
    SeahorsePgpKeyAlgorithm type;
    unsigned int length;
    GDateTime *expires;
};

G_DEFINE_TYPE (SeahorseGpgmeKeyParms, seahorse_gpgme_key_parms, G_TYPE_OBJECT);

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

    return self->type == SEAHORSE_PGP_KEY_ALGO_DSA_ELGAMAL ||
        self->type == SEAHORSE_PGP_KEY_ALGO_RSA_RSA;
}

gboolean
seahorse_gpgme_key_parms_has_valid_name (SeahorseGpgmeKeyParms *self)
{
    g_return_val_if_fail (SEAHORSE_GPGME_IS_KEY_PARMS (self), FALSE);

    return (self->name != NULL) && (strlen (self->name) > 4);
}

gboolean
seahorse_gpgme_key_parms_has_valid_key_length (SeahorseGpgmeKeyParms *self)
{
    g_return_val_if_fail (SEAHORSE_GPGME_IS_KEY_PARMS (self), FALSE);

    switch (self->type) {
    case SEAHORSE_PGP_KEY_ALGO_DSA_ELGAMAL:
        return (self->length >= ELGAMAL_MIN || self->length <= LENGTH_MAX);
    case SEAHORSE_PGP_KEY_ALGO_DSA:
        return (self->length >= DSA_MIN || self->length <= DSA_MAX);
    case SEAHORSE_PGP_KEY_ALGO_RSA_RSA:
    case SEAHORSE_PGP_KEY_ALGO_RSA_SIGN:
        return (self->length >= RSA_MIN || self->length <= LENGTH_MAX);
    default:
        g_return_val_if_reached (FALSE);
    }
}

char *
seahorse_gpgme_key_parms_to_string (SeahorseGpgmeKeyParms *self)
{
    g_autoptr(GString) str = NULL;
    g_autofree char *expires_date = NULL;

    g_return_val_if_fail (SEAHORSE_GPGME_IS_KEY_PARMS (self), NULL);
    g_return_val_if_fail (seahorse_gpgme_key_parms_has_valid_name (self), NULL);
    g_return_val_if_fail (seahorse_gpgme_key_parms_has_valid_key_length (self), NULL);

    /* Start */
    str = g_string_new ("<GnupgKeyParms format=\"internal\">");

    /* Key */
    if (self->type == SEAHORSE_PGP_KEY_ALGO_DSA || self->type == SEAHORSE_PGP_KEY_ALGO_DSA_ELGAMAL)
        g_string_append (str, "\nKey-Type: DSA");
    else
        g_string_append (str, "\nKey-Type: RSA");
    g_string_append (str, "\nKey-Usage: sign");
    g_string_append_printf (str, "\nKey-Length: %d", self->length);

    /* Subkey */
    if (seahorse_gpgme_key_parms_has_subkey (self)) {
        if (self->type == SEAHORSE_PGP_KEY_ALGO_DSA_ELGAMAL)
            g_string_append (str, "\nSubkey-Type: ELG-E");
        else if (self->type == SEAHORSE_PGP_KEY_ALGO_RSA_RSA)
            g_string_append (str, "\nSubkey-Type: RSA");
        else
            g_return_val_if_reached (NULL);
        g_string_append (str, "\nSubkey-Usage: encrypt");
        g_string_append_printf (str, "\nSubkey-Length: %d", self->length);
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
seahorse_gpgme_key_parms_init (SeahorseGpgmeKeyParms *self)
{
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
}


SeahorseGpgmeKeyParms *
seahorse_gpgme_key_parms_new (const char              *name,
                              const char              *email,
                              const char              *comment,
                              SeahorsePgpKeyAlgorithm  type,
                              unsigned int             length,
                              GDateTime               *expires)
{
    g_autoptr(SeahorseGpgmeKeyParms) self = NULL;

    self = g_object_new (SEAHORSE_GPGME_TYPE_KEY_PARMS, NULL);
    self->name = g_strdup (name);
    self->email = g_strdup (email);
    self->comment = g_strdup (comment);
    self->type = type;
    self->length = length;
    self->expires = expires? g_object_ref (expires) : NULL;

    return g_steal_pointer (&self);
}
