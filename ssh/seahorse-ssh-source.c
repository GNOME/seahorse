/*
 * Seahorse
 *
 * Copyright (C) 2004-2006 Stefan Walter
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


#include "seahorse-ssh-source.h"

#include "seahorse-ssh-key.h"
#include "seahorse-ssh-operation.h"

#include "seahorse-util.h"

#include "common/seahorse-registry.h"

#include <glib/gstdio.h>

#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <glib/gi18n.h>

#define DEBUG_FLAG SEAHORSE_DEBUG_LOAD
#include "seahorse-debug.h"

enum {
    PROP_0,
    PROP_SOURCE_TAG,
    PROP_SOURCE_LOCATION,
    PROP_BASE_DIRECTORY
};

struct _SeahorseSSHSourcePrivate {
    gchar *ssh_homedir;                     /* Home directory for SSH keys */
    guint scheduled_refresh;                /* Source for refresh timeout */
    GFileMonitor *monitor_handle;           /* For monitoring the .ssh directory */
};

static void seahorse_source_iface (SeahorseSourceIface *iface);

G_DEFINE_TYPE_EXTENDED (SeahorseSSHSource, seahorse_ssh_source, G_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (SEAHORSE_TYPE_SOURCE, seahorse_source_iface));

#define AUTHORIZED_KEYS_FILE    "authorized_keys"
#define OTHER_KEYS_FILE         "other_keys.seahorse"

/* -----------------------------------------------------------------------------
 * INTERNAL
 */

static gboolean
check_data_for_ssh_private (SeahorseSSHSource *ssrc, const gchar *data)
{
    /* Check for our signature */
    if (strstr (data, " PRIVATE KEY-----"))
        return TRUE;
    return FALSE;
}

static gboolean
check_file_for_ssh_private (SeahorseSSHSource *ssrc, const gchar *filename)
{
    gchar buf[128];
    int fd, r;
    
    if(!g_file_test (filename, G_FILE_TEST_IS_REGULAR))
        return FALSE;
    
    fd = open (filename, O_RDONLY, 0);
    if (fd == -1) {
        g_warning ("couldn't open file to check for SSH key: %s: %s", 
                   filename, g_strerror (errno));
        return FALSE;
    }
    
    r = read (fd, buf, sizeof (buf));
    close (fd);
    
    if (r == -1) {
        g_warning ("couldn't read file to check for SSH key: %s: %s", 
                   filename, g_strerror (errno));
        return FALSE;
    }
    
    /* File is too short */
    if (r != sizeof (buf))
        return FALSE;
    
    /* Null terminate */
    buf[sizeof(buf) - 1] = 0;
    
    return check_data_for_ssh_private (ssrc, buf);
}

static void
cancel_scheduled_refresh (SeahorseSSHSource *ssrc)
{
    if (ssrc->priv->scheduled_refresh != 0) {
        seahorse_debug ("cancelling scheduled refresh event");
        g_source_remove (ssrc->priv->scheduled_refresh);
        ssrc->priv->scheduled_refresh = 0;
    }
}

static void
remove_key_from_context (gpointer hkey, SeahorseObject *dummy, SeahorseSSHSource *ssrc)
{
    GQuark keyid = GPOINTER_TO_UINT (hkey);
    SeahorseObject *sobj;
    
    sobj = seahorse_context_get_object (SCTX_APP (), SEAHORSE_SOURCE (ssrc), keyid);
    if (sobj != NULL)
        seahorse_context_remove_object (SCTX_APP (), sobj);
}


static gboolean
scheduled_refresh (SeahorseSSHSource *ssrc)
{
    seahorse_debug ("scheduled refresh event ocurring now");
    cancel_scheduled_refresh (ssrc);
    seahorse_source_load_async (SEAHORSE_SOURCE (ssrc), NULL, NULL, NULL);
    return FALSE; /* don't run again */    
}

static gboolean
scheduled_dummy (SeahorseSSHSource *ssrc)
{
    seahorse_debug ("dummy refresh event occurring now");
    ssrc->priv->scheduled_refresh = 0;
    return FALSE; /* don't run again */    
}

static gboolean
ends_with (const gchar *haystack, const gchar *needle)
{
    gsize hlen = strlen (haystack);
    gsize nlen = strlen (needle);
    if (hlen < nlen)
        return FALSE;
    return strcmp (haystack + (hlen - nlen), needle) == 0;
}

