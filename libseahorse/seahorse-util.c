/*
 * Seahorse
 *
 * Copyright (C) 2003 Jacob Perkins
 * Copyright (C) 2004-2005 Stefan Walter
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

#include "seahorse-util.h"
#include "seahorse-key.h"
#include "seahorse-gconf.h"

#include "pgp/seahorse-pgp-module.h"

#include "ssh/seahorse-ssh-module.h"

#include <gio/gio.h>
#include <glib/gstdio.h>

#ifdef WITH_SHARING
#include <avahi-glib/glib-watch.h>
#include <avahi-glib/glib-malloc.h>
#endif 

#include <sys/wait.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <glib/gi18n.h>

static const gchar *bad_filename_chars = "/\\<>|";

void
seahorse_util_show_error (GtkWidget *parent, const gchar *heading, const gchar *message)
{
	GtkWidget *error;
	gchar *text;
    
	g_return_if_fail (message || heading);
	if (!message)
		message = "";
    
	if (heading)
		text = g_strconcat("<big><b>", heading, "</b></big>\n\n", message, NULL);
	else
		text = g_strdup (message);
	
	if (parent) {
		if (!GTK_IS_WIDGET (parent)) {
			g_warn_if_reached ();
			parent = NULL;
		} else {
			if (!GTK_IS_WINDOW (parent)) 
				parent = gtk_widget_get_toplevel (parent);
			if (!GTK_IS_WINDOW (parent) && GTK_WIDGET_TOPLEVEL (parent))
				parent = NULL;
		}
	}
	
	error = gtk_message_dialog_new_with_markup (GTK_WINDOW (parent), 
	                                            GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR,
	                                            GTK_BUTTONS_CLOSE, text);
	g_free (text);
	
	gtk_dialog_run (GTK_DIALOG (error));
	gtk_widget_destroy (error);
}

void
seahorse_util_handle_error (GError* err, const char* desc, ...)
{
    gchar *t = NULL;
    
    va_list ap;
    
    if(!err)
        return;

    va_start(ap, desc);
  
    if (desc)
        t = g_strdup_vprintf (desc, ap);
    
    va_end(ap);        
    
    seahorse_util_show_error (NULL, t, err->message ? err->message : "");
    g_free(t);
}    

gboolean
seahorse_util_prompt_delete (const gchar *text)
{
	GtkWidget *warning, *button;
	gint response;
	
	warning = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL,
	                                  GTK_MESSAGE_QUESTION, GTK_BUTTONS_NONE,
	                                  "%s", text);
    
	button = gtk_button_new_from_stock(GTK_STOCK_CANCEL);
	gtk_dialog_add_action_widget (GTK_DIALOG (warning), GTK_WIDGET (button), GTK_RESPONSE_REJECT);
	gtk_widget_show (button);
	
	button = gtk_button_new_from_stock(GTK_STOCK_DELETE);
	gtk_dialog_add_action_widget (GTK_DIALOG (warning), GTK_WIDGET (button), GTK_RESPONSE_ACCEPT);
	gtk_widget_show (button);
	
	response = gtk_dialog_run (GTK_DIALOG (warning));
	gtk_widget_destroy (warning);
	
	return (response == GTK_RESPONSE_ACCEPT);
}

/**
 * seahorse_util_error_domain
 * Returns: The GError domain for generic seahorse errors
 **/
GQuark
seahorse_util_error_domain ()
{
    static GQuark q = 0;
    if(q == 0)
        q = g_quark_from_static_string ("seahorse-error");
    return q;
}

/** 
 * seahorse_util_get_date_string:
 * @time: Time value to parse
 *
 * Creates a string representation of @time for use with gpg.
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
	g_date_set_time_t (created_date, time);
	created_string = g_new (gchar, 11);
	g_date_strftime (created_string, 11, "%Y-%m-%d", created_date);
	return created_string;
}

/** 
 * seahorse_util_get_display_date_string:
 * @time: Time value to parse
 *
 * Creates a string representation of @time for display in the UI.
 *
 * Returns: A string representing @time
 **/
