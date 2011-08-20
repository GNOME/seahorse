/*
 * Seahorse
 *
 * Copyright (C) 2003 Jacob Perkins
 * Copyright (C) 2004-2005 Stefan Walter
 * Copyright (C) 2011 Collabora Ltd.
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

#include "seahorse-object.h"
#include "seahorse-util.h"
#include "seahorse-widget.h"

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
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <glib/gi18n.h>

#include <sys/types.h>

static const gchar *bad_filename_chars = "/\\<>|";

/**
 * seahorse_util_show_error:
 * @parent: The parent widget. Can be NULL
 * @heading: The heading of the dialog
 * @message: The message to display
 *
 * This displays an error dialog.
 * The parent widget can be any widget. The dialog will be a child of the window
 * the widget is in.
 *
 */
void
seahorse_util_show_error (GtkWidget *parent, const gchar *heading, const gchar *message)
{
	GtkWidget *dialog;

	g_return_if_fail (message || heading);
	if (!message)
		message = "";

	if (parent) {
		if (!GTK_IS_WIDGET (parent)) {
			g_warn_if_reached ();
			parent = NULL;
		} else {
			if (!GTK_IS_WINDOW (parent)) 
				parent = gtk_widget_get_toplevel (parent);
			if (!GTK_IS_WINDOW (parent) && gtk_widget_is_toplevel (parent))
				parent = NULL;
		}
	}
	
	dialog = gtk_message_dialog_new (GTK_WINDOW (parent), 
	                                GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR,
	                                GTK_BUTTONS_CLOSE, NULL);
	if (heading)
	    g_object_set (G_OBJECT (dialog),
	                  "text", heading,
	                  "secondary-text", message,
	                  NULL);
    else
        g_object_set (G_OBJECT (dialog),
	                  "text", message,
	                  NULL);
	
	
	gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);
}

/**
 * seahorse_util_handle_error:
 * @error: The #GError to print, and clear
 * @desc: The heading of the box
 * @...: Parameters to insert into the format string desc.
 *
 * Displays an error box. The message is the error message.
 * Won't display cancel errors.
 */
