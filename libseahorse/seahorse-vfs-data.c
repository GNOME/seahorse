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
#include <stdlib.h>
#include <fcntl.h>

#include <gnome.h>
#include <libgnomevfs/gnome-vfs.h>

#include "seahorse-gpgmex.h"
#include "seahorse-context.h"
#include "seahorse-vfs-data.h"
#include "seahorse-util.h"

/* The current/last VFS operation */
typedef enum _VfsAsyncOp
{
    VFS_OP_NONE,
    VFS_OP_OPENING,
    VFS_OP_READING,
    VFS_OP_WRITING,
    VFS_OP_SEEKING
}
VfsAsyncOp;

/* The state of the VFS system */
typedef enum _VfsAsyncState
{
    VFS_ASYNC_PROCESSING,
    VFS_ASYNC_CANCELLED,
    VFS_ASYNC_READY
}
VfsAsyncState;

/* A handle for VFS work */
typedef struct _VfsAsyncHandle
{
    GnomeVFSAsyncHandle* handle;    /* Handle for operations */
    GtkWidget* widget;              /* Widget for progress and cancel */
    
    VfsAsyncOp    operation;        /* The last/current operation */
    VfsAsyncState state;            /* State of the last/current operation */

    GnomeVFSResult result;          /* Result of the current operation */
    gpointer buffer;                /* Current operation's buffer */
    GnomeVFSFileSize processed;     /* Number of bytes processed in current op */
}
VfsAsyncHandle;

static void vfs_data_cancel(VfsAsyncHandle *ah);