gchar*
seahorse_util_get_display_date_string (const time_t time)
{
	GDate *created_date;
	gchar *created_string;
	
	if (time == 0)
		return "0";
	
	created_date = g_date_new ();
	g_date_set_time_t (created_date, time);
	created_string = g_new (gchar, 11);
	g_date_strftime (created_string, 11, _("%Y-%m-%d"), created_date);
	return created_string;
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
 * seahorse_util_read_to_text:
 * @data: Data to write
 * @len: Length of the data
 *
 * Converts @data to a string. 
 *
 * Returns: The string read from data
 **/
gchar*
seahorse_util_read_to_text (GInputStream *input, guint *len)
{
	gint size = 128;
	gchar *buffer, *text;
	gsize nread = 0;
	GString *string;
	GSeekable *seek;

	if (G_IS_SEEKABLE (input)) {
		seek = G_SEEKABLE (input);
		g_seekable_seek (seek, 0, SEEK_SET, NULL, NULL);
	}

	string = g_string_new ("");
	buffer = g_new (gchar, size);
    
	while (g_input_stream_read_all (input, buffer, size, &nread, NULL, NULL) && nread == size)
		string = g_string_append_len (string, buffer, nread);

	if (len)
		*len = string->len;
    
	text = string->str;
	g_string_free (string, FALSE);
	g_free (buffer);
	
	return text;
}

/** 
 * seahorse_util_read_data_block
 * 
 * Breaks out one block of data (usually a key)
 * 
 * @buf: A string buffer to write the data to.
 * @input: The input stream to read from.
 * @start: The start signature to look for.
 * @end: The end signature to look for.
 * 
 * Returns: The number of bytes copied.
 */
guint
seahorse_util_read_data_block (GString *buf, GInputStream *input, 
                               const gchar *start, const gchar* end)
{
    const gchar *t;
    guint copied = 0;
    gchar ch;
    gsize read;
     
    /* Look for the beginning */
    t = start;
    while (g_input_stream_read_all (input, &ch, 1, &read, NULL, NULL) && read == 1) {
        
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
    while (g_input_stream_read_all (input, &ch, 1, &read, NULL, NULL) && read == 1) {
        
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

gsize
seahorse_util_memory_output_length (GMemoryOutputStream *output)
{
	GSeekable *seekable;
	goffset offset, end;
	
	/* 
	 * This is a replacement for g_memory_output_stream_get_data_size()
	 * which is not available in current version of glib.
	 */
	
	g_return_val_if_fail (G_IS_MEMORY_OUTPUT_STREAM (output), 0);
	g_return_val_if_fail (G_IS_SEEKABLE (output), 0);
	
	seekable = G_SEEKABLE (output);
	offset = g_seekable_tell (seekable);
	
	if (!g_seekable_seek (seekable, 0, G_SEEK_END, NULL, NULL))
		g_return_val_if_reached (0);
	
	end = g_seekable_tell (seekable);
	
	if (offset != end) {
		if (!g_seekable_seek (seekable, offset, G_SEEK_SET, NULL, NULL))
			g_return_val_if_reached (0);
	}
	
	return (gsize)end;
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
            if (errno == EPIPE)
                return FALSE;
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


gchar*      
seahorse_util_filename_for_keys (GList *keys)
{
    SeahorseKey *skey;
    gchar *t, *r;
    
    g_return_val_if_fail (g_list_length (keys) > 0, NULL);

    if (g_list_length (keys) == 1) {
        g_return_val_if_fail (SEAHORSE_IS_KEY (keys->data), NULL);
        skey = SEAHORSE_KEY (keys->data);
        t = seahorse_key_get_simple_name (skey);
        if (t == NULL)
            t = g_strdup (_("Key Data"));
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
    
    g_return_val_if_fail (uri, NULL);
    
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
	GFile *file;
	gboolean exists;

	file = g_file_new_for_uri (uri);
	g_return_val_if_fail (file, FALSE);
	
	exists = g_file_query_exists (file, NULL);
	g_object_unref (file);
	
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
 
    /* Only begin extension with . if provided extension doesn't start with
       one already. */
    if(ext[0] != '.')
        strcat (ret, ".");

    /* Finally append the caller's provided extension. */
    strcat (ret, ext);
    return ret;
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
    GFile *file, *fpackage;
    
    	fpackage = g_file_new_for_uri (package);
    	t = g_file_get_path (fpackage);
    	x = g_shell_quote (t);
    	g_free (t);
    
    	/* create execution */
    	str = g_string_new ("");
    	g_string_printf (str, "file-roller --add-to=%s", x);
    	g_free (x);
    
    	while(*uris) {
    		/* We should never be passed any remote uris at this point */
    		x = g_uri_parse_scheme (*uris);
    		if (x) 
    			file = g_file_new_for_uri (*uris);
    		else
    			file = g_file_new_for_path (*uris);
    		g_free (x);
    		
    		t = g_file_get_path (file);
    		g_object_unref (file);
    		g_return_val_if_fail (t != NULL, FALSE);

    		x = g_shell_quote (t);
    		g_free (t);

    		g_string_append_printf (str, " %s", x);
    		g_free (x);
        
    		uris++;
    	}
        
    	/* Execute the command */
    	cmd = g_string_free (str, FALSE);
    	r = g_spawn_command_line_sync (cmd, &out, NULL, &status, &err);
    	g_free (cmd); 
    
    	/* TODO: This won't work for remote packages if we support them in the future */
    	t = g_file_get_path (fpackage);
	g_chmod (t, S_IRUSR | S_IWUSR);
	g_free (t);
    	g_object_unref (fpackage);

    if (out) {
        g_print (out);
        g_free (out);
    }
    
    if (!r) {
        seahorse_util_handle_error (err, _("Couldn't run file-roller"));
        g_clear_error (&err);
        return FALSE;
    }
    
    if(!(WIFEXITED(status) && WEXITSTATUS(status) == 0)) {
        seahorse_util_show_error(NULL, _("Couldn't package files"), 
                                 _("The file-roller process did not complete successfully"));
        return FALSE;
    }
    
    return TRUE;
}

GQuark
seahorse_util_detect_mime_type (const gchar *mime)
{
	if (!mime || g_ascii_strcasecmp (mime, "application/octet-stream") == 0) {
		g_warning ("couldn't get mime type for data");
		return 0;
	}

	if (g_ascii_strcasecmp (mime, "application/pgp-encrypted") == 0 ||
	    g_ascii_strcasecmp (mime, "application/pgp-keys") == 0)
		return SEAHORSE_PGP;
    
#ifdef WITH_SSH 
	/* TODO: For now all PEM keys are treated as SSH keys */
	else if (g_ascii_strcasecmp (mime, "application/x-ssh-key") == 0 ||
	         g_ascii_strcasecmp (mime, "application/x-pem-key") == 0)
		return SEAHORSE_SSH;
#endif 
    
	g_warning ("unsupported type of key data: %s", mime);
	return 0;
}

GQuark
seahorse_util_detect_data_type (const gchar *data, guint length)
{
	gboolean uncertain;
	gchar *mime;
	GQuark type;
	
	mime = g_content_type_guess (NULL, (const guchar*)data, length, &uncertain);
	g_return_val_if_fail (mime, 0);
	
	type = seahorse_util_detect_mime_type (mime);
	g_free (mime);
	
	return type;
}

GQuark
seahorse_util_detect_file_type (const gchar *uri)
{
	gboolean uncertain;
	gchar *mime;
	GQuark type;
	
	mime = g_content_type_guess (uri, NULL, 0, &uncertain);
	g_return_val_if_fail (mime, 0);
	
	type = seahorse_util_detect_mime_type (mime);
	g_free (mime);
	
	return type;
}

gboolean
seahorse_util_write_file_private (const gchar* filename, const gchar* contents, GError **err)
{
    mode_t mask = umask (0077);
    gboolean ret = g_file_set_contents (filename, contents, -1, err);
    umask (mask);
    return ret;
}

GtkDialog*
seahorse_util_chooser_save_new (const gchar *title, GtkWindow *parent)
{
    GtkDialog *dialog;
    
    dialog = GTK_DIALOG (gtk_file_chooser_dialog_new (title, 
                parent, GTK_FILE_CHOOSER_ACTION_SAVE, 
                GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
                NULL));

    gtk_dialog_set_default_response (dialog, GTK_RESPONSE_ACCEPT);
    gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER (dialog), FALSE);
    return dialog;
}

GtkDialog*
seahorse_util_chooser_open_new (const gchar *title, GtkWindow *parent)
{
    GtkDialog *dialog;
    
    dialog = GTK_DIALOG (gtk_file_chooser_dialog_new (title, 
                parent, GTK_FILE_CHOOSER_ACTION_OPEN, 
                GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
                NULL));

    gtk_dialog_set_default_response (dialog, GTK_RESPONSE_ACCEPT);
    gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER (dialog), FALSE);
    return dialog;
}
void
seahorse_util_chooser_show_key_files (GtkDialog *dialog)
{
    GtkFileFilter* filter = gtk_file_filter_new ();
    
    /* Filter for PGP keys. We also include *.asc, as in many 
       cases that extension is associated with text/plain */
    gtk_file_filter_set_name (filter, _("All key files"));
    gtk_file_filter_add_mime_type (filter, "application/pgp-keys");
#ifdef WITH_SSH 
    gtk_file_filter_add_mime_type (filter, "application/x-ssh-key");
    gtk_file_filter_add_mime_type (filter, "application/x-pem-key");
#endif
    gtk_file_filter_add_pattern (filter, "*.asc");
    gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), filter);    
    gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (dialog), filter);

    filter = gtk_file_filter_new ();
    gtk_file_filter_set_name (filter, _("All files"));
    gtk_file_filter_add_pattern (filter, "*");    
    gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), filter);       
}

