/*
 * Seahorse
 *
 * Copyright (C) 2006  Stefan Walter
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

#include "seahorse-gpgme-dialogs.h"

#include "seahorse-gpgme-key-op.h"

#include "libseahorse/seahorse-util.h"

#include <glycin.h>

#include <glib/gi18n.h>

#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>

#define DEFAULT_WIDTH    120
#define DEFAULT_HEIGHT   150
#define LARGE_WIDTH      240
#define LARGE_HEIGHT     288

typedef struct _GpgmeAddPhotoClosure {
    GFile *input_file; /* The original input file */
    int width, height;
    GFile *temp_file;  /* Temp file that stores a resized/rewritten image */
    gboolean rewrite;
    gboolean resample;
    GtkWindow *parent_window;
} GpgmeAddPhotoClosure;

static void
gpgme_add_photo_closure_free (void *data)
{
    GpgmeAddPhotoClosure *closure = (GpgmeAddPhotoClosure *) data;

    g_clear_object (&closure->input_file);
    g_clear_object (&closure->temp_file);
    g_free (closure);
}

static gboolean
calc_scale (int *width, int *height)
{
    double ratio, recpx, imgpx;

    recpx = DEFAULT_WIDTH + DEFAULT_HEIGHT;
    imgpx = (*width) + (*height);

    if (imgpx <= recpx)
        return FALSE;

    /* Keep aspect ratio, and don't squash large aspect ratios
     * unnecessarily. */
    ratio = imgpx / recpx;
    *height = ((double) (*height)) / ratio;
    *width = ((double) (*width)) / ratio;
    return TRUE;
}

static GBytes *
scale_frame_with_cairo (GlyFrame *frame,
                        int       new_width,
                        int       new_height,
                        int      *out_stride)
{
    cairo_surface_t *src_surface, *dst_surface;
    cairo_t *cr;
    GBytes *src_bytes;
    int src_width, src_height, src_stride;
    int dst_stride;
    const unsigned char *src_data;
    unsigned char *dst_data;
    GBytes *result;

    src_width = gly_frame_get_width (frame);
    src_height = gly_frame_get_height (frame);
    src_stride = gly_frame_get_stride (frame);
    src_bytes = gly_frame_get_buf_bytes (frame);
    src_data = g_bytes_get_data (src_bytes, NULL);

    /* GLY_MEMORY_B8G8R8A8_PREMULTIPLIED matches CAIRO_FORMAT_ARGB32
     * on little-endian (which is all we support) */
    src_surface = cairo_image_surface_create_for_data (
        (unsigned char *) src_data,
        CAIRO_FORMAT_ARGB32,
        src_width, src_height, src_stride);

    dst_surface = cairo_image_surface_create (
        CAIRO_FORMAT_ARGB32, new_width, new_height);

    cr = cairo_create (dst_surface);
    cairo_scale (cr,
                 (double) new_width / src_width,
                 (double) new_height / src_height);
    cairo_set_source_surface (cr, src_surface, 0, 0);
    cairo_pattern_set_filter (cairo_get_source (cr), CAIRO_FILTER_BILINEAR);
    cairo_paint (cr);
    cairo_destroy (cr);

    cairo_surface_flush (dst_surface);
    dst_stride = cairo_image_surface_get_stride (dst_surface);
    dst_data = cairo_image_surface_get_data (dst_surface);

    result = g_bytes_new (dst_data, (gsize) dst_stride * new_height);
    *out_stride = dst_stride;

    cairo_surface_destroy (dst_surface);
    cairo_surface_destroy (src_surface);

    return result;
}

