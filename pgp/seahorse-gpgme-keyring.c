/*
 * Seahorse
 *
 * Copyright (C) 2004-2006 Stefan Walter
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

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "operation"

#include "seahorse-gpgme-keyring.h"

#include "seahorse-gpgme-data.h"
#include "seahorse-gpgme.h"
#include "seahorse-gpgme-key-op.h"
#include "seahorse-pgp-actions.h"
#include "seahorse-pgp-key.h"

#include "seahorse-common.h"

#include "libseahorse/seahorse-progress.h"
#include "libseahorse/seahorse-util.h"

#include <gcr/gcr.h>

#include <glib/gi18n.h>

#include <stdlib.h>
#include <string.h>
#include <libintl.h>
#include <locale.h>

/* Amount of keys to load in a batch */
#define DEFAULT_LOAD_BATCH 50

struct _SeahorseGpgmeKeyring {
    GObject parent_instance;

    GHashTable *keys;
    unsigned int scheduled_refresh;         /* Source for refresh timeout */
    GFileMonitor *monitor_handle;           /* For monitoring the .gnupg directory */
    GList *orphan_secret;                   /* Orphan secret keys */
    GActionGroup *actions;
};

enum {
    PROP_0,
    PROP_LABEL,
    PROP_DESCRIPTION,
    PROP_ICON,
    PROP_CATEGORY,
    PROP_URI,
    PROP_ACTIONS,
    PROP_ACTION_PREFIX,
    PROP_MENU_MODEL,
    PROP_SHOW_IF_EMPTY,
    N_PROPS
};

static void     seahorse_gpgme_keyring_place_iface        (SeahorsePlaceIface *iface);

static void     seahorse_gpgme_keyring_collection_iface   (GcrCollectionIface *iface);

G_DEFINE_TYPE_WITH_CODE (SeahorseGpgmeKeyring, seahorse_gpgme_keyring, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (GCR_TYPE_COLLECTION, seahorse_gpgme_keyring_collection_iface);
                         G_IMPLEMENT_INTERFACE (SEAHORSE_TYPE_PLACE, seahorse_gpgme_keyring_place_iface);
);

enum {
    LOAD_FULL = 0x01,
    LOAD_PHOTOS = 0x02
};

static gpgme_error_t
passphrase_get (void       *hook,
                const char *uid_hint,
                const char *passphrase_info,
                int         flags,
                int         fd)
{
    SeahorsePassphrasePrompt *dialog;
    gpgme_error_t err;
    g_auto(GStrv) split_uid = NULL;
    g_autofree char *label = NULL;
    g_autofree char *errmsg = NULL;
    const char *pass;
    gboolean confirm = FALSE;

    if (passphrase_info && strlen(passphrase_info) < 16) {
        flags |= SEAHORSE_PASS_NEW;
        confirm = TRUE;
    }

    if (uid_hint)
        split_uid = g_strsplit (uid_hint, " ", 2);

    if (flags & SEAHORSE_PASS_BAD)
        errmsg = g_strdup_printf (_("Wrong passphrase."));

    if (split_uid && split_uid[0] && split_uid[1]) {
        if (flags & SEAHORSE_PASS_NEW)
            label = g_strdup_printf (_("Enter new passphrase for “%s”"), split_uid[1]);
        else
            label = g_strdup_printf (_("Enter passphrase for “%s”"), split_uid[1]);
    } else {
        if (flags & SEAHORSE_PASS_NEW)
            label = g_strdup (_("Enter new passphrase"));
        else
            label = g_strdup (_("Enter passphrase"));
    }

    dialog = seahorse_passphrase_prompt_show_dialog (_("Passphrase"),
                                                     errmsg ? errmsg : label,
                                                     NULL,
                                                     NULL,
                                                     confirm);

    switch (gtk_dialog_run (GTK_DIALOG (dialog))) {
    case GTK_RESPONSE_ACCEPT:
        pass = seahorse_passphrase_prompt_get_text (dialog);
        seahorse_util_printf_fd (fd, "%s\n", pass);
        err = GPG_OK;
        break;
    default:
        err = GPG_E (GPG_ERR_CANCELED);
        break;
    };

    gtk_widget_destroy (GTK_WIDGET (dialog));
    return err;
}

typedef struct {
    SeahorseGpgmeKeyring *keyring;
    gpgme_ctx_t gctx;
    GHashTable *checks;
    int parts;
    int loaded;
} keyring_list_closure;

