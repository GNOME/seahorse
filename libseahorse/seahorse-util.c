/*
 * Seahorse
 *
 * Copyright (C) 2003 Jacob Perkins
 * Copyright (C) 2004-2005 Nate Nielsen
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

#include <gnome.h>
#include <time.h>
#include <stdio.h>
#include <sys/wait.h>

#include <libgnomevfs/gnome-vfs.h>

#include "seahorse-gpgmex.h"
#include "seahorse-util.h"
#include "seahorse-key.h"
#include "seahorse-gconf.h"
#include "seahorse-vfs-data.h"

typedef struct _SeahorsePGPHeader {
    const gchar *header;
    const gchar *footer;
    SeahorseTextType type;
} SeahorsePGPHeader;    

static const SeahorsePGPHeader seahorse_pgp_headers[] = {
    { 
        "-----BEGIN PGP MESSAGE-----", 
        "-----END PGP MESSAGE-----", 
        SEAHORSE_TEXT_TYPE_MESSAGE 
    }, 
    {
        "-----BEGIN PGP SIGNED MESSAGE-----",
        "-----END PGP SIGNATURE-----",
        SEAHORSE_TEXT_TYPE_SIGNED
    }, 
    {
        "-----BEGIN PGP PUBLIC KEY BLOCK-----",
        "-----END PGP PUBLIC KEY BLOCK-----",
        SEAHORSE_TEXT_TYPE_KEY
    }, 
    {
        "-----BEGIN PGP PRIVATE KEY BLOCK-----",
        "-----END PGP PRIVATE KEY BLOCK-----",
        SEAHORSE_TEXT_TYPE_KEY
    }
};

static const gchar *bad_filename_chars = "/\\<>|";

void
seahorse_util_show_error (GtkWindow *parent, const gchar *message)
{
	GtkWidget *error;
	g_return_if_fail (!g_str_equal (message, ""));
	
	error = gtk_message_dialog_new_with_markup (parent, GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR,
		GTK_BUTTONS_CLOSE, message);
	
	gtk_dialog_run (GTK_DIALOG (error));
	gtk_widget_destroy (error);
}

gchar*
seahorse_util_get_text_view_text (GtkTextView *view)
{
	GtkTextBuffer *buffer;
	GtkTextIter start;                                                      
        GtkTextIter end;
	gchar *text;
	
        g_return_val_if_fail (view != NULL, "");	
	
	buffer = gtk_text_view_get_buffer (view);                                                
        gtk_text_buffer_get_bounds (buffer, &start, &end);                      
        text = gtk_text_buffer_get_text (buffer, &start, &end, FALSE);   
        return text;
}

void
seahorse_util_set_text_view_string (GtkTextView *view, GString *string)
{
        GtkTextBuffer *buffer;
	g_return_if_fail (view != NULL && string != NULL);
	
	buffer = gtk_text_view_get_buffer (view);
	gtk_text_buffer_set_text (buffer, string->str, string->len);
}

/** 
 * seahorse_util_get_date_string:
 * @time: Time value to parse
 *
 * Creates a string representation of @time.
 *
 * Returns: A string representing @time
 **/
gchar*
seahorse_util_get_date_string (const time_t time)
{
	GDate *created_date;
	gchar *created_string;
	
	if (time == 0)
		return "0";
	
	created_date = g_date_new ();
	g_date_set_time (created_date, time);
	created_string = g_new (gchar, 11);
	g_date_strftime (created_string, 11, _("%Y-%m-%d"), created_date);
	return created_string;
}

/**
 * seahorse_util_handle_gpgme:
 * @err: Error value
 *
 * Shows an error dialog for @err.
 **/
void
seahorse_util_handle_gpgme (gpgme_error_t err, const char* desc, ...)
{
    gchar *t = NULL;
	
	switch (gpgme_err_code(err)) {
	case GPG_ERR_CANCELED:
	case GPG_ERR_NO_ERROR:
	case GPG_ERR_ECANCELED:
		return;
	default:
		break;
	}
  
    va_list ap;
    va_start(ap, desc);
  
    if (desc) {
        gchar *x = g_strdup_vprintf (desc, ap);
        t = g_strconcat("<big><b>", x, "</b></big>\n\n", gpgme_strerror (err), NULL);
    } else {
        t = g_strdup (gpgme_strerror (err));
    }
    
    va_end(ap);
        
    seahorse_util_show_error (NULL, t);
    g_free(t);
}

