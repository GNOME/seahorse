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

#include "seahorse-pgp-key.h"
#include "seahorse-pgp-uid.h"
#include "seahorse-pgp-signature.h"

#include <string.h>

#include <glib/gi18n.h>

enum {
    PROP_0,
    PROP_PARENT,
    PROP_SIGNATURES,
    PROP_VALIDITY,
    PROP_NAME,
    PROP_EMAIL,
    PROP_COMMENT
};

typedef struct _SeahorsePgpUidPrivate {
    SeahorsePgpKey *parent;
    GListModel *signatures;
    SeahorseValidity validity;
    gboolean realized;
    char *name;
    char *email;
    char *comment;
} SeahorsePgpUidPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (SeahorsePgpUid, seahorse_pgp_uid, G_TYPE_OBJECT);

/* -----------------------------------------------------------------------------
 * INTERNAL HELPERS
 */

static char *
convert_string (const char *str)
{
    if (!str)
            return NULL;

    /* If not utf8 valid, assume latin 1 */
     if (!g_utf8_validate (str, -1, NULL))
         return g_convert (str, -1, "UTF-8", "ISO-8859-1", NULL, NULL, NULL);

    return g_strdup (str);
}

#ifndef HAVE_STRSEP
/* code taken from glibc-2.2.1/sysdeps/generic/strsep.c */
char *
strsep (char **stringp, const char *delim)
{
    char *begin, *end;

    begin = *stringp;
    if (begin == NULL)
        return NULL;

      /* A frequent case is when the delimiter string contains only one
         character.  Here we don't need to call the expensive `strpbrk'
         function and instead work using `strchr'.  */
      if (delim[0] == '\0' || delim[1] == '\0') {
        char ch = delim[0];

        if (ch == '\0')
            end = NULL;
        else {
            if (*begin == ch)
                end = begin;
            else if (*begin == '\0')
                end = NULL;
            else
                end = strchr (begin + 1, ch);
        }
    } else
        /* Find the end of the token.  */
        end = strpbrk (begin, delim);

    if (end) {
      /* Terminate the token and set *STRINGP past NUL character.  */
      *end++ = '\0';
      *stringp = end;
    } else
        /* No more delimiters; this is the last token.  */
        *stringp = NULL;

    return begin;
}
#endif /*HAVE_STRSEP*/

/* Copied from GPGME */
static void
parse_user_id (const char *uid, char **name, char **email, char **comment)
{
    char *src, *tail;
    g_autofree char *x = NULL;
    int in_name = 0;
    int in_email = 0;
    int in_comment = 0;

    x = tail = src = g_strdup (uid);

    while (*src) {
        if (in_email) {
            if (*src == '<')
                /* Not legal but anyway.  */
                in_email++;
            else if (*src == '>') {
                if (!--in_email && !*email) {
                    *email = tail;
                    *src = 0;
                    tail = src + 1;
                }
            }
        } else if (in_comment) {
            if (*src == '(')
                in_comment++;
            else if (*src == ')') {
                if (!--in_comment && !*comment) {
                    *comment = tail;
                    *src = 0;
                    tail = src + 1;
                }
            }
        } else if (*src == '<') {
            if (in_name) {
                if (!*name) {
                    *name = tail;
                    *src = 0;
                    tail = src + 1;
                }
                in_name = 0;
            } else
                tail = src + 1;

            in_email = 1;
        } else if (*src == '(') {
            if (in_name) {
                if (!*name) {
                    *name = tail;
                    *src = 0;
                    tail = src + 1;
                }
                in_name = 0;
            }
            in_comment = 1;
        } else if (!in_name && *src != ' ' && *src != '\t') {
            in_name = 1;
        }
        src++;
    }

    if (in_name) {
        if (!*name) {
            *name = tail;
            *src = 0;
            tail = src + 1;
        }
    }

    /* Let unused parts point to an EOS.  */
    *name = g_strdup (*name ? *name : "");
    *email = g_strdup (*email ? *email : "");
    *comment = g_strdup (*comment ? *comment : "");

    g_strstrip (*name);
    g_strstrip (*email);
    g_strstrip (*comment);
}


/* -----------------------------------------------------------------------------
 * OBJECT
 */

void
seahorse_pgp_uid_realize (SeahorsePgpUid *self)
{
    SeahorsePgpUidPrivate *priv = seahorse_pgp_uid_get_instance_private (self);
    g_autofree char *markup = NULL;
    g_autofree char *label = NULL;

    /* Don't realize if no name present */
    if (!priv->name)
        return;

    priv->realized = TRUE;
}

