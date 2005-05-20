/*
 * Seahorse
 *
 * Copyright (C) 2005 Nate Nielsen
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
#include <howl.h>

/* Workaround broken howl installing config.h */
#undef PACKAGE
#undef VERSION
#undef PACKAGE_STRING
#undef PACKAGE_NAME
#undef PACKAGE_VERSION
#undef PACKAGE_TARNAME

#include "config.h"
#include "eggtrayicon.h"
#include "seahorse-daemon.h"
#include "seahorse-gconf.h"
#include "seahorse-util.h"

/* TODO: Need to be able to advertize in real domains */

static void start_sharing ();
static void stop_sharing ();

/* For the docklet icon */
static EggTrayIcon *tray_icon = NULL;
static guint gconf_notify_id = 0;

/* DNS-SD publishing -------------------------------------------------------- */

static sw_discovery_publish_id published_id = 0;
static sw_discovery howl_session = NULL;
static guint channel_notify_id = 0;

static sw_result
publish_reply (sw_discovery discovery, sw_discovery_publish_status status,
               sw_discovery_oid id, sw_opaque extra)
{
	return SW_OKAY;
}

static gboolean
howl_input (GIOChannel *io_channel, GIOCondition cond, gpointer callback_data)
{
	sw_discovery session = callback_data;
	sw_salt salt;

	if (sw_discovery_salt (session, &salt) == SW_OKAY) {
		sw_salt_lock (salt);
		sw_discovery_read_socket (session);
		sw_salt_unlock (salt);
	}
    
	return TRUE;
}

static void
stop_publishing (void)
{
    if (published_id != 0)
		sw_discovery_cancel (howl_session, published_id);
    published_id = 0;
    
    if (channel_notify_id != 0)
        g_source_remove (channel_notify_id);
    
    if (howl_session)
        sw_discovery_fina (howl_session);
    howl_session = NULL;
}

static gboolean
start_publishing (int port)
{
	GIOChannel *channel;
    sw_result result;
    gchar *user_name;
    gchar *share_name;
    int fd;

    if (sw_discovery_init (&howl_session) != SW_OKAY)
        return FALSE;
    
	fd = sw_discovery_socket (howl_session);
	channel = g_io_channel_unix_new (fd);
	channel_notify_id = g_io_add_watch (channel, G_IO_IN, howl_input, howl_session);
	g_io_channel_unref (channel);
    
    user_name = seahorse_util_string_up_first (g_get_user_name ());

	/* Translators: The %s will get filled in with the user name
	   of the user, to form a genitive. If this is difficult to
	   translate correctly so that it will work correctly in your
	   language, you may use something equivalent to
	   "Shared keys of %s", or leave out the %s altogether.
	   In the latter case, please put "%.0s" somewhere in the string,
	   which will match the user name string passed by the C code,
	   but not put the user name in the final string. This is to
	   avoid the warning that msgfmt might otherwise generate. */
    share_name = g_strdup_printf (_("%s's encryption keys"), user_name);
    g_free (user_name);
    
    result = sw_discovery_publish (howl_session, 0, share_name, "_hkp._tcp",
				                   NULL, NULL, port, /* text */"", 0,
				                   publish_reply, NULL, &published_id);
    g_free (share_name);
    
    if (result != SW_OKAY) {
        stop_publishing ();
        return FALSE;
    }
    
    return TRUE;
}   

/* Tray Icon ---------------------------------------------------------------- */

static void
stop_activate (GtkWidget *item, gpointer data)
{
    seahorse_gconf_set_boolean (KEYSHARING_KEY, FALSE);
}

static void
prefs_activate (GtkWidget *item, gpointer data)
{
    GError *err = NULL;
    g_spawn_command_line_async ("seahorse-pgp-preferences --sharing", &err);
    
    if (err != NULL) {
        g_warning ("couldn't execute seahorse-pgp-preferences: %s", err->message);
        g_error_free (err);
    }
}

/* Called when icon destroyed */
static void
tray_destroyed (GtkWidget *widget, void *data)
{
    g_object_unref (G_OBJECT (tray_icon));
    tray_icon = NULL;
}

