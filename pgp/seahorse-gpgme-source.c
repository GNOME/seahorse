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
 * along with this program; if not, write to the
 * Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include <glib/gi18n.h>
#include <string.h>
#include "seahorse-gpgme-source.h"

#include "seahorse-gpgme-data.h"
#include "seahorse-gpgme.h"
#include "seahorse-gpgme-key-op.h"
#include "seahorse-gpg-options.h"
#include "seahorse-pgp-key.h"

#include "seahorse-progress.h"
#include "seahorse-registry.h"
#include "seahorse-util.h"
#include "seahorse-passphrase.h"

#include <gio/gio.h>

#include <stdlib.h>
#include <libintl.h>
#include <locale.h>

#define DEBUG_FLAG SEAHORSE_DEBUG_OPERATION
#include "seahorse-debug.h"

/* TODO: Verify properly that all keys we deal with are PGP keys */

/* Amount of keys to load in a batch */
#define DEFAULT_LOAD_BATCH 200

enum {
    LOAD_FULL = 0x01,
    LOAD_PHOTOS = 0x02
};

enum {
    PROP_0,
    PROP_SOURCE_TAG,
    PROP_SOURCE_LOCATION
};

static gpgme_error_t
passphrase_get (gconstpointer dummy, const gchar *passphrase_hint, 
                const char* passphrase_info, int flags, int fd)
{
    GtkDialog *dialog;
    gpgme_error_t err;
    gchar **split_uid = NULL;
    gchar *label = NULL;
    gchar *errmsg = NULL;
    const gchar *pass;
    gboolean confirm = FALSE;

    if (passphrase_info && strlen(passphrase_info) < 16) {
        flags |= SEAHORSE_PASS_NEW;
        confirm = TRUE;
    }

    if (passphrase_hint)
        split_uid = g_strsplit (passphrase_hint, " ", 2);

    if (flags & SEAHORSE_PASS_BAD) 
        errmsg = g_strdup_printf (_("Wrong passphrase."));
    
    if (split_uid && split_uid[0] && split_uid[1]) {
        if (flags & SEAHORSE_PASS_NEW) 
            label = g_strdup_printf (_("Enter new passphrase for '%s'"), split_uid[1]);
        else 
            label = g_strdup_printf (_("Enter passphrase for '%s'"), split_uid[1]);
    } else {
        if (flags & SEAHORSE_PASS_NEW) 
            label = g_strdup (_("Enter new passphrase"));
        else 
            label = g_strdup (_("Enter passphrase"));
    }

    g_strfreev (split_uid);

    dialog = seahorse_passphrase_prompt_show (NULL, errmsg ? errmsg : label, 
                                              NULL, NULL, confirm);
    g_free (label);
    g_free (errmsg);
    
    switch (gtk_dialog_run (dialog)) {
    case GTK_RESPONSE_ACCEPT:
        pass = seahorse_passphrase_prompt_get (dialog);
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

/* Initialise a GPGME context for PGP keys */
static gpgme_error_t
init_gpgme (gpgme_ctx_t *ctx)
{
    gpgme_protocol_t proto = GPGME_PROTOCOL_OpenPGP;
    gpgme_error_t err;
 
    err = gpgme_engine_check_version (proto);
    g_return_val_if_fail (GPG_IS_OK (err), err);
   
    err = gpgme_new (ctx);
    g_return_val_if_fail (GPG_IS_OK (err), err);
   
    err = gpgme_set_protocol (*ctx, proto);
    g_return_val_if_fail (GPG_IS_OK (err), err);
    
    gpgme_set_passphrase_cb (*ctx, (gpgme_passphrase_cb_t)passphrase_get, 
                             NULL);
   
    gpgme_set_keylist_mode (*ctx, GPGME_KEYLIST_MODE_LOCAL);
    return err;
}

struct _SeahorseGpgmeSourcePrivate {
	guint scheduled_refresh;                /* Source for refresh timeout */
	GFileMonitor *monitor_handle;           /* For monitoring the .gnupg directory */
	GList *orphan_secret;                   /* Orphan secret keys */
};

static void seahorse_source_iface (SeahorseSourceIface *iface);

G_DEFINE_TYPE_EXTENDED (SeahorseGpgmeSource, seahorse_gpgme_source, G_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (SEAHORSE_TYPE_SOURCE, seahorse_source_iface));

typedef struct {
	SeahorseGpgmeSource *source;
	GCancellable *cancellable;
	gulong cancelled_sig;
	gpgme_ctx_t gctx;
	GHashTable *checks;
	gint parts;
	gint loaded;
} source_list_closure;

static void
source_list_free (gpointer data)
{
	source_list_closure *closure = data;
	gpgme_release (closure->gctx);
	if (closure->checks)
		g_hash_table_destroy (closure->checks);
	g_cancellable_disconnect (closure->cancellable,
	                          closure->cancelled_sig);
	g_clear_object (&closure->cancellable);
	g_clear_object (&closure->source);
	g_free (closure);
}

/* Add a key to the context  */
static SeahorseGpgmeKey*
add_key_to_context (SeahorseGpgmeSource *self,
                    gpgme_key_t key)
{
	SeahorseGpgmeKey *pkey = NULL;
	SeahorseGpgmeKey *prev;
	const gchar *id;
	gpgme_key_t seckey;
	GQuark keyid;
	GList *l;

	g_return_val_if_fail (key->subkeys && key->subkeys->keyid, NULL);

	id = key->subkeys->keyid;
	keyid = seahorse_pgp_key_canonize_id (id);
	g_return_val_if_fail (keyid, NULL);

	g_assert (SEAHORSE_IS_GPGME_SOURCE (self));
	prev = SEAHORSE_GPGME_KEY (seahorse_context_get_object (seahorse_context_instance (),
	                                                        SEAHORSE_SOURCE (self), keyid));

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
		pkey = seahorse_gpgme_key_new (SEAHORSE_SOURCE (self), NULL, key);

		/* Since we don't have a public key yet, save this away */
		self->pv->orphan_secret = g_list_append (self->pv->orphan_secret, pkey);

		/* No key was loaded as far as everyone is concerned */
		return NULL;
	}

	/* Just a new public key */

	/* Check for orphans */
	for (l = self->pv->orphan_secret; l; l = g_list_next (l)) {

		seckey = seahorse_gpgme_key_get_private (l->data);
		g_return_val_if_fail (seckey && seckey->subkeys && seckey->subkeys->keyid, NULL);
		g_assert (seckey);

		/* Look for a matching key */
		if (g_str_equal (id, seckey->subkeys->keyid)) {

			/* Set it up properly */
			pkey = SEAHORSE_GPGME_KEY (l->data);
			g_object_set (pkey, "pubkey", key, NULL);

			/* Remove item from orphan list cleanly */
			self->pv->orphan_secret = g_list_remove_link (self->pv->orphan_secret, l);
			g_list_free (l);
			break;
		}
	}

	if (pkey == NULL)
		pkey = seahorse_gpgme_key_new (SEAHORSE_SOURCE (self), key, NULL);

	/* Add to context */
	seahorse_context_take_object (SCTX_APP (), SEAHORSE_OBJECT (pkey));

	return pkey;
}

/* Remove the given key from the context */
static void
remove_key_from_context (gpointer hash_key,
                         SeahorseObject *dummy,
                         SeahorseGpgmeSource *self)
{
	/* This function gets called as a GHRFunc on the lctx->checks hashtable. */
	GQuark keyid = GPOINTER_TO_UINT (hash_key);
	SeahorseObject *object;

	object = seahorse_context_get_object (seahorse_context_instance (),
	                                      SEAHORSE_SOURCE (self), keyid);
	if (object != NULL)
		seahorse_context_remove_object (seahorse_context_instance (), object);
}

/* Completes one batch of key loading */
static gboolean
on_idle_list_batch_of_keys (gpointer data)
{
	GSimpleAsyncResult *res = G_SIMPLE_ASYNC_RESULT (data);
	source_list_closure *closure = g_simple_async_result_get_op_res_gpointer (res);
	SeahorseGpgmeKey *pkey;
	gpgme_key_t key;
	guint batch;
	GQuark keyid;
	gchar *detail;

	/* We load until done if batch is zero */
	batch = DEFAULT_LOAD_BATCH;

	while (batch-- > 0) {

		if (!GPG_IS_OK (gpgme_op_keylist_next (closure->gctx, &key))) {

			gpgme_op_keylist_end (closure->gctx);

			/* If we were a refresh loader, then we remove the keys we didn't find */
			if (closure->checks)
				g_hash_table_foreach (closure->checks, (GHFunc)remove_key_from_context,
				                      closure->source);

			seahorse_progress_end (closure->cancellable, res);
			g_simple_async_result_complete (res);
			return FALSE; /* Remove event handler */
		}

		g_return_val_if_fail (key->subkeys && key->subkeys->keyid, FALSE);
		keyid = seahorse_pgp_key_canonize_id (key->subkeys->keyid);

		/* Invalid id from GPG ? */
		if (keyid == 0) {
			gpgme_key_unref (key);
			continue;
		}

		/* During a refresh if only new or removed keys */
		if (closure->checks) {

			/* Make note that this key exists in key ring */
			g_hash_table_remove (closure->checks, GUINT_TO_POINTER (keyid));

		}

		pkey = add_key_to_context (closure->source, key);

		/* Load additional info */
		if (pkey && closure->parts & LOAD_PHOTOS)
			seahorse_gpgme_key_op_photos_load (pkey);

		gpgme_key_unref (key);
		closure->loaded++;
	}

	detail = g_strdup_printf (ngettext("Loaded %d key", "Loaded %d keys", closure->loaded), closure->loaded);
	seahorse_progress_update (closure->cancellable, res, detail);
	g_free (detail);

	return TRUE;
}

static void
on_source_list_cancelled (GCancellable *cancellable, gpointer user_data)
{
	GSimpleAsyncResult *res = G_SIMPLE_ASYNC_RESULT (user_data);
	source_list_closure *closure = g_simple_async_result_get_op_res_gpointer (res);

	gpgme_op_keylist_end (closure->gctx);
}

static void
seahorse_gpgme_source_list_async (SeahorseGpgmeSource *self,
                                  const gchar **patterns,
                                  gint parts,
                                  gboolean secret,
                                  GCancellable *cancellable,
                                  GAsyncReadyCallback callback,
                                  gpointer user_data)
{
	source_list_closure *closure;
	GSimpleAsyncResult *res;
	SeahorseObject *object;
	gpgme_error_t gerr;
	GList *keys, *l;

	res = g_simple_async_result_new (G_OBJECT (self), callback, user_data,
	                                 seahorse_gpgme_source_list_async);

	closure = g_new0 (source_list_closure, 1);
	closure->parts = parts;
	closure->gctx = seahorse_gpgme_source_new_context ();
	closure->cancellable = cancellable ? g_object_ref (cancellable) : NULL;
	closure->source = g_object_ref (self);
	g_simple_async_result_set_op_res_gpointer (res, closure, source_list_free);

	if (parts & LOAD_FULL) {
		gpgme_set_keylist_mode (closure->gctx, GPGME_KEYLIST_MODE_SIGS |
		                        gpgme_get_keylist_mode (closure->gctx));
	}

	/* Start the key listing */
	if (patterns)
		gerr = gpgme_op_keylist_ext_start (closure->gctx, patterns, secret, 0);
	else
		gerr = gpgme_op_keylist_start (closure->gctx, NULL, secret);
	g_return_if_fail (GPG_IS_OK (gerr));

	/* Loading all the keys? */
	if (patterns == NULL) {

		closure->checks = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, NULL);
		keys = seahorse_context_get_objects (seahorse_context_instance (),
		                                     SEAHORSE_SOURCE (self));
		for (l = keys; l != NULL; l = g_list_next (l)) {
			object = SEAHORSE_OBJECT (l->data);
			if ((secret && seahorse_object_get_usage (object) == SEAHORSE_USAGE_PRIVATE_KEY) ||
			    (!secret && seahorse_object_get_usage (object) == SEAHORSE_USAGE_PUBLIC_KEY)) {
				g_hash_table_insert (closure->checks,
				                     GUINT_TO_POINTER (seahorse_object_get_id (l->data)),
				                     GUINT_TO_POINTER (TRUE));
			}
		}
		g_list_free (keys);

	}

	seahorse_progress_prep_and_begin (cancellable, res, NULL);
	if (cancellable)
		closure->cancelled_sig = g_cancellable_connect (cancellable,
		                                                G_CALLBACK (on_source_list_cancelled),
		                                                res, NULL);

	g_idle_add_full (G_PRIORITY_DEFAULT, on_idle_list_batch_of_keys,
	                 g_object_ref (res), g_object_unref);

	g_object_unref (res);
}