static void
do_add_photo (GTask *task)
{
    SeahorseGpgmeKey *key = SEAHORSE_GPGME_KEY (g_task_get_source_object (task));
    GpgmeAddPhotoClosure *closure = g_task_get_task_data (task);
    GFile *file;
    g_autofree char *path = NULL;
    gpgme_error_t gerr;

    file = closure->temp_file? closure->temp_file : closure->input_file;
    path = g_file_get_path (file);
    gerr = seahorse_gpgme_key_op_photo_add (key, path);
    if (!GPG_IS_OK (gerr)) {

        /* A special error value set by seahorse_key_op_photoid_add to
           denote an invalid format file */
        if (gerr == GPG_E (GPG_ERR_USER_1))
            g_task_return_new_error_literal (task, SEAHORSE_ERROR, -1,
                                             _("Couldn't add photo: invalid format"));
        else
            g_task_return_new_error_literal (task, SEAHORSE_ERROR, -1,
                                             _("Couldn't add photo: unknown reason"));
        return;
    }

    g_task_return_boolean (task, TRUE);
}

static void
do_rewrite_or_add_photo (GTask *task)
{
    GpgmeAddPhotoClosure *closure = g_task_get_task_data (task);
    GCancellable *cancellable = g_task_get_cancellable (task);
    g_autoptr(GlyLoader) loader = NULL;
    g_autoptr(GlyImage) image = NULL;
    g_autoptr(GlyFrame) frame = NULL;
    g_autoptr(GlyCreator) creator = NULL;
    g_autoptr(GlyEncodedImage) encoded = NULL;
    g_autoptr(GBytes) jpeg_data = NULL;
    g_autoptr(GBytes) scaled_bytes = NULL;
    g_autoptr(GFileIOStream) tmp_iostream = NULL;
    g_autoptr(GError) error = NULL;
    GBytes *frame_bytes;
    uint32_t width, height, stride;
    GlyMemoryFormat format;

    if (!closure->rewrite) {
        do_add_photo (task);
        return;
    }

    /* Load the image */
    loader = gly_loader_new (closure->input_file);
    gly_loader_set_accepted_memory_formats (loader,
        GLY_MEMORY_SELECTION_B8G8R8A8_PREMULTIPLIED);

    image = gly_loader_load (loader, &error);
    if (image == NULL) {
        g_prefix_error (&error, "Couldn't load image: ");
        g_task_return_error (task, g_steal_pointer (&error));
        return;
    }

    frame = gly_image_next_frame (image, &error);
    if (frame == NULL) {
        g_prefix_error (&error, "Couldn't decode image: ");
        g_task_return_error (task, g_steal_pointer (&error));
        return;
    }

    frame_bytes = gly_frame_get_buf_bytes (frame);
    width = gly_frame_get_width (frame);
    height = gly_frame_get_height (frame);
    stride = gly_frame_get_stride (frame);
    format = gly_frame_get_memory_format (frame);

    /* Resize if needed */
    if (closure->resample && calc_scale (&closure->width, &closure->height)) {
        int scaled_stride;

        scaled_bytes = scale_frame_with_cairo (frame,
                                               closure->width,
                                               closure->height,
                                               &scaled_stride);
        frame_bytes = scaled_bytes;
        width = closure->width;
        height = closure->height;
        stride = scaled_stride;
        /* format stays B8G8R8A8_PREMULTIPLIED from Cairo */
    }

    /* Encode as JPEG */
    creator = gly_creator_new ("image/jpeg", &error);
    if (creator == NULL) {
        g_prefix_error (&error, "Couldn't create JPEG encoder: ");
        g_task_return_error (task, g_steal_pointer (&error));
        return;
    }
    gly_creator_set_encoding_quality (creator, 75);

    if (gly_creator_add_frame_with_stride (creator,
                                           width, height, stride,
                                           format, frame_bytes,
                                           &error) == NULL) {
        g_prefix_error (&error, "Couldn't add frame for JPEG encoding: ");
        g_task_return_error (task, g_steal_pointer (&error));
        return;
    }

    encoded = gly_creator_create (creator, &error);
    if (encoded == NULL) {
        g_prefix_error (&error, "Couldn't encode JPEG: ");
        g_task_return_error (task, g_steal_pointer (&error));
        return;
    }

    jpeg_data = gly_encoded_image_get_data (encoded);

    /* Write JPEG to temp file */
    closure->temp_file = g_file_new_tmp ("seahorse-photo.XXXXXX",
                                         &tmp_iostream,
                                         &error);
    if (closure->temp_file == NULL) {
        g_prefix_error (&error, "Couldn't open temporary file: ");
        g_task_return_error (task, g_steal_pointer (&error));
        return;
    }

    if (!g_output_stream_write_bytes (
            g_io_stream_get_output_stream (G_IO_STREAM (tmp_iostream)),
            jpeg_data, cancellable, &error)) {
        g_prefix_error (&error, "Couldn't write resized image: ");
        g_task_return_error (task, g_steal_pointer (&error));
        return;
    }

    do_add_photo (task);
}