void
seahorse_util_handle_error (GError* err, const char* desc, ...)
{
    gchar *t = NULL;
    va_list ap;
    
    if(!err)
        return;

    va_start(ap, desc);
  
    if (desc) {
        gchar *x = g_strdup_vprintf (desc, ap);
        t = g_strconcat("<big><b>", x, "</b></big>",
                err->message ? "\n\n" : NULL, err->message, NULL);
    } else if (err->message) {
        t = g_strdup (err->message);
    }
    
    va_end(ap);        
    
    if(t != NULL)
        seahorse_util_show_error (NULL, t);

    g_free(t);
    g_clear_error (&err);
}    

/**
 * seahorse_util_gpgme_error_domain
 * Returns: The GError domain for gpgme errors
 **/
GQuark
seahorse_util_gpgme_error_domain ()
{
    static GQuark q = 0;
    if(q == 0)
        q = g_quark_from_static_string ("seahorse-gpgme-error");
    return q;
}

/**
 * seahorse_util_gpgme_to_error
 * @gerr GPGME error
 * @err  Resulting GError object
 **/
void    
seahorse_util_gpgme_to_error(gpgme_error_t gerr, GError** err)
{
    /* Make sure this is actually an error */
    g_assert(!GPG_IS_OK(gerr));
    g_set_error(err, SEAHORSE_GPGME_ERROR, gpgme_err_code(gerr), 
                            "%s", gpgme_strerror(gerr));
}

/**
 * seahorse_util_write_data_to_file:
 * @path: Path of file to write to
 * @data: Data to write
 * @release: Whether to release @data or not
 *
 * Tries to write @data to the file @path. 
 *
 * Returns: GPG_ERR_NO_ERROR if write is successful,
 * GPGME_File_Error if the file could not be opened
 **/
gpgme_error_t
seahorse_util_write_data_to_file (const gchar *path, gpgme_data_t data, 
                                  gboolean release)
{
    gpgme_error_t err = GPG_OK;
	gpgme_data_t file;
	gchar *buffer;
	gint nread;
   
    /* 
     * TODO: gpgme_data_seek doesn't work for us right now
     * probably because of different off_t sizes 
     */
    gpgme_data_rewind (data);
	
    file = seahorse_vfs_data_create (path, SEAHORSE_VFS_WRITE, &err);
    if (file != NULL) {
    	buffer = g_new0 (gchar, 128);
    	
    	while ((nread = gpgme_data_read (data, buffer, 128)) > 0) {
            if(gpgme_data_write (file, buffer, nread) < 0) {
                gpg_err_code_t e = gpg_err_code_from_errno (errno);
                err = GPG_E (e);                
                break;
            }
        }

        g_free (buffer);
    }

    if (release)
        gpgme_data_release (data);
    
    gpgme_data_release (file);	
	return err;
}

/**
 * seahorse_util_write_data_to_text:
 * @data: Data to write
 * @release: Whether to release @data or not
 *
 * Converts @data to a string. 
 *
 * Returns: The string read from data
 **/
gchar*
seahorse_util_write_data_to_text (gpgme_data_t data, gboolean release)
{
	gint size = 128;
    gchar *buffer, *text;
    guint nread = 0;
	GString *string;

    /* 
     * TODO: gpgme_data_seek doesn't work for us right now
     * probably because of different off_t sizes 
     */
    gpgme_data_rewind (data);

	string = g_string_new ("");
	buffer = g_new (gchar, size);
	
	while ((nread = gpgme_data_read (data, buffer, size)) > 0)
		string = g_string_append_len (string, buffer, nread);

    if (release)
        gpgme_data_release (data);
    
	text = string->str;
	g_string_free (string, FALSE);
	
	return text;
}

/** 
 * seahorse_util_read_data_block
 * 
 * Breaks out one block of data (usually a key)
 * 
 * @buf: A string buffer to write the data to.
 * @data: The GPGME data block to read from.
 * @start: The start signature to look for.
 * @end: The end signature to look for.
 * 
 * Returns: The number of bytes copied.
 */