static void
keyring_list_free (void *data)
{
    keyring_list_closure *closure = data;
    if (closure->gctx)
        gpgme_release (closure->gctx);
    if (closure->checks)
        g_hash_table_destroy (closure->checks);
    g_clear_object (&closure->keyring);
    g_free (closure);
}

/* Add a key to the context  */
static SeahorseGpgmeKey *
add_key_to_context (SeahorseGpgmeKeyring *self,
                    gpgme_key_t           key)
{
    SeahorseGpgmeKey *pkey = NULL;
    SeahorseGpgmeKey *prev;
    const char *keyid;

    g_return_val_if_fail (SEAHORSE_IS_GPGME_KEYRING (self), NULL);
    g_return_val_if_fail (key->subkeys && key->subkeys->keyid, NULL);

    keyid = key->subkeys->keyid;
    g_return_val_if_fail (keyid, NULL);

    prev = seahorse_gpgme_keyring_lookup (self, keyid);

    /* Check if we can just replace the key on the object */
    if (prev != NULL) {
        if (key->secret)
            g_object_set (prev, "seckey", key, NULL);
        else
            g_object_set (prev, "pubkey", key, NULL);
        return prev;
    }

    /* Create a new key with secret */
    if (key->secret) {
        pkey = seahorse_gpgme_key_new (SEAHORSE_PLACE (self), NULL, key);

        /* Since we don't have a public key yet, save this away */
        self->orphan_secret = g_list_append (self->orphan_secret, pkey);

        /* No key was loaded as far as everyone is concerned */
        return NULL;
    }

    /* Just a new public key */

    /* Check for orphans */
    for (GList *l = self->orphan_secret; l; l = g_list_next (l)) {
        gpgme_key_t seckey;

        seckey = seahorse_gpgme_key_get_private (l->data);
        g_return_val_if_fail (seckey && seckey->subkeys && seckey->subkeys->keyid, NULL);

        /* Look for a matching key */
        if (g_str_equal (keyid, seckey->subkeys->keyid)) {

            /* Set it up properly */
            pkey = SEAHORSE_GPGME_KEY (l->data);
            g_object_set (pkey, "pubkey", key, NULL);

            /* Remove item from orphan list cleanly */
            self->orphan_secret = g_list_remove_link (self->orphan_secret, l);
            g_list_free (l);
            break;
        }
    }

    if (pkey == NULL)
        pkey = seahorse_gpgme_key_new (SEAHORSE_PLACE (self), key, NULL);

    /* Add to context */
    g_hash_table_insert (self->keys, g_strdup (keyid), pkey);
    gcr_collection_emit_added (GCR_COLLECTION (self), G_OBJECT (pkey));

    return pkey;
}


/* Remove the given key from the context */
static void
remove_key (SeahorseGpgmeKeyring *self,
            const char           *keyid)
{
    SeahorseGpgmeKey *key;

    key = g_hash_table_lookup (self->keys, keyid);
    if (key != NULL)
        seahorse_gpgme_keyring_remove_key (self, key);
}

/* Completes one batch of key loading */
static gboolean
on_idle_list_batch_of_keys (void *data)
{
    GTask *task = G_TASK (data);
    keyring_list_closure *closure = g_task_get_task_data (task);
    SeahorseGpgmeKey *pkey;
    GHashTableIter iter;
    gpgme_key_t key;
    unsigned int batch;
    g_autofree char *detail = NULL;
    const char *keyid;

    /* We load until done if batch is zero */
    batch = DEFAULT_LOAD_BATCH;

    while (batch-- > 0) {
        if (!GPG_IS_OK (gpgme_op_keylist_next (closure->gctx, &key))) {

            gpgme_op_keylist_end (closure->gctx);

            /* If we were a refresh loader, then we remove the keys we didn't find */
            if (closure->checks) {
                g_hash_table_iter_init (&iter, closure->checks);
                while (g_hash_table_iter_next (&iter, (void **) &keyid, NULL))
                    remove_key (closure->keyring, keyid);
            }

            seahorse_progress_end (g_task_get_cancellable (task), task);
            g_task_return_boolean (task, TRUE);
            return FALSE; /* Remove event handler */
        }

        g_return_val_if_fail (key->subkeys && key->subkeys->keyid, FALSE);

        /* During a refresh if only new or removed keys */
        if (closure->checks) {
            /* Make note that this key exists in key ring */
            g_hash_table_remove (closure->checks, key->subkeys->keyid);
        }

        pkey = add_key_to_context (closure->keyring, key);

        /* Load additional info */
        if (pkey && closure->parts & LOAD_PHOTOS)
            seahorse_gpgme_key_op_photos_load (pkey);

        gpgme_key_unref (key);
        closure->loaded++;
    }

    detail = g_strdup_printf (ngettext("Loaded %d key", "Loaded %d keys", closure->loaded), closure->loaded);
    seahorse_progress_update (g_task_get_cancellable (task), task, detail);

    return TRUE;
}