/* Waits for a given VFS operation to complete without halting the UI */
static gboolean
vfs_data_wait_results(VfsAsyncHandle* ah, gboolean errors)
{
    VfsAsyncOp op;
    
    seahorse_util_wait_until (ah->state != VFS_ASYNC_PROCESSING);

    op = ah->operation;
    ah->operation = VFS_OP_NONE;
    
    /* Cancelling looks like an error to our caller */
    if(ah->state == VFS_ASYNC_CANCELLED)
    {
        errno = 0;
        return FALSE;
    }
        
    g_assert(ah->state == VFS_ASYNC_READY);
    
    /* There was no operation going on, result codes are not relevant */
    if(op == VFS_OP_NONE)
        return TRUE;
        
    if(ah->result == GNOME_VFS_ERROR_EOF)
    {
        ah->processed = 0;
        ah->result = GNOME_VFS_OK;
    }
    
    else if(ah->result == GNOME_VFS_ERROR_CANCELLED)
    {
        vfs_data_cancel(ah);
        errno = 0;
        return FALSE;
    }
    
    /* Check for error codes */
    if(ah->result != GNOME_VFS_OK)
    {
        if(errors)
        {
            switch(ah->result)
            {
            #define VFS_TO_SYS_ERR(v, s)    \
                case v: errno = s; break;
            VFS_TO_SYS_ERR(GNOME_VFS_ERROR_NOT_FOUND, ENOENT);
            VFS_TO_SYS_ERR(GNOME_VFS_ERROR_GENERIC, EIO);
            VFS_TO_SYS_ERR(GNOME_VFS_ERROR_INTERNAL, EIO);
            VFS_TO_SYS_ERR(GNOME_VFS_ERROR_BAD_PARAMETERS, EINVAL);
            VFS_TO_SYS_ERR(GNOME_VFS_ERROR_NOT_SUPPORTED, EOPNOTSUPP);
            VFS_TO_SYS_ERR(GNOME_VFS_ERROR_IO, EIO);
            VFS_TO_SYS_ERR(GNOME_VFS_ERROR_CORRUPTED_DATA, EIO);
            VFS_TO_SYS_ERR(GNOME_VFS_ERROR_WRONG_FORMAT, EIO);
            VFS_TO_SYS_ERR(GNOME_VFS_ERROR_BAD_FILE, EBADF);
            VFS_TO_SYS_ERR(GNOME_VFS_ERROR_TOO_BIG, EFBIG);
            VFS_TO_SYS_ERR(GNOME_VFS_ERROR_NO_SPACE, ENOSPC);
            VFS_TO_SYS_ERR(GNOME_VFS_ERROR_READ_ONLY, EPERM);
            VFS_TO_SYS_ERR(GNOME_VFS_ERROR_INVALID_URI, EINVAL);
            VFS_TO_SYS_ERR(GNOME_VFS_ERROR_NOT_OPEN, EBADF);
            VFS_TO_SYS_ERR(GNOME_VFS_ERROR_INVALID_OPEN_MODE, EPERM);
            VFS_TO_SYS_ERR(GNOME_VFS_ERROR_ACCESS_DENIED, EACCES);
            VFS_TO_SYS_ERR(GNOME_VFS_ERROR_TOO_MANY_OPEN_FILES, EMFILE);
            VFS_TO_SYS_ERR(GNOME_VFS_ERROR_NOT_A_DIRECTORY, ENOTDIR);
            VFS_TO_SYS_ERR(GNOME_VFS_ERROR_IN_PROGRESS, EALREADY);
            VFS_TO_SYS_ERR(GNOME_VFS_ERROR_INTERRUPTED, EINTR);
            VFS_TO_SYS_ERR(GNOME_VFS_ERROR_FILE_EXISTS, EEXIST);
            VFS_TO_SYS_ERR(GNOME_VFS_ERROR_LOOP, ELOOP);
            VFS_TO_SYS_ERR(GNOME_VFS_ERROR_NOT_PERMITTED, EPERM);
            VFS_TO_SYS_ERR(GNOME_VFS_ERROR_IS_DIRECTORY, EISDIR);
            VFS_TO_SYS_ERR(GNOME_VFS_ERROR_NO_MEMORY, ENOMEM);
            VFS_TO_SYS_ERR(GNOME_VFS_ERROR_HOST_NOT_FOUND, ENOENT);
            VFS_TO_SYS_ERR(GNOME_VFS_ERROR_INVALID_HOST_NAME, EHOSTDOWN);
            VFS_TO_SYS_ERR(GNOME_VFS_ERROR_HOST_HAS_NO_ADDRESS, EHOSTUNREACH);
            VFS_TO_SYS_ERR(GNOME_VFS_ERROR_LOGIN_FAILED, EACCES);
            VFS_TO_SYS_ERR(GNOME_VFS_ERROR_DIRECTORY_BUSY, EBUSY);
            VFS_TO_SYS_ERR(GNOME_VFS_ERROR_DIRECTORY_NOT_EMPTY, ENOTEMPTY);
            VFS_TO_SYS_ERR(GNOME_VFS_ERROR_TOO_MANY_LINKS, EMLINK);
            VFS_TO_SYS_ERR(GNOME_VFS_ERROR_READ_ONLY_FILE_SYSTEM, EROFS);
            VFS_TO_SYS_ERR(GNOME_VFS_ERROR_NOT_SAME_FILE_SYSTEM, EIO);
            VFS_TO_SYS_ERR(GNOME_VFS_ERROR_NAME_TOO_LONG, ENAMETOOLONG);
            VFS_TO_SYS_ERR(GNOME_VFS_ERROR_SERVICE_NOT_AVAILABLE, ENOPROTOOPT);
            VFS_TO_SYS_ERR(GNOME_VFS_ERROR_SERVICE_OBSOLETE, ENOPROTOOPT);
            VFS_TO_SYS_ERR(GNOME_VFS_ERROR_PROTOCOL_ERROR, EIO);
            VFS_TO_SYS_ERR(GNOME_VFS_ERROR_NO_MASTER_BROWSER, EIO);
            VFS_TO_SYS_ERR(GNOME_VFS_ERROR_NO_DEFAULT, EIO);
            VFS_TO_SYS_ERR(GNOME_VFS_ERROR_NO_HANDLER, EIO);
            VFS_TO_SYS_ERR(GNOME_VFS_ERROR_PARSE, EIO);
            VFS_TO_SYS_ERR(GNOME_VFS_ERROR_LAUNCH, EIO);
            
            default:
                errno = EIO;
                break;
            }
            
            /* When errors on open we look cancelled */
            if(op == VFS_OP_OPENING)
                ah->state = VFS_ASYNC_CANCELLED;
        }

        return FALSE;
    }
    
    return TRUE;
}

/* Called when a file open completes */
static void
vfs_data_open_done(GnomeVFSAsyncHandle *handle, GnomeVFSResult result, 
                        gpointer callback_data)
{
    VfsAsyncHandle* ah = (VfsAsyncHandle*)callback_data;

    if(ah->state == VFS_ASYNC_PROCESSING)
    {
        g_assert(handle == ah->handle);
        g_assert(ah->operation == VFS_OP_OPENING);
    
        ah->result = result;
        ah->state = VFS_ASYNC_READY;
    }
}

/* Open the given URI */
static VfsAsyncHandle* 
vfs_data_open(const gchar* uri, gboolean write)
{
    VfsAsyncHandle* ah;

    ah = g_new0(VfsAsyncHandle, 1);
    ah->state = VFS_ASYNC_PROCESSING;
    ah->operation = VFS_OP_OPENING;

    if(write)
    {
        /* Note that we always overwrite the file */
        gnome_vfs_async_create(&(ah->handle), uri, GNOME_VFS_OPEN_WRITE | GNOME_VFS_OPEN_RANDOM,
                FALSE, 0644, GNOME_VFS_PRIORITY_DEFAULT, vfs_data_open_done, ah);        
    }
    else
    {
        gnome_vfs_async_open(&(ah->handle), uri, GNOME_VFS_OPEN_READ | GNOME_VFS_OPEN_RANDOM,
                GNOME_VFS_PRIORITY_DEFAULT, vfs_data_open_done, ah);
    }
    
    return ah;
}