guint
seahorse_util_read_data_block (GString *buf, gpgme_data_t data, 
                               const gchar *start, const gchar* end)
{
    const gchar *t;
    guint copied = 0;
    gchar ch;
     
    /* Look for the beginning */
    t = start;
    while (gpgme_data_read (data, &ch, 1) == 1) {
        
        /* Match next char */            
        if (*t == ch)
            t++;

        /* Did we find the whole string? */
        if (!*t) {
            buf = g_string_append (buf, start);
            copied += strlen (start);
            break;
        }
    } 
    
    /* Look for the end */
    t = end;
    while (gpgme_data_read (data, &ch, 1) == 1) {
        
        /* Match next char */
        if (*t == ch)
            t++;
        
        buf = g_string_append_c (buf, ch);
        copied++;
                
        /* Did we find the whole string? */
        if (!*t)
            break;
    }
    
    return copied;
}

/** 
 * seahorse_util_printf_fd:
 * @fd: The file descriptor to write to
 * @fmt: The data to write
 *
 * Returns: Whether the operation was successful or not.
 **/
gboolean 
seahorse_util_print_fd (int fd, const char* s)
{
    /* Guarantee all data is written */
    int r, l = strlen (s);

    while (l > 0) {
     
        r = write (fd, s, l);
        
        if (r == -1) {
            if (errno != EAGAIN && errno != EINTR) {
                g_critical ("couldn't write data to socket: %s", strerror (errno));
                return FALSE;
            }
            
        } else {
            s += r;
            l -= r;
        }
    }
    
    return TRUE;
}

/** 
 * seahorse_util_printf_fd:
 * @fd: The file descriptor to write to
 * @fmt: The printf format of the data to write
 *
 * Returns: Whether the operation was successful or not.
 **/
gboolean 
seahorse_util_printf_fd (int fd, const char* fmt, ...)
{
    gchar* t;
    va_list ap;
    gboolean ret;
    
    va_start (ap, fmt);    
    t = g_strdup_vprintf (fmt, ap);
    va_end (ap);
    
    ret = seahorse_util_print_fd (fd, t);
    g_free (t);
    return ret;
}

/**
 * seahorse_util_detect_text
 * @text: The text to process
 * @len: Length of the text or -1 for null-terminated
 * @start: Returns the start of detected area (pass NULL if not interested)
 * @end: Returns the end of the detected area (pass NULL if not interested)
 * 
 * Auto detects what kind of PGP crypted data a given block is.
 * 
 * Returns: The type
 **/
SeahorseTextType    
seahorse_util_detect_text (const gchar *text, gint len, const gchar **start, 
                            const gchar **end)
{
    const SeahorsePGPHeader *header;
    const gchar *pos = NULL;
    const gchar *t;
    int i;
    
    if (len == -1)
        len = strlen (text);
    
    /* Find the first of the headers */
    for (i = 0; i < (sizeof (seahorse_pgp_headers) / sizeof (seahorse_pgp_headers[0])); i++) {
        t = g_strstr_len (text, len, seahorse_pgp_headers[i].header);
        if (t != NULL) {
            if (pos == NULL || (t < pos)) {
                header = &(seahorse_pgp_headers[i]);
                pos = t;
            }
        }
    }
    
    if (pos != NULL) {
        
        if (start)
            *start = pos;
        
        /* Find the end of that block */
        t = g_strstr_len (pos, len - (pos - text), header->footer);
        if (t != NULL && end)
            *end = t + strlen(header->footer);
        else if (end)
            *end = NULL;
            
        return header->type;
    }
    
    return SEAHORSE_TEXT_TYPE_NONE;
}

gchar*      
seahorse_util_filename_for_keys (GList *keys)
{
    SeahorseKey *skey;
    gchar *t, *r;
    
    g_return_val_if_fail (g_list_length (keys) > 0, NULL);

    if (g_list_length (keys) == 1) {
        g_return_val_if_fail (SEAHORSE_IS_KEY (keys->data), NULL);
        skey = SEAHORSE_KEY (keys->data);
        t = seahorse_key_get_userid_name (skey, 0);
        if (t == NULL) {
            t = g_strdup (seahorse_key_get_id (skey->key));
            if (strlen (t) > 8)
                t[8] = 0;
        }
    } else {
        t = g_strdup (_("Multiple Keys"));
    }
    
    g_strstrip (t);
    g_strdelimit (t, bad_filename_chars, '_');
    r = g_strconcat (t, SEAHORSE_EXT_ASC, NULL);
    g_free (t);
    return r;
}