static void
on_keyring_list_cancelled (GCancellable *cancellable,
                           void         *user_data)
{
    GTask *task = G_TASK (user_data);
    keyring_list_closure *closure = g_task_get_task_data (task);

    gpgme_op_keylist_end (closure->gctx);
}

static void
seahorse_gpgme_keyring_list_async (SeahorseGpgmeKeyring *self,
                                   const char          **patterns,
                                   int                   parts,
                                   gboolean              secret,
                                   GCancellable         *cancellable,
                                   GAsyncReadyCallback   callback,
                                   void                 *user_data)
{
    g_autoptr(GTask) task = NULL;
    keyring_list_closure *closure;
    SeahorseObject *object;
    gpgme_error_t gerr = 0;
    GHashTableIter iter;
    g_autoptr(GError) error = NULL;

    task = g_task_new (self, cancellable, callback, user_data);

    closure = g_new0 (keyring_list_closure, 1);
    closure->parts = parts;
    closure->gctx = seahorse_gpgme_keyring_new_context (&gerr);
    closure->keyring = g_object_ref (self);
    g_task_set_task_data (task, closure, keyring_list_free);

    /* Start the key listing */
    if (closure->gctx) {
        if (parts & LOAD_FULL)
            gpgme_set_keylist_mode (closure->gctx, GPGME_KEYLIST_MODE_SIGS |
                                    gpgme_get_keylist_mode (closure->gctx));
        if (patterns)
            gerr = gpgme_op_keylist_ext_start (closure->gctx, patterns, secret, 0);
        else
            gerr = gpgme_op_keylist_start (closure->gctx, NULL, secret);
    }

    if (gerr != 0) {
        seahorse_gpgme_propagate_error (gerr, &error);
        g_task_return_error (task, g_steal_pointer (&error));
        return;
    }

    /* Loading all the keys? */
    if (patterns == NULL) {
        char *keyid;

        closure->checks = g_hash_table_new_full (seahorse_pgp_keyid_hash,
                                                 seahorse_pgp_keyid_equal,
                                                 g_free, NULL);
        g_hash_table_iter_init (&iter, self->keys);
        while (g_hash_table_iter_next (&iter, (void **) &keyid, (void **) &object)) {
            if ((secret && seahorse_object_get_usage (object) == SEAHORSE_USAGE_PRIVATE_KEY) ||
                (!secret && seahorse_object_get_usage (object) == SEAHORSE_USAGE_PUBLIC_KEY)) {
                keyid = g_strdup (keyid);
                g_hash_table_insert (closure->checks, keyid, keyid);
            }
        }
    }

    seahorse_progress_prep_and_begin (cancellable, task, NULL);
    if (cancellable)
        g_cancellable_connect (cancellable,
                               G_CALLBACK (on_keyring_list_cancelled),
                               task, NULL);

    g_idle_add_full (G_PRIORITY_LOW, on_idle_list_batch_of_keys,
                     g_steal_pointer (&task), g_object_unref);
}

static gboolean
seahorse_gpgme_keyring_list_finish (SeahorseGpgmeKeyring *keyring,
                                    GAsyncResult *result,
                                    GError **error)
{
    g_return_val_if_fail (g_task_is_valid (result, keyring), FALSE);

    return g_task_propagate_boolean (G_TASK (result), error);
}

static void
cancel_scheduled_refresh (SeahorseGpgmeKeyring *self)
{
    if (self->scheduled_refresh != 0) {
        g_debug ("cancelling scheduled refresh event");
        g_source_remove (self->scheduled_refresh);
        self->scheduled_refresh = 0;
    }
}

static gboolean
scheduled_dummy (void *user_data)
{
    SeahorseGpgmeKeyring *self = SEAHORSE_GPGME_KEYRING (user_data);
    g_debug ("dummy refresh event occurring now");
    self->scheduled_refresh = 0;
    return G_SOURCE_REMOVE;
}

typedef struct {
    SeahorseGpgmeKeyring *self;
    const char **patterns;
} keyring_load_closure;