static void
on_suggest_resize_chosen (GObject      *source_object,
                          GAsyncResult *result,
                          void         *user_data)
{
    AdwAlertDialog *dialog = ADW_ALERT_DIALOG (source_object);
    g_autoptr(GTask) task = G_TASK (user_data);
    GpgmeAddPhotoClosure *closure = g_task_get_task_data (task);
    const char *response;

    response = adw_alert_dialog_choose_finish (dialog, result);
    if (g_strcmp0 (response, "resize") == 0) {
        closure->resample = TRUE;
        closure->rewrite = TRUE;
        do_rewrite_or_add_photo (task);
    } else if (g_strcmp0 (response, "no-resize") == 0) {
        closure->resample = FALSE;
        do_rewrite_or_add_photo (task);
    } else {
        g_task_return_new_error_literal (task,
                                         G_IO_ERROR,
                                         G_IO_ERROR_CANCELLED,
                                         "Cancelled by user");
    }
}

static void
suggest_resize (GTask *task)
{
    AdwDialog *dialog;
    GpgmeAddPhotoClosure *closure = g_task_get_task_data (task);

    dialog = adw_alert_dialog_new (_("The photo is too large"), NULL);

    adw_alert_dialog_format_body (ADW_ALERT_DIALOG (dialog),
                                 _("The recommended size for a photo on your key is %d × %d pixels"),
                                 DEFAULT_WIDTH, DEFAULT_HEIGHT);

    adw_alert_dialog_add_responses (ADW_ALERT_DIALOG (dialog),
                                    "cancel",  _("_Cancel"),
                                    "no-resize", _("_Don't Resize"),
                                    "resize", _("_Resize"),
                                    NULL);
    adw_alert_dialog_set_response_appearance (ADW_ALERT_DIALOG (dialog),
                                              "resize",
                                              ADW_RESPONSE_SUGGESTED);
    adw_alert_dialog_set_default_response (ADW_ALERT_DIALOG (dialog), "resize");
    adw_alert_dialog_set_close_response (ADW_ALERT_DIALOG (dialog), "cancel");

    adw_alert_dialog_choose (ADW_ALERT_DIALOG (dialog),
                             GTK_WIDGET (closure->parent_window),
                             NULL,
                             on_suggest_resize_chosen,
                             g_object_ref (task));
}

gboolean
seahorse_gpgme_photo_add_finish (SeahorseGpgmeKey *pkey,
                                 GAsyncResult     *result,
                                 GError          **error)
{
    g_return_val_if_fail (g_task_is_valid (result, pkey), FALSE);

    return g_task_propagate_boolean (G_TASK (result), error);
}

