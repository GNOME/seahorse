/*
 * Seahorse
 *
 * Copyright (C) 2006 Nate Nielsen
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
#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <libintl.h>

#include <gnome.h>
#include <libgnomevfs/gnome-vfs.h>

#include "seahorse-tool.h"
#include "seahorse-context.h"
#include "seahorse-util.h"
#include "seahorse-widget.h"
#include "seahorse-gconf.h"
#include "seahorse-vfs-data.h"

#define ONE_GIGABYTE 1024 * 1024 * 1024

typedef struct _FileInfo {
    gchar *uri;
    GnomeVFSFileSize size;
    gboolean directory;
} FileInfo;

typedef struct _FilesCtx {
    GSList *uris;
    GList *files;
    FileInfo *cur;
    gboolean remote;
    
    guint64 total;
    guint64 done;
} FilesCtx;

static void
free_file_info (FileInfo *file, gpointer unused)
{
    if (file)
        g_free (file->uri);
    g_free (file);
}

/* -----------------------------------------------------------------------------
 * CHECK STEP
 */

gboolean
step_check_uris (FilesCtx *ctx, const gchar **uris, GError **err)
{
    GnomeVFSFileInfo* info = NULL;
    GnomeVFSResult res;
    gchar *uri = NULL;
    gboolean ret = TRUE;
    FileInfo *file;
    const gchar **k;

    g_assert (err && !*err);

    info = gnome_vfs_file_info_new ();

    for (k = uris; *k; k++) {
        
        if (!seahorse_tool_progress_check ()) {
            ret = FALSE;
            break;
        }

        if (uri)
            g_free (uri);
        uri = gnome_vfs_make_uri_canonical (*k);

        gnome_vfs_file_info_clear (info);
    
        res = gnome_vfs_get_file_info (uri, info, GNOME_VFS_FILE_INFO_DEFAULT);
        if (res != GNOME_VFS_OK) {
            g_set_error (err, G_FILE_ERROR, -1, gnome_vfs_result_to_string (res));
            ret = FALSE;
            break;
        }
        
        /* Only handle simple types */
        if (info->type != GNOME_VFS_FILE_TYPE_REGULAR &&
            info->type != GNOME_VFS_FILE_TYPE_UNKNOWN &&
            info->type != GNOME_VFS_FILE_TYPE_DIRECTORY)
            continue;

        if (!GNOME_VFS_FILE_INFO_LOCAL (info))
            ctx->remote = TRUE;
        
        file = g_new0 (FileInfo, 1);
        file->size = info->size;
        file->directory = (info->type == GNOME_VFS_FILE_TYPE_DIRECTORY);
        file->uri = uri;
        uri = NULL;
        
        ctx->total += file->size;
        ctx->files = g_list_prepend (ctx->files, file);
    }
    
    g_free (uri);
    gnome_vfs_file_info_unref (info);
    
    ctx->files = g_list_reverse (ctx->files);
    return ret;
}

/* -----------------------------------------------------------------------------
 * PACKAGE STEP
 */

/* Build a message for a given combination of files and folders */
static gchar* 
make_message (guint folders, guint files)
{
    gchar *msg, *s1, *s2;
    
    g_assert(folders > 0 || files > 0);
    
    /* Necessary hoopla for translations */
    if (folders > 0 && files > 0) {
        
        /* TRANSLATOR: This string will become
         * "You have selected %d files and %d folders" */
        s1 = g_strdup_printf(ngettext("You have selected %d file ", "You have selected %d files ", files),
                             files);

        /* TRANSLATOR: This string will become
         * "You have selected %d files and %d folders" */
        s2 = g_strdup_printf(ngettext("and %d folder", "and %d folders", folders),
                             folders);

        /* TRANSLATOR: "%s%s" are "You have selected %d files and %d folders"
         * Swap order with "%2$s%1$s" if needed */
        msg = g_strdup_printf(_("<b>%s%s</b>"), s1, s2);
        
        g_free (s1);
        g_free (s2);
        return msg;
        
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

/* Callback for main option buttons */
static void
seperate_toggled (GtkWidget *widget, GtkWidget *package)
{
    gtk_widget_set_sensitive (package, 
        !gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)));
}