static gboolean
seahorse_gpgme_source_list_finish (SeahorseGpgmeSource *source,
                                   GAsyncResult *result,
                                   GError **error)
{
	g_return_val_if_fail (g_simple_async_result_is_valid (result, G_OBJECT (source),
	                      seahorse_gpgme_source_list_async), FALSE);

	if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (result), error))
		return FALSE;

	return TRUE;
}

static void
cancel_scheduled_refresh (SeahorseGpgmeSource *self)
{
	if (self->pv->scheduled_refresh != 0) {
		seahorse_debug ("cancelling scheduled refresh event");
		g_source_remove (self->pv->scheduled_refresh);
		self->pv->scheduled_refresh = 0;
	}
}

static gboolean
scheduled_dummy (gpointer user_data)
{
	SeahorseGpgmeSource *self = SEAHORSE_GPGME_SOURCE (user_data);
	seahorse_debug ("dummy refresh event occurring now");
	self->pv->scheduled_refresh = 0;
	return FALSE; /* don't run again */
}

typedef struct {
	gboolean public_done;
	gboolean secret_done;
} source_load_closure;

static void
on_source_secret_list_complete (GObject *source,
                                GAsyncResult *result,
                                gpointer user_data)
{
	GSimpleAsyncResult *res = G_SIMPLE_ASYNC_RESULT (user_data);
	source_load_closure *closure = g_simple_async_result_get_op_res_gpointer (res);
	GError *error = NULL;

	if (!seahorse_gpgme_source_list_finish (SEAHORSE_GPGME_SOURCE (source),
	                                        result, &error))
		g_simple_async_result_take_error (res, error);

	closure->secret_done = TRUE;
	if (closure->public_done)
		g_simple_async_result_complete (res);

	g_object_unref (res);
}