static void
monitor_ssh_homedir (GFileMonitor *handle, GFile *file, GFile *other_file,
                     GFileMonitorEvent event_type, SeahorseSSHSource *ssrc)
{
	gchar *path;
    
	if (ssrc->priv->scheduled_refresh != 0 ||
	    (event_type != G_FILE_MONITOR_EVENT_CHANGED && 
	     event_type != G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT &&
	     event_type != G_FILE_MONITOR_EVENT_DELETED &&
	     event_type != G_FILE_MONITOR_EVENT_CREATED))
		return;
    
	path = g_file_get_path (file);
	if (path == NULL)
		return;

	/* Filter out any noise */
	if (event_type != G_FILE_MONITOR_EVENT_DELETED && 
	    !ends_with (path, AUTHORIZED_KEYS_FILE) &&
	    !ends_with (path, OTHER_KEYS_FILE) && 
		!ends_with (path, ".pub") &&
	    !check_file_for_ssh_private (ssrc, path)) {
		g_free (path);
		return;
	}

	g_free (path);
	seahorse_debug ("scheduling refresh event due to file changes");
	ssrc->priv->scheduled_refresh = g_timeout_add (500, (GSourceFunc)scheduled_refresh, ssrc);
}

static void
merge_keydata (SeahorseSSHKey *prev, SeahorseSSHKeyData *keydata)
{
    if (!prev)
        return;
    
    if (!prev->keydata->authorized && keydata->authorized) {
        prev->keydata->authorized = TRUE;
        
        /* Let the key know something's changed */
        g_object_set (prev, "key-data", prev->keydata, NULL);
    }
        
}

static void 
seahorse_ssh_source_get_property (GObject *object, guint prop_id, GValue *value, 
                                  GParamSpec *pspec)
{
    SeahorseSSHSource *ssrc = SEAHORSE_SSH_SOURCE (object);
    
    switch (prop_id) {
    case PROP_SOURCE_TAG:
        g_value_set_uint (value, SEAHORSE_SSH);
        break;
    case PROP_SOURCE_LOCATION:
        g_value_set_enum (value, SEAHORSE_LOCATION_LOCAL);
        break;
    case PROP_BASE_DIRECTORY:
        g_value_set_string (value, ssrc->priv->ssh_homedir);
        break;
    }
}

static void
seahorse_ssh_source_dispose (GObject *gobject)
{
	SeahorseSSHSource *ssrc = SEAHORSE_SSH_SOURCE (gobject);
    
	g_assert (ssrc->priv);

	cancel_scheduled_refresh (ssrc);    
    
	if (ssrc->priv->monitor_handle) {
		g_object_unref (ssrc->priv->monitor_handle);
		ssrc->priv->monitor_handle = NULL;
	}

	G_OBJECT_CLASS (seahorse_ssh_source_parent_class)->dispose (gobject);
}

static void
seahorse_ssh_source_finalize (GObject *gobject)
{
    SeahorseSSHSource *ssrc = SEAHORSE_SSH_SOURCE (gobject);
    g_assert (ssrc->priv);
    
    /* All monitoring and scheduling should be done */
    g_assert (ssrc->priv->scheduled_refresh == 0);
    g_assert (ssrc->priv->monitor_handle == 0);
    
    g_free (ssrc->priv);
 
    G_OBJECT_CLASS (seahorse_ssh_source_parent_class)->finalize (gobject);
}

static void
seahorse_ssh_source_init (SeahorseSSHSource *ssrc)
{
	GError *err = NULL;
	GFile *file;

	/* init private vars */
	ssrc->priv = g_new0 (SeahorseSSHSourcePrivate, 1);
    
	ssrc->priv->scheduled_refresh = 0;
	ssrc->priv->monitor_handle = NULL;

	ssrc->priv->ssh_homedir = g_strdup_printf ("%s/.ssh/", g_get_home_dir ());
    
	/* Make the .ssh directory if it doesn't exist */
	if (!g_file_test (ssrc->priv->ssh_homedir, G_FILE_TEST_EXISTS)) {
		if (g_mkdir (ssrc->priv->ssh_homedir, 0700) == -1)
			g_warning ("couldn't create .ssh directory: %s", ssrc->priv->ssh_homedir);
		return;
	}
    
	file = g_file_new_for_path (ssrc->priv->ssh_homedir);
	g_return_if_fail (file != NULL);
	
	ssrc->priv->monitor_handle = g_file_monitor_directory (file, G_FILE_MONITOR_NONE, NULL, &err);
	g_object_unref (file);
	
	if (ssrc->priv->monitor_handle)
		g_signal_connect (ssrc->priv->monitor_handle, "changed", 
		                  G_CALLBACK (monitor_ssh_homedir), ssrc);
	else
		g_warning ("couldn't monitor ssh directory: %s: %s", 
		           ssrc->priv->ssh_homedir, err && err->message ? err->message : "");
}

