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

#include "seahorse-util.h"
#include "seahorse-key.h"

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

const gchar*
seahorse_util_get_date_string (const time_t time)
{
	GDate *created_date;
	gchar *created_string;
	
	created_date = g_date_new ();
	g_date_set_time (created_date, time);
	created_string = g_new (gchar, 11);
	g_date_strftime (created_string, 11, _("%Y-%m-%d"), created_date);
	return created_string;
}