static void
on_source_public_list_complete (GObject *source,
                                GAsyncResult *result,
                                gpointer user_data)
{
	GSimpleAsyncResult *res = G_SIMPLE_ASYNC_RESULT (user_data);
	source_load_closure *closure = g_simple_async_result_get_op_res_gpointer (res);
	GError *error = NULL;

	if (!seahorse_gpgme_source_list_finish (SEAHORSE_GPGME_SOURCE (source),
	                                        result, &error))
		g_simple_async_result_take_error (res, error);

	closure->public_done = TRUE;
	if (closure->secret_done)
		g_simple_async_result_complete (res);

	g_object_unref (res);
}

static void
seahorse_gpgme_source_load_full_async (SeahorseGpgmeSource *source,
                                       const gchar **patterns,
                                       gint parts,
                                       GCancellable *cancellable,
                                       GAsyncReadyCallback callback,
                                       gpointer user_data)
{
	SeahorseGpgmeSource *self = SEAHORSE_GPGME_SOURCE (source);
	GSimpleAsyncResult *res;
	source_load_closure *closure;

	/* Schedule a dummy refresh. This blocks all monitoring for a while */
	cancel_scheduled_refresh (self);
	self->pv->scheduled_refresh = g_timeout_add (500, scheduled_dummy, self);
	seahorse_debug ("scheduled a dummy refresh");

	seahorse_debug ("refreshing keys...");

	res = g_simple_async_result_new (G_OBJECT (source), callback, user_data,
	                                 seahorse_gpgme_source_load_full_async);
	closure = g_new0 (source_load_closure, 1);
	g_simple_async_result_set_op_res_gpointer (res, closure, g_free);

	/* Secret keys */
	seahorse_gpgme_source_list_async (self, patterns, 0, TRUE, cancellable,
	                                  on_source_secret_list_complete,
	                                  g_object_ref (res));

	/* Public keys */
	seahorse_gpgme_source_list_async (self, patterns, 0, FALSE, cancellable,
	                                  on_source_public_list_complete,
	                                  g_object_ref (res));

	g_object_unref (res);
}