/* Called when a read completes */
static void
vfs_data_read_done(GnomeVFSAsyncHandle *handle, GnomeVFSResult result, gpointer buffer,
        GnomeVFSFileSize bytes_requested, GnomeVFSFileSize bytes_read, gpointer callback_data)
{
    VfsAsyncHandle* ah = (VfsAsyncHandle*)callback_data;

    if(ah->state == VFS_ASYNC_PROCESSING)
    {
        g_assert(handle == ah->handle);
        g_assert(buffer == ah->buffer);
        g_assert(ah->operation == VFS_OP_READING);
    
        ah->result = result;
        ah->processed = bytes_read;
        ah->state = VFS_ASYNC_READY;
    }
}    
    
/* Called by gpgme to read data */
static ssize_t
vfs_data_read(void *handle, void *buffer, size_t size)
{
    VfsAsyncHandle* ah = (VfsAsyncHandle*)handle;
    ssize_t sz = 0;
    
    /* Just in case we have an operation, like open */
    if(!vfs_data_wait_results(ah, TRUE))
        return -1;
        
    g_assert(ah->state == VFS_ASYNC_READY);
    
    /* Start async operation */
    ah->buffer = buffer;
    ah->state = VFS_ASYNC_PROCESSING;
    ah->operation = VFS_OP_READING;
    gnome_vfs_async_read(ah->handle, buffer, size, vfs_data_read_done, ah);
    
    /* Wait for it */
    if(!vfs_data_wait_results(ah, TRUE))
        return -1;
    
    /* Return results */
    sz = (ssize_t)ah->processed;
    ah->state = VFS_ASYNC_READY;
    
    ah->buffer = NULL;
    ah->processed = 0;
    
    return sz;
}

/* Called when a write completes */
static void
vfs_data_write_done(GnomeVFSAsyncHandle *handle, GnomeVFSResult result, gconstpointer buffer,
        GnomeVFSFileSize bytes_requested, GnomeVFSFileSize bytes_written, gpointer callback_data)
{
    VfsAsyncHandle* ah = (VfsAsyncHandle*)callback_data;

    if(ah->state == VFS_ASYNC_PROCESSING)
    {
        g_assert(handle == ah->handle);
        g_assert(buffer == ah->buffer);
        g_assert(ah->operation == VFS_OP_WRITING);
    
        ah->result = result;
        ah->processed = bytes_written;
        ah->state = VFS_ASYNC_READY;
    }
}    

/* Called by gpgme to write data */
static ssize_t
vfs_data_write(void *handle, const void *buffer, size_t size)
{
    VfsAsyncHandle* ah = (VfsAsyncHandle*)handle;
    ssize_t sz = 0;
    
    /* Just in case we have an operation, like open */
    if(!vfs_data_wait_results(ah, TRUE))
        return -1;
           
    g_assert(ah->state == VFS_ASYNC_READY);
    
    /* Start async operation */
    ah->buffer = (gpointer)buffer;
    ah->state = VFS_ASYNC_PROCESSING;
    ah->operation = VFS_OP_WRITING;
    gnome_vfs_async_write(ah->handle, buffer, size, vfs_data_write_done, ah);
    
    /* Wait for it */
    if(!vfs_data_wait_results(ah, TRUE))
        return -1;
    
    /* Return results */
    sz = (ssize_t)ah->processed;
    ah->state = VFS_ASYNC_READY;
    
    ah->buffer = NULL;
    ah->processed = 0;
    
    return sz;
}

/* Called when a VFS seek completes */
static void
vfs_data_seek_done(GnomeVFSAsyncHandle *handle, GnomeVFSResult result, 
                        gpointer callback_data)
{
    VfsAsyncHandle* ah = (VfsAsyncHandle*)callback_data;

    if(ah->state == VFS_ASYNC_PROCESSING)
    {
        g_assert(handle == ah->handle);
        g_assert(ah->operation == VFS_OP_SEEKING);
    
        ah->result = result;
        ah->state = VFS_ASYNC_READY;
    }
}