/* Build the multiple file dialog */
static SeahorseWidget*
prepare_dialog (FilesCtx *ctx, guint nfolders, guint nfiles, gchar* pkguri)
{
    SeahorseWidget *swidget;
    const gchar* pkg;
    GtkWidget *tog;
    GtkWidget *w;
    gchar *msg;
    gboolean sep;
    gint sel;
    
    g_assert (pkguri);

    swidget = seahorse_widget_new ("multi-encrypt");
    g_return_val_if_fail (swidget != NULL, NULL);
    
    /* The main 'selected' message */
    msg = make_message (nfolders, nfiles);
    w = glade_xml_get_widget (swidget->xml, "message");
    gtk_label_set_markup (GTK_LABEL(w), msg);
    g_free (msg);
    
    /* Setup the remote or local messages */
    w = glade_xml_get_widget (swidget->xml, 
            ctx->remote ? "remote-options" : "local-options");
    gtk_widget_show (w);
    
    tog = glade_xml_get_widget (swidget->xml, "do-separate");

    if (ctx->remote) {
        /* Always use the seperate option */        
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (tog), TRUE);
        
    /* The local stuff */        
    } else {
    
        sep = seahorse_gconf_get_boolean (MULTI_SEPERATE_KEY);
        
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
get_results (SeahorseWidget *swidget)
{
    const gchar* name;
    const gchar* t;
    GtkWidget *w;
    gboolean sep;
    
    w = glade_xml_get_widget (swidget->xml, "do-separate");
    sep = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (w));
    seahorse_gconf_set_boolean (MULTI_SEPERATE_KEY, sep);
    
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
                seahorse_gconf_set_string (MULTI_EXTENSION_KEY, t);
        }
        
        return g_strdup(name);
    }
    
    return NULL;
}

gboolean
step_process_multiple (FilesCtx *ctx, const gchar **orig_uris, GError **err)
{
    SeahorseWidget *swidget;
    gboolean done = FALSE;
    gchar *pkg_uri = NULL;
    gchar *package = NULL;
    gchar *ext;
    gboolean ok = FALSE;
    GtkWidget *dlg;
    guint nfolders, nfiles;
    gchar *uris[2];
    gchar *t;
    GList *l;
    
    g_assert (err && !*err);
 
    for (l = ctx->files, nfolders = nfiles = 0; l; l = g_list_next (l)) {
        if (((FileInfo*)(l->data))->directory)
            ++nfolders;
        ++nfiles;
    }
        
    /* In the case of one or less files, no dialog */
    if(nfolders == 0 && nfiles <= 1)
        return TRUE;
        
    /* The package extension */
    if ((ext = seahorse_gconf_get_string (MULTI_EXTENSION_KEY)) == NULL)
        ext = g_strdup ("zip"); /* Yes this happens when the schema isn't installed */    
        
    /* Figure out a good URI for our package */
    for (l = ctx->files; l; l = g_list_next (l)) {
        if (l->data) {
            pkg_uri = gnome_vfs_make_uri_canonical (((FileInfo*)(l->data))->uri);
            break;
        }
    }
    
    /* Add proper extensions etc... */
    g_assert (pkg_uri);
    t = seahorse_util_uri_replace_ext (pkg_uri, ext);
    g_free (pkg_uri);
    g_free (ext);
    
    pkg_uri = seahorse_util_uri_unique (t);
    g_free (t);
        
    /* This sets up but doesn't run the dialog */    
    swidget = prepare_dialog (ctx, nfolders, nfiles, pkg_uri);
    
    dlg = seahorse_widget_get_top (swidget);
    
    /* Inhibit popping up of progress dialog */
    seahorse_tool_progress_block (TRUE);
    
    while (!done) {
        switch (gtk_dialog_run (GTK_DIALOG (dlg)))
        {
        case GTK_RESPONSE_HELP:
            /* TODO: Implement help */
            break;
            
        case GTK_RESPONSE_OK:
            package = get_results (swidget);
            ok = TRUE;
            /* Fall through */
                
        default:
            done = TRUE;
            break;
        }
    }

    /* Let progress dialog pop up */
    seahorse_tool_progress_block (FALSE);
    
    seahorse_widget_destroy (swidget);
    
    /* Cancelled */
    if (!ok)
        return FALSE;
    
    /* No package was selected? */
    if (!package)
        return TRUE;
    
    /* A package was selected */
        
    /* Make a new path based on the first uri */
    t = g_strconcat (pkg_uri, "/", package, NULL);
    g_free (package);
    
    if (!seahorse_util_uris_package (t, orig_uris)) {
        g_free (t);
        return FALSE;
    }
    
    /* Free all file info */
    g_list_foreach (ctx->files, (GFunc)free_file_info, NULL);
    g_list_free (ctx->files);
    ctx->files = NULL;
    
    /* Reload up the new file, as what to encrypt */
    uris[0] = t;
    uris[1] = NULL;
    ok = step_check_uris (ctx, (const gchar**)uris, err);
    g_free (t);
    
    return ok;
}