static void
seahorse_gpgme_source_load_async (SeahorseSource *source,
                                  GCancellable *cancellable,
                                  GAsyncReadyCallback callback,
                                  gpointer user_data)
{
	seahorse_gpgme_source_load_full_async (SEAHORSE_GPGME_SOURCE (source),
	                                       NULL, 0, cancellable, callback,
	                                       user_data);
}

static gboolean
seahorse_gpgme_source_load_finish (SeahorseSource *source,
                                   GAsyncResult *result,
                                   GError **error)
{
	g_return_val_if_fail (g_simple_async_result_is_valid (result, G_OBJECT (source),
	                      seahorse_gpgme_source_load_full_async), FALSE);

	if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (result), error))
		return FALSE;

	return TRUE;
}

typedef struct {
	GCancellable *cancellable;
	SeahorseGpgmeSource *source;
	gpgme_ctx_t gctx;
	gpgme_data_t data;
	gchar **patterns;
	GList *keys;
} source_import_closure;

static void
source_import_free (gpointer data)
{
	source_import_closure *closure = data;
	g_clear_object (&closure->cancellable);
	gpgme_release (closure->gctx);
	gpgme_data_release (closure->data);
	g_object_unref (closure->source);
	g_strfreev (closure->patterns);
	g_list_free (closure->keys);
	g_free (closure);
}

