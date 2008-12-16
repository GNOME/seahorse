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

#include "seahorse-operation.h"
#include "seahorse-util.h"

#include "common/seahorse-registry.h"

#include <glib/gstdio.h>

#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <glib/gi18n.h>

/* Override DEBUG switches here */
#define DEBUG_REFRESH_ENABLE 0
/* #define DEBUG_OPERATION_ENABLE 1 */

#ifndef DEBUG_REFRESH_ENABLE
#if _DEBUG
#define DEBUG_REFRESH_ENABLE 1
#else
#define DEBUG_REFRESH_ENABLE 0
#endif
#endif

#if DEBUG_REFRESH_ENABLE
#define DEBUG_REFRESH(x)    g_printerr x
#else
#define DEBUG_REFRESH(x)
#endif

enum {
    PROP_0,
    PROP_KEY_TYPE,
    PROP_KEY_DESC,
    PROP_LOCATION,
    PROP_BASE_DIRECTORY
};

struct _SeahorseSSHSourcePrivate {
    gchar *ssh_homedir;                     /* Home directory for SSH keys */
    guint scheduled_refresh;                /* Source for refresh timeout */
    GFileMonitor *monitor_handle;           /* For monitoring the .ssh directory */
};

typedef struct _LoadContext {
    SeahorseSSHSource *ssrc;
    GHashTable *loaded;
    GHashTable *checks;
    gchar *pubfile;
    gchar *privfile;
} LoadContext;

typedef struct _ImportContext {
    SeahorseSSHSource *ssrc;
    SeahorseMultiOperation *mop;
} ImportContext;

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
        DEBUG_REFRESH ("cancelling scheduled refresh event\n");
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
    DEBUG_REFRESH ("scheduled refresh event ocurring now\n");
    cancel_scheduled_refresh (ssrc);
    seahorse_source_load_async (SEAHORSE_SOURCE (ssrc));
    return FALSE; /* don't run again */    
}