void
seahorse_util_handle_error (GError **error,
                            gpointer parent,
                            const char* description,
                            ...)
{
	gchar *text = NULL;
	GtkWidget *widget = NULL;
	va_list ap;

	if (!error || !(*error) ||
	    g_error_matches (*error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
		g_clear_error (error);
		return;
	}

	va_start (ap, description);
	if (description)
		text = g_strdup_vprintf (description, ap);
	va_end (ap);

	if (parent == NULL)
		widget = NULL;
	else if (GTK_IS_WIDGET (parent))
		widget = GTK_WIDGET (parent);
	else if (GTK_IS_WINDOW (parent))
		widget = GTK_WIDGET (parent);
	else if (SEAHORSE_IS_WIDGET (parent))
		widget = seahorse_widget_get_toplevel (parent);
	else
		g_warning ("unsupported 'parent' argument passed to seahorse_util_handle_error() ");

	seahorse_util_show_error (widget, text,
	                          (*error)->message ? (*error)->message : "");
	g_free (text);
	g_clear_error (error);
}

/**
 * seahorse_util_prompt_delete:
 * @text: The text to display in the delete-dialog
 * @parent: The widget to display the dialog for. Can be NULL
 *
 * Displays a modal dialog with "cancel" and "delete"
 *
 * Returns: TRUE if the user pressed "delete", FALSE else
 */
gboolean
seahorse_util_prompt_delete (const gchar *text, GtkWidget *parent)
{
	GtkWidget *warning, *button;
	gint response;
	
	if (parent) {
		if (!GTK_IS_WIDGET (parent)) {
			g_warn_if_reached ();
			parent = NULL;
		} else {
			if (!GTK_IS_WINDOW (parent)) 
				parent = gtk_widget_get_toplevel (parent);
			if (!GTK_IS_WINDOW (parent) && gtk_widget_is_toplevel (parent))
				parent = NULL;
		}
	}
	
	warning = gtk_message_dialog_new (GTK_WINDOW (parent), GTK_DIALOG_MODAL,
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
 * seahorse_util_error_domain:
 *
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
 * Returns: A string representing @time. The returned string should be freed
 * with #g_free when no longer needed.
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
 * Returns: A string representing @time. The returned string should be freed 
 * with #g_free when no longer needed.
 **/
gchar*
seahorse_util_get_display_date_string (const time_t time)
{
	GDate *created_date;
	gchar *created_string;
	
	if (time == 0)
		return g_strdup ("");
	
	created_date = g_date_new ();
	g_date_set_time_t (created_date, time);
	created_string = g_new (gchar, 11);
	g_date_strftime (created_string, 11, _("%Y-%m-%d"), created_date);
	return created_string;
}

/**
 * seahorse_util_get_text_view_text:
 * @view: The text view #GtkTextView to extract text from
 *
 * Returns the whole text buffer
 *
 * Returns: The text buffer extracted. The returned string should be freed with 
 * #g_free when no longer needed.
 */
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

/**
 * seahorse_util_set_text_view_string:
 * @view: The view to set the text for (#GtkTextView)
 * @string: The string to set (#GString)
 *
 *
 */
void
seahorse_util_set_text_view_string (GtkTextView *view, GString *string)
{
    GtkTextBuffer *buffer;
	g_return_if_fail (view != NULL && string != NULL);
	
	buffer = gtk_text_view_get_buffer (view);
	gtk_text_buffer_set_text (buffer, string->str, string->len);
}

/**
 * seahorse_util_read_to_memory:
 * @input: Data to read. The #GInputStream is read till the end.
 * @len: Length of the data read (out)
 *
 * Reads data from the input stream and returns them as #guchar
 *
 * Returns: The string read from data. The returned string should be freed
 * with #g_free when no longer needed.
 **/
guchar*
seahorse_util_read_to_memory (GInputStream *input, guint *len)
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
    
	while (g_input_stream_read_all (input, buffer, size, &nread, NULL, NULL)) {
		string = g_string_append_len (string, buffer, nread);
		if (nread != size)
			break;
	}

	if (len)
		*len = string->len;
    
	text = g_string_free (string, FALSE);
	g_free (buffer);
	
	return (guchar*)text;
}

/** 
 * seahorse_util_read_data_block:
 * @buf: A string buffer to write the data to.
 * @input: The input stream to read from.
 * @start: The start signature to look for.
 * @end: The end signature to look for.
 *
 * Breaks out one block of data (usually a key)
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

/** 
 * seahorse_util_print_fd:
 * @fd: The file descriptor to write to
 * @s:  The data to write
 *
 * Returns: FALSE on error, TRUE on success
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
 * @...: The parameters to insert
 *
 * Returns: TRUE on success, FALSE on error
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
 * seahorse_util_filename_for_objects:
 * @objects: A list of objects
 *
 * If the single object has a nickname, this will be returned (with .asc attached)
 * If there are multiple objects, "Multiple Keys.asc" will be returned.
 * Single objects default to "Key Data.asc".
 * Results are internationalized
 *
 * Returns: NULL on error, the filename else. The returned string should be
 * freed with #g_free when no longer needed.
 */
gchar*      
seahorse_util_filename_for_objects (GList *objects)
{
	SeahorseObject *object;
	const gchar *name;
	gchar *filename;
    
	g_return_val_if_fail (g_list_length (objects) > 0, NULL);

	if (g_list_length (objects) == 1) {
		g_return_val_if_fail (SEAHORSE_IS_OBJECT (objects->data), NULL);
		object = SEAHORSE_OBJECT (objects->data);
		name = seahorse_object_get_nickname (object);
		if (name == NULL)
			name = _("Key Data");
	} else {
		name = _("Multiple Keys");
	}
    
	filename = g_strconcat (name, SEAHORSE_EXT_ASC, NULL);
	g_strstrip (filename);
	g_strdelimit (filename, bad_filename_chars, '_');
	return filename;
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
 * seahorse_util_uri_exists:
 * @uri: The uri to check
 * 
 * Verify whether a given uri exists or not.
 * 
 * Returns: FALSE if it does not exist, TRUE else
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
 * seahorse_util_uri_unique:
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
 * seahorse_util_detect_mime_type:
 * @mime: The mime string
 *
 * Return the mime type depending on the mime string
 *
 * Returns: SEAHORSE_PGP, SEAHORSE_SSH or 0
 */
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

	g_message ("unsupported type of key data: %s", mime);
	return 0;
}

/**
 * seahorse_util_detect_data_type:
 * @data: The buffer to test for content type
 * @length: The length of this buffer
 *
 * Returns: SEAHORSE_PGP, SEAHORSE_SSH or 0
 */
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

/**
 * seahorse_util_detect_file_type:
 * @uri: The file uri to test for content type
 *
 * Returns: SEAHORSE_PGP, SEAHORSE_SSH or 0
 */
GQuark
seahorse_util_detect_file_type (const gchar *uri)
{
	const gchar *mime = NULL;
	GQuark type;
	GFile *file;
	GFileInfo *info;

	file = g_file_new_for_uri (uri);
	info = g_file_query_info (file,
                G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
                0,
                NULL,
                NULL);

	g_return_val_if_fail (info, 0);

	mime = g_file_info_get_content_type (info);
	
	g_return_val_if_fail (mime, 0);
	
	type = seahorse_util_detect_mime_type (mime);
	g_object_unref (info);
	
	return type;
}

/**
 * seahorse_util_write_file_private:
 * @filename: file to write to
 * @contents: nul-terminated string to write to the file
 * @err: error of the write operation
 *
 * Returns: #TRUE on success, #FALSE if an error occured
 */
gboolean
seahorse_util_write_file_private (const gchar* filename, const gchar* contents, GError **err)
{
    mode_t mask = umask (0077);
    gboolean ret = g_file_set_contents (filename, contents, -1, err);
    umask (mask);
    return ret;
}

/**
 * seahorse_util_chooser_save_new:
 * @title: The title of the dialog
 * @parent: The parent of the dialog
 *
 * Creates a file chooser dialog to save files.
 *
 * Returns: The new save dialog
 */
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

/**
 * seahorse_util_chooser_open_new:
 * @title: The title of the dialog
 * @parent: The parent of the dialog
 *
 * Creates a file chooser dialog to open files.
 *
 * Returns: The new open dialog
 */
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

/**
 * seahorse_util_chooser_show_key_files:
 * @dialog: the dialog to add the filter for
 *
 * Adds a key file filter and a "All files" filter. The key filter
 * is used.
 *
 */
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

/**
 * seahorse_util_chooser_show_archive_files:
 * @dialog: the dialog to add the filter for
 *
 * Adds a archive file filter and a "All files" filter. The archive filter
 * is used.
 *
 */
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

/**
 * seahorse_util_chooser_set_filename_full:
 * @dialog: The dialog to pre set the name
 * @objects: generate the file name from this object
 *
 *
 */
void
seahorse_util_chooser_set_filename_full (GtkDialog *dialog, GList *objects)
{
    gchar *t = NULL;
    
    if (g_list_length (objects) > 0) {
        t = seahorse_util_filename_for_objects (objects);
        gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (dialog), t);
        g_free (t);
    }
}

/**
 * seahorse_util_chooser_set_filename:
 * @dialog: set the dialog for this
 * @object: The object to use for the filename. #SeahorseObject
 *
 */
void
seahorse_util_chooser_set_filename (GtkDialog *dialog, SeahorseObject *object)
{
	GList *objects = g_list_append (NULL, object);
	seahorse_util_chooser_set_filename_full (dialog, objects);
	g_list_free (objects);
}

/**
 * seahorse_util_chooser_save_prompt:
 * @dialog: save dialog to show
 *
 * If the selected file already exists, a confirmation dialog will be displayed
 *
 * Returns: the uri of the chosen file or NULL
 */
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
  
    gtk_widget_destroy (GTK_WIDGET (dialog));
    return uri;
}

