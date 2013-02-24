/*
 * Seahorse
 *
 * Copyright (C) 2003 Jacob Perkins
 * Copyright (C) 2005, 2006 Stefan Walter
 * Copyright (C) 2011 Collabora Ltd.
 * Copyright (C) 2012 Stefan Walter
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

#include "seahorse-application.h"
#include "seahorse-icons.h"

#include "gkr/seahorse-gkr.h"
#include "pgp/seahorse-pgp.h"
#include "ssh/seahorse-ssh.h"
#include "pkcs11/seahorse-pkcs11.h"

#include "seahorse-search-provider.h"

#include <gtk/gtk.h>

#include <glib/gi18n.h>

struct _SeahorseApplication {
	GtkApplication parent;
	GSettings *seahorse_settings;
	GSettings *crypto_pgp_settings;

	SeahorseSearchProvider *search_provider;
};

struct _SeahorseApplicationClass {
	GtkApplicationClass parent_class;
};

#define INACTIVITY_TIMEOUT 60 * 1000 /* One minute, in milliseconds */

G_DEFINE_TYPE (SeahorseApplication, seahorse_application, GTK_TYPE_APPLICATION);

static SeahorseApplication *the_application = NULL;

static void
seahorse_application_constructed (GObject *obj)
{
	SeahorseApplication *self = SEAHORSE_APPLICATION (obj);

	g_return_if_fail (the_application == NULL);

	G_OBJECT_CLASS (seahorse_application_parent_class)->constructed (obj);

	the_application = self;

	self->seahorse_settings = g_settings_new ("org.gnome.seahorse");

#ifdef WITH_PGP
	/* This is installed by gnome-keyring */
	self->crypto_pgp_settings = g_settings_new ("org.gnome.crypto.pgp");
#endif
}

static void
seahorse_application_finalize (GObject *gobject)
{
	SeahorseApplication *self = SEAHORSE_APPLICATION (gobject);
	the_application = NULL;

#ifdef WITH_PGP
	g_clear_object (&self->crypto_pgp_settings);
#endif
	g_clear_object (&self->seahorse_settings);

	g_clear_object (&self->search_provider);

	G_OBJECT_CLASS (seahorse_application_parent_class)->finalize (gobject);
}

static void
seahorse_application_startup (GApplication *application)
{
	/* Insert Icons into Stock */
	seahorse_icons_init ();

	/* Initialize the various components */
#ifdef WITH_PGP
	seahorse_pgp_backend_initialize ();
#endif
#ifdef WITH_SSH
	seahorse_ssh_backend_initialize ();
#endif
#ifdef WITH_PKCS11
	seahorse_pkcs11_backend_initialize ();
#endif
	seahorse_gkr_backend_initialize ();

	seahorse_search_provider_initialize (SEAHORSE_APPLICATION (application)->search_provider);

	/* HACK: get the inactivity timeout started */
	g_application_hold (application);
	g_application_release (application);

	G_APPLICATION_CLASS (seahorse_application_parent_class)->startup (application);
}

static gboolean
seahorse_application_local_command_line (GApplication *application,
                                         gchar ***arguments,
                                         gint *exit_status)
{
	gboolean show_version = FALSE;
	GOptionContext *context;
	GError *error = NULL;
	int argc;

	GOptionEntry options[] = {
		{ "version", 'v', 0, G_OPTION_ARG_NONE, &show_version, N_("Version of this application"), NULL },
		{ NULL, 0, 0, 0, NULL, NULL, NULL }
	};

	argc = g_strv_length (*arguments);

	context = g_option_context_new (N_("- System Settings"));
	g_option_context_set_ignore_unknown_options (context, TRUE);
	g_option_context_add_main_entries (context, options, GETTEXT_PACKAGE);
	g_option_context_set_translation_domain(context, GETTEXT_PACKAGE);
	g_option_context_add_group (context, gtk_get_option_group (TRUE));

	if (g_option_context_parse (context, &argc, arguments, &error) == FALSE) {
		g_printerr ("seahorse: %s\n", error->message);
		g_option_context_free (context);
		g_error_free (error);
		*exit_status = 1;
		return TRUE;
	}

	g_option_context_free (context);

	if (show_version) {
		g_print ("%s\n", PACKAGE_STRING);
#ifdef WITH_PGP
		g_print ("GNUPG: %s (%d.%d.%d)\n", GNUPG, GPG_MAJOR, GPG_MINOR, GPG_MICRO);
#endif
		*exit_status = 0;
		return TRUE;
	}

	return G_APPLICATION_CLASS (seahorse_application_parent_class)->local_command_line (application, arguments, exit_status);
}

