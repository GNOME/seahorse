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


#include "seahorse-ssh-key.h"
#include "seahorse-ssh-operation.h"
#include "seahorse-ssh-source.h"

#include "seahorse-common.h"
#include "seahorse-util.h"

#include <gcr/gcr.h>

#include <glib/gi18n.h>
#include <glib/gstdio.h>

#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#define DEBUG_FLAG SEAHORSE_DEBUG_LOAD
#include "seahorse-debug.h"

enum {
	PROP_0,
	PROP_LABEL,
	PROP_DESCRIPTION,
	PROP_ICON,
	PROP_BASE_DIRECTORY,
	PROP_URI,
	PROP_ACTIONS
};

struct _SeahorseSSHSourcePrivate {
    gchar *ssh_homedir;                     /* Home directory for SSH keys */
    guint scheduled_refresh;                /* Source for refresh timeout */
    GFileMonitor *monitor_handle;           /* For monitoring the .ssh directory */
    GHashTable *keys;
};

static void       seahorse_ssh_source_place_iface       (SeahorsePlaceIface *iface);

static void       seahorse_ssh_source_collection_iface  (GcrCollectionIface *iface);

G_DEFINE_TYPE_EXTENDED (SeahorseSSHSource, seahorse_ssh_source, G_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (GCR_TYPE_COLLECTION, seahorse_ssh_source_collection_iface);
                        G_IMPLEMENT_INTERFACE (SEAHORSE_TYPE_PLACE, seahorse_ssh_source_place_iface)
);

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
remove_key_from_context (gpointer key,
                         SeahorseObject *dummy,
                         SeahorseSSHSource *self)
{
	const gchar *filename = key;
	SeahorseSSHKey *skey;

	skey = g_hash_table_lookup (self->priv->keys, filename);
	if (skey != NULL)
		seahorse_ssh_source_remove_object (self, skey);
}