/** 
 * seahorse_util_uri_get_last:
 * @uri: The uri to parse
 *
 * Finds the last portion of @uri. Note that this does
 * not modify @uri. If the uri is invalid or has no 
 * directories then the entire thing is returned.
 *
 * Returns: Last portion of @uri
 **/
const gchar* 
seahorse_util_uri_get_last (const gchar* uri)
{
    const gchar* t;
    
    t = uri + strlen (uri);
    
    if (*(t - 1) == '/' && t != uri)
        t--;
    
    while (*(t - 1) != '/' && t != uri)
        t--;
    
    return t;
}    

/**
 * seahorse_util_uri_split_last
 * @uri: The uri to split
 * 
 * Splits the uri in two at it's last component. The result
 * is still part of the same string, so don't free it. This 
 * modifies the @uri argument.
 * 
 * Returns: The last component
 **/
const gchar* 
seahorse_util_uri_split_last (gchar* uri)
{
    gchar* t;
    
    t = (gchar*)seahorse_util_uri_get_last (uri);
    if(t != uri)
        *(t - 1) = 0;
        
    return t;
}

/**
 * seahurse_util_uri_exists
 * @uri: The uri to check
 * 
 * Verify whether a given uri exists or not.
 * 
 * Returns: Whether it exists or not.
 **/
gboolean
seahorse_util_uri_exists (const gchar* uri)
{
    GnomeVFSURI* vuri;    
    gboolean exists;

    vuri = gnome_vfs_uri_new (uri);
    g_return_val_if_fail (vuri != NULL, FALSE);
    
    exists = gnome_vfs_uri_exists (vuri);
    gnome_vfs_uri_unref (vuri);
    
    return exists;
}       
    
/**
 * seahorse_util_uri_unique
 * @uri: The uri to guarantee is unique
 * 
 * Creates a URI based on @uri that does not exist.
 * A simple numbering scheme is used to create new
 * URIs. Not meant for temp file creation.
 * 
 * Returns: Newly allocated unique URI.
 **/
gchar*
seahorse_util_uri_unique (const gchar* uri)
{
    gchar* suffix;
    gchar* prefix;
    gchar* uri_try;
    gchar* x;
    guint len;
    int i;
    
    /* Simple when doesn't exist */
    if (!seahorse_util_uri_exists (uri))
        return g_strdup (uri);
    
    prefix = g_strdup (uri);
    len = strlen (prefix); 
    
    /* Always take off a slash at end */
    g_return_val_if_fail (len > 1, g_strdup (uri));
    if (prefix[len - 1] == '/')
        prefix[len - 1] = 0;
            
    /* Split into prefix and suffix */
    suffix = strrchr (prefix, '.');
    x = strrchr(uri, '/');
    if (suffix == NULL || (x != NULL && suffix < x)) {
        suffix = g_strdup ("");
    } else {
        x = suffix;
        suffix = g_strdup (suffix);
        *x = 0;
    }                
        
    for (i = 1; i < 1000; i++) {
       
        uri_try = g_strdup_printf ("%s-%d%s", prefix, i, suffix);
       
        if (!seahorse_util_uri_exists (uri_try))
            break;
        
        g_free (uri_try);
        uri_try = NULL;
    }
        
    g_free (suffix);    
    g_free (prefix);
    return uri_try ? uri_try : g_strdup (uri);       
}

/**
 * seahorse_util_uri_replace_ext
 * @uri: The uri with old extension
 * @ext: The new extension
 * 
 * Replaces the extension on @uri
 * 
 * Returns: Newly allocated URI string with new extension.
 **/
gchar* 
seahorse_util_uri_replace_ext (const gchar *uri, const gchar *ext)
{
    gchar* ret;
    gchar* dot;
    gchar* slash;
    guint len;
    
    len = strlen (uri);
    ret = g_new0 (gchar, len + strlen(ext) + 16);
    strcpy (ret, uri);
 
    /* Always take off a slash at end */
    g_return_val_if_fail (len > 1, ret);
    if (ret[len - 1] == '/')
        ret[len - 1] = 0;
        
    dot = strrchr (ret, '.');
    if (dot != NULL) {
        slash = strrchr (ret, '/');
        if (slash == NULL || dot > slash)
            *dot = 0;
    }
 
    strcat (ret, ".");
    strcat (ret, ext);
    return ret;    
}