/* -----------------------------------------------------------------------------
 * EXPAND STEP
 */

typedef struct _VisitCtx {
    FilesCtx *ctx;
    GList *cur;
    const gchar *base;
} VisitCtx;

/* Called for each sub file or directory */
static gboolean
visit_uri (const gchar *rel_path, GnomeVFSFileInfo *info, gboolean recursing_will_loop,
           VisitCtx *vctx, gboolean *recurse)
{
    GList *l;
    FileInfo *file;
    gchar *t, *uri;

    /* Only files get added to our list */
    if(info->type == GNOME_VFS_FILE_TYPE_REGULAR ||
       info->type == GNOME_VFS_FILE_TYPE_UNKNOWN) {
           
        t = g_strconcat (vctx->base, "/", rel_path, NULL);
        uri = gnome_vfs_make_uri_canonical (t);
        g_free (t);
           
        file = g_new0(FileInfo, 1);
        file->uri = uri;
        file->size = info->size;
        
        /* A bit of list hanky panky */
        l = g_list_insert (vctx->cur, file, 1);
        g_assert (l == vctx->cur);
           
        vctx->ctx->total += file->size;
    }
    
    if (!seahorse_tool_progress_check ())
        return FALSE;

    *recurse = !recursing_will_loop;
    return TRUE;
}

gboolean
step_expand_uris (FilesCtx *ctx, GError **err)
{
    GnomeVFSResult res;
    gboolean ret = TRUE;
    FileInfo *file;
    VisitCtx vctx;
    GList *l;

    g_assert (err && !*err);
 
    for (l = ctx->files; l; l = g_list_next (l)) {
        
        if (!seahorse_tool_progress_check ()) {
            ret = FALSE;
            break;
        }

        file = (FileInfo*)(l->data);
        if (!file || !file->uri)
            continue;
        
        if (file->directory) {
            
            vctx.ctx = ctx;
            vctx.cur = l;
            vctx.base = file->uri;
            
            res = gnome_vfs_directory_visit (file->uri, GNOME_VFS_FILE_INFO_DEFAULT, 
                                             GNOME_VFS_DIRECTORY_VISIT_LOOPCHECK,
                                             (GnomeVFSDirectoryVisitFunc)visit_uri, &vctx);
            if (res != GNOME_VFS_OK) {
                g_set_error (err, G_FILE_ERROR, -1, gnome_vfs_result_to_string (res));
                ret = FALSE;
                break;
            }
            
            /* We don't actually do operations on the dirs */
            free_file_info (file, NULL);
            l->data = NULL;
        }
    }
    
    return ret;
}

/* -----------------------------------------------------------------------------
 * ACTUAL OPERATION STEP
 */

static void 
progress_cb (gpgme_data_t data, GnomeVFSFileSize pos, FilesCtx *ctx)
{
    gdouble total, done, size, portion;
    
    g_assert (ctx && ctx->cur);
    
    total = ctx->total > ONE_GIGABYTE ? ctx->total / 1000 : ctx->total;
    done = ctx->total > ONE_GIGABYTE ? ctx->done / 1000 : ctx->done;
    size = ctx->total > ONE_GIGABYTE ? ctx->cur->size / 1000 : ctx->cur->size;
    portion = ctx->total > ONE_GIGABYTE ? pos / 1000 : pos;
    
    total = total <= 0 ? 1 : total;
    size = size <= 0 ? 1 : size;
        
    /* The cancel check is done elsewhere */
    seahorse_tool_progress_update ((done / total) + ((size / total) * (portion / size)), 
                                   seahorse_util_uri_get_last (ctx->cur->uri));
}