/* Called from gpgme to seek a file */
static off_t
vfs_data_seek(void *handle, off_t offset, int whence)
{
    VfsAsyncHandle* ah = (VfsAsyncHandle*)handle;
    GnomeVFSSeekPosition wh;

    /* Just in case we have an operation, like open */
    if(!vfs_data_wait_results(ah, TRUE))
        return (off_t)-1;
           
    g_assert(ah->state == VFS_ASYNC_READY);
                
    switch(whence)
    {
    case SEEK_SET:
        wh = GNOME_VFS_SEEK_START;
        break;
    case SEEK_CUR:
        wh = GNOME_VFS_SEEK_CURRENT;
        break;
    case SEEK_END:
        wh = GNOME_VFS_SEEK_END;
        break;
    default:
        g_assert_not_reached();
        break;
    }
    
    /* Start async operation */
    ah->state = VFS_ASYNC_PROCESSING;
    ah->operation = VFS_OP_SEEKING;
    gnome_vfs_async_seek(ah->handle, wh, (GnomeVFSFileOffset)offset, 
                            vfs_data_seek_done, ah);
    
    /* Wait for it */
    if(!vfs_data_wait_results(ah, TRUE))
        return -1;
    
    /* Return results */
    ah->state = VFS_ASYNC_READY;
    return offset;
}

/* Dummy callback for closing a file */
static void
vfs_data_close_done(GnomeVFSAsyncHandle *handle, GnomeVFSResult result, 
                        gpointer callback_data)
{
    /* A noop, we just let this go */    
}

/* Called to cancel the current operation.
 * Puts the async handle into a cancelled state. */
static void 
vfs_data_cancel(VfsAsyncHandle *ah)
{
    gboolean close = FALSE;
    
    switch(ah->state)
    {
    case VFS_ASYNC_CANCELLED:
        break;

    case VFS_ASYNC_PROCESSING:
        close = (ah->operation != VFS_OP_OPENING);
        gnome_vfs_async_cancel(ah->handle);
        vfs_data_wait_results(ah, FALSE);
        break;
        
    case VFS_ASYNC_READY:
        close = TRUE;
        break;
    };
    
    if(close)
    {
        gnome_vfs_async_close (ah->handle, vfs_data_close_done, NULL);
        ah->handle = NULL;
    }
    
    ah->state = VFS_ASYNC_CANCELLED;
}

/* Called by gpgme to close a file */
static void
vfs_data_release(void *handle)
{
    VfsAsyncHandle* ah = (VfsAsyncHandle*)handle;

    vfs_data_cancel(ah);
    g_free(ah);
}

/* GPGME vfs file operations */
static struct gpgme_data_cbs vfs_data_cbs = 
{
    vfs_data_read,
    vfs_data_write,
    vfs_data_seek,
    vfs_data_release
};

/* -----------------------------------------------------------------------------
 * LOCAL OPERATIONS
 */

static ssize_t
fd_data_read(void *handle, void *buffer, size_t size)
{
    int fd = *((int*)handle);
    return read (fd, buffer, size);
}


static ssize_t
fd_data_write(void *handle, const void *buffer, size_t size)
{
    int fd = *((int*)handle);
    return write (fd, buffer, size);
}


static off_t
fd_data_seek(void *handle, off_t offset, int whence)
{
    int fd = *((int*)handle);
    return lseek (fd, offset, whence);
}

static void
fd_data_release(void *handle)
{
    int fd = *((int*)handle);
    close(fd);
    g_free(handle);     
}

static struct gpgme_data_cbs fd_data_cbs = 
{
    fd_data_read,
    fd_data_write,
    fd_data_seek,
    fd_data_release
};  

/* -----------------------------------------------------------------------------
 */
 
/* Create a data on the given uri, remote uris get gnome-vfs backends,
 * local uris get normal file access. */
gpgme_data_t
seahorse_vfs_data_create(const gchar *uri, gboolean write, gpg_error_t* err) 
{
    gpgme_error_t gerr;
    gpgme_data_cbs_t cbs;
    gpgme_data_t ret = NULL;
    void* handle = NULL;
    char* local;
    char* ruri;
    
    if(!err)
        err = &gerr;
    
    ruri = gnome_vfs_make_uri_canonical(uri);
    local = gnome_vfs_get_local_path_from_uri(ruri);
    
    /* A local uri, we use simple IO */
    if(local != NULL)
    {
        int fd;
        
        fd = open(local, write ? O_CREAT | O_TRUNC | O_RDWR : O_RDONLY, 0644);
        
        if(fd == -1)
        {
            gpg_err_code_t e = gpg_err_code_from_errno(errno);
            *err = GPG_E(e);
        }
        else
        {
            handle = g_malloc0(sizeof(int));
            *((int*)handle) = fd;
            cbs = &fd_data_cbs;
        }
    }
    
    /* A remote uri, much more complex */
    else
    {
        handle = vfs_data_open(ruri, write);
        cbs = &vfs_data_cbs;
    }
    
    if(handle)
    {
        *err = gpgme_data_new_from_cbs(&ret, cbs, handle);
        if(!GPG_IS_OK(*err))
        {
            cbs->release(handle);
            ret = NULL;
        }
    }
    
    g_free(ruri);
    g_free(local);

    return ret;
}