static gboolean
scheduled_refresh (SeahorseSSHSource *ssrc)
{
    seahorse_debug ("scheduled refresh event ocurring now");
    cancel_scheduled_refresh (ssrc);
    seahorse_place_load (SEAHORSE_PLACE (ssrc), NULL, NULL, NULL);
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

static gchar *
seahorse_ssh_source_get_label (SeahorsePlace *place)
{
	return g_strdup (_("OpenSSH keys"));
}

static gchar *
seahorse_ssh_source_get_description (SeahorsePlace *place)
{
	SeahorseSSHSource *self = SEAHORSE_SSH_SOURCE (place);
	return g_strdup_printf (_("OpenSSH: %s"), self->priv->ssh_homedir);
}

static gchar *
seahorse_ssh_source_get_uri (SeahorsePlace *place)
{
	SeahorseSSHSource *self = SEAHORSE_SSH_SOURCE (place);
	return g_strdup_printf ("openssh://%s", self->priv->ssh_homedir);
}

static GIcon *
seahorse_ssh_source_get_icon (SeahorsePlace *place)
{
	return g_themed_icon_new (GCR_ICON_HOME_DIRECTORY);
}

static GtkActionGroup *
seahorse_ssh_source_get_actions (SeahorsePlace* self)
{
	return NULL;
}

static void 
seahorse_ssh_source_get_property (GObject *obj,
                                  guint prop_id,
                                  GValue *value,
                                  GParamSpec *pspec)
{
	SeahorseSSHSource *self = SEAHORSE_SSH_SOURCE (obj);
	SeahorsePlace *place = SEAHORSE_PLACE (obj);

	switch (prop_id) {
	case PROP_LABEL:
		g_value_take_string (value, seahorse_ssh_source_get_label (place));
		break;
	case PROP_DESCRIPTION:
		g_value_take_string (value, seahorse_ssh_source_get_description (place));
		break;
	case PROP_ICON:
		g_value_take_object (value, seahorse_ssh_source_get_icon (place));
		break;
	case PROP_BASE_DIRECTORY:
		g_value_set_string (value, self->priv->ssh_homedir);
		break;
	case PROP_URI:
		g_value_take_string (value, seahorse_ssh_source_get_uri (place));
		break;
	case PROP_ACTIONS:
		g_value_take_object (value, seahorse_ssh_source_get_actions (place));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
		break;
	}
}

static void
seahorse_ssh_source_dispose (GObject *gobject)
{
	SeahorseSSHSource *ssrc = SEAHORSE_SSH_SOURCE (gobject);
    
	g_assert (ssrc->priv);

	g_hash_table_remove_all (ssrc->priv->keys);
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

    g_hash_table_destroy (ssrc->priv->keys);

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

	ssrc->priv->keys = g_hash_table_new_full (g_str_hash, g_str_equal,
	                                          g_free, g_object_unref);

	ssrc->priv->scheduled_refresh = 0;
	ssrc->priv->monitor_handle = NULL;

	ssrc->priv->ssh_homedir = g_strdup_printf ("%s/.ssh", g_get_home_dir ());

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

    g_object_class_override_property (gobject_class, PROP_LABEL, "label");
    g_object_class_override_property (gobject_class, PROP_DESCRIPTION, "description");
    g_object_class_override_property (gobject_class, PROP_URI, "uri");
    g_object_class_override_property (gobject_class, PROP_ICON, "icon");
    g_object_class_override_property (gobject_class, PROP_ACTIONS, "actions");

    g_object_class_install_property (gobject_class, PROP_BASE_DIRECTORY,
        g_param_spec_string ("base-directory", "Key directory", "Directory where the keys are stored",
                             NULL, G_PARAM_READABLE));
}

static guint
seahorse_ssh_source_get_length (GcrCollection *collection)
{
	SeahorseSSHSource *self = SEAHORSE_SSH_SOURCE (collection);
	return g_hash_table_size (self->priv->keys);
}

static GList *
seahorse_ssh_source_get_objects (GcrCollection *collection)
{
	SeahorseSSHSource *self = SEAHORSE_SSH_SOURCE (collection);
	return g_hash_table_get_values (self->priv->keys);
}

static gboolean
seahorse_ssh_source_contains (GcrCollection *collection,
                              GObject *object)
{
	SeahorseSSHSource *self = SEAHORSE_SSH_SOURCE (collection);
	const gchar *filename;

	if (!SEAHORSE_IS_SSH_KEY (object))
		return FALSE;
	filename = seahorse_ssh_key_get_location (SEAHORSE_SSH_KEY (object));
	return g_hash_table_lookup (self->priv->keys, filename) == object;
}

static void
seahorse_ssh_source_collection_iface (GcrCollectionIface *iface)
{
	iface->get_length = seahorse_ssh_source_get_length;
	iface->get_objects = seahorse_ssh_source_get_objects;
	iface->contains = seahorse_ssh_source_contains;
}

typedef struct {
	SeahorseSSHSource *source;
	GHashTable *loaded;
	GHashTable *checks;
	gchar *pubfile;
	gchar *privfile;
	SeahorseSSHKey *last_key;
} source_load_closure;

static void
source_load_free (gpointer data)
{
	source_load_closure *closure = data;
	g_object_unref (closure->source);
	if (closure->loaded)
		g_hash_table_destroy (closure->loaded);
	if (closure->checks)
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
	SeahorsePlace *place = SEAHORSE_PLACE (self);
	SeahorseSSHKey *skey;
	SeahorseObject *prev;
	const gchar *location;

	if (!seahorse_ssh_key_data_is_valid (keydata)) {
		seahorse_ssh_key_data_free (keydata);
		return NULL;
	}

	location = keydata->privfile ? keydata->privfile : keydata->pubfile;
	g_return_val_if_fail (location, NULL);

	/* Does this key exist in the context? */
	prev = g_hash_table_lookup (self->priv->keys, location);

	/* Mark this key as seen */
	if (closure->checks)
		g_hash_table_remove (closure->checks, location);

	if (closure->loaded) {

		/* See if we've already gotten a key like this in this load batch */
		if (g_hash_table_lookup (closure->loaded, location)) {

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
		g_hash_table_insert (closure->loaded, g_strdup (location),
		                     GUINT_TO_POINTER (TRUE));
	}

	/* If we already have this key then just transfer ownership of keydata */
	if (prev) {
		g_object_set (prev, "key-data", keydata, NULL);
		return SEAHORSE_SSH_KEY (prev);
	}

	/* Create a new key */
	g_assert (keydata);
	skey = seahorse_ssh_key_new (place, keydata);
	g_assert (g_strcmp0 (seahorse_ssh_key_get_location (skey), location) == 0);

	g_hash_table_insert (self->priv->keys, g_strdup (location), skey);
	gcr_collection_emit_added (GCR_COLLECTION (self), G_OBJECT (skey));

	return skey;
}

static GHashTable*
load_present_keys (SeahorseSSHSource *self)
{
	const gchar *filename;
	GHashTable *checks;
	GHashTableIter iter;

	checks = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
	g_hash_table_iter_init (&iter, self->priv->keys);
	while (g_hash_table_iter_next (&iter, (gpointer*)&filename, NULL))
		g_hash_table_insert (checks, g_strdup (filename), "PRESENT");

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
	closure->last_key = ssh_key_from_data (closure->source, closure, data);
	return TRUE;
}

static void
load_key_for_private_file (SeahorseSSHSource *self,
                           source_load_closure *closure,
                           const gchar *privfile)
{
	GError *error = NULL;

	closure->privfile = g_strdup (privfile);
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

static SeahorseSSHKey *
seahorse_ssh_source_load_one_sync (SeahorseSSHSource *self,
                                   const gchar *privfile)
{
	source_load_closure *closure;
	SeahorseSSHKey *key;

	closure = g_new0 (source_load_closure, 1);
	closure->source = g_object_ref (self);

	load_key_for_private_file (self, closure, privfile);

	key = closure->last_key;
	source_load_free (closure);

	return key;
}

static void
seahorse_ssh_source_load_async (SeahorsePlace *place,
                                GCancellable *cancellable,
                                GAsyncReadyCallback callback,
                                gpointer user_data)
{
	SeahorseSSHSource *self = SEAHORSE_SSH_SOURCE (place);
	GSimpleAsyncResult *res;
	source_load_closure *closure;
	GError *error = NULL;
	const gchar *filename;
	gchar *privfile;
	GDir *dir;

	res = g_simple_async_result_new (G_OBJECT (self), callback, user_data,
	                                 seahorse_ssh_source_load_async);
	closure = g_new0 (source_load_closure, 1);
	closure->source = g_object_ref (self);

	/* Since we can find duplicate keys, limit them with this hash */
	closure->loaded = g_hash_table_new_full (g_str_hash, g_str_equal,
	                                         g_free, NULL);

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

		privfile = g_build_filename (self->priv->ssh_homedir, filename, NULL);
		load_key_for_private_file (self, closure, privfile);
		g_free (privfile);
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
seahorse_ssh_source_load_finish (SeahorsePlace *place,
                                 GAsyncResult *result,
                                 GError **error)
{
	g_return_val_if_fail (g_simple_async_result_is_valid (result, G_OBJECT (place),
	                      seahorse_ssh_source_load_async), FALSE);

	if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (result), error))
		return FALSE;

	return TRUE;
}

typedef struct {
	SeahorseSSHSource *source;
	GCancellable *cancellable;
	GtkWindow *transient_for;
	gint imports;
} source_import_closure;

static void
source_import_free (gpointer data)
{
	source_import_closure *closure = data;
	g_object_unref (closure->source);
	g_clear_object (&closure->transient_for);
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
	                                     closure->transient_for, closure->cancellable,
	                                     on_import_public_complete, g_object_ref (res));
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
	                                      closure->transient_for, closure->cancellable,
	                                      on_import_private_complete, g_object_ref (res));

	seahorse_ssh_sec_data_free (data);
	return TRUE;
}

