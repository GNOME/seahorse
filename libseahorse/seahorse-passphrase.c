/*
 * Seahorse
 *
 * Copyright (C) 2003 Jacob Perkins
 * Copyright (C) 2004 - 2006 Stefan Walter
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
#include <sys/types.h>
#include <sys/param.h>
#include <sys/signal.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>

#include <signal.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <err.h>
#include <unistd.h>
#include <syslog.h>
#include <fcntl.h>
#include <errno.h>

#include <glib.h>
#include <glib/gi18n.h>

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <gdk/gdkkeysyms-compat.h>

#include "seahorse-passphrase.h"
#include "seahorse-secure-buffer.h"
#include "seahorse-util.h"
#include "seahorse-widget.h"

#define HIG_SMALL      6        /* gnome hig small space in pixels */
#define HIG_LARGE     12        /* gnome hig large space in pixels */

/* Convert passed text to utf-8 if not valid */
static gchar *
utf8_validate (const gchar *text)
{
    gchar *result;

    if (!text)
        return NULL;

    if (g_utf8_validate (text, -1, NULL))
        return g_strdup (text);

    result = g_locale_to_utf8 (text, -1, NULL, NULL, NULL);
    if (!result) {
        gchar *p;

        result = p = g_strdup (text);
        while (!g_utf8_validate (p, -1, (const gchar **) &p))
            *p = '?';
    }
    return result;
}

static gboolean
key_press (GtkWidget *widget, GdkEventKey *event, gpointer data)
{
    /* Close the dialog when hitting "Esc". */
    if (event->keyval == GDK_Escape)
    {
        gtk_dialog_response(GTK_DIALOG (widget), GTK_RESPONSE_REJECT);
        return TRUE;
    }
    
    return FALSE;
}

static gboolean
grab_keyboard (GtkWidget *win, GdkEvent *event, gpointer data)
{
#ifndef _DEBUG
	if (!g_object_get_data (G_OBJECT (win), "keyboard-grabbed"))
		if (gdk_keyboard_grab (gtk_widget_get_window (win), FALSE, gdk_event_get_time (event)))
			g_message ("could not grab keyboard");
	g_object_set_data (G_OBJECT (win), "keyboard-grabbed", GINT_TO_POINTER (TRUE));
#endif
	return FALSE;
}

/* ungrab_keyboard - remove grab */
static gboolean
ungrab_keyboard (GtkWidget *win, GdkEvent *event, gpointer data)
{
#ifndef _DEBUG
	if (g_object_get_data (G_OBJECT (win), "keyboard-grabbed"))
		gdk_keyboard_ungrab (gdk_event_get_time (event));
	g_object_set_data (G_OBJECT (win), "keyboard-grabbed", NULL);
#endif
	return FALSE;
}

/* When enter is pressed in the confirm entry, move */
static void
confirm_callback (GtkWidget *widget, GtkDialog *dialog)
{
	GtkWidget *entry = GTK_WIDGET (g_object_get_data (G_OBJECT (dialog), "secure-entry"));
	g_assert (GTK_IS_ENTRY (entry));
	gtk_widget_grab_focus (entry);
}

/* When enter is pressed in the entry, we simulate an ok */
static void
enter_callback (GtkWidget *widget, GtkDialog *dialog)
{
	GtkWidget *widg = gtk_dialog_get_widget_for_response (dialog, GTK_RESPONSE_ACCEPT);
	if (gtk_widget_get_sensitive (widg))
		gtk_dialog_response (dialog, GTK_RESPONSE_ACCEPT);
}

static void
entry_changed (GtkEditable *editable, GtkDialog *dialog)
{
	GtkEntry *entry, *confirm;

	entry = GTK_ENTRY (g_object_get_data (G_OBJECT (dialog), "secure-entry"));
	confirm = GTK_ENTRY (g_object_get_data (G_OBJECT (dialog), "confirm-entry"));

	gtk_dialog_set_response_sensitive (dialog, GTK_RESPONSE_ACCEPT,
	                                   strcmp (gtk_entry_get_text (entry),
	                                           gtk_entry_get_text (confirm)) == 0);
}