/**
 * seahorse_util_chooser_open_prompt:
 * @dialog: open dialog to display
 *
 * Display an open dialog
 *
 * Returns: The uri of the file to open or NULL
 */
gchar*      
seahorse_util_chooser_open_prompt (GtkDialog *dialog)
{
    gchar *uri = NULL;
    
    if(gtk_dialog_run (dialog) == GTK_RESPONSE_ACCEPT)
        uri = gtk_file_chooser_get_uri (GTK_FILE_CHOOSER (dialog));

    gtk_widget_destroy (GTK_WIDGET (dialog));
    return uri;
}

/**
 * sort_objects_by_source:
 * @k1: the first seahorse object
 * @k2: The second seahorse object
 *
 * Sorts the seahorse objects by their source
 *
 * Returns: if source of k1<k2 it returns -1,
 *          1 will be returned  if k1>k2. If the sources are equal it returns 0
 */
static gint 
sort_objects_by_source (SeahorseObject *k1, SeahorseObject *k2)
{
    SeahorseSource *sk1, *sk2;
    
    g_assert (SEAHORSE_IS_OBJECT (k1));
    g_assert (SEAHORSE_IS_OBJECT (k2));
    
    sk1 = seahorse_object_get_source (k1);
    sk2 = seahorse_object_get_source (k2);
    
    if (sk1 == sk2)
        return 0;
    
    return sk1 < sk2 ? -1 : 1;
}


/**
 * seahorse_util_objects_sort:
 * @objects: #SeahorseObject #GList to sort
 *
 * The objects are sorted by their source
 *
 * Returns: The sorted list
 */
GList*        
seahorse_util_objects_sort (GList *objects)
{
    return g_list_sort (objects, (GCompareFunc)sort_objects_by_source);
}


/**
 * seahorse_util_objects_splice:
 * @objects: A #GList of #SeahorseObject. Must be sorted
 *
 * Splices the list at the source disconuity
 *
 * Returns: The second part of the list.
 */