gboolean
step_operation (FilesCtx *ctx, SeahorseToolMode *mode, GError **err)
{
    SeahorsePGPOperation *pop = NULL;
    gpgme_data_t data = NULL;
    gboolean ret = FALSE;
    
    SeahorseOperation *op;
    FileInfo *file;
    GList *l;
    
    /* Reset our done counter */
    ctx->done = 0;
    
    for (l = ctx->files; l; l = g_list_next (l)) {
        
        file = (FileInfo*)l->data;
        if (!file || !file->uri)
            continue;
        
        ctx->cur = file;

        /* A new operation for each context */
        pop = seahorse_pgp_operation_new (NULL);
        op = SEAHORSE_OPERATION (pop);
       
        data = seahorse_vfs_data_create_full (file->uri, SEAHORSE_VFS_READ, 
                                              (SeahorseVfsProgressCb)progress_cb, 
                                              ctx, err);
        if (!data)
            goto finally;
        
        /* Inhibit popping up of progress dialog */
        seahorse_tool_progress_block (TRUE);
    
        /* The start callback */
        if (mode->startcb) {
            if (!(mode->startcb) (mode, file->uri, data, pop, err))
                goto finally;
        }
        
        /* Let progress dialog pop up */
        seahorse_tool_progress_block (FALSE);
        
        /* Run until the operation completes */
        seahorse_util_wait_until ((!seahorse_operation_is_running (op) || 
                                   !seahorse_tool_progress_check ()));
        
        if (!seahorse_operation_is_successful (op)) {
            seahorse_operation_steal_error (op, err);
            goto finally;
        }
        
        /* The done callback */
        if (mode->donecb) {
            if (!(mode->donecb) (mode, file->uri, data, pop, err))
                goto finally;
        }
        
        ctx->done += file->size;
        ctx->cur = NULL;
        
        g_object_unref (pop);
        pop = NULL;
        
        gpgmex_data_release (data);
        data = NULL;
    }
    
    seahorse_tool_progress_update (1.0, "");
    ret = TRUE;
    
finally:
    if (pop)
        g_object_unref (pop);
    if (data)
        gpgmex_data_release (data);
    
    return ret;
}

/* -----------------------------------------------------------------------------
 * MAIN COORDINATION
 */

int
seahorse_tool_files_process (SeahorseToolMode *mode, const gchar **uris)
{
    const gchar *errdesc = NULL;
    GError *err = NULL;
    FilesCtx ctx;
    int ret = 1;
    
    memset (&ctx, 0, sizeof (ctx));
    
    /* Start progress bar */
    seahorse_tool_progress_start (mode->title);
    if (!seahorse_tool_progress_update (-1, _("Preparing...")))
        goto finally;
    
    /*
     * 1. Check all the uris, format them properly, and figure
     * out what they are (ie: files, directories, size etc...)
     */
    
    if (!step_check_uris (&ctx, uris, &err)) {
        errdesc = _("Couldn't list files");
        goto finally;
    }
        
    /* 
     * 2. Prompt user and see what they want to do 
     * with multiple files.
     */
    if (mode->package) {
        if (!step_process_multiple (&ctx, uris, &err)) {
            errdesc = _("Couldn't package files");
            goto finally;
        }
    }
    
    if (!seahorse_tool_progress_check ())
        goto finally;

    /* 
     * 2. Expand remaining URIs, so we know what we're 
     * dealing with. This also gets file size info.
     */
    if (!step_expand_uris (&ctx, &err)) {
        errdesc = _("Couldn't list files");
        goto finally;
    }

    if (!seahorse_tool_progress_update (0.0, NULL))
        goto finally;
    
    /* 
     * 3. Now execute enc operation on every file 
     */
    if (!step_operation (&ctx, mode, &err)) {
        errdesc = mode->errmsg;
        goto finally;
    }
    
    seahorse_tool_progress_update (1.0, NULL);
    ret = 0;

finally:

    seahorse_tool_progress_stop ();
    
    if (err) {
        seahorse_util_handle_error (err, errdesc, ctx.cur ? 
                                    seahorse_util_uri_get_last (ctx.cur->uri) : "");
    }
    
    if (ctx.files) {
        g_list_foreach (ctx.files, (GFunc)free_file_info, NULL);
        g_list_free (ctx.files);
    }
    
    return ret;
}
