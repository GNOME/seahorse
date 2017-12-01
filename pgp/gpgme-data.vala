/*
 * Seahorse
 *
 * Copyright (C) 2008 Stefan Walter
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

namespace Seahorse.GpgME.Data {

private int handle_gio_error (GLib.Error err) {
    g_return_val_if_fail (err, -1);

    if (err.message)
        message (err.message);

    switch (err->code) {
        case G_IO_ERROR_FAILED:
            errno = EIO;
            break;
        case G_IO_ERROR_NOT_FOUND:
            errno = ENOENT;
            break;
        case G_IO_ERROR_EXISTS:
            errno = EEXIST;
            break;
        case G_IO_ERROR_IS_DIRECTORY:
            errno = EISDIR;
            break;
        case G_IO_ERROR_NOT_DIRECTORY:
            errno = ENOTDIR;
            break;
        case G_IO_ERROR_NOT_EMPTY:
            errno = ENOTEMPTY;
            break;
        case G_IO_ERROR_NOT_REGULAR_FILE:
        case G_IO_ERROR_NOT_SYMBOLIC_LINK:
        case G_IO_ERROR_NOT_MOUNTABLE_FILE:
            errno = EBADF;
            break;
        case G_IO_ERROR_FILENAME_TOO_LONG:
            errno = ENAMETOOLONG;
            break;
        case G_IO_ERROR_INVALID_FILENAME:
            errno = EINVAL;
            break;
        case G_IO_ERROR_TOO_MANY_LINKS:
            errno = EMLINK;
            break;
        case G_IO_ERROR_NO_SPACE:
            errno = ENOSPC;
            break;
        case G_IO_ERROR_INVALID_ARGUMENT:
            errno = EINVAL;
            break;
        case G_IO_ERROR_PERMISSION_DENIED:
            errno = EPERM;
            break;
        case G_IO_ERROR_NOT_SUPPORTED:
            errno = ENOTSUP;
            break;
        case G_IO_ERROR_NOT_MOUNTED:
            errno = ENOENT;
            break;
        case G_IO_ERROR_ALREADY_MOUNTED:
            errno = EALREADY;
            break;
        case G_IO_ERROR_CLOSED:
            errno = EBADF;
            break;
        case G_IO_ERROR_CANCELLED:
            errno = EINTR;
            break;
        case G_IO_ERROR_PENDING:
            errno = EALREADY;
            break;
        case G_IO_ERROR_READ_ONLY:
            errno = EACCES;
            break;
        case G_IO_ERROR_CANT_CREATE_BACKUP:
            errno = EIO;
            break;
        case G_IO_ERROR_WRONG_ETAG:
            errno = EACCES;
            break;
        case G_IO_ERROR_TIMED_OUT:
            errno = EIO;
            break;
        case G_IO_ERROR_WOULD_RECURSE:
            errno = ELOOP;
            break;
        case G_IO_ERROR_BUSY:
            errno = EBUSY;
            break;
        case G_IO_ERROR_WOULD_BLOCK:
            errno = EWOULDBLOCK;
            break;
        case G_IO_ERROR_HOST_NOT_FOUND:
            errno = EHOSTDOWN;
            break;
        case G_IO_ERROR_WOULD_MERGE:
            errno = EIO;
            break;
        case G_IO_ERROR_FAILED_HANDLED:
            errno = 0;
            break;
        default:
            errno = EIO;
            break;
    };

    return -1;
}

/* ----------------------------------------------------------------------------------------
 * OUTPUT
 */

/* Called by gpgme to read data */
private ssize_t output_write(void *handle, void *buffer, size_t size) {
    GLib.OutputStream output = handle;
    GError *err = null;
    gsize written;

    g_return_val_if_fail (G_IS_OUTPUT_STREAM (output), -1);

    if (!g_output_stream_write_all (output, buffer, size, &written, null, &err))
        return handle_gio_error (err);

    if (!g_output_stream_flush (output, null, &err))
        return handle_gio_error (err);

    return written;
}

/* Called from gpgme to seek a file */
private Posix.off_t output_seek (void *handle, Posix.off_t offset, int whence) {
    GError *err = null;
    GLib.OutputStream output = handle;

    g_return_val_if_fail (G_IS_OUTPUT_STREAM (output), -1);

    if (!G_IS_SEEKABLE (output)) {
        errno = EOPNOTSUPP;
        return -1;
    }

    GSeekType from = 0;
    switch(whence) {
        case SEEK_SET:
            from = G_SEEK_SET;
            break;
        case SEEK_CUR:
            from = G_SEEK_CUR;
            break;
        case SEEK_END:
            from = G_SEEK_END;
            break;
        default:
            g_assert_not_reached();
            break;
    };

    Seekable seek = G_SEEKABLE (output);
    if (!g_seekable_seek (seek, offset, from, null, &err))
        return handle_gio_error (err);

    return offset;
}