/* Context for callback below */
typedef struct _VisitUriCtx
{
    GArray* files;
    const gchar* base_uri;
}
VisitUriCtx;

/* Called for each sub file or directory */
static gboolean
visit_uri (const gchar *rel_path, GnomeVFSFileInfo *info, gboolean recursing_will_loop,
                gpointer data, gboolean *recurse)
{
    VisitUriCtx* ctx = (VisitUriCtx*)data;
    gchar* t = g_strconcat (ctx->base_uri, "/", rel_path, NULL);
    gchar* uri = gnome_vfs_make_uri_canonical (t);
    g_free (t);

    if(info->type != GNOME_VFS_FILE_TYPE_DIRECTORY)
        g_array_append_val (ctx->files, uri);

    *recurse = !recursing_will_loop;
    return TRUE;
}

/**
 * seahorse_util_uris_expand
 * @uris: The null-terminated vector of URIs to enumerate
 * 
 * Find all files in the given set of uris.                                         
 * 
 * Returns: Newly allocated null-terminated string vector of URIs.
 */
gchar** 
seahorse_util_uris_expand (const gchar** uris)
{
    GnomeVFSFileInfo* info;
    GArray* files  = NULL;
    const gchar** u;
    gchar* uri;

    files = g_array_new (TRUE, FALSE, sizeof(gchar*));    
    info = gnome_vfs_file_info_new ();
    
    for(u = uris; *u; u++) {

        uri = gnome_vfs_make_uri_canonical (*u);

        if (gnome_vfs_get_file_info (uri, info, GNOME_VFS_FILE_INFO_DEFAULT) == GNOME_VFS_OK &&
            info->type == GNOME_VFS_FILE_TYPE_DIRECTORY) 
        {
                VisitUriCtx ctx;
                ctx.files = files;
                ctx.base_uri = uri;
                
                gnome_vfs_directory_visit (uri, GNOME_VFS_FILE_INFO_DEFAULT, 
                    GNOME_VFS_DIRECTORY_VISIT_DEFAULT | GNOME_VFS_DIRECTORY_VISIT_LOOPCHECK,
                    visit_uri, &ctx);
               
        /* Not a directory */     
        } else {
           
            g_array_append_val (files, uri);
            uri = NULL; /* To prevent freeing below */
        }
        
        g_free (uri);
    }            
    
    return (gchar**)g_array_free (files, FALSE);            
}

/**
 * seahorse_util_uris_package
 * @package: Package uri
 * @uris: null-terminated array of uris to package 
 * 
 * Package uris into an archive. The uris must be local.
 * 
 * Returns: Success or failure
 */
gboolean
seahorse_util_uris_package (const gchar* package, const char** uris)
{
    GError* err = NULL;
    gchar *out = NULL;
    gint status;
    gboolean r;
    GString *str;
    gchar *cmd;
    gchar *t;
    gchar *x;
    GnomeVFSFileInfo *info;
    GnomeVFSResult result;
    
    t = gnome_vfs_get_local_path_from_uri (package);
    x = g_shell_quote(t);
    g_free(t);
    
    /* create execution */
    str = g_string_new ("");
    g_string_printf (str, "file-roller --add-to=%s", x);
    g_free(x);
    
    while(*uris) {
        /* We should never be passed any remote uris at this point */
        x = gnome_vfs_make_uri_canonical (*uris);
        
        t = gnome_vfs_get_local_path_from_uri (x);
        g_free(x);
        
        g_return_val_if_fail (t != NULL, FALSE);

        x = g_shell_quote(t);
        g_free(t);

        g_string_append_printf (str, " %s", x);
        g_free(x);
        
        uris++;
    }
        
    /* Execute the command */
    cmd = g_string_free (str, FALSE);
    r = g_spawn_command_line_sync (cmd, &out, NULL, &status, &err);
    g_free (cmd); 
    
    if (out) {
        g_print (out);
        g_free (out);
    }
    
    if (!r) {
        seahorse_util_handle_error (err, _("Couldn't run file-roller"));
        return FALSE;
    }
    
    if (!(WIFEXITED (status) && WEXITSTATUS (status) == 0)) {
        seahorse_util_show_error (NULL, _("The file-roller process did not complete successfully"));
        return FALSE;
    }
    
    info = gnome_vfs_file_info_new ();
	info->permissions = GNOME_VFS_PERM_USER_READ | GNOME_VFS_PERM_USER_WRITE;
	result = gnome_vfs_set_file_info (package, info, GNOME_VFS_SET_FILE_INFO_PERMISSIONS);
    gnome_vfs_file_info_unref (info);
	    
	if (result != GNOME_VFS_OK) {
    	seahorse_util_handle_error (err, _("Couldn't set permissions on backup file."));
        return FALSE;
    }

    return TRUE;
}

