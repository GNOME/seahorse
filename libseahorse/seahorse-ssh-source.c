/*
 * Seahorse
 *
 * Copyright (C) 2004 Nate Nielsen
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

#include <sys/wait.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <gnome.h>
#include <glib/gstdio.h>
#include <libgnomevfs/gnome-vfs.h>

#include "seahorse-ssh-source.h"
#include "seahorse-operation.h"
#include "seahorse-util.h"
#include "seahorse-ssh-key.h"

/* Override the DEBUG_REFRESH_ENABLE switch here */
#define DEBUG_REFRESH_ENABLE 0

#ifndef DEBUG_REFRESH_ENABLE
#if _DEBUG
#define DEBUG_REFRESH_ENABLE 1
#else
#define DEBUG_REFRESH_ENABLE 0
#endif
#endif

#if DEBUG_REFRESH_ENABLE
#define DEBUG_REFRESH(x)    g_printerr(x)
#else
#define DEBUG_REFRESH(x)
#endif

enum {
    PROP_0,
    PROP_KEY_TYPE,
    PROP_LOCATION
};

struct _SeahorseSSHSourcePrivate {
    gchar *ssh_homedir;                     /* Home directory for SSH keys */
    guint scheduled_refresh;                /* Source for refresh timeout */
    GnomeVFSMonitorHandle *monitor_handle;  /* For monitoring the .ssh directory */
};

G_DEFINE_TYPE (SeahorseSSHSource, seahorse_ssh_source, SEAHORSE_TYPE_KEY_SOURCE);

/* -----------------------------------------------------------------------------
 * INTERNAL
 */

static gboolean
check_file_for_ssh_key (SeahorseSSHSource *ssrc, const gchar *filename)
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
    
    /* Check for our signature */
    if (strstr (buf, " PRIVATE KEY-----"))
        return TRUE;
    
    return FALSE;
}

static void
key_changed (SeahorseKey *skey, SeahorseKeyChange change, SeahorseKeySource *sksrc)
{
    /* TODO: We need to fix these key change flags. Currently the only thing
     * that sets 'ALL' is when we replace a key in an skey. We don't 
     * need to reload in that case */
    
    if (change != SKEY_CHANGE_ALL)
        seahorse_key_source_load_async (sksrc, SKSRC_LOAD_KEY, 
                                        seahorse_key_get_keyid (skey));
}