/* Called by gpgme to close a file */
private void output_release (void *handle) {
    GLib.OutputStream output = handle;
    g_return_if_fail (G_IS_OUTPUT_STREAM (output));
    
    g_object_unref (output);
}

/* GPGME vfs file operations */
gpgme_data_cbs output_cbs =  {
    null,
    output_write,
    output_seek,
    output_release
};

public gpgme_data_t seahorse_gpgme_data_output (GLib.OutputStream output) {
    GPGError.ErrorCode gerr;
    gpgme_data_t ret = null;

    g_return_val_if_fail (G_IS_OUTPUT_STREAM (output), null);
    
    gerr = gpgme_data_new_from_cbs (&ret, &output_cbs, output);
    if (!gerr.is_success())
        return null;
    
    g_object_ref (output);
    return ret;
}

/* -------------------------------------------------------------------------------------
 * INPUT STREAMS
 */

/* Called by gpgme to read data */
private ssize_t input_read (void *handle, void *buffer, size_t size) {
    GLib.InputStream input = handle;
    GError *err = null;
    gsize nread;
    
    g_return_val_if_fail (G_IS_INPUT_STREAM (input), -1);
    
    if (!g_input_stream_read_all (input, buffer, size, &nread, null, &err))
        return handle_gio_error (err);
    
    return nread;
}

/* Called from gpgme to seek a file */
private Posix.off_t input_seek (void *handle, Posix.off_t offset, int whence) {
    GLib.Seekable seek;
    GSeekType from = 0;
    GError *err = null;
    GLib.InputStream input = handle;

    g_return_val_if_fail (G_IS_INPUT_STREAM (input), -1);

    if (!G_IS_SEEKABLE (input)) {
        errno = EOPNOTSUPP;
        return -1;
    }
    
    switch(whence)
    {
    case SEEK_SET:
        from = G_SEEK_SET;
        break;
    case SEEK_CUR:
        from = G_SEEK_CUR;
        break;
    case SEEK_END:
        from = G_SEEK_END;
        break;
    default:
        g_assert_not_reached();
        break;
    };

    seek = G_SEEKABLE (input);
    if (!g_seekable_seek (seek, offset, from, null, &err))
        return handle_gio_error (err);

    return offset;
}

/* Called by gpgme to close a file */
private void input_release(void *handle) {
    GLib.InputStream input = handle;
    g_return_if_fail (G_IS_INPUT_STREAM (input));
    
    g_object_unref (input);
}

/* GPGME vfs file operations */
static gpgme_data_cbs input_cbs =  {
    input_read,
    null,
    input_seek,
    input_release
};

gpgme_data_t seahorse_gpgme_data_input (GLib.InputStream input) {
    gpgme_data_t ret = null;

    g_return_val_if_fail (G_IS_INPUT_STREAM (input), null);

    GPGError.ErrorCode gerr = gpgme_data_new_from_cbs (&ret, &input_cbs, input);
    if (!gerr.is_success())
        return null;

    g_object_ref (input);
    return ret;
}

gpgme_data_t seahorse_gpgme_data_new () {
    gpgme_data_t data;

    GPGError.ErrorCode gerr = gpgme_data_new (&data);
    if (!gerr.is_success()) {
        if (gpgme_err_code_to_errno (gerr) == ENOMEM ||
            gpgme_err_code (gerr) == GPG_ERR_ENOMEM) {

            g_error ("%s: failed to allocate gpgme_data_t", G_STRLOC);

        } else {
            /* The only reason this should fail is above */
            g_assert_not_reached ();

            /* Just in case */
            abort ();
        }
    }

    return data;
}

gpgme_data_t seahorse_gpgme_data_new_from_mem (string buffer, size_t size, bool copy) {
    gpgme_data_t data;

    GPGError.ErrorCode gerr = gpgme_data_new_from_mem (&data, buffer, size, copy ? 1 : 0);
    if (!gerr.is_success()) {
        if (gpgme_err_code_to_errno (gerr) == ENOMEM ||
            gpgme_err_code (gerr) == GPG_ERR_ENOMEM) {

            g_error ("%s: failed to allocate gpgme_data_t", G_STRLOC);

        } else {
            /* The only reason this should fail is above */
            g_assert_not_reached ();

            /* Just in case */
            abort ();
        }
    }

    return data;
}

public int seahorse_gpgme_data_write_all(gpgme_data_t data, void* buffer, size_t len) {
    guchar *text = (guchar*)buffer;
    int written = 0;

    if (len < 0)
        len = strlen ((gchar*)text);

    while (len > 0) {
        written = gpgme_data_write (data, (void*)text, len);
        if (written < 0) {
            if (errno == EAGAIN || errno == EINTR)
                continue;
            return -1;
        }

        len -= written;
        text += written;
    }

    return written;
}

public void seahorse_gpgme_data_release (gpgme_data_t data) {
    if (data)
        gpgme_data_release (data);
}
}