static void
seahorse_ssh_source_class_init (SeahorseSSHSourceClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

    gobject_class->dispose = seahorse_ssh_source_dispose;
    gobject_class->finalize = seahorse_ssh_source_finalize;
    gobject_class->get_property = seahorse_ssh_source_get_property;
    
	g_object_class_override_property (gobject_class, PROP_SOURCE_TAG, "source-tag");
	g_object_class_override_property (gobject_class, PROP_SOURCE_LOCATION, "source-location");

    g_object_class_install_property (gobject_class, PROP_BASE_DIRECTORY,
        g_param_spec_string ("base-directory", "Key directory", "Directory where the keys are stored",
                             NULL, G_PARAM_READABLE));
    
	seahorse_registry_register_type (NULL, SEAHORSE_TYPE_SSH_SOURCE, "source", "local", SEAHORSE_SSH_STR, NULL);
	
	seahorse_registry_register_function (NULL, seahorse_ssh_key_calc_cannonical_id, "canonize", SEAHORSE_SSH_STR, NULL);
}


typedef struct {
	SeahorseSSHSource *source;
	GHashTable *loaded;
	GHashTable *checks;
	gchar *pubfile;
	gchar *privfile;
} source_load_closure;

static void
source_load_free (gpointer data)
{
	source_load_closure *closure = data;
	g_object_unref (closure->source);
	g_hash_table_destroy (closure->loaded);
	g_hash_table_destroy (closure->checks);
	g_free (closure->pubfile);
	g_free (closure->privfile);
	g_free (closure);
}

static SeahorseSSHKey *
ssh_key_from_data (SeahorseSSHSource *self,
                   source_load_closure *closure,
                   SeahorseSSHKeyData *keydata)
{
	SeahorseSource *source = SEAHORSE_SOURCE (self);
	SeahorseSSHKey *skey;
	SeahorseObject *prev;
	GQuark keyid;

	if (!seahorse_ssh_key_data_is_valid (keydata)) {
		seahorse_ssh_key_data_free (keydata);
		return NULL;
	}

	/* Make sure it's valid */
	keyid = seahorse_ssh_key_calc_cannonical_id (keydata->fingerprint);
	g_return_val_if_fail (keyid, NULL);

	/* Does this key exist in the context? */
	prev = seahorse_context_get_object (SCTX_APP (), source, keyid);

	/* Mark this key as seen */
	if (closure->checks)
		g_hash_table_remove (closure->checks, GUINT_TO_POINTER (keyid));

	if (closure->loaded) {

		/* See if we've already gotten a key like this in this load batch */
		if (g_hash_table_lookup (closure->loaded, GUINT_TO_POINTER (keyid))) {

			/*
			 * Sometimes later keys loaded have more information (for
			 * example keys loaded from authorized_keys), so propogate
			 * that up to the previously loaded key
			 */
			merge_keydata (SEAHORSE_SSH_KEY (prev), keydata);
			seahorse_ssh_key_data_free (keydata);
			return NULL;
		}

		/* Mark this key as loaded */
		g_hash_table_insert (closure->loaded, GUINT_TO_POINTER (keyid),
		                     GUINT_TO_POINTER (TRUE));
	}

	/* If we already have this key then just transfer ownership of keydata */
	if (prev) {
		g_object_set (prev, "key-data", keydata, NULL);
		return SEAHORSE_SSH_KEY (prev);
	}

	/* Create a new key */
	g_assert (keydata);
	skey = seahorse_ssh_key_new (source, keydata);

	seahorse_context_take_object (seahorse_context_instance (),
	                              SEAHORSE_OBJECT (skey));
	return skey;
}