static void
on_gpgme_photo_file_opened (GObject      *source_object,
                            GAsyncResult *result,
                            void         *user_data)
{
    GtkFileDialog *file_dialog = GTK_FILE_DIALOG (source_object);
    g_autoptr(GTask) task = G_TASK (user_data);
    GpgmeAddPhotoClosure *closure = g_task_get_task_data (task);
    g_autoptr(GError) error = NULL;
    g_autoptr(GlyLoader) loader = NULL;
    g_autoptr(GlyImage) image = NULL;
    const char *mime_type;
    gboolean suggest = FALSE;

    closure->input_file = gtk_file_dialog_open_finish (file_dialog, result, &error);
    if (closure->input_file == NULL) {
        g_task_return_error (task, g_steal_pointer (&error));
        return;
    }

    /* Probe the image format and dimensions */
    loader = gly_loader_new (closure->input_file);
    image = gly_loader_load (loader, &error);
    if (image == NULL) {
        g_task_return_new_error (task, SEAHORSE_ERROR, -1,
                                 _("This is not a image file, or an unrecognized kind of image file. Try to use a JPEG image."));
        return;
    }

    closure->width = gly_image_get_width (image);
    closure->height = gly_image_get_height (image);
    mime_type = gly_image_get_mime_type (image);

    /* Check if it's a JPEG */
    if (g_strcmp0 (mime_type, "image/jpeg") == 0) {
        g_autofree char *path = g_file_get_path (closure->input_file);
        struct stat sb;

        /* If so we may just be able to use it straight up */
        if (stat (path, &sb) != -1) {
            /* Large file size, suggest resampling */
            if (sb.st_size > 8192)
                suggest = TRUE;
        }
    } else {
        /* Other formats */
        closure->rewrite = TRUE;

        /* Check for large, but allow strange orientations */
        if ((closure->width + closure->height) > (LARGE_WIDTH + LARGE_HEIGHT))
            suggest = TRUE;
    }

    /* Suggest to the user that we resize the photo */
    if (suggest) {
        suggest_resize (task);
    } else {
        do_rewrite_or_add_photo (task);
    }
}

void
seahorse_gpgme_photo_add (SeahorseGpgmeKey    *key,
                          GtkWindow           *parent,
                          GCancellable        *cancellable,
                          GAsyncReadyCallback  callback,
                          void                *user_data)
{
    g_autoptr(GTask) task = NULL;
    GpgmeAddPhotoClosure *closure;
    g_autoptr(GtkFileDialog) file_dialog = NULL;
    g_autoptr(GListStore) filters = NULL;
    g_autoptr(GtkFileFilter) mime_filter = NULL;
    g_autoptr(GtkFileFilter) jpeg_filter = NULL;
    g_autoptr(GtkFileFilter) all_filter = NULL;
    g_auto(GStrv) mime_types = NULL;

    g_return_if_fail (SEAHORSE_GPGME_IS_KEY (key));

    task = g_task_new (key, cancellable, callback, user_data);
    closure = g_new0 (GpgmeAddPhotoClosure, 1);
    closure->parent_window = parent;
    g_task_set_task_data (task, closure, gpgme_add_photo_closure_free);

    file_dialog = gtk_file_dialog_new ();
    gtk_file_dialog_set_title (file_dialog, _("Choose Photo to Add to Key"));

    /* Add file filters */
    filters = g_list_store_new (GTK_TYPE_FILE_FILTER);

    mime_filter = gtk_file_filter_new ();
    gtk_file_filter_set_name (mime_filter, _("All image files"));
    mime_types = gly_loader_get_mime_types ();
    for (char **t = mime_types; t && *t; t++)
        gtk_file_filter_add_mime_type (mime_filter, *t);
    g_list_store_append (filters, mime_filter);

    jpeg_filter = gtk_file_filter_new ();
    gtk_file_filter_set_name (jpeg_filter, _("All JPEG files"));
    gtk_file_filter_add_mime_type (jpeg_filter, "image/jpeg");
    g_list_store_append (filters, jpeg_filter);

    all_filter = gtk_file_filter_new ();
    gtk_file_filter_set_name (all_filter, _("All files"));
    gtk_file_filter_add_pattern (all_filter, "*");
    g_list_store_append (filters, all_filter);

    gtk_file_dialog_set_filters (file_dialog, G_LIST_MODEL (filters));

    gtk_file_dialog_open (file_dialog, parent, cancellable,
                          on_gpgme_photo_file_opened, g_steal_pointer (&task));
}
