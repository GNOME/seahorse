/*
 * Seahorse
 *
 * Copyright (C) 2008 Stefan Walter
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

#include "seahorse-gpgme-io.h"

#include "seahorse-gpgmex.h"

#include <glib.h>
#include <gio/gio.h>

#include <gpgme.h>

#include <sys/types.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>

static int
handle_gio_error (GError *err)
{
	g_return_val_if_fail (err, -1);
	
	if (err->message)
		g_message ("%s", err->message);
	
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
	
	g_error_free (err);
	return -1;
}

/* ----------------------------------------------------------------------------------------
 * OUTPUT 
 */

/* Called by gpgme to read data */
static ssize_t
output_write(void *handle, const void *buffer, size_t size)
{
	GOutputStream* output = handle;
	GError *err = NULL;
	gsize written;
	
	g_return_val_if_fail (G_IS_OUTPUT_STREAM (output), -1);
	
	if (!g_output_stream_write_all (output, buffer, size, &written, NULL, &err))
		return handle_gio_error (err);
	
	if (!g_output_stream_flush (output, NULL, &err))
		return handle_gio_error (err);
		
	return written;
}

/* Called from gpgme to seek a file */
static off_t
output_seek (void *handle, off_t offset, int whence)
{
	GSeekable *seek;
	GSeekType from;
	GError *err = NULL;
	GOutputStream* output = handle;

	g_return_val_if_fail (G_IS_OUTPUT_STREAM (output), -1);

	if (!G_IS_SEEKABLE (output)) {
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

	seek = G_SEEKABLE (output);
	if (!g_seekable_seek (seek, offset, from, NULL, &err))
		return handle_gio_error (err);
			
	return offset;
}

/* Called by gpgme to close a file */
static void
output_release (void *handle)
{
	GOutputStream* output = handle;
	g_return_if_fail (G_IS_OUTPUT_STREAM (output));
	
	g_object_unref (output);
}

/* GPGME vfs file operations */
static struct gpgme_data_cbs output_cbs = 
{
    NULL,
    output_write,
    output_seek,
    output_release
};

gpgme_data_t
seahorse_gpgme_output_data (GOutputStream* output)
{
	gpgme_error_t gerr;
	gpgme_data_t ret = NULL;

	g_return_val_if_fail (G_IS_OUTPUT_STREAM (output), NULL);
	
	gerr = gpgme_data_new_from_cbs (&ret, &output_cbs, output);
	if (!GPG_IS_OK (gerr))
		return NULL;
	
	g_object_ref (output);
	return ret;
}

/* -------------------------------------------------------------------------------------
 * INPUT STREAMS
 */

/* Called by gpgme to read data */
static ssize_t
input_read (void *handle, void *buffer, size_t size)
{
	GInputStream* input = handle;
	GError *err = NULL;
	gsize nread;
	
	g_return_val_if_fail (G_IS_INPUT_STREAM (input), -1);
	
	if (!g_input_stream_read_all (input, buffer, size, &nread, NULL, &err))
		return handle_gio_error (err);
	
	return nread;
}

/* Called from gpgme to seek a file */
static off_t
input_seek (void *handle, off_t offset, int whence)
{
	GSeekable *seek;
	GSeekType from;
	GError *err = NULL;
	GInputStream* input = handle;

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
	if (!g_seekable_seek (seek, offset, from, NULL, &err))
		return handle_gio_error (err);

	return offset;
}

/* Called by gpgme to close a file */
static void
input_release (void *handle)
{
	GInputStream* input = handle;
	g_return_if_fail (G_IS_INPUT_STREAM (input));
	
	g_object_unref (input);
}

/* GPGME vfs file operations */
static struct gpgme_data_cbs input_cbs = 
{
    input_read,
    NULL,
    input_seek,
    input_release
};

gpgme_data_t
seahorse_gpgme_input_data (GInputStream* input)
{
	gpgme_error_t gerr;
	gpgme_data_t ret = NULL;
	
	g_return_val_if_fail (G_IS_INPUT_STREAM (input), NULL);
	
	gerr = gpgme_data_new_from_cbs (&ret, &input_cbs, input);
	if (!GPG_IS_OK (gerr))
		return NULL;
	
	g_object_ref (input);
	return ret;
}