static int
seahorse_application_command_line (GApplication            *application,
				   GApplicationCommandLine *command_line)
{
	GOptionContext *context;
	gboolean no_window = FALSE;
	char **arguments;
	GError *error = NULL;
	int ret;
	int argc;

	GOptionEntry options[] = {
		{ "no-window", 0, 0, G_OPTION_ARG_NONE, &no_window, N_("Don't display a window"), NULL },
		{ NULL, 0, 0, 0, NULL, NULL, NULL }
	};

	context = g_option_context_new (N_("- System Settings"));
	g_option_context_set_ignore_unknown_options (context, TRUE);
	g_option_context_add_main_entries (context, options, GETTEXT_PACKAGE);
	g_option_context_set_translation_domain(context, GETTEXT_PACKAGE);

	arguments = g_application_command_line_get_arguments (command_line, &argc);
	if (g_option_context_parse (context, &argc, &arguments, &error) == FALSE) {
		g_warning ("seahorse: %s\n", error->message);
		g_error_free (error);
		ret = 1;
	} else if (no_window) {
		g_application_hold (application);
		g_application_set_inactivity_timeout (application, INACTIVITY_TIMEOUT);
		g_application_release (application);
		ret = 0;
	} else {
		g_application_activate (application);
		ret = 0;
	}

	g_strfreev (arguments);
	g_option_context_free (context);
	return ret;
}

static gboolean
seahorse_application_dbus_register (GApplication    *application,
                                    GDBusConnection *connection,
                                    const gchar     *object_path,
                                    GError         **error)
{
	SeahorseApplication *self;

	if (!G_APPLICATION_CLASS (seahorse_application_parent_class)->dbus_register (application,
	                                                                             connection,
	                                                                             object_path,
	                                                                             error))
		return FALSE;

	self = SEAHORSE_APPLICATION (application);

	return seahorse_search_provider_dbus_register (self->search_provider, connection,
	                                               object_path, error);
}

static void
seahorse_application_dbus_unregister (GApplication    *application,
                                        GDBusConnection *connection,
                                        const gchar     *object_path)
{
	SeahorseApplication *self;

	self = SEAHORSE_APPLICATION (application);
	if (self->search_provider)
		seahorse_search_provider_dbus_unregister (self->search_provider, connection, object_path);

	G_APPLICATION_CLASS (seahorse_application_parent_class)->dbus_unregister (application,
	                                                                          connection,
	                                                                          object_path);
}

static void
seahorse_application_class_init (SeahorseApplicationClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	GApplicationClass *application_class = G_APPLICATION_CLASS (klass);

	gobject_class->constructed = seahorse_application_constructed;
	gobject_class->finalize = seahorse_application_finalize;

	application_class->startup = seahorse_application_startup;
	application_class->local_command_line = seahorse_application_local_command_line;
	application_class->command_line = seahorse_application_command_line;

	application_class->dbus_register = seahorse_application_dbus_register;
	application_class->dbus_unregister = seahorse_application_dbus_unregister;
}

static void
seahorse_application_init (SeahorseApplication *self)
{
	self->search_provider = seahorse_search_provider_new ();
}

GtkApplication *
seahorse_application_new (void)
{
	return g_object_new (SEAHORSE_TYPE_APPLICATION,
	                     "application-id", "org.gnome.seahorse.Application",
	                     "flags", G_APPLICATION_HANDLES_COMMAND_LINE,
	                     NULL);
}

GtkApplication *
seahorse_application_get (void)
{
	g_return_val_if_fail (the_application != NULL, NULL);
	return GTK_APPLICATION (the_application);
}

GSettings *
seahorse_application_settings (SeahorseApplication *self)
{
	if (self == NULL)
		self = the_application;
	g_return_val_if_fail (SEAHORSE_IS_APPLICATION (self), NULL);
	return self->seahorse_settings;
}

GSettings *
seahorse_application_pgp_settings (SeahorseApplication *self)
{
	if (self == NULL)
		self = the_application;
	g_return_val_if_fail (SEAHORSE_IS_APPLICATION (self), NULL);
	return self->crypto_pgp_settings;
}
