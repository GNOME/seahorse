/*
 * Seahorse
 *
 * Copyright (C) 2003 Jacob Perkins
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

#include <gnome.h>
#include <libgnomevfs/gnome-vfs.h>

#include "seahorse-util.h"
#include "seahorse-key.h"
#include "seahorse-vfs-data.h"

#define ASC ".asc"
#define SIG ".sig"
#define GPG ".gpg"

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
 * seahorse_util_handle_error:
 * @err: Error value
 *
 * Shows an error dialog for @err.
 **/
void
seahorse_util_handle_error (gpgme_error_t err, const char* desc, ...)
{
    gchar *t = NULL;
  
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
seahorse_util_handle_gerror (GError* err, const char* desc, ...)
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
    g_error_free(err);
}    



/**
 * seahorse_util_write_data_to_file:
 * @path: Path of file to write to
 * @data: Data to write
 *
 * Tries to write @data to the file @path. @data will be released upon completion.
 *
 * Returns: GPG_ERR_NO_ERROR if write is successful,
 * GPGME_File_Error if the file could not be opened
 **/
gpgme_error_t
seahorse_util_write_data_to_file (const gchar *path, gpgme_data_t data)
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
	
    file = seahorse_vfs_data_create (path, TRUE, &err);
    if(file != NULL)
    {
    	buffer = g_new0 (gchar, 128);
    	
    	while ((nread = gpgme_data_read (data, buffer, 128)) > 0)
        {
            if(gpgme_data_write (file, buffer, nread) < 0)
            {
                gpg_err_code_t e = gpg_err_code_from_errno (errno);
                err = GPG_E (e);                
                break;
            }
        }
    }
	
    gpgme_data_release (file);
	gpgme_data_release (data);
	
	return err;
}

/**
 * seahorse_util_write_data_to_text:
 * @data: Data to write
 *
 * Converts @data to a string. @data will be released upon completion.
 *
 * Returns: The string read from data
 **/
gchar*
seahorse_util_write_data_to_text (gpgme_data_t data)
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
	
	gpgme_data_release (data);
	text = string->str;
	g_string_free (string, FALSE);
	
	return text;
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
    
    if (t == uri)
        return uri;
        
    if (*(t - 1) == '/')
        t--;
    
    while (*(t - 1) != '/')
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
	gchar *ext;
	
	if (suffix == SEAHORSE_SIG_SUFFIX)
		ext = SIG;
	else
		ext = GPG;
	
	return (g_pattern_match_simple (g_strdup_printf ("*%s", ASC), path) ||
		g_pattern_match_simple (g_strdup_printf ("*%s", ext), path));
}

/**
 * seahorse_util_add_suffix:
 * @ctx: Gpgme Context
 * @path: Path of an existing file
 * @suffix: Suffix type
 *
 * Constructs a new path for a file based on @path plus a suffix determined by
 * @suffix and the ASCII Armor setting of @ctx. If ASCII Armor is enabled, the
 * suffix will be '.asc'. Otherwise the suffix will be '.gpg' if @suffix is
 * %SEAHORSE_CRYPT_SUFFIX or '.sig' if @suffix is %SEAHORSE_SIG_SUFFIX.
 *
 * Returns: A new path with the suffix appended to @path
 **/
gchar*
seahorse_util_add_suffix (gpgme_ctx_t ctx, const gchar *path, SeahorseSuffix suffix)
{
	gchar *ext;
	
	if (gpgme_get_armor (ctx) || suffix == SEAHORSE_ASC_SUFFIX)
		ext = ASC;
	else if (suffix == SEAHORSE_CRYPT_SUFFIX)
		ext = GPG;
	else
		ext = SIG;
	
	return g_strdup_printf ("%s%s", path, ext);
}

/**
 * seahorse_util_remove_suffix:
 * @path: Path with a suffix
 *
 * Removes a suffix from @path. Does not check if @path actually has a suffix.
 *
 * Returns: @path without a suffix
 **/
gchar*
seahorse_util_remove_suffix (const gchar *path)
{
	return g_strndup (path, strlen (path) - 4);
}

void
seahorse_util_free_keys (gpgme_key_t* keys)
{
    gpgme_key_t* k = keys;
    while (*k)
        gpgme_key_unref (*(k++));
    g_free (keys);
}
