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

#include "seahorse-util.h"
#include "seahorse-key.h"

#define ASC ".asc"
#define SIG ".sig"
#define GPG ".gpg"

void
seahorse_util_show_error (GtkWindow *parent, const gchar *message)
{
	GtkWidget *error;
	g_return_if_fail (!g_str_equal (message, ""));
	
	error = gtk_message_dialog_new (parent, GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR,
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
seahorse_util_handle_error (gpgme_error_t err)
{
	GtkWidget *dialog;
	
	g_printerr ("%d", err);
	
	dialog = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR,
		GTK_BUTTONS_CLOSE, gpgme_strerror (err));
	gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);
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
	FILE *fp;
	gchar *buffer;
	gint nread;
   
    /* 
     * TODO: gpgme_data_seek doesn't work for us right now
     * probably because of different off_t sizes 
     */
    gpgme_data_rewind (data);
	
	fp = fopen (path, "w");
	
	if (fp == NULL) {
		gpgme_data_release (data);
		g_return_val_if_reached (gpgme_err_code_from_errno (errno));
	}
	
	buffer = g_new0 (gchar, 128);
	
	while ((nread = gpgme_data_read (data, buffer, 128)) > 0)
		fwrite (buffer, nread, 1, fp);
	
	fflush (fp);
	fclose (fp);
	gpgme_data_release (data);
	
	return GPG_OK;
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