static void
on_keyring_public_list_complete (GObject      *source,
                                 GAsyncResult *result,
                                 void         *user_data);

static void
on_keyring_secret_list_complete (GObject      *source,
                                 GAsyncResult *result,
                                 void         *user_data)
{
    g_autoptr(GTask) task = G_TASK (user_data);
    GCancellable *cancellable = g_task_get_cancellable (task);
    keyring_load_closure *closure = g_task_get_task_data (task);
    SeahorseGpgmeKeyring *self = closure->self;
    const char **patterns = closure->patterns;
    g_autoptr(GError) error = NULL;

    if (!seahorse_gpgme_keyring_list_finish (SEAHORSE_GPGME_KEYRING (source),
                                             result, &error)) {
        g_task_return_error (task, g_steal_pointer (&error));
        return;
    }

    /* Public keys */
    seahorse_gpgme_keyring_list_async (self, patterns, 0, FALSE, cancellable,
                                       on_keyring_public_list_complete,
                                       g_steal_pointer (&task));
}

static void
on_keyring_public_list_complete (GObject      *source,
                                 GAsyncResult *result,
                                 void         *user_data)
{
    g_autoptr(GTask) task = G_TASK (user_data);
    g_autoptr(GError) error = NULL;

    if (!seahorse_gpgme_keyring_list_finish (SEAHORSE_GPGME_KEYRING (source),
                                             result, &error)) {
        g_task_return_error (task, g_steal_pointer (&error));
        return;
    }

    g_task_return_boolean (task, TRUE);
}

static void
seahorse_gpgme_keyring_load_full_async (SeahorseGpgmeKeyring *self,
                                        const char          **patterns,
                                        int                   parts,
                                        GCancellable         *cancellable,
                                        GAsyncReadyCallback   callback,
                                        void                 *user_data)
{
    g_autoptr(GTask) task = NULL;
    keyring_load_closure *closure;

    /* Schedule a dummy refresh. This blocks all monitoring for a while */
    cancel_scheduled_refresh (self);
    self->scheduled_refresh = g_timeout_add (500, scheduled_dummy, self);
    g_debug ("scheduled a dummy refresh");

    g_debug ("refreshing keys...");

    task = g_task_new (self, cancellable, callback, user_data);
    closure = g_new0 (keyring_load_closure, 1);
    closure->self = self;
    closure->patterns = patterns;
    g_task_set_task_data (task, closure, g_free);

    /* Secret keys */
    seahorse_gpgme_keyring_list_async (self, patterns, 0, TRUE, cancellable,
                                       on_keyring_secret_list_complete,
                                       g_object_ref (task));

    /* Public keys -- see on_keyring_secret_list_complete() */
}

/**
 * seahorse_gpgme_keyring_lookup:
 * @self: A #SeahorseGpgmeKeyring
 * @keyid: A PGP key id
 *
 * Looks up the key for @keyid in @self and returns it (or %NULL if not found).
 *
 * Returns: (transfer none) (nullable): The requested key, or %NULL
 */
SeahorseGpgmeKey *
seahorse_gpgme_keyring_lookup (SeahorseGpgmeKeyring *self,
                               const char           *keyid)
{
    g_return_val_if_fail (SEAHORSE_IS_GPGME_KEYRING (self), NULL);
    g_return_val_if_fail (keyid != NULL, NULL);

    return g_hash_table_lookup (self->keys, keyid);
}

void
seahorse_gpgme_keyring_remove_key (SeahorseGpgmeKeyring *self,
                                   SeahorseGpgmeKey *key)
{
    const char *keyid;

    g_return_if_fail (SEAHORSE_IS_GPGME_KEYRING (self));
    g_return_if_fail (SEAHORSE_GPGME_IS_KEY (key));

    keyid = seahorse_pgp_key_get_keyid (SEAHORSE_PGP_KEY (key));
    g_return_if_fail (g_hash_table_lookup (self->keys, keyid) == key);

    g_object_ref (key);
    g_hash_table_remove (self->keys, keyid);
    gcr_collection_emit_removed (GCR_COLLECTION (self), G_OBJECT (key));
    g_object_unref (key);

}

static void
seahorse_gpgme_keyring_load_async (SeahorsePlace      *place,
                                   GCancellable       *cancellable,
                                   GAsyncReadyCallback callback,
                                   void               *user_data)
{
    SeahorseGpgmeKeyring *self = SEAHORSE_GPGME_KEYRING (place);
    seahorse_gpgme_keyring_load_full_async (self, NULL, 0, cancellable,
                                            callback, user_data);
}