static void
on_source_import_loaded (GObject *source,
                         GAsyncResult *result,
                         gpointer user_data)
{
	GSimpleAsyncResult *res = G_SIMPLE_ASYNC_RESULT (user_data);
	source_import_closure *closure = g_simple_async_result_get_op_res_gpointer (res);
	SeahorseObject *object;
	GQuark keyid;
	guint i;

	for (i = 0; closure->patterns[i] != NULL; i++) {
		keyid = seahorse_pgp_key_canonize_id (closure->patterns[i]);
		if (!keyid) {
			g_warning ("imported non key with strange keyid: %s",
			           closure->patterns[i]);
			continue;
		}

		object = seahorse_context_get_object (seahorse_context_instance (),
		                                      SEAHORSE_SOURCE (closure->source),
		                                      keyid);
		if (object == NULL) {
			g_warning ("imported key but then couldn't find it in keyring: %s",
			           closure->patterns[i]);
			continue;
		}

		closure->keys = g_list_prepend (closure->keys, object);
	}

	seahorse_progress_end (closure->cancellable, res);
	g_simple_async_result_complete (res);
	g_object_unref (res);
}

static gboolean
on_source_import_complete (gpgme_error_t gerr,
                           gpointer user_data)
{
	GSimpleAsyncResult *res = G_SIMPLE_ASYNC_RESULT (user_data);
	source_import_closure *closure = g_simple_async_result_get_op_res_gpointer (res);
	gpgme_import_result_t results;
	gpgme_import_status_t import;
	GError *error = NULL;
	const gchar *msg;
	guint i;

	if (seahorse_gpgme_propagate_error (gerr, &error)) {
		g_simple_async_result_take_error (res, error);
		g_simple_async_result_complete (res);
		return FALSE; /* don't call again */
	}

	/* Figure out which keys were imported */
	results = gpgme_op_import_result (closure->gctx);
	if (results == NULL) {
		g_simple_async_result_complete (res);
		return FALSE; /* don't call again */
	}

	/* Dig out all the fingerprints for use as load patterns */
	closure->patterns = g_new0 (gchar*, results->considered + 1);
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
			g_simple_async_result_set_error (res, SEAHORSE_ERROR, -1, "%s", msg);
		}

		g_simple_async_result_complete (res);
		return FALSE; /* don't call again */
	}

	/* Reload public keys */
	seahorse_gpgme_source_load_full_async (closure->source, (const gchar **)closure->patterns,
	                                       LOAD_FULL, closure->cancellable,
	                                       on_source_import_loaded, g_object_ref (res));

	return FALSE; /* don't call again */
}

static void
seahorse_gpgme_source_import_async (SeahorseSource *source,
                                    GInputStream *input,
                                    GCancellable *cancellable,
                                    GAsyncReadyCallback callback,
                                    gpointer user_data)
{
	SeahorseGpgmeSource *self = SEAHORSE_GPGME_SOURCE (source);
	GSimpleAsyncResult *res;
	source_import_closure *closure;
	gpgme_error_t gerr;
	GError *error = NULL;
	GSource *gsource;

	res = g_simple_async_result_new (G_OBJECT (source), callback, user_data,
	                                 seahorse_gpgme_source_import_async);
	closure = g_new0 (source_import_closure, 1);
	closure->cancellable = cancellable ? g_object_ref (cancellable) : NULL;
	closure->gctx = seahorse_gpgme_source_new_context ();
	closure->data = seahorse_gpgme_data_input (input);
	closure->source = g_object_ref (self);
	g_simple_async_result_set_op_res_gpointer (res, closure, source_import_free);

	seahorse_progress_prep_and_begin (cancellable, res, NULL);
	gsource = seahorse_gpgme_gsource_new (closure->gctx, cancellable);
	g_source_set_callback (gsource, (GSourceFunc)on_source_import_complete,
	                       g_object_ref (res), g_object_unref);

	gerr = gpgme_op_import_start (closure->gctx, closure->data);

	if (seahorse_gpgme_propagate_error (gerr, &error)) {
		g_simple_async_result_take_error (res, error);
		g_simple_async_result_complete_in_idle (res);

	} else {
		g_source_attach (gsource, g_main_context_default ());
	}

	g_source_unref (gsource);
	g_object_unref (res);
}