GtkWidget*
seahorse_util_chooser_save_new (const gchar *title, GtkWindow *parent)
{
    GtkWidget *dialog;
    
    dialog = gtk_file_chooser_dialog_new (title, 
                parent, GTK_FILE_CHOOSER_ACTION_SAVE, 
                GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
                NULL);

    gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_ACCEPT);
    gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER (dialog), FALSE);
    return dialog;
}

GtkWidget*
seahorse_util_chooser_open_new (const gchar *title, GtkWindow *parent)
{
    GtkWidget *dialog;
    
    dialog = gtk_file_chooser_dialog_new (title, 
                parent, GTK_FILE_CHOOSER_ACTION_OPEN, 
                GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
                NULL);

    gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_ACCEPT);
    gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER (dialog), FALSE);
    return dialog;
}
void
seahorse_util_chooser_show_key_files (GtkWidget *dialog)
{
    GtkFileFilter* filter = gtk_file_filter_new ();
    
    /* Filter for PGP keys. We also include *.asc, as in many 
       cases that extension is associated with text/plain */
    gtk_file_filter_set_name (filter, _("All key files"));
    gtk_file_filter_add_mime_type (filter, "application/pgp-keys");
    gtk_file_filter_add_pattern (filter, "*.asc");    
    gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), filter);    
    gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (dialog), filter);

    filter = gtk_file_filter_new ();
    gtk_file_filter_set_name (filter, _("All files"));
    gtk_file_filter_add_pattern (filter, "*");    
    gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), filter);       
}
    
void
seahorse_util_chooser_show_archive_files (GtkWidget *dialog)
{
    GtkFileFilter* filter;
    int i;
    
    static const char *archive_mime_type[] = {
        "application/x-ar",
        "application/x-arj",
        "application/x-bzip",
        "application/x-bzip-compressed-tar",
        "application/x-cd-image",
        "application/x-compress",
        "application/x-compressed-tar",
        "application/x-gzip",
        "application/x-java-archive",
        "application/x-jar",
        "application/x-lha",
        "application/x-lzop",
        "application/x-rar",
        "application/x-rar-compressed",
        "application/x-tar",
        "application/x-zoo",
        "application/zip",
        "application/x-7zip"
    };
    
    filter = gtk_file_filter_new ();
    gtk_file_filter_set_name (filter, _("Archive files"));
    for (i = 0; i < G_N_ELEMENTS (archive_mime_type); i++)
        gtk_file_filter_add_mime_type (filter, archive_mime_type[i]);
    gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), filter);    
    gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (dialog), filter);

    filter = gtk_file_filter_new ();
    gtk_file_filter_set_name (filter, _("All files"));
    gtk_file_filter_add_pattern (filter, "*");    
    gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), filter);   
}

void
seahorse_util_chooser_set_filename (GtkWidget *dialog, GList *keys)
{
    gchar *t = NULL;
    
    if (g_list_length (keys) > 0) {
        t = seahorse_util_filename_for_keys (keys);
        gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (dialog), t);
        g_free (t);
    }
}
    
gchar*      
seahorse_util_chooser_save_prompt (GtkWidget *dialog)
{
    GtkWidget* edlg;
    gchar *uri = NULL;
    
    while (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT) {
     
        uri = gtk_file_chooser_get_uri(GTK_FILE_CHOOSER (dialog));

        if (uri == NULL)
            continue;
            
        if (seahorse_util_uri_exists (uri)) {

            edlg = gtk_message_dialog_new_with_markup (GTK_WINDOW (dialog),
                        GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_QUESTION,
                        GTK_BUTTONS_NONE, _("<b>A file already exists with this name.</b>\n\nDo you want to replace it with a new file?"));
            gtk_dialog_add_buttons (GTK_DIALOG (edlg), 
                        GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                        _("_Replace"), GTK_RESPONSE_ACCEPT, NULL);

            gtk_dialog_set_default_response (GTK_DIALOG (edlg), GTK_RESPONSE_CANCEL);
                  
            if (gtk_dialog_run (GTK_DIALOG (edlg)) != GTK_RESPONSE_ACCEPT) {
                g_free (uri);
                uri = NULL;
            }
                
            gtk_widget_destroy (edlg);
        } 
             
        if (uri != NULL)
            break;
    }
  
    gtk_widget_destroy (dialog);
    return uri;
}