static void
seahorse_pgp_uid_init (SeahorsePgpUid *self)
{
    SeahorsePgpUidPrivate *priv = seahorse_pgp_uid_get_instance_private (self);

    priv->signatures = G_LIST_MODEL (g_list_store_new (SEAHORSE_PGP_TYPE_SIGNATURE));
}

static void
seahorse_pgp_uid_constructed (GObject *object)
{
    G_OBJECT_CLASS (seahorse_pgp_uid_parent_class)->constructed (object);
    seahorse_pgp_uid_realize (SEAHORSE_PGP_UID (object));
}

static void
seahorse_pgp_uid_get_property (GObject     *object,
                               unsigned int prop_id,
                               GValue      *value,
                               GParamSpec  *pspec)
{
    SeahorsePgpUid *self = SEAHORSE_PGP_UID (object);

    switch (prop_id) {
    case PROP_SIGNATURES:
        g_value_set_object (value, seahorse_pgp_uid_get_signatures (self));
        break;
    case PROP_PARENT:
        g_value_set_object (value, seahorse_pgp_uid_get_parent (self));
        break;
    case PROP_VALIDITY:
        g_value_set_uint (value, seahorse_pgp_uid_get_validity (self));
        break;
    case PROP_NAME:
        g_value_set_string (value, seahorse_pgp_uid_get_name (self));
        break;
    case PROP_EMAIL:
        g_value_set_string (value, seahorse_pgp_uid_get_email (self));
        break;
    case PROP_COMMENT:
        g_value_set_string (value, seahorse_pgp_uid_get_comment (self));
        break;
    }
}

static void
seahorse_pgp_uid_set_property (GObject      *object,
                               unsigned int  prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
    SeahorsePgpUid *self = SEAHORSE_PGP_UID (object);
    SeahorsePgpUidPrivate *priv = seahorse_pgp_uid_get_instance_private (self);

    switch (prop_id) {
    case PROP_PARENT:
        g_return_if_fail (priv->parent == NULL);
        priv->parent = g_value_get_object (value);
        break;
    case PROP_VALIDITY:
        seahorse_pgp_uid_set_validity (self, g_value_get_uint (value));
        break;
    case PROP_NAME:
        seahorse_pgp_uid_set_name (self, g_value_get_string (value));
        break;
    case PROP_EMAIL:
        seahorse_pgp_uid_set_email (self, g_value_get_string (value));
        break;
    case PROP_COMMENT:
        seahorse_pgp_uid_set_comment (self, g_value_get_string (value));
        break;
    }
}

static void
seahorse_pgp_uid_object_finalize (GObject *gobject)
{
    SeahorsePgpUid *self = SEAHORSE_PGP_UID (gobject);
    SeahorsePgpUidPrivate *priv = seahorse_pgp_uid_get_instance_private (self);

    g_clear_object (&priv->signatures);
    g_clear_pointer (&priv->name, g_free);
    g_clear_pointer (&priv->email, g_free);
    g_clear_pointer (&priv->comment, g_free);

    G_OBJECT_CLASS (seahorse_pgp_uid_parent_class)->finalize (gobject);
}