GList*       
seahorse_util_objects_splice (GList *objects)
{
    SeahorseSource *psk = NULL;
    SeahorseSource *sk;
    GList *prev = NULL;
    
    /* Note that the objects must be sorted */
    
    for ( ; objects; objects = g_list_next (objects)) {
     
        g_return_val_if_fail (SEAHORSE_IS_OBJECT (objects->data), NULL);
        sk = seahorse_object_get_source (SEAHORSE_OBJECT (objects->data));
        
        /* Found a disconuity */
        if (psk && sk != psk) {
            g_assert (prev != NULL);
            
            /* Break the list */
            prev->next = NULL;
            
            /* And return the new list */
            return objects;
        }
        
        psk = sk;
        prev = objects;
    }
    
    return NULL;
}

/**
 * seahorse_util_string_equals:
 * @s1: String, can be NULL
 * @s2: String, can be NULL
 *
 * Compares two string. If they are equal, it returns TRUE
 *
 * Returns: TRUE if strings are equal, FALSE else
 */
gboolean    
seahorse_util_string_equals (const gchar *s1, const gchar *s2)
{
    if (!s1 && !s2)
        return TRUE;
    if (!s1 || !s2)
        return FALSE;
    return g_str_equal (s1, s2);
}

/**
 * seahorse_util_string_lower:
 * @s: ASCII string to change
 *
 * The whole ASCII string will be lower cased.
 */
void        
seahorse_util_string_lower (gchar *s)
{
    for ( ; *s; s++)
        *s = g_ascii_tolower (*s);
}

/**
 * seahorse_util_string_is_whitespace:
 * @text: The UTF8 string to test
 *
 *
 * Returns: TRUE if @text consists of whitespaces
 */
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

/**
 * seahorse_util_string_trim_whitespace:
 * @text: The text to trim (UTF8)
 *
 * Whitespaces will be removed from the start and the end of the text.
 */
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

/**
 * seahorse_util_hex_encode:
 * @value: a buffer containing data
 * @length: The length of this buffer
 *
 * Creates a string contining the @value in hex for printing.
 *
 * Returns: The hex encoded @value. The returned string should be freed
 * with #g_free when no longer needed.
 */
gchar*
seahorse_util_hex_encode (gconstpointer value, gsize length)
{
	GString *result;
	gsize i;
	
	result = g_string_sized_new ((length * 2) + 1);

	for (i = 0; i < length; i++) {
		char hex[3];
		snprintf (hex, sizeof (hex), "%02x", (int)(((guchar*)value)[i]));
		g_string_append (result, hex);
	}

	return g_string_free (result, FALSE);
}

/**
 * seahorse_util_determine_popup_menu_position:
 * @menu: The menu to place
 * @x: (out) x pos of the menu
 * @y: (out) y pos of the menu
 * @push_in: (out) will be set to TRUE
 * @gdata: GTK_WIDGET for which the menu is
 *
 *
 * Callback to determine where a popup menu should be placed
 *
 */
void
seahorse_util_determine_popup_menu_position (GtkMenu *menu, int *x, int *y,
                                             gboolean *push_in, gpointer  gdata)
{
        GtkWidget      *widget;
        GtkRequisition  requisition;
        GtkAllocation   allocation;
        gint            menu_xpos;
        gint            menu_ypos;

        widget = GTK_WIDGET (gdata);

        gtk_widget_get_preferred_size (GTK_WIDGET (menu), &requisition, NULL);

        gdk_window_get_origin (gtk_widget_get_window (widget), &menu_xpos, &menu_ypos);

        gtk_widget_get_allocation (widget, &allocation);
        menu_xpos += allocation.x;
        menu_ypos += allocation.y;


        if (menu_ypos > gdk_screen_get_height (gtk_widget_get_screen (widget)) / 2)
                menu_ypos -= requisition.height;
        else
                menu_ypos += allocation.height;

        *x = menu_xpos;
        *y = menu_ypos;
        *push_in = TRUE;
}

/**
 * seahorse_util_parse_version:
 *
 * @version: Version number string in the form xx.yy.zz
 *
 * Converts an (up to) four-part version number into a 64-bit
 * unsigned integer for simple comparison.
 *
 * Returns: SeahorseVersion
 **/
SeahorseVersion
seahorse_util_parse_version (const char *version)
{
	SeahorseVersion ret = 0, tmp = 0;
	int offset = 48;
	gchar **tokens = g_strsplit (version, ".", 5);
	int i;
	for (i=0; tokens[i] && offset >= 0; i++) {
		tmp = atoi(tokens[i]);
		ret += tmp << offset;
		offset -= 16;
	}
	g_strfreev(tokens);
	return ret;
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
seahorse_util_dns_sd_get_poll (void)
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
