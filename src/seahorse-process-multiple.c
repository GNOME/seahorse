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

#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <libintl.h>

#include <gnome.h>
#include <eel/eel.h>
#include <libgnomevfs/gnome-vfs.h>

#include "seahorse-widget.h"
#include "seahorse-context.h"
#include "seahorse-util.h"

/* Build a message for a given combination of files and folders */
static gchar* 
make_message (guint folders, guint files)
{
    const gchar* t;
    
    g_assert(folders > 0 || files > 0);
    
    if (folders > 0 && files > 0) {
        /* Necessary hoopla for translations */
        if (files > 1 && folders > 1)
            t = _("You have selected %d files and %d folders");
        else if (files > 1)
            t = _("You have selected %d files and %d folder");
        else if (folders > 1)
            t = _("You have selected %d file and %d folders");
        else 
            t = _("You have selected %d file and %d folder");
        
        return g_strdup_printf(t, files, folders);
        
    } else if (files > 0) {
        g_assert (files > 1);    /* should never be called for just one file */
        return g_strdup_printf (_("You have selected %d files"), files);

    } else if (folders > 0) {
        return g_strdup_printf (
            ngettext ("You have selected %d folder", "You have selected %d folders", folders), folders);

    } else {
        g_assert_not_reached ();
        return NULL; /* to fix warnings */
    }
}

/* Figure out how many files and folders in uri vector */
static void
discover_uris (const gchar** uris, guint *folders, guint *files, gboolean *remote)
{
    GnomeVFSFileInfo* info;
    const gchar** u;
    gchar* uri;
    
    g_assert (folders && files);
    
    *folders = 0;
    *files = 0;
    *remote = FALSE;
    
    info = gnome_vfs_file_info_new ();
    
    for (u = uris; *u; u++) {
        uri = gnome_vfs_make_uri_canonical (*u);

        if (gnome_vfs_get_file_info (uri, info, GNOME_VFS_FILE_INFO_DEFAULT) == GNOME_VFS_OK) {
            
            if (!GNOME_VFS_FILE_INFO_LOCAL (info))
                *remote = TRUE;
            
            if (info->type == GNOME_VFS_FILE_TYPE_DIRECTORY)
                (*folders)++;
            else 
                (*files)++;
        }
        
        else    /* We count things we can't see as files. Errors occur later */
            (*files)++;
        
        g_free (uri);
        gnome_vfs_file_info_clear (info);
    } 
  
    gnome_vfs_file_info_unref (info);
}

/* Callback for main option buttons */
static void
seperate_toggled (GtkWidget *widget, GtkWidget *package)
{
    gtk_widget_set_sensitive (package, 
        !gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)));
}  

/* Build the multiple file dialog */
static SeahorseWidget*
prepare_dialog (SeahorseContext *sctx, const gchar* glade, const gchar* msg, 
                    gchar* pkguri, gboolean remote)
{
    SeahorseWidget *swidget;
    const gchar* pkg;
    GtkWidget *tog;
    GtkWidget *w;
    gchar *x;
    gboolean sep;
    gint sel;
    
    g_assert (sctx);
    g_assert (pkguri);

    swidget = seahorse_widget_new ((gchar*)glade, sctx);
    g_return_val_if_fail (swidget != NULL, NULL);
    
    /* The main 'selected' message */
    if (msg) {
        w = glade_xml_get_widget (swidget->xml, "message");
        x = g_strconcat ("<b>", msg, "</b>", NULL);
        gtk_label_set_markup (GTK_LABEL(w), x);
        g_free (x);
    }
    
    /* Setup the remote or local messages */
    w = glade_xml_get_widget (swidget->xml, remote ? "remote-options" : "local-options");
    gtk_widget_show (w);
    
    tog = glade_xml_get_widget (swidget->xml, "do-separate");

    if (remote) {
        /* Always use the seperate option */        
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (tog), TRUE);
        
    /* The local stuff */        
    } else {
    
        sep = eel_gconf_get_boolean (MULTI_SEPERATE_KEY);
        
        /* Setup the package */
        w = glade_xml_get_widget (swidget->xml, "package-name");
        pkg = seahorse_util_uri_split_last (pkguri);
        gtk_entry_set_text (GTK_ENTRY (w), pkg);
    
        if(sep) {
            gtk_widget_grab_focus (w);
            gtk_editable_select_region (GTK_EDITABLE (w), 0, sel);
        }

        /* Setup the main radio buttons */
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (tog), sep);
        g_signal_connect (tog, "toggled", G_CALLBACK (seperate_toggled), w);
        seperate_toggled (tog, w);
    }
    
    return swidget;
}