static void
seahorse_pgp_uid_class_init (SeahorsePgpUidClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

    gobject_class->constructed = seahorse_pgp_uid_constructed;
    gobject_class->finalize = seahorse_pgp_uid_object_finalize;
    gobject_class->set_property = seahorse_pgp_uid_set_property;
    gobject_class->get_property = seahorse_pgp_uid_get_property;

    g_object_class_install_property (gobject_class, PROP_VALIDITY,
        g_param_spec_uint ("validity", "Validity", "Validity of this identity",
                           0, G_MAXUINT, 0,
                           G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property (gobject_class, PROP_PARENT,
        g_param_spec_object ("parent", "Parent Key", "Parent Key",
                             SEAHORSE_PGP_TYPE_KEY,
                             G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT_ONLY));

    g_object_class_install_property (gobject_class, PROP_NAME,
        g_param_spec_string ("name", "Name", "User ID name",
                             "", G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property (gobject_class, PROP_EMAIL,
        g_param_spec_string ("email", "Email", "User ID email",
                             "", G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property (gobject_class, PROP_COMMENT,
        g_param_spec_string ("comment", "Comment", "User ID comment",
                             "", G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property (gobject_class, PROP_SIGNATURES,
        g_param_spec_object ("signatures", "Signatures", "Signatures on this UID",
                             G_TYPE_LIST_MODEL,
                             G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
}

/* -----------------------------------------------------------------------------
 * PUBLIC
 */

SeahorsePgpUid *
seahorse_pgp_uid_new (SeahorsePgpKey *parent,
                      const char     *uid_string)
{
    g_autofree char *name = NULL;
    g_autofree char *email = NULL;
    g_autofree char *comment = NULL;

    g_return_val_if_fail (SEAHORSE_PGP_IS_KEY (parent), NULL);

    if (uid_string)
        parse_user_id (uid_string, &name, &email, &comment);

    return g_object_new (SEAHORSE_PGP_TYPE_UID,
                         "parent", parent,
                         "name", name,
                         "email", email,
                         "comment", comment,
                         NULL);
}

/**
 * seahorse_pgp_uid_get_signatures:
 * @self: A uid
 *
 * Returns: (transfer none): The PGP key this UID belongs to
 */
SeahorsePgpKey *
seahorse_pgp_uid_get_parent (SeahorsePgpUid *self)
{
    SeahorsePgpUidPrivate *priv = seahorse_pgp_uid_get_instance_private (self);

    g_return_val_if_fail (SEAHORSE_PGP_IS_UID (self), NULL);
    return priv->parent;
}

/**
 * seahorse_pgp_uid_get_signatures:
 * @self: A uid
 *
 * Returns: (transfer none): The list of #SeahorsePgpSignatures
 */
GListModel *
seahorse_pgp_uid_get_signatures (SeahorsePgpUid *self)
{
    SeahorsePgpUidPrivate *priv = seahorse_pgp_uid_get_instance_private (self);

    g_return_val_if_fail (SEAHORSE_PGP_IS_UID (self), NULL);
    return priv->signatures;
}

void
seahorse_pgp_uid_add_signature (SeahorsePgpUid       *self,
                                SeahorsePgpSignature *signature)
{
    SeahorsePgpUidPrivate *priv = seahorse_pgp_uid_get_instance_private (self);
    const char *keyid;

    g_return_if_fail (SEAHORSE_PGP_IS_UID (self));
    g_return_if_fail (SEAHORSE_PGP_IS_SIGNATURE (signature));

    keyid = seahorse_pgp_signature_get_keyid (signature);

    /* Don't add signature of the parent key */
    if (seahorse_pgp_key_has_keyid (priv->parent, keyid))
        return;

    /* Don't allow duplicates */
    for (unsigned i = 0; i < g_list_model_get_n_items (priv->signatures); i++) {
        g_autoptr(SeahorsePgpSignature) sig = g_list_model_get_item (priv->signatures, i);
        const char *sig_keyid;

        sig = g_list_model_get_item (priv->signatures, i);
        sig_keyid = seahorse_pgp_signature_get_keyid (sig);
        if (seahorse_pgp_keyid_equal (keyid, sig_keyid))
            return;
    }

    g_list_store_append (G_LIST_STORE (priv->signatures), signature);
}

void
seahorse_pgp_uid_remove_signature (SeahorsePgpUid       *self,
                                   SeahorsePgpSignature *signature)
{
    SeahorsePgpUidPrivate *priv = seahorse_pgp_uid_get_instance_private (self);

    g_return_if_fail (SEAHORSE_PGP_IS_UID (self));
    g_return_if_fail (SEAHORSE_PGP_IS_SIGNATURE (signature));

    for (unsigned i = 0; i < g_list_model_get_n_items (priv->signatures); i++) {
        g_autoptr(SeahorsePgpSignature) sig = NULL;

        sig = g_list_model_get_item (priv->signatures, i);
        if (signature == sig) {
            g_list_store_remove (G_LIST_STORE (priv->signatures), i);
            break;
        }
    }
}

SeahorseValidity
seahorse_pgp_uid_get_validity (SeahorsePgpUid *self)
{
    SeahorsePgpUidPrivate *priv = seahorse_pgp_uid_get_instance_private (self);

    g_return_val_if_fail (SEAHORSE_PGP_IS_UID (self), SEAHORSE_VALIDITY_UNKNOWN);
    return priv->validity;
}

void
seahorse_pgp_uid_set_validity (SeahorsePgpUid *self, SeahorseValidity validity)
{
    SeahorsePgpUidPrivate *priv = seahorse_pgp_uid_get_instance_private (self);

    g_return_if_fail (SEAHORSE_PGP_IS_UID (self));
    priv->validity = validity;
    g_object_notify (G_OBJECT (self), "validity");
}

/**
 * seahorse_pgp_uid_get_name:
 * @self: A uid
 *
 * Returns: (transfer none): The name part of the UID
 */
const char *
seahorse_pgp_uid_get_name (SeahorsePgpUid *self)
{
    SeahorsePgpUidPrivate *priv = seahorse_pgp_uid_get_instance_private (self);

    g_return_val_if_fail (SEAHORSE_PGP_IS_UID (self), NULL);
    if (!priv->name)
        priv->name = g_strdup ("");
    return priv->name;
}

void
seahorse_pgp_uid_set_name (SeahorsePgpUid *self, const char *name)
{
    SeahorsePgpUidPrivate *priv = seahorse_pgp_uid_get_instance_private (self);
    GObject *obj;

    g_return_if_fail (SEAHORSE_PGP_IS_UID (self));

    g_free (priv->name);
    priv->name = convert_string (name);

    obj = G_OBJECT (self);
    g_object_freeze_notify (obj);
    if (!priv->realized)
        seahorse_pgp_uid_realize (self);
    g_object_notify (obj, "name");
    g_object_thaw_notify (obj);
}

/**
 * seahorse_pgp_uid_get_email:
 * @self: A uid
 *
 * Returns: (transfer none): The email part of the UID (if empty, returns "")
 */
const char *
seahorse_pgp_uid_get_email (SeahorsePgpUid *self)
{
    SeahorsePgpUidPrivate *priv = seahorse_pgp_uid_get_instance_private (self);

    g_return_val_if_fail (SEAHORSE_PGP_IS_UID (self), NULL);
    if (!priv->email)
        priv->email = g_strdup ("");
    return priv->email;
}

void
seahorse_pgp_uid_set_email (SeahorsePgpUid *self, const char *email)
{
    SeahorsePgpUidPrivate *priv = seahorse_pgp_uid_get_instance_private (self);
    GObject *obj = G_OBJECT (self);

    g_return_if_fail (SEAHORSE_PGP_IS_UID (self));

    g_free (priv->email);
    priv->email = convert_string (email);

    g_object_freeze_notify (obj);
    if (!priv->realized)
        seahorse_pgp_uid_realize (self);
    g_object_notify (obj, "email");
    g_object_thaw_notify (obj);
}

/**
 * seahorse_pgp_uid_get_comment:
 * @self: A uid
 *
 * Returns: (transfer none): The comment part of the UID (if empty, returns "")
 */
const char *
seahorse_pgp_uid_get_comment (SeahorsePgpUid *self)
{
    SeahorsePgpUidPrivate *priv = seahorse_pgp_uid_get_instance_private (self);

    g_return_val_if_fail (SEAHORSE_PGP_IS_UID (self), NULL);
    if (!priv->comment)
        priv->comment = g_strdup ("");
    return priv->comment;
}

void
seahorse_pgp_uid_set_comment (SeahorsePgpUid *self, const char *comment)
{
    SeahorsePgpUidPrivate *priv = seahorse_pgp_uid_get_instance_private (self);
    GObject *obj = G_OBJECT (self);

    g_return_if_fail (SEAHORSE_PGP_IS_UID (self));

    g_free (priv->comment);
    priv->comment = convert_string (comment);

    g_object_freeze_notify (obj);
    if (!priv->realized)
        seahorse_pgp_uid_realize (self);
    g_object_notify (obj, "comment");
    g_object_thaw_notify (obj);
}

/**
 * seahorse_pgp_uid_calc_label:
 * @name:
 * @email: (nullable):
 * @command: (nullable):
 *
 * Builds a PGP UID from the name (and if available) name and comment in the
 * form of "name (comment) <email>"
 *
 * Returns: (transfer full): The PGP UID string
 */
char *
seahorse_pgp_uid_calc_label (const char *name,
                             const char *email,
                             const char *comment)
{
    GString *string;

    g_return_val_if_fail (name, NULL);

    string = g_string_new ("");
    g_string_append (string, name);

    if (email && email[0]) {
        g_string_append (string, " <");
        g_string_append (string, email);
        g_string_append (string, ">");
    }

    if (comment && comment[0]) {
        g_string_append (string, " (");
        g_string_append (string, comment);
        g_string_append (string, ")");
    }

    return g_string_free (string, FALSE);
}