void
seahorse_ssh_source_import_async (SeahorseSSHSource *self,
                                  GInputStream *input,
                                  GtkWindow *transient_for,
                                  GCancellable *cancellable,
                                  GAsyncReadyCallback callback,
                                  gpointer user_data)
{
	source_import_closure *closure;
	gchar *contents;
	GSimpleAsyncResult *res;
	gint count;

	res = g_simple_async_result_new (G_OBJECT (self), callback, user_data,
	                                 seahorse_ssh_source_import_async);
	closure = g_new0 (source_import_closure, 1);
	closure->cancellable = cancellable ? g_object_ref (cancellable) : NULL;
	closure->source = g_object_ref (self);
	closure->transient_for = transient_for ? g_object_ref (transient_for) : NULL;
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

GList *
seahorse_ssh_source_import_finish (SeahorseSSHSource *self,
                                   GAsyncResult *result,
                                   GError **error)
{
	g_return_val_if_fail (g_simple_async_result_is_valid (result, G_OBJECT (self),
	                      seahorse_ssh_source_import_async), NULL);

	if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (result), error))
		return NULL;

	/* TODO: The list of keys imported? */
	return NULL;
}

static void 
seahorse_ssh_source_place_iface (SeahorsePlaceIface *iface)
{
	iface->load = seahorse_ssh_source_load_async;
	iface->load_finish = seahorse_ssh_source_load_finish;
	iface->get_actions = seahorse_ssh_source_get_actions;
	iface->get_description = seahorse_ssh_source_get_description;
	iface->get_icon = seahorse_ssh_source_get_icon;
	iface->get_label = seahorse_ssh_source_get_label;
	iface->get_uri = seahorse_ssh_source_get_uri;
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
seahorse_ssh_source_key_for_filename (SeahorseSSHSource *self,
                                      const gchar *privfile)
{
	SeahorseSSHKey *key;

	g_return_val_if_fail (SEAHORSE_IS_SSH_SOURCE (self), NULL);
	g_return_val_if_fail (privfile, NULL);

	key = g_hash_table_lookup (self->priv->keys, privfile);
	if (key == NULL)
		key = seahorse_ssh_source_load_one_sync (self, privfile);

	return key;
}

gchar*
seahorse_ssh_source_file_for_algorithm (SeahorseSSHSource *ssrc, guint algo)
{
    const gchar *pref;
    gchar *filename, *t;
    gint i = 0;
    
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

void
seahorse_ssh_source_remove_object (SeahorseSSHSource *self,
                                   SeahorseSSHKey *skey)
{
	const gchar *filename;

	g_return_if_fail (SEAHORSE_SSH_SOURCE (self));
	g_return_if_fail (SEAHORSE_SSH_KEY (skey));

	filename = seahorse_ssh_key_get_location (skey);
	g_return_if_fail (filename != NULL);
	g_return_if_fail (g_hash_table_lookup (self->priv->keys, filename) == skey);

	g_object_ref (skey);
	g_hash_table_remove (self->priv->keys, filename);
	gcr_collection_emit_removed (GCR_COLLECTION (self), G_OBJECT (skey));
	g_object_unref (skey);
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