void
seahorse_util_chooser_show_archive_files (GtkDialog *dialog)
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
seahorse_util_chooser_set_filename (GtkDialog *dialog, GList *keys)
{
    gchar *t = NULL;
    
    if (g_list_length (keys) > 0) {
        t = seahorse_util_filename_for_keys (keys);
        gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (dialog), t);
        g_free (t);
    }
}
    
gchar*      
seahorse_util_chooser_save_prompt (GtkDialog *dialog)
{
    GtkWidget* edlg;
    gchar *uri = NULL;
    
    while (gtk_dialog_run (dialog) == GTK_RESPONSE_ACCEPT) {
     
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
seahorse_util_chooser_open_prompt (GtkDialog *dialog)
{
    gchar *uri = NULL;
    
    if(gtk_dialog_run (dialog) == GTK_RESPONSE_ACCEPT)
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
        return g_str_has_suffix (path, SEAHORSE_EXT_SIG) || 
               g_str_has_suffix (path, SEAHORSE_EXT_ASC);
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
 * @suffix. If ASCII Armor is enabled, the suffix will be '.asc'. Otherwise the 
 * suffix will be '.pgp' if @suffix is %SEAHORSE_CRYPT_SUFFIX or '.sig' if 
 * @suffix is %SEAHORSE_SIG_SUFFIX.
 *
 * Returns: A new path with the suffix appended to @path. NULL if prompt cancelled
 **/
gchar*
seahorse_util_add_suffix (const gchar *path, SeahorseSuffix suffix, const gchar *prompt)
{
    GtkDialog *dialog;
    const gchar *ext;
    gchar *uri;
    gchar *t;
    
    if (suffix == SEAHORSE_CRYPT_SUFFIX)
        ext = SEAHORSE_EXT_PGP;
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
        gtk_widget_destroy (dialog);
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
    GtkDialog *dialog;
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
        gtk_widget_destroy (dialog);
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

guint 
seahorse_util_strvec_length (const gchar **vec)
{
    guint len = 0;
    while (*(vec++))
        len++;
    return len;
}

static gint 
sort_keys_by_source (SeahorseKey *k1, SeahorseKey *k2)
{
    SeahorseKeySource *sk1, *sk2;
    
    g_assert (SEAHORSE_IS_KEY (k1));
    g_assert (SEAHORSE_IS_KEY (k2));
    
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

gchar*
seahorse_util_string_up_first (const gchar *orig)
{
    gchar *t, *t2, *ret;
    
    if (g_utf8_validate (orig, -1, NULL)) {
        
        t = g_utf8_find_next_char (orig, NULL); 
        if (t != NULL) {
            t2 = g_utf8_strup (orig, t - orig);
            ret = g_strdup_printf ("%s%s", t2, t);
            g_free (t2);
            
        /* Can't find first UTF8 char */
        } else {
            ret = g_strdup (orig);
        }
    
    /* Just use ASCII functions when not UTF8 */        
    } else {
        ret = g_strdup (orig);
        ret[0] = g_ascii_toupper (ret[0]);
    }
    
    return ret;
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

gboolean 
seahorse_util_string_is_whitespace (const gchar *text)
{
    g_assert (text);
    g_assert (g_utf8_validate (text, -1, NULL));
    
    while (*text) {
        if (!g_unichar_isspace (g_utf8_get_char (text)))
            return FALSE;
        text = g_utf8_next_char (text);
    }
    return TRUE;
}

void
seahorse_util_string_trim_whitespace (gchar *text)
{
    gchar *b, *e, *n;
    
    g_assert (text);
    g_assert (g_utf8_validate (text, -1, NULL));
    
    /* Trim the front */
    b = text;
    while (*b && g_unichar_isspace (g_utf8_get_char (b)))
        b = g_utf8_next_char (b);
    
    /* Trim the end */
    n = e = b + strlen (b);
    while (n >= b) {
        if (*n && !g_unichar_isspace (g_utf8_get_char (n)))
            break;
        e = n;
        n = g_utf8_prev_char (e);
    }
    
    g_assert (b >= text);
    g_assert (e >= b);

    *e = 0;
    g_memmove (text, b, (e + 1) - b);
}

/* Callback to determine where a popup menu should be placed */
void
seahorse_util_determine_popup_menu_position (GtkMenu *menu, int *x, int *y,
                                             gboolean *push_in, gpointer  gdata)
{
        GtkWidget      *widget;
        GtkRequisition  requisition;
        gint            menu_xpos;
        gint            menu_ypos;

        widget = GTK_WIDGET (gdata);

        gtk_widget_size_request (GTK_WIDGET (menu), &requisition);

        gdk_window_get_origin (widget->window, &menu_xpos, &menu_ypos);

        menu_xpos += widget->allocation.x;
        menu_ypos += widget->allocation.y;


        if (menu_ypos > gdk_screen_get_height (gtk_widget_get_screen (widget)) / 2)
                menu_ypos -= requisition.height;
        else
                menu_ypos += widget->allocation.height;
           
        *x = menu_xpos;
        *y = menu_ypos;
        *push_in = TRUE;
}            

/* -----------------------------------------------------------------------------
 * DNS-SD Stuff 
 */

#ifdef WITH_SHARING

/* 
 * We have this stuff here because it needs to:
 *  - Be usable from libseahorse and anything within libseahorse
 *  - Be initialized properly only once per process. 
 */

#include <avahi-glib/glib-watch.h>
#include <avahi-glib/glib-malloc.h>

static AvahiGLibPoll *avahi_poll = NULL;

static void
free_avahi ()
{
    if (avahi_poll)
        avahi_glib_poll_free (avahi_poll);
    avahi_poll = NULL;
}

const AvahiPoll*
seahorse_util_dns_sd_get_poll ()
{
    if (!avahi_poll) {
        
        avahi_set_allocator (avahi_glib_allocator ());
        
        avahi_poll = avahi_glib_poll_new (NULL, G_PRIORITY_DEFAULT);
        if (!avahi_poll) {
            g_warning ("couldn't initialize avahi glib poll integration");
            return NULL;
        }
        
        g_atexit (free_avahi);
    }
    
    return avahi_glib_poll_get (avahi_poll);
}

#endif
