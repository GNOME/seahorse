/*
 * Seahorse
 *
 * Copyright (C) 2006  Nate Nielsen
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

#include <sys/stat.h>
#include <errno.h>

#include <gnome.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include "seahorse-widget.h"
#include "seahorse-util.h"
#include "seahorse-key-dialogs.h"
#include "seahorse-pgp-key-op.h"

#define DEFAULT_WIDTH    120
#define DEFAULT_HEIGHT   150
#define LARGE_WIDTH      240
#define LARGE_HEIGHT     288

static gboolean
calc_scale (gint *width, gint *height)
{
    gdouble ratio, recpx, imgpx;
    
    recpx = DEFAULT_WIDTH + DEFAULT_HEIGHT;
    imgpx = (*width) + (*height);
    
    if (imgpx <= recpx)
        return FALSE;
    
    /* 
     * Keep aspect ratio, and don't squash large aspect ratios
     * unnecessarily.
     */
    ratio = imgpx / recpx;
    *height = ((gdouble)(*height)) / ratio;
    *width = ((gdouble)(*width)) / ratio;
    return TRUE;
}

static guint 
suggest_resize (GtkWindow *parent)
{
    GtkWidget *dlg;
    guint response;
    
    dlg = gtk_message_dialog_new_with_markup (parent, 
                GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                GTK_MESSAGE_QUESTION, GTK_BUTTONS_NONE, 
                _("<big><b>The photo is too large</b></big>\nThe recommended size for a photo on your key is %d x %d pixels."),
                DEFAULT_WIDTH, DEFAULT_HEIGHT);
    
    gtk_dialog_add_buttons (GTK_DIALOG (dlg), 
                            GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                            _("_Don't Resize"), GTK_RESPONSE_REJECT,
                            _("_Resize"), GTK_RESPONSE_ACCEPT,
                            NULL);
    
    response = gtk_dialog_run (GTK_DIALOG (dlg));
    gtk_widget_destroy (dlg);
    
    return response;
}

static gboolean
save_to_fd (const gchar *buf, gsize count, GError **error, gpointer data)
{
    int fd = GPOINTER_TO_INT (data);
    int written;
    
    written = write (fd, buf, count);
    if (written != count) {
        g_set_error (error, G_FILE_ERROR, g_file_error_from_errno (errno), 
                     "%s", strerror (errno));
        return FALSE;
    }
    
    return TRUE;
}

gboolean
prepare_photo_id (GtkWindow *parent, gchar *path, gchar **result, GError **error)
{
    GdkPixbuf *pixbuf = NULL;
    GdkPixbuf *sampled;
    GdkPixbufFormat *format;
    gint width, height;
    gboolean rewrite = FALSE;
    gboolean resample = FALSE;
    gboolean suggest = FALSE;
    gboolean r;
    gchar *name;
    struct stat sb;
    int fd;
    
    g_assert (path);
    g_assert (result);
    g_assert (!error || !*error);
    
    *result = NULL;
   
    format = gdk_pixbuf_get_file_info (path, &width, &height);
    if (!format) {
        g_set_error (error, SEAHORSE_ERROR, -1, 
                     _("This is not a image file, or an unrecognized kind of image file. Try to use a JPEG image."));
        return FALSE;
    }
    
    /* Check if it's a JPEG */
    name = gdk_pixbuf_format_get_name (format);
    r = strcmp (name, "jpeg") == 0;
    g_free (name);
    
    /* JPEGs we can use straight up */
    if (r) {
        
        /* If so we may just be able to use it straight up */
        if (stat (path, &sb) != -1) {
            
            /* Large file size, suggest resampling */
            if (sb.st_size > 8192) 
                suggest = TRUE;
        }
        
    /* Other formats */
    } else {
        rewrite = TRUE;
        
        /* Check for large, but allow strange orientations */
        if ((width + height) > (LARGE_WIDTH + LARGE_HEIGHT))
            suggest = TRUE;
    }
    
    /* Suggest to the user that we resize the photo */
    if (suggest) {
        switch (suggest_resize (parent)) {
        case GTK_RESPONSE_ACCEPT:
            resample = TRUE;
            rewrite = TRUE;
            break;
        case GTK_RESPONSE_REJECT:
            resample = FALSE;
            break;
        default:
            /* FALSE with error not set = cancel */
            return FALSE;
        }
    }
    
    /* No rewrite */
    if (!rewrite)
        return TRUE;

    /* Load the photo if necessary */
    pixbuf = gdk_pixbuf_new_from_file (path, error);
    if (!pixbuf)
        return FALSE;
    
    /* Resize it properly */
    if (resample && calc_scale (&width, &height)) {
        sampled = gdk_pixbuf_scale_simple (pixbuf, width, height, 
                                           GDK_INTERP_BILINEAR);
        g_object_unref (pixbuf);
        
        g_return_val_if_fail (sampled != NULL, FALSE);
        pixbuf = sampled;
        sampled = NULL;
    }
    
    /* And write it out to a temp */
    fd = g_file_open_tmp ("seahorse-photo.XXXXXX", result, error);
    if (fd == -1) {
        g_object_unref (pixbuf);
        return FALSE;
    }
    
    r = gdk_pixbuf_save_to_callback (pixbuf, save_to_fd, GINT_TO_POINTER (fd),
                                     "jpeg", error, "quality", "75", NULL);
    
    close (fd);
    g_object_unref (pixbuf);
    
    if (!r) {
        g_free (*result);
        *result = NULL;
        return FALSE;
    }
    
    return TRUE;
}