gchar*      
seahorse_util_chooser_open_prompt (GtkWidget *dialog)
{
    gchar *uri = NULL;
    
    if(gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT)
        uri = gtk_file_chooser_get_uri (GTK_FILE_CHOOSER (dialog));
        
    gtk_widget_destroy (dialog);
    return uri;
}

/**
 * seahorse_util_check_suffix:
 * @path: Path of file to check
 * @suffix: Suffix type to check for
 *
 * Checks that @path has a suffix specified by @suffix.
 *
 * Returns: TRUE if the file has a correct suffix, FALSE otherwise
 **/
gboolean
seahorse_util_check_suffix (const gchar *path, SeahorseSuffix suffix)
{
	if (suffix == SEAHORSE_SIG_SUFFIX)
        return g_str_has_suffix (path, SEAHORSE_EXT_SIG);
	else
        return g_str_has_suffix (path, SEAHORSE_EXT_PGP) ||
               g_str_has_suffix (path, SEAHORSE_EXT_GPG) ||
               g_str_has_suffix (path, SEAHORSE_EXT_ASC);
}

/**
 * seahorse_util_add_suffix:
 * @ctx: Gpgme Context
 * @path: Path of an existing file
 * @suffix: Suffix type
 * @prompt: Overwrite prompt text
 *
 * Constructs a new path for a file based on @path plus a suffix determined by
 * @suffix and the ASCII Armor setting of @ctx. If ASCII Armor is enabled, the
 * suffix will be '.asc'. Otherwise the suffix will be '.pgp' if @suffix is
 * %SEAHORSE_CRYPT_SUFFIX or '.sig' if @suffix is %SEAHORSE_SIG_SUFFIX.
 *
 * Returns: A new path with the suffix appended to @path. NULL if prompt cancelled
 **/
gchar*
seahorse_util_add_suffix (gpgme_ctx_t ctx, const gchar *path, 
                          SeahorseSuffix suffix, const gchar *prompt)
{
    GtkWidget *dialog;
    const gchar *ext;
    gchar *uri;
    gchar *t;
	
	if (suffix == SEAHORSE_CRYPT_SUFFIX)
        ext = seahorse_gconf_get_boolean (ARMOR_KEY) ? SEAHORSE_EXT_ASC : SEAHORSE_EXT_PGP;
	else if (suffix == SEAHORSE_ASC_SUFFIX)
		ext = SEAHORSE_EXT_ASC;
	else
		ext = SEAHORSE_EXT_SIG;
	
	uri = g_strdup_printf ("%s%s", path, ext);
    
    if (prompt && uri && seahorse_util_uri_exists (uri)) {
            
        t = g_strdup_printf (prompt, seahorse_util_uri_get_last (uri));
        dialog = seahorse_util_chooser_save_new (t, NULL);
        g_free (t);

        seahorse_util_chooser_show_key_files (dialog);
		gtk_file_chooser_select_uri (GTK_FILE_CHOOSER (dialog), uri);

        g_free (uri);                
        uri = NULL;
            
        uri = seahorse_util_chooser_save_prompt (dialog);
    }

    return uri;         
}

/**
 * seahorse_util_remove_suffix:
 * @path: Path with a suffix
 * @prompt:Overwrite prompt text
 *
 * Removes a suffix from @path. Does not check if @path actually has a suffix.
 *
 * Returns: @path without a suffix. NULL if prompt cancelled
 **/
gchar*
seahorse_util_remove_suffix (const gchar *path, const gchar *prompt)
{
    GtkWidget *dialog;
    gchar *uri;
    gchar *t;
    
    g_return_val_if_fail (path != NULL, NULL);
    uri =  g_strndup (path, strlen (path) - 4);
   
    if (prompt && uri && seahorse_util_uri_exists (uri)) {
            
        t = g_strdup_printf (prompt, seahorse_util_uri_get_last (uri));
        dialog = seahorse_util_chooser_save_new (t, NULL);
        g_free (t);

        seahorse_util_chooser_show_key_files (dialog);
		gtk_file_chooser_select_uri (GTK_FILE_CHOOSER (dialog), uri);
		
        g_free (uri);                
        uri = NULL;
            
        uri = seahorse_util_chooser_save_prompt (dialog);
    }   
    
    return uri;
}