static gboolean
seahorse_gpgme_keyring_load_finish (SeahorsePlace *place,
                                    GAsyncResult  *result,
                                    GError       **error)
{
    g_return_val_if_fail (g_task_is_valid (result, place), FALSE);

    return g_task_propagate_boolean (G_TASK (result), error);
}

typedef struct {
    SeahorseGpgmeKeyring *keyring;
    gpgme_ctx_t gctx;
    gpgme_data_t data;
    char **patterns;
} keyring_import_closure;

static void
keyring_import_free (void *data)
{
    keyring_import_closure *closure = data;
    if (closure->gctx)
        gpgme_release (closure->gctx);
    gpgme_data_release (closure->data);
    g_object_unref (closure->keyring);
    g_strfreev (closure->patterns);
    g_free (closure);
}

static void
on_keyring_import_loaded (GObject      *source,
                          GAsyncResult *result,
                          void         *user_data)
{
    g_autoptr(GTask) task = G_TASK (user_data);
    keyring_import_closure *closure = g_task_get_task_data (task);
    g_autoptr(GList) keys = NULL;

    for (unsigned int i = 0; closure->patterns[i] != NULL; i++) {
        SeahorseObject *object;

        object = g_hash_table_lookup (closure->keyring->keys, closure->patterns[i]);
        if (object == NULL) {
            g_warning ("imported key but then couldn't find it in keyring: %s",
                       closure->patterns[i]);
            continue;
        }

        keys = g_list_prepend (keys, object);
    }

    seahorse_progress_end (g_task_get_cancellable (task), task);
    g_task_return_pointer (task, g_steal_pointer (&keys), (GDestroyNotify) g_list_free);
}

static gboolean
on_keyring_import_complete (gpgme_error_t gerr, void *user_data)
{
    GTask *task = G_TASK (user_data);
    keyring_import_closure *closure = g_task_get_task_data (task);
    gpgme_import_result_t results;
    int i;
    gpgme_import_status_t import;
    g_autoptr(GError) error = NULL;
    const char *msg;

    if (seahorse_gpgme_propagate_error (gerr, &error)) {
        g_task_return_error (task, g_steal_pointer (&error));
        return FALSE; /* don't call again */
    }

    /* Figure out which keys were imported */
    results = gpgme_op_import_result (closure->gctx);
    if (results == NULL) {
        g_task_return_pointer (task, NULL, NULL);
        return FALSE; /* don't call again */
    }

    /* Dig out all the fingerprints for use as load patterns */
    closure->patterns = g_new0 (char*, results->considered + 1);
    for (i = 0, import = results->imports;
         i < results->considered && import;
         import = import->next) {
        if (GPG_IS_OK (import->result))
            closure->patterns[i++] = g_strdup (import->fpr);
    }

    /* See if we've managed to import any ... */
    if (closure->patterns[0] == NULL) {
        /* ... try and find out why */
        if (results->considered > 0 && results->no_user_id) {
            msg = _("Invalid key data (missing UIDs). This may be due to a computer with a date set in the future or a missing self-signature.");
            g_task_return_new_error (task, SEAHORSE_ERROR, -1, "%s", msg);
        }

        g_task_return_pointer (task, NULL, NULL);
        return FALSE; /* don't call again */
    }

    /* Reload public keys */
    seahorse_gpgme_keyring_load_full_async (closure->keyring,
                                            (const char **) closure->patterns,
                                            LOAD_FULL,
                                            g_task_get_cancellable (task),
                                            on_keyring_import_loaded,
                                            g_object_ref (task));

    return FALSE; /* don't call again */
}

void
seahorse_gpgme_keyring_import_async (SeahorseGpgmeKeyring *self,
                                     GInputStream *input,
                                     GCancellable *cancellable,
                                     GAsyncReadyCallback callback,
                                     gpointer user_data)
{
    g_autoptr(GTask) task = NULL;
    keyring_import_closure *closure;
    gpgme_error_t gerr = 0;
    g_autoptr(GError) error = NULL;
    g_autoptr(GSource) gsource = NULL;

    task = g_task_new (self, cancellable, callback, user_data);
    closure = g_new0 (keyring_import_closure, 1);
    closure->gctx = seahorse_gpgme_keyring_new_context (&gerr);
    closure->data = seahorse_gpgme_data_input (input);
    closure->keyring = g_object_ref (self);
    g_task_set_task_data (task, closure, keyring_import_free);

    if (gerr == 0) {
        seahorse_progress_prep_and_begin (cancellable, task, NULL);
        gsource = seahorse_gpgme_gsource_new (closure->gctx, cancellable);
        g_source_set_callback (gsource, (GSourceFunc)on_keyring_import_complete,
                               g_steal_pointer (&task), g_object_unref);
        gerr = gpgme_op_import_start (closure->gctx, closure->data);
    }

    if (seahorse_gpgme_propagate_error (gerr, &error)) {
        g_task_return_error (task, g_steal_pointer (&error));
        return;
    }

    g_source_attach (gsource, g_main_context_default ());
}