static GList *
seahorse_gpgme_source_import_finish (SeahorseSource *source,
                                     GAsyncResult *result,
                                     GError **error)
{
	source_import_closure *closure;
	GList *results;

	g_return_val_if_fail (g_simple_async_result_is_valid (result, G_OBJECT (source),
	                      seahorse_gpgme_source_import_async), NULL);

	if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (result), error))
		return NULL;

	closure = g_simple_async_result_get_op_res_gpointer (G_SIMPLE_ASYNC_RESULT (result));
	results = closure->keys;
	closure->keys = NULL;
	return results;
}

typedef struct {
	GPtrArray *keyids;
	gint at;
	gpgme_data_t data;
	gpgme_ctx_t gctx;
	GOutputStream *output;
	GCancellable *cancellable;
	gulong cancelled_sig;
} source_export_closure;

static void
source_export_free (gpointer data)
{
	source_export_closure *closure = data;
	g_cancellable_disconnect (closure->cancellable, closure->cancelled_sig);
	g_clear_object (&closure->cancellable);
	gpgme_data_release (closure->data);
	gpgme_release (closure->gctx);
	g_ptr_array_free (closure->keyids, TRUE);
	g_object_unref (closure->output);
	g_free (closure);
}

static gboolean
on_source_export_complete (gpgme_error_t gerr,
                           gpointer user_data)
{
	GSimpleAsyncResult *res = G_SIMPLE_ASYNC_RESULT (user_data);
	source_export_closure *closure = g_simple_async_result_get_op_res_gpointer (res);
	GError *error = NULL;

	if (seahorse_gpgme_propagate_error (gerr, &error)) {
		g_simple_async_result_take_error (res, error);
		g_simple_async_result_complete (res);
		return FALSE; /* don't call again */
	}

	if (closure->at >= 0)
		seahorse_progress_end (closure->cancellable,
		                       closure->keyids->pdata[closure->at]);

	g_assert (closure->at < (gint)closure->keyids->len);
	closure->at++;

	if (closure->at == closure->keyids->len) {
		g_simple_async_result_complete (res);
		return FALSE; /* don't run this again */
	}

	/* Do the next key in the list */
	gerr = gpgme_op_export_start (closure->gctx, closure->keyids->pdata[closure->at],
	                              0, closure->data);

	if (seahorse_gpgme_propagate_error (gerr, &error)) {
		g_simple_async_result_take_error (res, error);
		g_simple_async_result_complete (res);
		return FALSE; /* don't run this again */
	}

	seahorse_progress_begin (closure->cancellable,
	                         closure->keyids->pdata[closure->at]);
	return TRUE; /* call this source again */
}