static gboolean
window_state_changed (GtkWidget *win, GdkEventWindowState *event, gpointer data)
{
	GdkWindowState state = gdk_window_get_state (gtk_widget_get_window (win));
	
	if (state & GDK_WINDOW_STATE_WITHDRAWN ||
	    state & GDK_WINDOW_STATE_ICONIFIED ||
	    state & GDK_WINDOW_STATE_FULLSCREEN ||
	    state & GDK_WINDOW_STATE_MAXIMIZED)
	    	ungrab_keyboard (win, (GdkEvent*)event, data);
	else
		grab_keyboard (win, (GdkEvent*)event, data);
		
	return FALSE;
}

GtkDialog*
seahorse_passphrase_prompt_show (const gchar *title,
                                 const gchar *description,
                                 const gchar *prompt,
                                 const gchar *check,
                                 gboolean confirm)
{
	GtkEntryBuffer *buffer;
	GtkEntry *entry;
	GtkDialog *dialog;
	GtkWidget *widget;
	GtkWidget *box;
	GtkGrid *grid;
	GtkWidget *wvbox;
	GtkWidget *chbox;
	gchar *msg;

	if (!prompt)
		prompt = _("Password:");

	widget = gtk_dialog_new_with_buttons (title, NULL, GTK_DIALOG_MODAL, NULL);
	gtk_window_set_icon_name (GTK_WINDOW (widget), GTK_STOCK_DIALOG_AUTHENTICATION);
	dialog = GTK_DIALOG (widget);

	g_signal_connect (G_OBJECT (dialog), "map-event", G_CALLBACK (grab_keyboard), NULL);
	g_signal_connect (G_OBJECT (dialog), "unmap-event", G_CALLBACK (ungrab_keyboard), NULL);
	g_signal_connect (G_OBJECT (dialog), "window-state-event", G_CALLBACK (window_state_changed), NULL);

	wvbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, HIG_LARGE * 2);
	gtk_container_add (GTK_CONTAINER (gtk_dialog_get_content_area (dialog)), wvbox);
	gtk_container_set_border_width (GTK_CONTAINER (wvbox), HIG_LARGE);

	chbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, HIG_LARGE);
	gtk_box_pack_start (GTK_BOX (wvbox), chbox, FALSE, FALSE, 0);

	/* The image */
	widget = gtk_image_new_from_stock (GTK_STOCK_DIALOG_AUTHENTICATION, GTK_ICON_SIZE_DIALOG);
	gtk_misc_set_alignment (GTK_MISC (widget), 0.0, 0.0);
	gtk_box_pack_start (GTK_BOX (chbox), widget, FALSE, FALSE, 0);

	box = gtk_box_new (GTK_ORIENTATION_VERTICAL, HIG_SMALL);
	gtk_box_pack_start (GTK_BOX (chbox), box, TRUE, TRUE, 0);

	/* The description text */
	if (description) {
		msg = utf8_validate (description);
		widget = gtk_label_new (msg);
		g_free (msg);

		gtk_misc_set_alignment (GTK_MISC (widget), 0.0, 0.5);
		gtk_label_set_line_wrap (GTK_LABEL (widget), TRUE);
		gtk_box_pack_start (GTK_BOX (box), widget, TRUE, FALSE, 0);
	}

	grid = GTK_GRID (gtk_grid_new ());
	gtk_grid_set_row_spacing (grid, HIG_SMALL);
	gtk_grid_set_column_spacing (grid, HIG_LARGE);
	gtk_box_pack_start (GTK_BOX (box), GTK_WIDGET (grid), FALSE, FALSE, 0);

	/* The first entry if we have one */
	if (confirm) {
		msg = utf8_validate (prompt);
		widget = gtk_label_new (msg);
		g_free (msg);
		gtk_misc_set_alignment (GTK_MISC (widget), 0.0, 0.5);
		gtk_grid_attach (grid, widget, 0, 0, 1, 1);

		buffer = seahorse_secure_buffer_new ();
		entry = GTK_ENTRY (gtk_entry_new_with_buffer (buffer));
		g_object_unref (buffer);
		gtk_entry_set_visibility (entry, FALSE);
		gtk_widget_set_size_request (GTK_WIDGET (entry), 200, -1);
		g_object_set_data (G_OBJECT (dialog), "confirm-entry", entry);
		g_signal_connect (G_OBJECT (entry), "activate", G_CALLBACK (confirm_callback), dialog);
		g_signal_connect (G_OBJECT (entry), "changed", G_CALLBACK (entry_changed), dialog);
		gtk_grid_attach (grid, GTK_WIDGET (entry), 1, 0, 1, 1);
		gtk_widget_grab_focus (GTK_WIDGET (entry));
	}

	/* The second and main entry */
	msg = utf8_validate (confirm ? _("Confirm:") : prompt);
	widget = gtk_label_new (msg);
	g_free (msg);
	gtk_misc_set_alignment (GTK_MISC (widget), 0.0, 0.5);
	gtk_grid_attach (grid, widget, 0, 1, 1, 1);

	buffer = seahorse_secure_buffer_new ();
	entry = GTK_ENTRY (gtk_entry_new_with_buffer (buffer));
	g_object_unref (buffer);
	gtk_widget_set_size_request (GTK_WIDGET (entry), 200, -1);
	gtk_entry_set_visibility (entry, FALSE);
	g_object_set_data (G_OBJECT (dialog), "secure-entry", entry);
	g_signal_connect (G_OBJECT (entry), "activate", G_CALLBACK (enter_callback), dialog);
	gtk_grid_attach (grid, GTK_WIDGET (entry), 1, 1, 1, 1);
	if (!confirm)
		gtk_widget_grab_focus (GTK_WIDGET (entry));
	else
		g_signal_connect (G_OBJECT (entry), "changed", G_CALLBACK (entry_changed), dialog);

	/* The checkbox */
	if (check) {
		widget = gtk_check_button_new_with_mnemonic (check);
		gtk_grid_attach (grid, widget, 1, 2, 1, 1);
		g_object_set_data (G_OBJECT (dialog), "check-option", widget);
	}

	gtk_widget_show_all (GTK_WIDGET (grid));

	widget = gtk_button_new_from_stock (GTK_STOCK_CANCEL);
	gtk_dialog_add_action_widget (dialog, widget, GTK_RESPONSE_REJECT);
	gtk_widget_set_can_default (widget, TRUE);

	widget = gtk_button_new_from_stock (GTK_STOCK_OK);
	gtk_dialog_add_action_widget (dialog, widget, GTK_RESPONSE_ACCEPT);
	gtk_widget_set_can_default (widget, TRUE);
	gtk_widget_grab_default (widget);

	g_signal_connect (dialog, "key_press_event", G_CALLBACK (key_press), NULL);

	gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);
	gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
	gtk_window_set_keep_above (GTK_WINDOW (dialog), TRUE);
	gtk_widget_show_all (GTK_WIDGET (dialog));
	gdk_window_focus (gtk_widget_get_window (GTK_WIDGET (dialog)), GDK_CURRENT_TIME);

	if (confirm)
		entry_changed (NULL, dialog);

	return dialog;
}

const gchar*
seahorse_passphrase_prompt_get (GtkDialog *dialog)
{
    GtkEntry *entry = GTK_ENTRY (g_object_get_data (G_OBJECT (dialog), "secure-entry"));
    return gtk_entry_get_text (entry);
}

gboolean
seahorse_passphrase_prompt_checked (GtkDialog *dialog)
{
    GtkToggleButton *button = GTK_TOGGLE_BUTTON (g_object_get_data (G_OBJECT (dialog), "check-option"));
    return GTK_IS_TOGGLE_BUTTON (button) ? gtk_toggle_button_get_active (button) : FALSE;
}