GList *
seahorse_gpgme_keyring_import_finish (SeahorseGpgmeKeyring *self,
                                      GAsyncResult *result,
                                      GError **error)
{
    g_return_val_if_fail (g_task_is_valid (result, self), NULL);

    return g_task_propagate_pointer (G_TASK (result), error);
}

static gboolean
scheduled_refresh (void *user_data)
{
    SeahorseGpgmeKeyring *self = SEAHORSE_GPGME_KEYRING (user_data);

    g_debug ("scheduled refresh event ocurring now");
    cancel_scheduled_refresh (self);
    seahorse_gpgme_keyring_load_async (SEAHORSE_PLACE (self), NULL, NULL, NULL);

    return G_SOURCE_REMOVE;
}

static void
monitor_gpg_homedir (GFileMonitor     *file_monitor,
                     GFile            *file,
                     GFile            *other_file,
                     GFileMonitorEvent event_type,
                     void             *user_data)
{
    SeahorseGpgmeKeyring *self = SEAHORSE_GPGME_KEYRING (user_data);
    g_autofree char *name = NULL;

    /* Ignore other event types */
    if (event_type != G_FILE_MONITOR_EVENT_CHANGED &&
        event_type != G_FILE_MONITOR_EVENT_DELETED &&
        event_type != G_FILE_MONITOR_EVENT_CREATED)
        return;

    /* If it's not the pubring.kbx or *ring.gpg (GPG <2.1), then also ignore */
    name = g_file_get_basename (file);
    if (!g_str_has_suffix (name, ".kbx") && !g_str_has_suffix (name, ".gpg"))
        return;

    /* Schedule a refresh if none planned yet */
    if (self->scheduled_refresh == 0) {
        g_debug ("scheduling refresh event due to file changes");
        self->scheduled_refresh = g_timeout_add (500, scheduled_refresh, self);
    }
}

static void
seahorse_gpgme_keyring_init (SeahorseGpgmeKeyring *self)
{
    const char *gpg_homedir;
    g_autoptr(GFile) file = NULL;
    g_autoptr(GError) err = NULL;

    self->keys = g_hash_table_new_full (seahorse_pgp_keyid_hash,
                                        seahorse_pgp_keyid_equal,
                                        g_free, g_object_unref);

    self->scheduled_refresh = 0;
    self->monitor_handle = NULL;

    /* Get the GPG home directory directly from GPGME */
    gpg_homedir = gpgme_get_dirinfo ("homedir");
    if (!gpg_homedir) {
        g_warning ("Can't monitor GPG home directory: GPGME has no homedir");
        return;
    }

    /* Try to put a file monitor on it */
    file = g_file_new_for_path (gpg_homedir);
    self->monitor_handle = g_file_monitor_directory (file,
                                                     G_FILE_MONITOR_NONE,
                                                     NULL,
                                                     &err);
    if (!self->monitor_handle) {
        g_warning ("Can't monitor GPG home directory: %s: %s",
                   gpg_homedir, err && err->message ? err->message : "");
        return;
    }

    /* Listen for changes */
    g_debug ("Monitoring GPG home directory '%s'", gpg_homedir);
    g_signal_connect (self->monitor_handle, "changed",
                      G_CALLBACK (monitor_gpg_homedir), self);
}

static char *
seahorse_gpgme_keyring_get_label (SeahorsePlace *place)
{
    return g_strdup (_("GnuPG keys"));
}

static void
seahorse_gpgme_keyring_set_label (SeahorsePlace *place, const char *label)
{
}

static char *
seahorse_gpgme_keyring_get_description (SeahorsePlace *place)
{
    return g_strdup (_("GnuPG: default keyring directory"));
}

static GIcon *
seahorse_gpgme_keyring_get_icon (SeahorsePlace *place)
{
    return g_themed_icon_new (GCR_ICON_GNUPG);
}