static void
key_destroyed (SeahorseKey *skey, SeahorseKeySource *sksrc)
{
    g_signal_handlers_disconnect_by_func (skey, key_changed, sksrc);
    g_signal_handlers_disconnect_by_func (skey, key_destroyed, sksrc);
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
remove_key_from_context (const gchar *id, SeahorseKey *dummy, SeahorseSSHSource *ssrc)
{
    SeahorseKey *skey;
    
    skey = seahorse_context_get_key (SCTX_APP (), SEAHORSE_KEY_SOURCE (ssrc), id);    
fprintf (stderr, "trying to remove key: %s %d\n", id, (guint)skey);            
    if (skey != NULL)
        seahorse_context_remove_key (SCTX_APP (), skey);
}


static gboolean
scheduled_refresh (SeahorseSSHSource *ssrc)
{
    DEBUG_REFRESH ("scheduled refresh event ocurring now\n");
    cancel_scheduled_refresh (ssrc);
    seahorse_key_source_load_async (SEAHORSE_KEY_SOURCE (ssrc), SKSRC_LOAD_ALL, NULL);    
    return FALSE; /* don't run again */    
}

static gboolean
scheduled_dummy (SeahorseSSHSource *ssrc)
{
    DEBUG_REFRESH ("dummy refresh event occurring now\n");
    ssrc->priv->scheduled_refresh = 0;
    return FALSE; /* don't run again */    
}

static void
monitor_ssh_homedir (GnomeVFSMonitorHandle *handle, const gchar *monitor_uri,
                     const gchar *info_uri, GnomeVFSMonitorEventType event_type,
                     SeahorseSSHSource *ssrc)
{
    gchar *path;
    
    if (ssrc->priv->scheduled_refresh != 0 ||
        (event_type != GNOME_VFS_MONITOR_EVENT_CREATED && 
         event_type != GNOME_VFS_MONITOR_EVENT_CHANGED &&
         event_type != GNOME_VFS_MONITOR_EVENT_DELETED))
        return;
    
    path = gnome_vfs_get_local_path_from_uri (info_uri);
    if (path == NULL)
        return;

    if (event_type != GNOME_VFS_MONITOR_EVENT_DELETED && 
        !check_file_for_ssh_key (ssrc, path))
        return;        
    
    DEBUG_REFRESH ("scheduling refresh event due to file changes\n");
    ssrc->priv->scheduled_refresh = g_timeout_add (500, (GSourceFunc)scheduled_refresh, ssrc);
}

/* -----------------------------------------------------------------------------
 * OBJECT
 */

static SeahorseOperation*
seahorse_ssh_source_load (SeahorseKeySource *src, SeahorseKeySourceLoad load,
                          const gchar *match)
{
    SeahorseSSHSource *ssrc;
    SeahorseSSHKey *skey;
    SeahorseKey *key;
    SeahorseSSHKeyData *keydata;
    GError *error = NULL;
    GHashTable *checks = NULL;
    GList *keys, *l;
    const gchar *filename;
    gchar *t;
    GDir *dir;
    
    g_return_val_if_fail (SEAHORSE_IS_SSH_SOURCE (src), NULL);
    ssrc = SEAHORSE_SSH_SOURCE (src);
 
    /* Schedule a dummy refresh. This blocks all monitoring for a while */
    cancel_scheduled_refresh (ssrc);
    ssrc->priv->scheduled_refresh = g_timeout_add (500, (GSourceFunc)scheduled_dummy, ssrc);
    DEBUG_REFRESH ("scheduled a dummy refresh\n");
        
    if (load == SKSRC_LOAD_NEW) {
        /* This hashtable contains only allocated strings with no 
         * values. We allocate the strings in case a key goes away
         * while we're holding the ids in this table */
        checks = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
        
        keys = seahorse_context_get_keys (SCTX_APP (), src);
        for (l = keys; l; l = g_list_next (l)) {
fprintf (stderr, "have key: %s\n", seahorse_key_get_keyid (SEAHORSE_KEY (l->data)));            
            g_hash_table_insert (checks, g_strdup (seahorse_key_get_keyid (l->data)), NULL);
        }
        g_list_free (keys);
    }

    /* List the directory */
    dir = g_dir_open (ssrc->priv->ssh_homedir, 0, &error);
    if (!dir)
        return seahorse_operation_new_complete (error);

    /* For each file */
    for(;;) {
        filename = g_dir_read_name (dir);
        if (filename == NULL)
            break;
        
        t = g_strconcat (ssrc->priv->ssh_homedir, filename, NULL);
        
        /* possibly an SSH key? */
        if(!check_file_for_ssh_key (ssrc, t)) {
            g_free (t);
            continue;
        }
        
        /* Try to load it */
        keydata = seahorse_ssh_key_data_read (t);
        g_free (t);

        if (!keydata->keyid) {
            seahorse_ssh_key_data_free (keydata);
            continue;
        }
        
        switch(load) {
            
        case SKSRC_LOAD_NEW:
            /* If we already have this key then just transfer ownership of keydata */
            key = seahorse_context_get_key (SCTX_APP (), src, keydata->keyid);
            if (key) {
                g_object_set (key, "key-data", keydata, NULL);
                keydata = NULL;
            }
            g_hash_table_remove (checks, keydata->keyid);
            break;
            
        case SKSRC_LOAD_KEY:
            if (!g_str_equal (match, keydata->keyid)) {
                seahorse_ssh_key_data_free (keydata);
                keydata = NULL;
            }
            break;
            
        case SKSRC_LOAD_ALL:
            key = seahorse_context_get_key (SCTX_APP (), src, keydata->keyid);
            if (key)
                seahorse_context_remove_key (SCTX_APP (), key);
            break;
            
        default:
            break;
        }

        if (keydata) {
            
            skey = seahorse_ssh_key_new (src, keydata);
            
            /* We listen in to get notified of changes on this key */
            g_signal_connect (skey, "changed", G_CALLBACK (key_changed), SEAHORSE_KEY_SOURCE (ssrc));
            g_signal_connect (skey, "destroy", G_CALLBACK (key_destroyed), SEAHORSE_KEY_SOURCE (ssrc));
        
            seahorse_context_take_key (SCTX_APP (), SEAHORSE_KEY (skey));
        }
    }

    if (checks) 
        g_hash_table_foreach (checks, (GHFunc)remove_key_from_context, ssrc);

    g_dir_close (dir);

    return seahorse_operation_new_complete (NULL);
}

static void
seahorse_ssh_source_stop (SeahorseKeySource *src)
{

}

static guint
seahorse_ssh_source_get_state (SeahorseKeySource *src)
{
    return 0;
}

static SeahorseOperation* 
seahorse_ssh_source_import (SeahorseKeySource *sksrc, gpgme_data_t data)
{
    // TODO: Implement this
    g_warning ("no import support yet for SSH");
    return seahorse_operation_new_complete (NULL);              
}

static SeahorseOperation* 
seahorse_ssh_source_export (SeahorseKeySource *sksrc, GList *keys, 
                            gboolean complete, gpgme_data_t data)
{
    gchar *results, *cmd;
    const gchar *filename, *filepub;
    GError *error = NULL;
    SeahorseKey *skey;
    gint len, r;
    GList *l;
    
    for (l = keys; l; l = g_list_next (l)) {
        skey = SEAHORSE_KEY (l->data);
        
        g_return_val_if_fail (SEAHORSE_IS_SSH_KEY (skey), NULL);
        
        filename = seahorse_ssh_key_get_filename (SEAHORSE_SSH_KEY (skey), TRUE);
        filepub = seahorse_ssh_key_get_filename (SEAHORSE_SSH_KEY (skey), FALSE);

        g_return_val_if_fail (filename != NULL, NULL);

        /* Complete key means the private key */
        if (complete) {
            if (!g_file_get_contents (filename, &results, NULL, &error))
                results = NULL;
           
        /* Public key. If an identity.pub exists, return it */
        } else if (filepub) { 
            if (!g_file_get_contents (filepub, &results, NULL, &error))
                results = NULL;
            
        /* Public key without identity.pub. Export it. */
        } else {
            cmd = g_strdup_printf (SSH_KEYGEN_PATH " -y %s", filename);
            results = seahorse_ssh_source_execute (cmd, &error);
            g_free (cmd);
        }
        
        if (!results)
            return seahorse_operation_new_complete (error);
        
        len = strlen (results);
        r = gpgme_data_write (data, results, len);
        g_free (results);
        
        if (r != len) {
            g_set_error (&error, G_FILE_ERROR, g_file_error_from_errno (errno),
                         strerror (errno));
            return seahorse_operation_new_complete (error);
        }
    }
    
    return seahorse_operation_new_complete (NULL);
}

static gboolean            
seahorse_ssh_source_remove (SeahorseKeySource *sksrc, SeahorseKey *skey,
                            guint name, GError **error)
{
    const gchar *filename, *filepub;
    gboolean ret = TRUE;
    
    g_return_val_if_fail (name == 0, FALSE);
    g_return_val_if_fail (seahorse_key_get_source (skey) == sksrc, FALSE);
    g_assert (!error || !*error);
    
    filename = seahorse_ssh_key_get_filename (SEAHORSE_SSH_KEY (skey), TRUE);
    filepub = seahorse_ssh_key_get_filename (SEAHORSE_SSH_KEY (skey), FALSE);

    g_return_val_if_fail (filename != NULL, FALSE);
    
    if (filepub) {
        if (g_unlink (filepub) == -1) {
            g_set_error (error, G_FILE_ERROR, g_file_error_from_errno (errno), 
                         "%s", g_strerror (errno));
            ret = FALSE;
        }
    }

    if (ret && filename) {
        if (g_unlink (filename) == -1) {
            g_set_error (error, G_FILE_ERROR, g_file_error_from_errno (errno), 
                         "%s", g_strerror (errno));
            ret = FALSE;
        }
    }
    
    if (ret)
        seahorse_key_destroy (SEAHORSE_KEY (skey));

    return ret;
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
    switch (prop_id) {
    case PROP_KEY_TYPE:
        g_value_set_uint (value, SKEY_SSH);
        break;
    case PROP_LOCATION:
        g_value_set_uint (value, SKEY_LOC_LOCAL);
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
        gnome_vfs_monitor_cancel (ssrc->priv->monitor_handle);
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
    GnomeVFSResult res;
    gchar *uri;
    
    /* init private vars */
    ssrc->priv = g_new0 (SeahorseSSHSourcePrivate, 1);
    
    ssrc->priv->scheduled_refresh = 0;
    ssrc->priv->monitor_handle = NULL;

    ssrc->priv->ssh_homedir = g_strdup_printf ("%s/.ssh/", g_get_home_dir ());
    uri = gnome_vfs_make_uri_canonical (ssrc->priv->ssh_homedir);
    g_return_if_fail (uri != NULL);
    
    res = gnome_vfs_monitor_add (&(ssrc->priv->monitor_handle), uri, 
                                 GNOME_VFS_MONITOR_DIRECTORY, 
                                 (GnomeVFSMonitorCallback)monitor_ssh_homedir, ssrc);
    g_free (uri);
    
    if (res != GNOME_VFS_OK) {
        ssrc->priv->monitor_handle = NULL;        
        g_return_if_reached ();
    }
}

static void
seahorse_ssh_source_class_init (SeahorseSSHSourceClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    SeahorseKeySourceClass *parent_class = SEAHORSE_KEY_SOURCE_CLASS (klass);
   
    seahorse_ssh_source_parent_class = g_type_class_peek_parent (klass);
    
    gobject_class->dispose = seahorse_ssh_source_dispose;
    gobject_class->finalize = seahorse_ssh_source_finalize;
    gobject_class->set_property = seahorse_ssh_source_set_property;
    gobject_class->get_property = seahorse_ssh_source_get_property;
    
    parent_class->load = seahorse_ssh_source_load;
    parent_class->stop = seahorse_ssh_source_stop;
    parent_class->get_state = seahorse_ssh_source_get_state;
    parent_class->import = seahorse_ssh_source_import;
    parent_class->export = seahorse_ssh_source_export;
    parent_class->remove = seahorse_ssh_source_remove;
 
    g_object_class_install_property (gobject_class, PROP_KEY_TYPE,
        g_param_spec_uint ("key-type", "Key Type", "Key type that originates from this key source.", 
                           0, G_MAXUINT, SKEY_INVALID, G_PARAM_READABLE));

    g_object_class_install_property (gobject_class, PROP_LOCATION,
        g_param_spec_uint ("location", "Key Location", "Where the key is stored. See SeahorseKeyLoc", 
                           0, G_MAXUINT, SKEY_LOC_UNKNOWN, G_PARAM_READABLE));    
}

/* -----------------------------------------------------------------------------
 * PUBLIC 
 */

SeahorseSSHSource*
seahorse_ssh_source_new (void)
{
   return g_object_new (SEAHORSE_TYPE_SSH_SOURCE, NULL);
}   

gchar*
seahorse_ssh_source_execute (const gchar *command, GError **error)
{
    GError *err = NULL;
    gchar *sout, *serr;
    gint status;
    
    g_assert (!error || !*error);
    
    /* We use this internally, so guarantee it exists */
    if (!error)
        error = &err;
    
    if(!g_spawn_command_line_sync (command, &sout, &serr, &status, error)) {
        g_critical ("couldn't execute SSH command: %s (%s)", command, (*error)->message);
        return NULL;
    }
    
    if (!WIFEXITED (status)) {
        g_critical ("SSH command didn't exit properly: %s", command);
        g_set_error (error, SEAHORSE_ERROR, 0, "%s", _("The SSH command was terminated unexpectedly."));
        g_free (sout);
        g_free (serr);
        return NULL;
    }
    
    if (WEXITSTATUS (status) != 0) {
        g_set_error (error, SEAHORSE_ERROR, 0, "%s", _("The SSH command failed."));
        g_warning ("SSH command failed: %s (%d)", command, WEXITSTATUS (status));
        if (serr && serr[0])
            g_warning ("SSH error output: %s", serr);
        g_free (serr);
        g_free (sout);
        return NULL;
    }
    
    g_free (serr);
    return sout;
}