static GHashTable*
load_present_keys (SeahorseSSHSource *self)
{
	GList *keys, *l;
	GHashTable *checks;

	checks = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, NULL);
	keys = seahorse_context_get_objects (seahorse_context_instance (),
	                                     SEAHORSE_SOURCE (self));
	for (l = keys; l != NULL; l = g_list_next (l))
		g_hash_table_insert (checks,
		                     GUINT_TO_POINTER (seahorse_object_get_id (l->data)),
		                     GUINT_TO_POINTER (TRUE));
	g_list_free (keys);
	return checks;
}

static gboolean
on_load_found_authorized_key (SeahorseSSHKeyData *data,
                              gpointer user_data)
{
	source_load_closure *closure = user_data;

	data->pubfile = g_strdup (closure->pubfile);
	data->partial = TRUE;
	data->authorized = TRUE;

	/* Check and register thet key with the context, frees keydata */
	ssh_key_from_data (closure->source, closure, data);
	return TRUE;
}

static gboolean
on_load_found_other_key (SeahorseSSHKeyData *data,
                         gpointer user_data)
{
	source_load_closure *closure = user_data;

	data->pubfile = g_strdup (closure->pubfile);
	data->partial = TRUE;
	data->authorized = FALSE;

	/* Check and register thet key with the context, frees keydata */
	ssh_key_from_data (closure->source, closure, data);
	return TRUE;
}

static gboolean
on_load_found_public_key (SeahorseSSHKeyData *data,
                          gpointer user_data)
{
	source_load_closure *closure = user_data;

	data->pubfile = g_strdup (closure->pubfile);
	data->privfile = g_strdup (closure->privfile);
	data->partial = FALSE;

	/* Check and register thet key with the context, frees keydata */
	ssh_key_from_data (closure->source, closure, data);
	return TRUE;
}

static void
seahorse_ssh_source_load_async (SeahorseSource *source,
                                GCancellable *cancellable,
                                GAsyncReadyCallback callback,
                                gpointer user_data)
{
	SeahorseSSHSource *self = SEAHORSE_SSH_SOURCE (source);
	GSimpleAsyncResult *res;
	source_load_closure *closure;
	GError *error = NULL;
	const gchar *filename;
	GDir *dir;

	res = g_simple_async_result_new (G_OBJECT (source), callback, user_data,
	                                 seahorse_ssh_source_load_async);
	closure = g_new0 (source_load_closure, 1);
	closure->source = g_object_ref (self);

	/* Since we can find duplicate keys, limit them with this hash */
	closure->loaded = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, NULL);

	/* Keys that currently exist, so we can remove any that disappeared */
	closure->checks = load_present_keys (self);

	g_simple_async_result_set_op_res_gpointer (res, closure, source_load_free);

	/* Schedule a dummy refresh. This blocks all monitoring for a while */
	cancel_scheduled_refresh (self);
	self->priv->scheduled_refresh = g_timeout_add (500, (GSourceFunc)scheduled_dummy, self);
	seahorse_debug ("scheduled a dummy refresh");

	/* List the .ssh directory for private keys */
	dir = g_dir_open (self->priv->ssh_homedir, 0, &error);
	if (dir == NULL) {
		g_simple_async_result_take_error (res, error);
		g_simple_async_result_complete_in_idle (res);
		g_object_unref (res);
		return;
	}

	/* For each private key file */
	for(;;) {
		filename = g_dir_read_name (dir);
		if (filename == NULL)
			break;

		closure->privfile = g_build_filename (self->priv->ssh_homedir, filename, NULL);
		closure->pubfile = g_strconcat (closure->privfile, ".pub", NULL);

		/* possibly an SSH key? */
		if (g_file_test (closure->privfile, G_FILE_TEST_EXISTS) &&
		    g_file_test (closure->pubfile, G_FILE_TEST_EXISTS) &&
		    check_file_for_ssh_private (self, closure->privfile)) {

			seahorse_ssh_key_data_parse_file (closure->pubfile, on_load_found_public_key,
			                                  NULL, closure, &error);
			if (error != NULL) {
				g_warning ("couldn't read SSH file: %s (%s)",
				           closure->pubfile, error->message);
				g_clear_error (&error);
			}
		}

		g_free (closure->privfile);
		g_free (closure->pubfile);
		closure->privfile = closure->pubfile = NULL;
	}

	g_dir_close (dir);

	/* Now load the authorized file */
	closure->privfile = NULL;
	closure->pubfile = seahorse_ssh_source_file_for_public (self, TRUE);

	if (g_file_test (closure->pubfile, G_FILE_TEST_EXISTS)) {
		seahorse_ssh_key_data_parse_file (closure->pubfile, on_load_found_authorized_key,
		                                  NULL, closure, &error);
		if (error != NULL) {
			g_warning ("couldn't read SSH file: %s (%s)",
			           closure->pubfile, error->message);
			g_clear_error (&error);
		}
	}

	g_free (closure->pubfile);
	closure->pubfile = NULL;

	/* Load the other keys file */
	closure->privfile = NULL;
	closure->pubfile = seahorse_ssh_source_file_for_public (self, FALSE);
	if (g_file_test (closure->pubfile, G_FILE_TEST_EXISTS)) {
		seahorse_ssh_key_data_parse_file (closure->pubfile, on_load_found_other_key,
		                                  NULL, closure, &error);
		if (error != NULL) {
			g_warning ("couldn't read SSH file: %s (%s)",
			           closure->pubfile, error->message);
			g_clear_error (&error);
		}
	}

	g_free (closure->pubfile);
	closure->pubfile = NULL;

	/* Clean up and done */
	g_hash_table_foreach (closure->checks, (GHFunc)remove_key_from_context, self);

	g_simple_async_result_complete_in_idle (res);
	g_object_unref (res);
}