static void
seahorse_gpgme_source_export_async (SeahorseSource *source,
                                    GList *objects,
                                    GOutputStream *output,
                                    GCancellable *cancellable,
                                    GAsyncReadyCallback callback,
                                    gpointer user_data)
{
	GSimpleAsyncResult *res;
	source_export_closure *closure;
	SeahorsePgpKey *key;
	gchar *keyid;
	GSource *gsource;
	GList *l;

	res = g_simple_async_result_new (G_OBJECT (source), callback, user_data,
	                                 seahorse_gpgme_source_export_async);
	closure = g_new0 (source_export_closure, 1);
	closure->cancellable = cancellable ? g_object_ref (cancellable) : NULL;
	closure->gctx = seahorse_gpgme_source_new_context ();
	closure->data = seahorse_gpgme_data_output (output);
	closure->keyids = g_ptr_array_new_with_free_func (g_free);
	closure->output = g_object_ref (output);
	closure->at = -1;
	g_simple_async_result_set_op_res_gpointer (res, closure, source_export_free);

	gpgme_set_armor (closure->gctx, TRUE);
	gpgme_set_textmode (closure->gctx, TRUE);

	for (l = objects; l != NULL; l = g_list_next (l)) {

		/* Ignore PGP Uids */
		if (SEAHORSE_IS_PGP_UID (l->data))
			continue;

		g_return_if_fail (SEAHORSE_IS_PGP_KEY (l->data));
		key = SEAHORSE_PGP_KEY (l->data);
		g_return_if_fail (seahorse_object_get_source (SEAHORSE_OBJECT (key)) == source);

		/* Building list */
		keyid = g_strdup (seahorse_pgp_key_get_keyid (key));
		seahorse_progress_prep (closure->cancellable, keyid, NULL);
		g_ptr_array_add (closure->keyids, keyid);
	}

	gsource = seahorse_gpgme_gsource_new (closure->gctx, cancellable);
	g_source_set_callback (gsource, (GSourceFunc)on_source_export_complete,
	                       g_object_ref (res), g_object_unref);

	/* Get things started */
	if (on_source_export_complete (0, res))
		g_source_attach (gsource, g_main_context_default ());

	g_source_unref (gsource);
	g_object_unref (res);
}

static GOutputStream *
seahorse_gpgme_source_export_finish (SeahorseSource *source,
                                     GAsyncResult *result,
                                     GError **error)
{
	source_export_closure *closure;

	g_return_val_if_fail (g_simple_async_result_is_valid (result, G_OBJECT (source),
	                      seahorse_gpgme_source_export_async), NULL);

	if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (result), error))
		return NULL;

	closure = g_simple_async_result_get_op_res_gpointer (G_SIMPLE_ASYNC_RESULT (result));
	return closure->output;
}

static gboolean
scheduled_refresh (gpointer user_data)
{
	SeahorseGpgmeSource *self = SEAHORSE_GPGME_SOURCE (user_data);

	seahorse_debug ("scheduled refresh event ocurring now");
	cancel_scheduled_refresh (self);
	seahorse_source_load_async (SEAHORSE_SOURCE (self), NULL, NULL, NULL);

	return FALSE; /* don't run again */
}

static void
monitor_gpg_homedir (GFileMonitor *handle, GFile *file, GFile *other_file,
                     GFileMonitorEvent event_type, gpointer user_data)
{
	SeahorseGpgmeSource *self = SEAHORSE_GPGME_SOURCE (user_data);
	gchar *name;

	if (event_type == G_FILE_MONITOR_EVENT_CHANGED ||
	    event_type == G_FILE_MONITOR_EVENT_DELETED ||
	    event_type == G_FILE_MONITOR_EVENT_CREATED) {

		name = g_file_get_basename (file);
		if (g_str_has_suffix (name, SEAHORSE_EXT_GPG)) {
			if (self->pv->scheduled_refresh == 0) {
				seahorse_debug ("scheduling refresh event due to file changes");
				self->pv->scheduled_refresh = g_timeout_add (500, scheduled_refresh, self);
			}
		}
	}
}

static void
seahorse_gpgme_source_init (SeahorseGpgmeSource *self)
{
	gpgme_error_t gerr;
	GError *err = NULL;
	const gchar *gpg_homedir;
	GFile *file;

	gerr = init_gpgme (&self->gctx);
	g_return_if_fail (GPG_IS_OK (gerr));

	/* init private vars */
	self->pv = G_TYPE_INSTANCE_GET_PRIVATE (self, SEAHORSE_TYPE_GPGME_SOURCE, SeahorseGpgmeSourcePrivate);

	self->pv->scheduled_refresh = 0;
	self->pv->monitor_handle = NULL;

	gpg_homedir = seahorse_gpg_homedir ();
	file = g_file_new_for_path (gpg_homedir);
	g_return_if_fail (file != NULL);

	self->pv->monitor_handle = g_file_monitor_directory (file, G_FILE_MONITOR_NONE, NULL, &err);
	g_object_unref (file);

	if (self->pv->monitor_handle) {
		g_signal_connect (self->pv->monitor_handle, "changed",
		                  G_CALLBACK (monitor_gpg_homedir), self);
	} else {
		g_warning ("couldn't monitor the GPG home directory: %s: %s", 
		           gpg_homedir, err && err->message ? err->message : "");
	}
}

