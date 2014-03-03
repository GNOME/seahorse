/*
 * Seahorse
 *
 * Copyright (C) 2005 Stefan Walter
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
 * along with this program; if not, see
 * <http://www.gnu.org/licenses/>.
 *
 * Author: Stef Walter <stefw@collabora.co.uk>
 */
 
#include "config.h"

#include "libseahorse/seahorse-passphrase.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#ifdef GDK_WINDOWING_X11
#include <gdk/gdkx.h>
#endif

int
main (int argc, char* argv[])
{
	GdkWindow *transient_for = NULL;
	GtkDialog *dialog;
	const gchar *title;
	const gchar *argument;
	gchar *message;
	const gchar *flags;
	gint result;
	const gchar *pass;
	gulong xid;
	gssize len;

	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	gtk_init (&argc, &argv);

	/* Non buffered stdout */
	setvbuf (stdout, 0, _IONBF, 0);

#ifdef GDK_WINDOWING_X11
	argument = g_getenv ("SEAHORSE_SSH_ASKPASS_PARENT");
	if (argument) {
		GdkDisplay *display = gdk_display_get_default ();
		if (GDK_IS_X11_DISPLAY (display)) {
			xid = strtoul (argument, NULL, 10);
			if (xid != 0)
				transient_for = gdk_x11_window_foreign_new_for_display (display, xid);
			if (transient_for == NULL)
				g_warning ("Couldn't find window to be transient for: %s", argument);
		}
	}
#endif

	title = g_getenv ("SEAHORSE_SSH_ASKPASS_TITLE");
	if (!title || !title[0])
		title = _("Enter your Secure Shell passphrase:");

	message = (gchar *)g_getenv ("SEAHORSE_SSH_ASKPASS_MESSAGE");
	if (message && message[0])
		message = g_strdup (message);
	else if (argc > 1)
		message = g_strjoinv (" ", argv + 1);
	else
		message = g_strdup (_("Enter your Secure Shell passphrase:"));

	argument = g_getenv ("SEAHORSE_SSH_ASKPASS_ARGUMENT");
	if (!argument)
		argument = "";

	flags = g_getenv ("SEAHORSE_SSH_ASKPASS_FLAGS");
	if (!flags)
		flags = "";
	if (strstr (flags, "multiple")) {
		gchar *lower = g_ascii_strdown (message, -1);

		/* Need the old passphrase */
		if (strstr (lower, "old pass")) {
			title = _("Old Key Passphrase");
			message = g_strdup_printf (_("Enter the old passphrase for: %s"), argument);

		/* Look for the new passphrase thingy */
		} else if (strstr (lower, "new pass")) {
			title = _("New Key Passphrase");
			message = g_strdup_printf (_("Enter the new passphrase for: %s"), argument);

		/* Confirm the new passphrase, just send it again */
		} else if (strstr (lower, "again")) {
			title = _("New Key Passphrase");
			message = g_strdup_printf (_("Enter the new passphrase again: %s"), argument);
		}

		g_free (lower);
	}

	dialog = seahorse_passphrase_prompt_show (title, message, _("Password:"),
	                                          NULL, FALSE);

	g_free (message);

	if (transient_for) {
		gdk_window_set_transient_for (gtk_widget_get_window (GTK_WIDGET (dialog)),
		                              transient_for);
	}

	result = 1;
	if (gtk_dialog_run (dialog) == GTK_RESPONSE_ACCEPT) {
		pass = seahorse_passphrase_prompt_get (dialog);
		len = strlen (pass ? pass : "");
		if (write (1, pass, len) != len) {
			g_warning ("couldn't write out password properly");
			result = 1;
		} else {
			result = 0;
		}
	}

	if (transient_for)
		g_object_unref (transient_for);
	gtk_widget_destroy (GTK_WIDGET (dialog));
	return result;
}