static gboolean
seahorse_ssh_source_load_finish (SeahorseSource *source,
                                 GAsyncResult *result,
                                 GError **error)
{
	g_return_val_if_fail (g_simple_async_result_is_valid (result, G_OBJECT (source),
	                      seahorse_ssh_source_load_async), FALSE);

	if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (result), error))
		return FALSE;

	return TRUE;
}

typedef struct {
	SeahorseSSHSource *source;
	GCancellable *cancellable;
	gint imports;
} source_import_closure;

static void
source_import_free (gpointer data)
{
	source_import_closure *closure = data;
	g_object_unref (closure->source);
	g_clear_object (&closure->cancellable);
	g_free (closure);
}

static void
on_import_public_complete (GObject *source,
                           GAsyncResult *result,
                           gpointer user_data)
{
	GSimpleAsyncResult *res = G_SIMPLE_ASYNC_RESULT (user_data);
	source_import_closure *closure = g_simple_async_result_get_op_res_gpointer (res);
	GError *error = NULL;
	gchar *path;

	g_assert (closure->imports > 0);
	closure->imports--;

	path = seahorse_ssh_op_import_public_finish (closure->source, result, &error);
	if (error != NULL)
		g_simple_async_result_take_error (res, error);
	g_free (path);

	if (closure->imports == 0)
		g_simple_async_result_complete (res);

	g_object_unref (res);
}

static gboolean
on_import_found_public_key (SeahorseSSHKeyData *data,
                      gpointer user_data)
{
	GSimpleAsyncResult *res = G_SIMPLE_ASYNC_RESULT (user_data);
	source_import_closure *closure = g_simple_async_result_get_op_res_gpointer (res);
	gchar *fullpath;

	fullpath = seahorse_ssh_source_file_for_public (closure->source, FALSE);
	seahorse_ssh_op_import_public_async (closure->source, data, fullpath,
	                                     closure->cancellable, on_import_public_complete,
	                                     g_object_ref (res));
	closure->imports++;
	g_free (fullpath);
	seahorse_ssh_key_data_free (data);

	return TRUE;
}

static void
on_import_private_complete (GObject *source,
                            GAsyncResult *result,
                            gpointer user_data)
{
	GSimpleAsyncResult *res = G_SIMPLE_ASYNC_RESULT (user_data);
	source_import_closure *closure = g_simple_async_result_get_op_res_gpointer (res);
	GError *error = NULL;
	gchar *path;

	g_assert (closure->imports > 0);
	closure->imports--;

	path = seahorse_ssh_op_import_private_finish (closure->source, result, &error);
	if (error != NULL)
		g_simple_async_result_take_error (res, error);
	g_free (path);

	if (closure->imports == 0)
		g_simple_async_result_complete (res);

	g_object_unref (res);
}