static SeahorsePlaceCategory
seahorse_gpgme_keyring_get_category (SeahorsePlace *place)
{
    return SEAHORSE_PLACE_CATEGORY_KEYS;
}

static GActionGroup *
seahorse_gpgme_keyring_get_actions (SeahorsePlace *place)
{
    SeahorseGpgmeKeyring *self = SEAHORSE_GPGME_KEYRING (place);
    if (self->actions)
        return g_object_ref (self->actions);
    return NULL;
}

static const char *
seahorse_gpgme_keyring_get_action_prefix (SeahorsePlace* self)
{
    return NULL;
}

static GMenuModel *
seahorse_gpgme_keyring_get_menu_model (SeahorsePlace *place)
{
    return NULL;
}

static gchar *
seahorse_gpgme_keyring_get_uri (SeahorsePlace *place)
{
    return g_strdup ("gnupg://");
}

static gboolean
seahorse_gpgme_keyring_get_show_if_empty (SeahorsePlace *place)
{
    return TRUE;
}

static void
seahorse_gpgme_keyring_get_property (GObject     *obj,
                                     unsigned int prop_id,
                                     GValue      *value,
                                     GParamSpec  *pspec)
{
    SeahorsePlace *place = SEAHORSE_PLACE (obj);

    switch (prop_id) {
    case PROP_LABEL:
        g_value_take_string (value, seahorse_gpgme_keyring_get_label (place));
        break;
    case PROP_DESCRIPTION:
        g_value_take_string (value, seahorse_gpgme_keyring_get_description (place));
        break;
    case PROP_ICON:
        g_value_take_object (value, seahorse_gpgme_keyring_get_icon (place));
        break;
    case PROP_CATEGORY:
        g_value_set_enum (value, seahorse_gpgme_keyring_get_category (place));
        break;
    case PROP_URI:
        g_value_take_string (value, seahorse_gpgme_keyring_get_uri (place));
        break;
    case PROP_ACTIONS:
        g_value_take_object (value, seahorse_gpgme_keyring_get_actions (place));
        break;
    case PROP_ACTION_PREFIX:
        g_value_set_string (value, seahorse_gpgme_keyring_get_action_prefix (place));
        break;
    case PROP_MENU_MODEL:
        g_value_take_object (value, seahorse_gpgme_keyring_get_menu_model (place));
        break;
    case PROP_SHOW_IF_EMPTY:
        g_value_set_boolean (value, TRUE);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
        break;
    }
}