static void
add_image_files (GtkWidget *dialog)
{
    GtkFileFilter* filter;
    GSList *formats, *l;
    gchar **mimes, **t;
    
    filter = gtk_file_filter_new ();
    gtk_file_filter_set_name (filter, _("All image files"));
    formats = gdk_pixbuf_get_formats ();
    for (l = formats; l; l = g_slist_next (l)) {
        mimes = gdk_pixbuf_format_get_mime_types ((GdkPixbufFormat*)l->data);
        for (t = mimes; *t; t++)
            gtk_file_filter_add_mime_type (filter, *t);
        g_strfreev (mimes);
    }
    g_slist_free (formats);
    gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), filter);
    gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (dialog), filter);
    
    filter = gtk_file_filter_new ();
    gtk_file_filter_set_name (filter, _("All JPEG files"));
    gtk_file_filter_add_mime_type (filter, "image/jpeg");
    gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), filter);

    filter = gtk_file_filter_new ();
    gtk_file_filter_set_name (filter, _("All files"));
    gtk_file_filter_add_pattern (filter, "*");    
    gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), filter);
}   


gboolean
seahorse_photo_add (SeahorsePGPKey *pkey, GtkWindow *parent)
{
    gchar *filename = NULL;
    gchar *tempfile = NULL;
    GError *error = NULL;
    gpgme_error_t gerr;
    GtkWidget *chooser;
    gboolean res = TRUE;
    
    g_return_val_if_fail (SEAHORSE_IS_PGP_KEY (pkey), FALSE);

    chooser = seahorse_util_chooser_open_new (_("Choose Photo to Add to Key"), parent);
    gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER (chooser), TRUE);
    add_image_files (chooser);

    if (gtk_dialog_run (GTK_DIALOG (chooser)) == GTK_RESPONSE_ACCEPT)
        filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (chooser));
        
    gtk_widget_destroy (chooser);

    if (!filename)
        return FALSE;
    
    if (!prepare_photo_id (parent, filename, &tempfile, &error)) {
        if (error)
            seahorse_util_handle_error (error, "Couldn't prepare photo");
        return FALSE;
    }
    
    gerr = seahorse_pgp_key_op_photoid_add (pkey, tempfile ? tempfile : filename);
    if (!GPG_IS_OK (gerr)) {
        
        /* A special error value set by seahorse_key_op_photoid_add to 
           denote an invalid format file */
        if (gerr == GPG_E (GPG_ERR_USER_1)) 
            seahorse_util_show_error (NULL, _("Couldn't add photo"), 
                                      _("The file could not be loaded. It may be in an invalid format"));
        
        else
            seahorse_util_handle_gpgme (gerr, _("Couldn't add photo"));

        res = FALSE;
    }
    
    g_free (filename);
    
    if (tempfile) {
        unlink (tempfile);
        g_free (tempfile);
    }
    
    return res;
}

gboolean
seahorse_photo_delete (SeahorsePGPKey *pkey, gpgmex_photo_id_t photo, 
                       GtkWindow *parent)
{
    gpgme_error_t gerr;
    GtkWidget *dlg;
    gint response; 

    g_return_val_if_fail (SEAHORSE_IS_PGP_KEY (pkey), FALSE);
    g_return_val_if_fail (photo != NULL, FALSE);
    
    dlg = gtk_message_dialog_new (parent, GTK_DIALOG_MODAL,
                                  GTK_MESSAGE_QUESTION, GTK_BUTTONS_NONE,
                                  _("Are you sure you want to remove the current photo from your key?"));

    gtk_dialog_add_button (GTK_DIALOG (dlg), GTK_STOCK_DELETE, GTK_RESPONSE_ACCEPT);
    gtk_dialog_add_button (GTK_DIALOG (dlg), GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT);
       
    response = gtk_dialog_run (GTK_DIALOG (dlg));
    gtk_widget_destroy (dlg);
    
    if (response != GTK_RESPONSE_ACCEPT)
        return FALSE;
    
    gerr = seahorse_pgp_key_op_photoid_delete (pkey, photo->uid);
    if (!GPG_IS_OK (gerr)) {
        seahorse_util_handle_gpgme (gerr, _("Couldn't delete photo"));
        return FALSE;
    }
    
    return TRUE;
}
