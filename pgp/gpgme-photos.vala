/*
 * Seahorse
 *
 * Copyright (C) 2006  Stefan Walter
 * Copyright (C) 2017 Niels De Graef
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

// XXX namespace or class (or part of Photo?)
namespace Seahorse.GpgME.Photos {
public const string DEFAULT_WIDTH = 120;
public const string DEFAULT_HEIGHT = 150;
public const string LARGE_WIDTH = 240;
public const string LARGE_HEIGHT = 288;

public bool add(Key pkey, Gtk.Window? parent, string? path) {
    GError *error = null;
    gpgme_error_t gerr;
    bool res = true;

    string? filename = path;
    if (filename == null) {
        Gtk.FileChooserDialog chooser =
            new Gtk.FileChooserDialog(_("Choose Photo to Add to Key"), parent,
                                      Gtk.FileChooserAction.OPEN,
                                      Gtk.Stock.CANCEL, Gtk.ResponseType.CANCEL,
                                      Gtk.Stock.OPEN, Gtk.ResponseType.ACCEPT,
                                      null);

        chooser.set_default_response(Gtk.ResponseType.ACCEPT);
        chooser.set_local_only(true);
        add_image_files (chooser);

        if (chooser.run() == Gtk.ResponseType.ACCEPT)
            filename = chooser.get_filename();

        chooser.destroy();

        if (!filename)
            return false;
    }

    string? tempfile = null;
    try {
        prepare_photo_id(parent, filename, out tempfile);
    } catch (GLib.Error e) {
        Util.show_error(null, _("Couldn’t prepare photo"), e.message);
        return false;
    }

    GPG.Error gerr = GpgME.KeyOperation.photo_add(pkey, tempfile ?? filename);
    if (!gerr.is_success()) {
        // A special error value set by seahorse_key_op_photoid_add to
        // denote an invalid format file
        if (gerr.error_code() == GPGError.ErrorCode.USER_1)
            Util.show_error(null, _("Couldn’t add photo"),
                            _("The file could not be loaded. It may be in an invalid format"));
        else
            Util.show_error(null, _("Couldn’t add photo"), gerr.strerror());
        res = false;
    }

    if (tempfile)
        unlink (tempfile);

    return res;
}

public bool delete(Photo photo, Gtk.Window? parent) {
    Gtk.Dialog dialog = new Gtk.MessageDialog(parent, Gtk.DialogFlags.MODAL,
                                              Gtk.MessageType.QUESTION, Gtk.ButtonsType.NONE,
                                              _("Are you sure you want to remove the current photo from your key?"));

    dialog.add_button(Gtk.Stock.DELETE, Gtk.ResponseType.ACCEPT);
    dialog.add_button(Gtk.Stock.CANCEL, Gtk.ResponseType.REJECT);

    int response = dialog.run();
    dialog.destroy();

    if (response != Gtk.ResponseType.ACCEPT)
        return false;

    GPG.Error gerr = GpgME.KeyOperation.photo_delete(photo);
    if (!gerr.is_success() || gerr.is_cancelled()) {
        Util.show_error(null, _("Couldn’t delete photo"), gerr.strerror());
        return false;
    }

    return true;
}

private bool calc_scale(out int width, out int height) {
    double recpx = DEFAULT_WIDTH + DEFAULT_HEIGHT;
    double imgpx = width + height;

    if (imgpx <= recpx)
        return false;

    // Keep aspect ratio, and don't squash large aspect ratios unnecessarily.
    double ratio = imgpx / recpx;
    height = ((gdouble)(*height)) / ratio;
    width = ((gdouble)(*width)) / ratio;
    return true;
}

private guint suggest_resize (Gtk.Window? parent) {
    Gtk.Dialog dialog = new Gtk.MessageDialog.with_markup(parent,
                Gtk.DialogFlags.MODAL | Gtk.DialogFlags.DESTROY_WITH_PARENT,
                Gtk.MessageType.QUESTION, Gtk.ButtonsType.NONE,
                _("<big><b>The photo is too large</b></big>\nThe recommended size for a photo on your key is %d × %d pixels."),
                DEFAULT_WIDTH, DEFAULT_HEIGHT);

    dialog.add_buttons(Gtk.Stock.CANCEL, Gtk.ResponseType.CANCEL,
                       _("_Don’t Resize"), Gtk.ResponseType.REJECT,
                       _("_Resize"), Gtk.ResponseType.ACCEPT);

    uint response = dialog.run();
    dialog.destroy();

    return response;
}

private bool save_to_fd(string buf, gsize count, GError **error, gpointer data) {
    int fd = GPOINTER_TO_INT (data);
    gssize written;

    written = write (fd, buf, count);
    if (written != (gssize) count) {
        g_set_error (error, G_FILE_ERROR, g_file_error_from_errno (errno),
                     "%s", g_strerror (errno));
        return false;
    }

    return true;
}

private bool prepare_photo_id (Gtk.Window parent, string path, out string result) throws GLib.Error {
    GdkPixbuf *pixbuf = null;
    GdkPixbuf *sampled;
    bool rewrite = false;
    bool resample = false;
    bool suggest = false;
    int fd;

    assert (path);
    assert (result);
    assert (!error || !*error);

    result = null;

    int width, height;
    Gdk.PixbufFormat? format = Gdk.Pixbuf.get_file_info(path, out width, out height);
    if (!format) {
        g_set_error (error, SEAHORSE_ERROR, -1,
                     _("This is not a image file, or an unrecognized kind of image file. Try to use a JPEG image."));
        return false;
    }

    // JPEGs we can use straight up
    if (format.get_name() == "jpeg") {
        // If so we may just be able to use it straight up
        Posix.stat sb;
        if (stat (path, &sb) != -1) {

            // Large file size, suggest resampling
            if (sb.st_size > 8192)
                suggest = true;
        }

    // Other formats
    } else {
        rewrite = true;

        // Check for large, but allow strange orientations
        if ((width + height) > (LARGE_WIDTH + LARGE_HEIGHT))
            suggest = true;
    }

    // Suggest to the user that we resize the photo
    if (suggest) {
        switch (suggest_resize (parent)) {
        case Gtk.ResponseType.ACCEPT:
            resample = true;
            rewrite = true;
            break;
        case Gtk.ResponseType.REJECT:
            resample = false;
            break;
        default:
            // false with error not set == cancel
            return false;
        }
    }

    /* No rewrite */
    if (!rewrite)
        return true;

    // Load the photo if necessary
    pixbuf = new Gdk.Pixbuf.from_file(path, error);
    if (!pixbuf)
        return false;

    /* Resize it properly */
    if (resample && calc_scale(out width, out height)) {
        sampled = pixbuf.scale_simple(width, height, GDK_INTERP_BILINEAR);

        g_return_val_if_fail (sampled != null, false);
        pixbuf = sampled;
        sampled = null;
    }

    /* And write it out to a temp */
    fd = g_file_open_tmp ("seahorse-photo.XXXXXX", result, error);
    if (fd == -1) {
        g_object_unref (pixbuf);
        return false;
    }

    bool r = pixbuf.save_to_callback(save_to_fd, GINT_TO_POINTER (fd),
                                "jpeg", error, "quality", "75", null);

    close (fd);

    if (!r) {
        g_free (*result);
        *result = null;
        return false;
    }

    return true;
}

private void add_image_files(Gtk.Widget dialog) {
    Gtk.FileFilter filter = new Gtk.FileFilter();
    filter.set_name(filter, _("All image files"));
    Gdk.Pixbuf.get_formats().foreach((format) => {
        foreach (string mime_type in format.get_mime_types())
            filter.add_mime_type(mime_type);
    });

    dialog.add_filter(filter);
    dialog.set_filter(filter);

    filter = new Gtk.FileFilter();
    filter.set_name(filter, _("All JPEG files"));
    filter.add_mime_type(filter, "image/jpeg");
    dialog.add_filter(filter);

    filter = new Gtk.FileFilter();
    filter.set_name(filter, _("All files"));
    filter.add_pattern(filter, "*");
    dialog.add_filter(filter);
}
}