/* Get the package name and selection */
static gchar*
get_results (SeahorseContext *sctx, SeahorseWidget *swidget)
{
    const gchar* name;
    const gchar* t;
    GtkWidget *w;
    gboolean sep;
    
    w = glade_xml_get_widget (swidget->xml, "do-separate");
    sep = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (w));
    eel_gconf_set_boolean (MULTI_SEPERATE_KEY, sep);
              
    /* no packaging */                    
    if(!sep) { 

        w = glade_xml_get_widget (swidget->xml, "package-name");
        name = gtk_entry_get_text (GTK_ENTRY (w));
        
        /* No paths */
        t = strrchr(name, '/');
        name = t ? ++t : name;
        
        /* If someone goes out of their way to delete the file name, 
         * we're simply unimpressed and put back a default. */
        if(name[0] == 0)
            name = "encrypted-package.zip";
            
        /* Save the extension */
        t = strchr(name, '.');
        if(t != NULL)
        {
            t++;
            if(t[0] != 0) 
                eel_gconf_set_string (MULTI_EXTENSION_KEY, t);
        }
        
        return g_strdup(name);
    }
    
    return NULL;
}

/**
 * seahorse_process_multiple
 * @sctx: A valid SeahorseContext
 * @uris: null-terminated vector of URIs
 * @glade: The form to display, or NULL for default
 * 
 * When more than on URI (usually to encrypt) prompt the user 
 * whether to archive them or do them all individually.
 * 
 * Returns: Newly allocated null-terminated vector of URIs to encrypt
 **/
gchar** 
seahorse_process_multiple(SeahorseContext *sctx, const gchar **uris, const gchar *glade)
{
    SeahorseWidget *swidget;
    gboolean done = FALSE;
    gchar* pkg_uri = NULL;
    gchar* package = NULL;
    gchar* ext;
    gchar** ret = NULL;
    gboolean ok = FALSE;
    GtkWidget *dlg;
    guint folders;
    guint files;
    gboolean remote;
    gchar* t;
    
    g_assert (uris && *uris);
 
    /* Figure out how many folders and files */
    discover_uris (uris, &folders, &files, &remote);
    
    /* In the case of one or less files, no dialog */
    if(folders == 0 && files <= 1)
    {
        ret = g_new0 (gchar*, 2);
        ret[0] = g_strdup (uris[0]);
        return ret;
    }

    if(glade == NULL)
        glade = "multi-encrypt";
        
    /* The package extension */
    if ((ext = eel_gconf_get_string (MULTI_EXTENSION_KEY)) == NULL)
        ext = g_strdup ("zip"); /* Yes this happens when the schema isn't installed */    
        
    /* Figure out a good URI for our package */
    pkg_uri = gnome_vfs_make_uri_canonical (uris[0]);
    t = seahorse_util_uri_replace_ext (pkg_uri, ext);
    g_free (pkg_uri);
    g_free (ext);
    
    pkg_uri = seahorse_util_uri_unique (t);
    g_free (t);
        
    /* This sets up but doesn't run the dialog */    
    t = make_message (folders, files);
    swidget = prepare_dialog (sctx, glade, t, pkg_uri, remote);
    g_free (t);
    
    dlg = glade_xml_get_widget (swidget->xml, glade);
    
    while (!done) {
        switch (gtk_dialog_run (GTK_DIALOG (dlg)))
        {
            case GTK_RESPONSE_HELP:
                /* TODO: Implement help */
                break;
            
            case GTK_RESPONSE_OK:
                package = get_results (sctx, swidget);
                ok = TRUE;
                /* Fall through */
                
            default:
               done = TRUE;
               break;
        }
    }
    
    seahorse_widget_destroy (swidget);
    
    if (ok) {
        /* A package was selected */
        if (package) {
         
            /* Make a new path based on the first uri */
            t = g_strconcat (pkg_uri, "/", package, NULL);
    
            if (seahorse_util_uris_package (t, uris)) {
                ret = g_new0 (gchar*, 2);
                ret[0] = t;
                t = NULL;
            }
            
            g_free(t);
                
        /* No packaging, process seperately */
        } else {
    
            ret = seahorse_util_uris_expand (uris);
        }      
    }
            
    g_free (package);
    return ret;    
}