static gboolean
scheduled_dummy (SeahorseSSHSource *ssrc)
{
    DEBUG_REFRESH ("dummy refresh event occurring now\n");
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
	DEBUG_REFRESH ("scheduling refresh event due to file changes\n");
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

static SeahorseSSHKey*
ssh_key_from_data (SeahorseSSHSource *ssrc, LoadContext *ctx, 
                   SeahorseSSHKeyData *keydata)
{   
    SeahorseSource *sksrc = SEAHORSE_SOURCE (ssrc);
    SeahorseSSHKey *skey;
    SeahorseObject *prev;
    GQuark keyid;

    g_assert (ctx);

    if (!seahorse_ssh_key_data_is_valid (keydata)) {
        seahorse_ssh_key_data_free (keydata);
        return NULL;
    }

    /* Make sure it's valid */
    keyid = seahorse_ssh_key_calc_cannonical_id (keydata->fingerprint);
    g_return_val_if_fail (keyid, NULL);

    /* Does this key exist in the context? */
    prev = seahorse_context_get_object (SCTX_APP (), sksrc, keyid);
    
    /* Mark this key as seen */
    if (ctx->checks)
        g_hash_table_remove (ctx->checks, GUINT_TO_POINTER (keyid));

    if (ctx->loaded) {

        /* See if we've already gotten a key like this in this load batch */
        if (g_hash_table_lookup (ctx->loaded, GUINT_TO_POINTER (keyid))) {
            
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
        g_hash_table_insert (ctx->loaded, GUINT_TO_POINTER (keyid), 
                                          GUINT_TO_POINTER (TRUE));
    }
     
    /* If we already have this key then just transfer ownership of keydata */
    if (prev) {
        g_object_set (prev, "key-data", keydata, NULL);
        return SEAHORSE_SSH_KEY (prev);
    }

    /* Create a new key */        
    g_assert (keydata);
    skey = seahorse_ssh_key_new (sksrc, keydata);

    seahorse_context_take_object (SCTX_APP (), SEAHORSE_OBJECT (skey));
    return skey;
}

static gboolean
parsed_authorized_key (SeahorseSSHKeyData *data, gpointer arg)
{
    LoadContext *ctx = (LoadContext*)arg;
    
    g_assert (ctx);
    g_assert (SEAHORSE_IS_SSH_SOURCE (ctx->ssrc));

    data->pubfile = g_strdup (ctx->pubfile);
    data->partial = TRUE;
    data->authorized = TRUE;
    
    /* Check and register thet key with the context, frees keydata */
    ssh_key_from_data (ctx->ssrc, ctx, data);
    
    return TRUE;
}

static gboolean
parsed_other_key (SeahorseSSHKeyData *data, gpointer arg)
{
    LoadContext *ctx = (LoadContext*)arg;
    
    g_assert (ctx);
    g_assert (SEAHORSE_IS_SSH_SOURCE (ctx->ssrc));

    data->pubfile = g_strdup (ctx->pubfile);
    data->partial = TRUE;
    data->authorized = FALSE;
    
    /* Check and register thet key with the context, frees keydata */
    ssh_key_from_data (ctx->ssrc, ctx, data);
    
    return TRUE;
}

static gboolean
parsed_public_key (SeahorseSSHKeyData *data, gpointer arg)
{
    LoadContext *ctx = (LoadContext*)arg;
    
    g_assert (ctx);
    g_assert (SEAHORSE_IS_SSH_SOURCE (ctx->ssrc));

    data->pubfile = g_strdup (ctx->pubfile);
    data->privfile = g_strdup (ctx->privfile);
    data->partial = FALSE;
    
    /* Check and register thet key with the context, frees keydata */
    ssh_key_from_data (ctx->ssrc, ctx, data);
    
    return TRUE;
}

static GHashTable*
load_present_keys (SeahorseSource *sksrc)
{
    GList *keys, *l;
    GHashTable *checks;

    checks = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, NULL);
    keys = seahorse_context_get_objects (SCTX_APP (), sksrc);
    for (l = keys; l; l = g_list_next (l))
        g_hash_table_insert (checks, GUINT_TO_POINTER (seahorse_object_get_id (l->data)), 
                                     GUINT_TO_POINTER (TRUE));
    g_list_free (keys);
    return checks;
}

static gboolean
import_public_key (SeahorseSSHKeyData *data, gpointer arg)
{
    ImportContext *ctx = (ImportContext*)arg;
    gchar *fullpath;
    
    g_assert (data->rawdata);
    g_assert (SEAHORSE_IS_MULTI_OPERATION (ctx->mop));
    g_assert (SEAHORSE_IS_SSH_SOURCE (ctx->ssrc));

    fullpath = seahorse_ssh_source_file_for_public (ctx->ssrc, FALSE);
    seahorse_multi_operation_take (ctx->mop, 
                seahorse_ssh_operation_import_public (ctx->ssrc, data, fullpath));
    g_free (fullpath);
    seahorse_ssh_key_data_free (data);
    return TRUE;
}

static gboolean
import_private_key (SeahorseSSHSecData *data, gpointer arg)
{
    ImportContext *ctx = (ImportContext*)arg;
    
    g_assert (SEAHORSE_IS_MULTI_OPERATION (ctx->mop));
    g_assert (SEAHORSE_IS_SSH_SOURCE (ctx->ssrc));
    
    seahorse_multi_operation_take (ctx->mop, 
                seahorse_ssh_operation_import_private (ctx->ssrc, data, NULL));
    
    seahorse_ssh_sec_data_free (data);
    return TRUE;
}

/* -----------------------------------------------------------------------------
 * OBJECT
 */

static SeahorseOperation*
seahorse_ssh_source_load (SeahorseSource *sksrc)
{
    SeahorseSSHSource *ssrc;
    GError *err = NULL;
    LoadContext ctx;
    const gchar *filename;
    GDir *dir;
    
    g_assert (SEAHORSE_IS_SSH_SOURCE (sksrc));
    ssrc = SEAHORSE_SSH_SOURCE (sksrc);
 
    /* Schedule a dummy refresh. This blocks all monitoring for a while */
    cancel_scheduled_refresh (ssrc);
    ssrc->priv->scheduled_refresh = g_timeout_add (500, (GSourceFunc)scheduled_dummy, ssrc);
    DEBUG_REFRESH ("scheduled a dummy refresh\n");

    /* List the .ssh directory for private keys */
    dir = g_dir_open (ssrc->priv->ssh_homedir, 0, &err);
    if (!dir)
        return seahorse_operation_new_complete (err);

    memset (&ctx, 0, sizeof (ctx));
    ctx.ssrc = ssrc;
    
    /* Since we can find duplicate keys, limit them with this hash */
    ctx.loaded = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, NULL);
    
    /* Keys that currently exist, so we can remove any that disappeared */
    ctx.checks = load_present_keys (sksrc);

    /* For each private key file */
    for(;;) {
        filename = g_dir_read_name (dir);
        if (filename == NULL)
            break;

        ctx.privfile = g_build_filename (ssrc->priv->ssh_homedir, filename, NULL);
        ctx.pubfile = g_strconcat (ctx.privfile, ".pub", NULL);
        
        /* possibly an SSH key? */
        if (g_file_test (ctx.privfile, G_FILE_TEST_EXISTS) && 
            g_file_test (ctx.pubfile, G_FILE_TEST_EXISTS) &&
            check_file_for_ssh_private (ssrc, ctx.privfile)) {
                
            seahorse_ssh_key_data_parse_file (ctx.pubfile, parsed_public_key, NULL, &ctx, &err);
            if (err != NULL) {
                g_warning ("couldn't read SSH file: %s (%s)", ctx.pubfile, err->message);
                g_clear_error (&err);
            }
        }
        
        g_free (ctx.privfile);
        g_free (ctx.pubfile);
        ctx.privfile = ctx.pubfile = NULL;
    }

    g_dir_close (dir);
    
    /* Now load the authorized file */
    ctx.privfile = NULL;
    ctx.pubfile = seahorse_ssh_source_file_for_public (ssrc, TRUE);
        
    if (g_file_test (ctx.pubfile, G_FILE_TEST_EXISTS)) {
        seahorse_ssh_key_data_parse_file (ctx.pubfile, parsed_authorized_key, NULL, &ctx, &err);
        if (err != NULL) {
            g_warning ("couldn't read SSH file: %s (%s)", ctx.pubfile, err->message);
            g_clear_error (&err);
        }
    }
        
    g_free (ctx.pubfile);
    ctx.pubfile = NULL;
    
    /* Load the other keys file */
    ctx.privfile = NULL;
    ctx.pubfile = seahorse_ssh_source_file_for_public (ssrc, FALSE);
    if (g_file_test (ctx.pubfile, G_FILE_TEST_EXISTS)) {
        seahorse_ssh_key_data_parse_file (ctx.pubfile, parsed_other_key, NULL, &ctx, &err);
        if (err != NULL) {
            g_warning ("couldn't read SSH file: %s (%s)", ctx.pubfile, err->message);
            g_clear_error (&err);
        }
    }
        
    g_free (ctx.pubfile);
    ctx.pubfile = NULL;

    /* Clean up and done */
    if (ctx.checks) {
        g_hash_table_foreach (ctx.checks, (GHFunc)remove_key_from_context, ssrc);
        g_hash_table_destroy (ctx.checks);
    }
    
    g_hash_table_destroy (ctx.loaded);

    return seahorse_operation_new_complete (NULL);
}