static gboolean
on_import_found_private_key (SeahorseSSHSecData *data,
                             gpointer user_data)
{
	GSimpleAsyncResult *res = G_SIMPLE_ASYNC_RESULT (user_data);
	source_import_closure *closure = g_simple_async_result_get_op_res_gpointer (res);

	seahorse_ssh_op_import_private_async (closure->source, data, NULL,
	                                      closure->cancellable, on_import_private_complete,
	                                      g_object_ref (res));

	seahorse_ssh_sec_data_free (data);
	return TRUE;
}

static void
seahorse_ssh_source_import_async (SeahorseSource *source,
                                  GInputStream *input,
                                  GCancellable *cancellable,
                                  GAsyncReadyCallback callback,
                                  gpointer user_data)
{
	SeahorseSSHSource *self = SEAHORSE_SSH_SOURCE (source);
	source_import_closure *closure;
	gchar *contents;
	GSimpleAsyncResult *res;
	guint count;

	res = g_simple_async_result_new (G_OBJECT (source), callback, user_data,
	                                 seahorse_ssh_source_import_async);
	closure = g_new0 (source_import_closure, 1);
	closure->cancellable = cancellable ? g_object_ref (cancellable) : NULL;
	closure->source = g_object_ref (self);
	g_simple_async_result_set_op_res_gpointer (res, closure, source_import_free);

	contents = (gchar*)seahorse_util_read_to_memory (input, NULL);
	count = seahorse_ssh_key_data_parse (contents, on_import_found_public_key,
	                                     on_import_found_private_key, res);
	g_assert (count == closure->imports);
	g_free (contents);

	if (count == 0)
		g_simple_async_result_complete_in_idle (res);
	g_object_unref (res);
}

static GList *
seahorse_ssh_source_import_finish (SeahorseSource *source,
                                   GAsyncResult *result,
                                   GError **error)
{
	g_return_val_if_fail (g_simple_async_result_is_valid (result, G_OBJECT (source),
	                      seahorse_ssh_source_import_async), NULL);

	if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (result), error))
		return NULL;

	/* TODO: The list of keys imported? */
	return NULL;
}

static void
seahorse_ssh_source_export_async (SeahorseSource *source,
                                  GList *keys,
                                  GOutputStream *output,
                                  GCancellable *cancellable,
                                  GAsyncReadyCallback callback,
                                  gpointer user_data)
{
	GSimpleAsyncResult *res;
	SeahorseSSHKeyData *keydata;
	gchar *results = NULL;
	gchar *raw = NULL;
	GError *error = NULL;
	SeahorseObject *object;
	GList *l;
	gsize written;

	res = g_simple_async_result_new (G_OBJECT (source), callback, user_data,
	                                 seahorse_ssh_source_export_async);

	for (l = keys; l; l = g_list_next (l)) {
		object = SEAHORSE_OBJECT (l->data);
		g_assert (SEAHORSE_IS_SSH_KEY (object));

		results = NULL;
		raw = NULL;

		keydata = NULL;
		g_object_get (object, "key-data", &keydata, NULL);
		g_assert (keydata);

		/* We should already have the data loaded */
		if (keydata->pubfile) {
			g_assert (keydata->rawdata);
			results = g_strdup_printf ("%s", keydata->rawdata);

		} else if (!keydata->pubfile) {

			/*
			 * TODO: We should be able to get at this data by using ssh-keygen
			 * to make a public key from the private
			 */
			g_warning ("private key without public, not exporting: %s", keydata->privfile);
		}

		/* Write the data out */
		if (results) {
			if (g_output_stream_write_all (output, results, strlen (results),
			                               &written, NULL, &error))
				g_output_stream_flush (output, NULL, &error);
			g_free (results);
		}

		g_free (raw);

		if (error != NULL) {
			g_simple_async_result_take_error (res, error);
			break;
		}
	}

	g_simple_async_result_set_op_res_gpointer (res, g_object_ref (output), g_object_unref);
	g_simple_async_result_complete_in_idle (res);
	g_object_unref (res);
}