/* Called when icon clicked */
static void
tray_clicked (GtkWidget *button, GdkEventButton *event, void *data)
{
    GtkWidget *image, *menu;
    GtkWidget *item;
    
    if (event->type != GDK_BUTTON_PRESS)
        return;

    /* Right click, show menu */
    if (event->button == 3) {

        menu = gtk_menu_new ();
        
        /* Stop Sharing menu item */
        item = gtk_image_menu_item_new_with_mnemonic (_("_Stop Sharing My Keys"));
        image = gtk_image_new_from_stock (GTK_STOCK_STOP, GTK_ICON_SIZE_MENU);
        gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);
        g_signal_connect (item, "activate", G_CALLBACK (stop_activate), NULL);
        gtk_menu_append (menu, item);
        
        /* Sharing Preferences menu item */
        item = gtk_image_menu_item_new_from_stock (GTK_STOCK_PREFERENCES, NULL);
        g_signal_connect (item, "activate", G_CALLBACK (prefs_activate), NULL);
        gtk_menu_append (menu, item);

        gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL, NULL,
                        event->button, gtk_get_current_event_time ());
        gtk_widget_show_all (menu);
    }
}

static void
show_tray ()
{
    GtkWidget *box;
    GtkWidget *image;

    if (!tray_icon) {
        tray_icon = egg_tray_icon_new ("seahorse-daemon-sharing");
        box = gtk_event_box_new ();

        image = gtk_image_new_from_file (PIXMAPSDIR "seahorse-share-keys.png");
        gtk_container_add (GTK_CONTAINER (box), image);
        gtk_container_add (GTK_CONTAINER (tray_icon), box);

        g_signal_connect (G_OBJECT (tray_icon), "destroy", 
                          G_CALLBACK (tray_destroyed), NULL);
        g_signal_connect (G_OBJECT (box), "button-press-event",
                          G_CALLBACK (tray_clicked), NULL);


        if (!gtk_check_version (2, 4, 0))
            g_object_set (G_OBJECT (box), "visible-window", FALSE, NULL);

        gtk_widget_show_all (GTK_WIDGET (tray_icon));
        g_object_ref (G_OBJECT (tray_icon));        
    }    
}

static void
hide_tray ()
{
    if (tray_icon) {
        g_signal_handlers_disconnect_by_func (G_OBJECT (tray_icon), 
                                              G_CALLBACK (tray_destroyed), NULL);
        gtk_widget_destroy (GTK_WIDGET (tray_icon));
        
        g_object_unref (G_OBJECT (tray_icon));
        tray_icon = NULL;    
    }
}

/* -------------------------------------------------------------------------- */

static void
start_sharing ()
{
    GError *err = NULL;
    
    if (!seahorse_hkp_server_is_running ()) {
        
        if (!seahorse_hkp_server_start (&err)) {
            seahorse_util_handle_error (err, _("Couldn't start the key sharing server."));
            return;
        }

        if (!start_publishing (seahorse_hkp_server_get_port ())) {
            seahorse_hkp_server_stop ();

            /* TODO: Do we need something more descriptive here? */
            seahorse_util_show_error (NULL, _("Couldn't publish key sharing information."));
            return;
        }
    }

    show_tray ();
}

static void
stop_sharing ()
{
    stop_publishing ();
    
    if (seahorse_hkp_server_is_running ())
        seahorse_hkp_server_stop ();
    
    hide_tray ();
}

static void
gconf_notify (GConfClient *client, guint id, GConfEntry *entry, gpointer data)
{
	if (g_str_equal (KEYSHARING_KEY, gconf_entry_get_key (entry))) {
        if (gconf_value_get_bool (gconf_entry_get_value (entry)))
            start_sharing ();
        else
            stop_sharing ();
	}
}

void 
seahorse_sharing_init ()
{
    gconf_notify_id = seahorse_gconf_notify (KEYSHARING_KEY, gconf_notify, NULL);
    
    if (seahorse_gconf_get_boolean (KEYSHARING_KEY))
        start_sharing();
}

void 
seahorse_sharing_cleanup ()
{
    if (gconf_notify_id)
        seahorse_gconf_unnotify (gconf_notify_id);
    gconf_notify_id = 0;

    stop_sharing ();    
}
