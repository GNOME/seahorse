/* 
 * Seahorse
 * 
 * Copyright (C) 2009 Stefan Walter
 * 
 * This program is free software; you can redistribute it and/or modify 
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *  
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *  
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.  
 */

#include "config.h"
#include "seahorse-gkr-dialogs.h"

#include <gtk/gtk.h>

static void
update_wait_cursor (GtkWidget *dialog, gpointer unused)
{
	GdkCursor *cursor;
    
	g_return_if_fail (gtk_widget_get_window (dialog));
    
	/* No request active? */
	if (!g_object_get_data (G_OBJECT (dialog), "gkr-request")) {
		gdk_window_set_cursor (gtk_widget_get_window (dialog), NULL);
		return;
	}
    
	/* 
	 * Get the wait cursor. Create a new one and cache it on the widget 
	 * if first time.
	 */
	cursor = (GdkCursor*)g_object_get_data (G_OBJECT (dialog), "wait-cursor");
	if (!cursor) {
		cursor = gdk_cursor_new (GDK_WATCH);
		g_object_set_data_full (G_OBJECT (dialog), "wait-cursor", cursor, g_object_unref);
	}
    
	/* Indicate that we're loading stuff */
	gdk_window_set_cursor (gtk_widget_get_window (dialog), cursor);
}

static void
cancel_and_unref_cancellable (gpointer data)
{
	GCancellable *cancellable = data;
	g_cancellable_cancel (cancellable);
	g_object_unref (cancellable);
}

GCancellable *
seahorse_gkr_dialog_begin_request (SeahorseWidget *swidget)
{
	GCancellable *cancellable;
	GtkWidget *dialog;

	dialog = seahorse_widget_get_toplevel (swidget);
	g_return_val_if_fail (GTK_IS_WIDGET (dialog), NULL);

	/* Cancel any old operation going on */
	seahorse_gkr_dialog_complete_request (swidget, TRUE);
    
	/* 
	 * Start the operation and tie it to the widget so that it will get 
	 * cancelled if the widget is destroyed before the operation is complete
	 */ 
	cancellable = g_cancellable_new ();
	g_object_set_data_full (G_OBJECT (dialog), "gkr-request",
	                        g_object_ref (cancellable),
	                        cancel_and_unref_cancellable);

	if (gtk_widget_get_realized (dialog))
		update_wait_cursor (dialog, NULL);
	else
		g_signal_connect (dialog, "realize", G_CALLBACK (update_wait_cursor), dialog);
    
	gtk_widget_set_sensitive (dialog, FALSE);    
	return cancellable;
}

void
seahorse_gkr_dialog_complete_request (SeahorseWidget *swidget, gboolean cancel)
{
	GtkWidget *dialog;
	GCancellable *cancellable;

	dialog = seahorse_widget_get_toplevel (swidget);
	g_return_if_fail (GTK_IS_WIDGET (dialog));

	cancellable = g_object_steal_data (G_OBJECT (dialog), "gkr-request");
	if (cancellable && cancel)
		g_cancellable_cancel (cancellable);
	if (cancellable)
		g_object_unref (cancellable);

	if (gtk_widget_get_realized (dialog))
		update_wait_cursor (dialog, NULL);
	gtk_widget_set_sensitive (dialog, TRUE);
}