static void
seahorse_gpgme_source_dispose (GObject *object)
{
	SeahorseGpgmeSource *self = SEAHORSE_GPGME_SOURCE (object);
	GList *l;

	/*
	 * Note that after this executes the rest of the object should
	 * still work without a segfault. This basically nullifies the
	 * object, but doesn't free it.
	 *
	 * This function should also be able to run multiple times.
	 */

	cancel_scheduled_refresh (self);
	if (self->pv->monitor_handle) {
		g_object_unref (self->pv->monitor_handle);
		self->pv->monitor_handle = NULL;
	}

	for (l = self->pv->orphan_secret; l != NULL; l = g_list_next (l))
		g_object_unref (l->data);
	g_list_free (self->pv->orphan_secret);
	self->pv->orphan_secret = NULL;

	if (self->gctx)
		gpgme_release (self->gctx);

	G_OBJECT_CLASS (seahorse_gpgme_source_parent_class)->dispose (object);
}

static void
seahorse_gpgme_source_finalize (GObject *object)
{
	SeahorseGpgmeSource *self = SEAHORSE_GPGME_SOURCE (object);

	/* All monitoring and scheduling should be done */
	g_assert (self->pv->scheduled_refresh == 0);
	g_assert (self->pv->monitor_handle == 0);

	G_OBJECT_CLASS (seahorse_gpgme_source_parent_class)->finalize (object);
}

static void 
seahorse_gpgme_source_get_property (GObject *object,
                                    guint prop_id,
                                    GValue *value,
                                    GParamSpec *pspec)
{
	switch (prop_id) {
	case PROP_SOURCE_TAG:
		g_value_set_uint (value, SEAHORSE_PGP);
		break;
	case PROP_SOURCE_LOCATION:
		g_value_set_enum (value, SEAHORSE_LOCATION_LOCAL);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
seahorse_gpgme_source_class_init (SeahorseGpgmeSourceClass *klass)
{
	GObjectClass *gobject_class;

	g_message ("init gpgme version %s", gpgme_check_version (NULL));

#ifdef ENABLE_NLS
	gpgme_set_locale (NULL, LC_CTYPE, setlocale (LC_CTYPE, NULL));
	gpgme_set_locale (NULL, LC_MESSAGES, setlocale (LC_MESSAGES, NULL));
#endif

	gobject_class = G_OBJECT_CLASS (klass);
	gobject_class->dispose = seahorse_gpgme_source_dispose;
	gobject_class->finalize = seahorse_gpgme_source_finalize;
	gobject_class->get_property = seahorse_gpgme_source_get_property;

	g_object_class_override_property (gobject_class, PROP_SOURCE_TAG, "source-tag");
	g_object_class_override_property (gobject_class, PROP_SOURCE_LOCATION, "source-location");

	g_type_class_add_private (klass, sizeof (SeahorseGpgmeSourcePrivate));

	seahorse_registry_register_type (NULL, SEAHORSE_TYPE_GPGME_SOURCE, "source", "local", SEAHORSE_PGP_STR, NULL);
	seahorse_registry_register_function (NULL, seahorse_pgp_key_canonize_id, "canonize", SEAHORSE_PGP_STR, NULL);
}

static void
seahorse_source_iface (SeahorseSourceIface *iface)
{
	iface->load_async = seahorse_gpgme_source_load_async;
	iface->load_finish = seahorse_gpgme_source_load_finish;
	iface->import_async = seahorse_gpgme_source_import_async;
	iface->import_finish = seahorse_gpgme_source_import_finish;
	iface->export_async = seahorse_gpgme_source_export_async;
	iface->export_finish = seahorse_gpgme_source_export_finish;
}

/**
 * seahorse_gpgme_source_new
 * 
 * Creates a new PGP key source
 * 
 * Returns: The key source.
 **/
SeahorseGpgmeSource *
seahorse_gpgme_source_new (void)
{
	return g_object_new (SEAHORSE_TYPE_GPGME_SOURCE, NULL);
}

gpgme_ctx_t
seahorse_gpgme_source_new_context (void)
{
	gpgme_ctx_t ctx = NULL;
	g_return_val_if_fail (GPG_IS_OK (init_gpgme (&ctx)), NULL);
	return ctx;
}