gchar**
seahorse_util_strvec_dup (const gchar** vec)
{
    gint len = 0;
    gchar** ret;
    const gchar** v;
    
    if (vec) {
        for(v = vec; *v; v++)
            len++;
    }
   
    ret = (gchar**)g_new0(gchar*, len + 1);

    while((--len) >= 0)
        ret[len] = g_strdup(vec[len]);
    
    return ret;
}

gpgme_key_t* 
seahorse_util_keylist_to_keys (GList *keys)
{
    gpgme_key_t *recips;
    int i;
    
    recips = g_new0 (gpgme_key_t, g_list_length (keys) + 1);
    
    for (i = 0; keys != NULL; keys = g_list_next (keys), i++) {
        g_return_val_if_fail (SEAHORSE_IS_KEY (keys->data), recips);
        recips[i] = SEAHORSE_KEY (keys->data)->key;
        gpgmex_key_ref (recips[i]);
    }
    
    return recips;
}

void
seahorse_util_free_keys (gpgme_key_t* keys)
{
    gpgme_key_t* k = keys;
    while (*k)
        gpgmex_key_unref (*(k++));
    g_free (keys);
}

static gint 
sort_keys_by_source (SeahorseKey *k1, SeahorseKey *k2)
{
    SeahorseKeySource *sk1, *sk2;
    
    g_return_val_if_fail (SEAHORSE_IS_KEY (k1), 0);
    g_return_val_if_fail (SEAHORSE_IS_KEY (k2), 0);
    
    sk1 = seahorse_key_get_source (k1);
    sk2 = seahorse_key_get_source (k2);
    
    if (sk1 == sk2)
        return 0;
    
    return sk1 < sk2 ? -1 : 1;
}

GList*        
seahorse_util_keylist_sort (GList *keys)
{
    return g_list_sort (keys, (GCompareFunc)sort_keys_by_source);
}

GList*       
seahorse_util_keylist_splice (GList *keys)
{
    SeahorseKeySource *psk = NULL;
    SeahorseKeySource *sk;
    GList *prev = NULL;
    
    /* Note that the keylist must be sorted */
    
    for ( ; keys; keys = g_list_next (keys)) {
     
        g_return_val_if_fail (SEAHORSE_IS_KEY (keys->data), NULL);
        sk = seahorse_key_get_source (SEAHORSE_KEY (keys->data));
        
        /* Found a disconuity */
        if (psk && sk != psk) {
            g_assert (prev != NULL);
            
            /* Break the list */
            prev->next = NULL;
            
            /* And return the new list */
            return keys;
        }
        
        psk = sk;
        prev = keys;
    }
    
    return NULL;
}

gboolean    
seahorse_util_string_equals (const gchar *s1, const gchar *s2)
{
    if (!s1 && !s2)
        return TRUE;
    if (!s1 || !s2)
        return FALSE;
    return g_str_equal (s1, s2);
}

void        
seahorse_util_string_lower (gchar *s)
{
    for ( ; *s; s++)
        *s = g_ascii_tolower (*s);
}

/* Free a GSList along with string values */
GSList*
seahorse_util_string_slist_free (GSList *list)
{
    GSList *l;
    for (l = list; l; l = l->next)
        g_free (l->data);
    g_slist_free (list);
    return NULL;
}

/* Copy a GSList along with string values */
GSList*
seahorse_util_string_slist_copy (GSList *list)
{
    GSList *l = NULL;
    for ( ; list; list = g_slist_next(list))
        l = g_slist_append (l, g_strdup(list->data));
    return l;
}

/* Compare two string GSLists */
gboolean    
seahorse_util_string_slist_equal (GSList *l1, GSList *l2)
{
    while (l1 && l2) {
        if (!g_str_equal ((const gchar*)l1->data, (const gchar*)l2->data))
            break;
        l1 = g_slist_next (l1);
        l2 = g_slist_next (l2);
    }
    
    return !l1 && !l2;   
}