static SeahorseOperation* 
seahorse_ssh_source_import (SeahorseSource *sksrc, GInputStream *input)
{
    SeahorseSSHSource *ssrc = SEAHORSE_SSH_SOURCE (sksrc);
    ImportContext ctx;
    gchar *contents;
    
    	g_return_val_if_fail (SEAHORSE_IS_SSH_SOURCE (ssrc), NULL);
    	g_return_val_if_fail (G_IS_INPUT_STREAM (input), NULL);

    	contents = (gchar*)seahorse_util_read_to_memory (input, NULL);
    
    memset (&ctx, 0, sizeof (ctx));
    ctx.ssrc = ssrc;
    ctx.mop = seahorse_multi_operation_new ();
    
    seahorse_ssh_key_data_parse (contents, import_public_key, import_private_key, &ctx);
    g_free (contents);
    
    /* TODO: The list of keys imported? */
    
    return SEAHORSE_OPERATION (ctx.mop);
}

static SeahorseOperation* 
seahorse_ssh_source_export (SeahorseSource *sksrc, GList *keys, 
                            GOutputStream *output)
{
    SeahorseSSHKeyData *keydata;
    SeahorseOperation *op;
    gchar *results = NULL;
    gchar *raw = NULL;
    GError *error = NULL;
    SeahorseObject *object;
    GList *l;
    gsize written;
    
    g_return_val_if_fail (SEAHORSE_IS_SSH_SOURCE (sksrc), NULL);
    g_return_val_if_fail (G_IS_OUTPUT_STREAM (output), NULL);
    
    for (l = keys; l; l = g_list_next (l)) {
        object = SEAHORSE_OBJECT (l->data);
        
        g_assert (SEAHORSE_IS_SSH_KEY (object));
        
        results = NULL;
        raw = NULL;
        
        keydata = NULL;
        g_object_get (object, "key-data", &keydata, NULL);
        g_return_val_if_fail (keydata, NULL);
        
        /* We should already have the data loaded */
        if (keydata->pubfile) { 
            g_assert (keydata->rawdata);
            results = g_strdup_printf ("%s\n", keydata->rawdata);
            
        /* Public key without identity.pub. Export it. */
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
        
        	if (error != NULL)
        		break;
    	}
    
    	if (error == NULL)
    		g_output_stream_close (output, NULL, &error);
    
    	op = seahorse_operation_new_complete (error);
    	g_object_ref (output);
    	seahorse_operation_mark_result (op, output, g_object_unref);
    	return op;
}

static void 
seahorse_ssh_source_set_property (GObject *object, guint prop_id, const GValue *value, 
                                  GParamSpec *pspec)
{
    
}

static void 
seahorse_ssh_source_get_property (GObject *object, guint prop_id, GValue *value, 
                                  GParamSpec *pspec)
{
    SeahorseSSHSource *ssrc = SEAHORSE_SSH_SOURCE (object);
    
    switch (prop_id) {
    case PROP_KEY_TYPE:
        g_value_set_uint (value, SEAHORSE_SSH);
        break;
    case PROP_KEY_DESC:
        g_value_set_string (value, _("Secure Shell Key"));
        break;
    case PROP_LOCATION:
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
   
    seahorse_ssh_source_parent_class = g_type_class_peek_parent (klass);
    
    gobject_class->dispose = seahorse_ssh_source_dispose;
    gobject_class->finalize = seahorse_ssh_source_finalize;
    gobject_class->set_property = seahorse_ssh_source_set_property;
    gobject_class->get_property = seahorse_ssh_source_get_property;
    
	g_object_class_override_property (gobject_class, PROP_KEY_TYPE, "key-type");
	g_object_class_override_property (gobject_class, PROP_KEY_DESC, "key-desc");
	g_object_class_override_property (gobject_class, PROP_LOCATION, "location");

    g_object_class_install_property (gobject_class, PROP_BASE_DIRECTORY,
        g_param_spec_string ("base-directory", "Key directory", "Directory where the keys are stored",
                             NULL, G_PARAM_READABLE));
    
	seahorse_registry_register_type (NULL, SEAHORSE_TYPE_SSH_SOURCE, "source", "local", SEAHORSE_SSH_STR, NULL);
	
	seahorse_registry_register_function (NULL, seahorse_ssh_key_calc_cannonical_id, "canonize", SEAHORSE_SSH_STR, NULL);
}

static void 
seahorse_source_iface (SeahorseSourceIface *iface)
{
	iface->load = seahorse_ssh_source_load;
	iface->import = seahorse_ssh_source_import;
	iface->export = seahorse_ssh_source_export;
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
seahorse_ssh_source_key_for_filename (SeahorseSSHSource *ssrc, const gchar *privfile)
{
    SeahorseSSHKeyData *data;
    GList *keys, *l;
    int i;
    
    g_assert (privfile);
    g_return_val_if_fail (SEAHORSE_IS_SSH_SOURCE (ssrc), NULL);
    
    for (i = 0; i < 2; i++) {
    
        /* Try to find it first */
        keys = seahorse_context_get_objects (SCTX_APP (), SEAHORSE_SOURCE (ssrc));
        for (l = keys; l; l = g_list_next (l)) {
            
            g_object_get (l->data, "key-data", &data, NULL);
            g_return_val_if_fail (data, NULL);
        
            /* If it's already loaded then just leave it at that */
            if (data->privfile && strcmp (privfile, data->privfile) == 0)
                return SEAHORSE_SSH_KEY (l->data);
        }
        
        /* Force loading of all new keys */
        if (!i) {
            seahorse_source_load_sync (SEAHORSE_SOURCE (ssrc));
        }
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