static GOutputStream *
seahorse_ssh_source_export_finish (SeahorseSource *source,
                                   GAsyncResult *result,
                                   GError **error)
{
	g_return_val_if_fail (g_simple_async_result_is_valid (result, G_OBJECT (source),
	                      seahorse_ssh_source_export_async), NULL);

	if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (result), error))
		return NULL;

	return g_simple_async_result_get_op_res_gpointer (G_SIMPLE_ASYNC_RESULT (result));
}

static void 
seahorse_source_iface (SeahorseSourceIface *iface)
{
	iface->load_async = seahorse_ssh_source_load_async;
	iface->load_finish = seahorse_ssh_source_load_finish;
	iface->import_async = seahorse_ssh_source_import_async;
	iface->import_finish = seahorse_ssh_source_import_finish;
	iface->export_async = seahorse_ssh_source_export_async;
	iface->export_finish = seahorse_ssh_source_export_finish;
}

/* -----------------------------------------------------------------------------
 * PUBLIC 
 */

SeahorseSSHSource*
seahorse_ssh_source_new (void)
{
   return g_object_new (SEAHORSE_TYPE_SSH_SOURCE, NULL);
}

SeahorseSSHKey*
seahorse_ssh_source_key_for_filename (SeahorseSSHSource *ssrc,
                                      const gchar *privfile)
{
	SeahorseSSHKeyData *data;
	GList *keys, *l;

	g_return_val_if_fail (privfile, NULL);
	g_return_val_if_fail (SEAHORSE_IS_SSH_SOURCE (ssrc), NULL);

	/* Try to find it first */
	keys = seahorse_context_get_objects (SCTX_APP (), SEAHORSE_SOURCE (ssrc));
	for (l = keys; l; l = g_list_next (l)) {

		g_object_get (l->data, "key-data", &data, NULL);
		g_return_val_if_fail (data, NULL);

		/* If it's already loaded then just leave it at that */
		if (data->privfile && strcmp (privfile, data->privfile) == 0)
			return SEAHORSE_SSH_KEY (l->data);
	}

	return NULL;
}

gchar*
seahorse_ssh_source_file_for_algorithm (SeahorseSSHSource *ssrc, guint algo)
{
    const gchar *pref;
    gchar *filename, *t;
    guint i = 0;
    
    switch (algo) {
    case SSH_ALGO_DSA:
        pref = "id_dsa";
        break;
    case SSH_ALGO_RSA:
        pref = "id_rsa";
        break;
    case SSH_ALGO_UNK:
        pref = "id_unk";
        break;
    default:
        g_return_val_if_reached (NULL);
        break;
    }
    
    for (i = 0; i < ~0; i++) {
        t = (i == 0) ? g_strdup (pref) : g_strdup_printf ("%s.%d", pref, i);
        filename = g_build_filename (ssrc->priv->ssh_homedir, t, NULL);
        g_free (t);
        
        if (!g_file_test (filename, G_FILE_TEST_EXISTS))
            break;
        
        g_free (filename);
        filename = NULL;
    }
    
    return filename;
}

gchar*
seahorse_ssh_source_file_for_public (SeahorseSSHSource *ssrc, gboolean authorized)
{
    return g_build_filename (ssrc->priv->ssh_homedir, 
            authorized ? AUTHORIZED_KEYS_FILE : OTHER_KEYS_FILE, NULL);
}

guchar*
seahorse_ssh_source_export_private (SeahorseSSHSource *ssrc, SeahorseSSHKey *skey,
                                    gsize *n_results, GError **err)
{
	SeahorseSSHKeyData *keydata;
	gchar *results;
	
	g_return_val_if_fail (SEAHORSE_IS_SSH_SOURCE (ssrc), NULL);
	g_return_val_if_fail (SEAHORSE_IS_SSH_KEY (skey), NULL);
	g_return_val_if_fail (n_results, NULL);
	g_return_val_if_fail (!err || !*err, NULL);
	
	g_object_get (skey, "key-data", &keydata, NULL);
	g_return_val_if_fail (keydata, NULL);

	if (!keydata->privfile) {
		g_set_error (err, SEAHORSE_ERROR, 0, "%s", _("No private key file is available for this key."));
		return NULL;
	}

        /* And then the data itself */
        if (!g_file_get_contents (keydata->privfile, &results, n_results, err))
        	return NULL;
        
        return (guchar*)results;
}