static void
seahorse_gpgme_keyring_set_property (GObject      *obj,
                                     unsigned int  prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
    SeahorsePlace *place = SEAHORSE_PLACE (obj);

    switch (prop_id) {
    case PROP_LABEL:
        seahorse_gpgme_keyring_set_label (place, g_value_get_boxed (value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
        break;
    }
}

static void
seahorse_gpgme_keyring_dispose (GObject *object)
{
    SeahorseGpgmeKeyring *self = SEAHORSE_GPGME_KEYRING (object);

    g_hash_table_remove_all (self->keys);

    cancel_scheduled_refresh (self);
    g_clear_object (&self->monitor_handle);

    for (GList *l = self->orphan_secret; l; l = g_list_next (l))
        g_object_unref (l->data);
    g_clear_pointer (&self->orphan_secret, g_list_free);

    G_OBJECT_CLASS (seahorse_gpgme_keyring_parent_class)->dispose (object);
}

static void
seahorse_gpgme_keyring_finalize (GObject *object)
{
    SeahorseGpgmeKeyring *self = SEAHORSE_GPGME_KEYRING (object);

    g_clear_object (&self->actions);
    g_hash_table_destroy (self->keys);

    /* All monitoring and scheduling should be done */
    g_assert (self->scheduled_refresh == 0);
    g_assert (self->monitor_handle == 0);

    G_OBJECT_CLASS (seahorse_gpgme_keyring_parent_class)->finalize (object);
}

static void
seahorse_gpgme_keyring_class_init (SeahorseGpgmeKeyringClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

    g_debug ("init gpgme version %s", gpgme_check_version (NULL));

#ifdef ENABLE_NLS
    gpgme_set_locale (NULL, LC_CTYPE, setlocale (LC_CTYPE, NULL));
    gpgme_set_locale (NULL, LC_MESSAGES, setlocale (LC_MESSAGES, NULL));
#endif

    gobject_class->get_property = seahorse_gpgme_keyring_get_property;
    gobject_class->set_property = seahorse_gpgme_keyring_set_property;
    gobject_class->dispose = seahorse_gpgme_keyring_dispose;
    gobject_class->finalize = seahorse_gpgme_keyring_finalize;

    g_object_class_override_property (gobject_class, PROP_LABEL, "label");
    g_object_class_override_property (gobject_class, PROP_DESCRIPTION, "description");
    g_object_class_override_property (gobject_class, PROP_URI, "uri");
    g_object_class_override_property (gobject_class, PROP_ICON, "icon");
    g_object_class_override_property (gobject_class, PROP_CATEGORY, "category");
    g_object_class_override_property (gobject_class, PROP_ACTIONS, "actions");
    g_object_class_override_property (gobject_class, PROP_ACTION_PREFIX, "action-prefix");
    g_object_class_override_property (gobject_class, PROP_MENU_MODEL, "menu-model");
    g_object_class_override_property (gobject_class, PROP_SHOW_IF_EMPTY, "show-if-empty");
}

static void
seahorse_gpgme_keyring_place_iface (SeahorsePlaceIface *iface)
{
    iface->load = seahorse_gpgme_keyring_load_async;
    iface->load_finish = seahorse_gpgme_keyring_load_finish;
    iface->get_actions = seahorse_gpgme_keyring_get_actions;
    iface->get_action_prefix = seahorse_gpgme_keyring_get_action_prefix;
    iface->get_menu_model = seahorse_gpgme_keyring_get_menu_model;
    iface->get_description = seahorse_gpgme_keyring_get_description;
    iface->get_icon = seahorse_gpgme_keyring_get_icon;
    iface->get_category = seahorse_gpgme_keyring_get_category;
    iface->get_label = seahorse_gpgme_keyring_get_label;
    iface->set_label = seahorse_gpgme_keyring_set_label;
    iface->get_uri = seahorse_gpgme_keyring_get_uri;
    iface->get_show_if_empty = seahorse_gpgme_keyring_get_show_if_empty;
}

static unsigned int
seahorse_gpgme_keyring_get_length (GcrCollection *collection)
{
    SeahorseGpgmeKeyring *self = SEAHORSE_GPGME_KEYRING (collection);
    return g_hash_table_size (self->keys);
}

static GList *
seahorse_gpgme_keyring_get_objects (GcrCollection *collection)
{
    SeahorseGpgmeKeyring *self = SEAHORSE_GPGME_KEYRING (collection);
    return g_hash_table_get_values (self->keys);
}

static gboolean
seahorse_gpgme_keyring_contains (GcrCollection *collection,
                                 GObject *object)
{
    SeahorseGpgmeKeyring *self = SEAHORSE_GPGME_KEYRING (collection);
    const char *keyid;

    if (!SEAHORSE_GPGME_IS_KEY (object))
        return FALSE;

    keyid = seahorse_pgp_key_get_keyid (SEAHORSE_PGP_KEY (object));
    return g_hash_table_lookup (self->keys, keyid) == object;
}

static void
seahorse_gpgme_keyring_collection_iface (GcrCollectionIface *iface)
{
    iface->get_objects = seahorse_gpgme_keyring_get_objects;
    iface->get_length = seahorse_gpgme_keyring_get_length;
    iface->contains = seahorse_gpgme_keyring_contains;
}

/**
 * seahorse_gpgme_keyring_new:
 *
 * Creates a new PGP key source
 *
 * Returns: (transfer full): The key source.
 */
SeahorseGpgmeKeyring *
seahorse_gpgme_keyring_new (void)
{
    return g_object_new (SEAHORSE_TYPE_GPGME_KEYRING, NULL);
}

gpgme_ctx_t
seahorse_gpgme_keyring_new_context (gpgme_error_t *gerr)
{
    gpgme_protocol_t proto = GPGME_PROTOCOL_OpenPGP;
    gpgme_error_t error = 0;
    gpgme_ctx_t ctx = NULL;

    error = gpgme_engine_check_version (proto);
    if (error == 0)
        error = gpgme_new (&ctx);
    if (error == 0)
        error = gpgme_set_protocol (ctx, proto);

    if (error != 0) {
        g_message ("couldn't initialize gnupg properly: %s",
                   gpgme_strerror (error));
        if (gerr)
            *gerr = error;
        return NULL;
    }

    gpgme_set_passphrase_cb (ctx, passphrase_get, NULL);
    gpgme_set_keylist_mode (ctx, GPGME_KEYLIST_MODE_LOCAL);
    if (gerr)
        *gerr = 0;
    return ctx;
}
