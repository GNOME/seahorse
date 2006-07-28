/*
 * Seahorse
 *
 * Copyright (C) 2004 Nate Nielsen
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
#include <sys/wait.h>

#include <stdlib.h>
#include <gnome.h>

#define DBUS_API_SUBJECT_TO_CHANGE
#include <dbus/dbus.h>

#include "seahorse-context.h"
#include "seahorse-widget.h"
#include "seahorse-gpg-options.h"
#include "seahorse-gconf.h"
#include "seahorse-util.h"
#include "seahorse-check-button-control.h"
#include "agent/seahorse-agent.h"
#include "seahorse-passphrase.h"

#include "seahorse-pgp-key.h"
#include "seahorse-ssh-key.h"

#define METHOD_INTERNAL     "internal"

/* -----------------------------------------------------------------------------
 *  CONTROLS
 */

static void
update_cache_choices (const char *gconf_key, SeahorseWidget *swidget)
{
    GtkWidget *widget;
    gchar *str;
    gint ttl;
    gboolean set;
    
    if (strcmp (gconf_key, SETTING_CACHE) == 0) {
        
        set = seahorse_gconf_get_boolean (SETTING_CACHE);
        if (!set) {
            widget = seahorse_widget_get_widget (swidget, "no-cache");
            g_return_if_fail (widget != NULL);
            gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), TRUE);
        }
        
        return;
    }
    
    if (strcmp (gconf_key, SETTING_METHOD) == 0) {
        
        str = seahorse_gconf_get_string (SETTING_METHOD);
        if (!str || strcmp (str, METHOD_GNOME) != 0)
            widget = seahorse_widget_get_widget (swidget, "session-cache");
        else
            widget = seahorse_widget_get_widget (swidget, "keyring-cache");
        g_free (str);

        g_return_if_fail (widget != NULL);
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), TRUE);
        
        return;
    }
    
    if (strcmp (gconf_key, SETTING_TTL) == 0) {
        
        ttl = seahorse_gconf_get_integer (SETTING_TTL);
        if (ttl < 0)
            ttl = 0;
        widget = seahorse_widget_get_widget (swidget, "ttl");
        g_return_if_fail (widget != NULL);
        gtk_spin_button_set_value (GTK_SPIN_BUTTON (widget), ttl);
        
        return;
    }
}

static void
cache_gconf_notify (GConfClient *client, guint id, 
              GConfEntry *entry, SeahorseWidget *swidget)
{
    update_cache_choices (gconf_entry_get_key (entry), swidget);
}


static void
save_cache_choices (GtkWidget *unused, SeahorseWidget *swidget)
{
    GtkWidget *widget, *widget_ttl;
    int ttl;
    
    widget_ttl = seahorse_widget_get_widget (swidget, "ttl");
    g_return_if_fail (widget_ttl != NULL);
    
    widget = seahorse_widget_get_widget (swidget, "no-cache");
    g_return_if_fail (widget != NULL);
    
    /* No cache */
    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget))) {
        seahorse_gconf_set_boolean (SETTING_CACHE, FALSE);
        gtk_widget_set_sensitive (widget_ttl, FALSE);
        
        return;
    }
    
    widget = seahorse_widget_get_widget (swidget, "session-cache");
    g_return_if_fail (widget != NULL);
    
    /* Session cache */
    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget))) {
        seahorse_gconf_set_boolean (SETTING_CACHE, TRUE);
        seahorse_gconf_set_string (SETTING_METHOD, METHOD_INTERNAL);
        gtk_widget_set_sensitive (widget_ttl, TRUE);
        ttl = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (widget_ttl));
        seahorse_gconf_set_boolean (SETTING_EXPIRE, ttl > 0 ? TRUE : FALSE);
        seahorse_gconf_set_integer (SETTING_TTL, ttl);
        return;
    }
    
#ifdef WITH_GNOME_KEYRING
    
    widget = seahorse_widget_get_widget (swidget, "keyring-cache");
    g_return_if_fail (widget != NULL);
    
    /* gnome-keyring cache */
    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget))) {
        seahorse_gconf_set_boolean (SETTING_CACHE, TRUE);
        seahorse_gconf_set_string (SETTING_METHOD, METHOD_GNOME);
        gtk_widget_set_sensitive (widget_ttl, FALSE);
        seahorse_gconf_set_boolean (SETTING_EXPIRE, FALSE);
    }

#endif /* WITH_GNOME_KEYRING */
    
}

static void
save_ttl (GtkSpinButton *spinner, SeahorseWidget *swidget)
{
    int ttl = gtk_spin_button_get_value_as_int (spinner);
    if (ttl < 0)
        ttl = 0;
    seahorse_gconf_set_integer (SETTING_TTL, ttl);
}

/* Start up the gnome-session-properties */
static void
show_session_properties (GtkWidget *widget, gpointer data)
{
    GError *err = NULL;

    g_spawn_command_line_async ("gnome-session-properties", &err);

    if (err)
        seahorse_util_handle_error (err, _("Couldn't open the Session Properties"));
}

/* Generate the Hand Cursor */
void
set_hand_cursor_on_realize(GtkWidget *widget, gpointer user_data)
{
    GdkCursor *cursor;

    cursor = gdk_cursor_new (GDK_HAND2);
    gdk_window_set_cursor (GTK_BUTTON (widget)->event_window, cursor);
    gdk_cursor_unref (cursor);
}

/* Find button label, underline and paint it blue. 
 * TODO: Get the system theme link color and use that instead of default blue.
 **/
void
paint_button_label_as_link (GtkButton *button, GtkLabel *label)
{
    const gchar * button_text;
    gchar *markup;
    
    button_text = gtk_label_get_label (label);
    
    markup = g_strdup_printf ("<u>%s</u>", button_text);
    gtk_label_set_markup (GTK_LABEL (label), markup);
    g_free (markup);

    GdkColor *link_color;
    GdkColor blue = { 0, 0x0000, 0x0000, 0xffff }; /* Default color */

    /* Could optionaly set link_color to the current theme color... */
    link_color = &blue;

    gtk_widget_modify_fg (GTK_WIDGET (label),
                  GTK_STATE_NORMAL, link_color);
    gtk_widget_modify_fg (GTK_WIDGET (label),
                  GTK_STATE_ACTIVE, link_color);
    gtk_widget_modify_fg (GTK_WIDGET (label),
                  GTK_STATE_PRELIGHT, link_color);
    gtk_widget_modify_fg (GTK_WIDGET (label),
                  GTK_STATE_SELECTED, link_color);

    if (link_color != &blue)
        gdk_color_free (link_color);
}


static void 
start_seahorse_daemon (SeahorseWidget *swidget)
{
    DBusConnection *conn;
    DBusError err;
    dbus_bool_t ret;
    dbus_uint32_t reply;
    GtkWidget *widget;
    
    dbus_error_init (&err);
    
    conn = dbus_bus_get (DBUS_BUS_SESSION, &err);
    if (!conn) {
        g_warning ("Accessing DBUS session bus failed: %s", err.message);
        dbus_error_free (&err);
        return;
    }
    
    /* Check if seahorse-daemon is running */
    ret = dbus_bus_name_has_owner (conn, "org.gnome.seahorse", &err);
    if (dbus_error_is_set (&err)) {
        g_warning ("Couldn't check if seahorse-daemon is running: %s", err.message);
        dbus_error_free (&err);
        return;
    }
    
    if (ret)
        return;
    
    /* Start seahorse daemon */
    ret = dbus_bus_start_service_by_name (conn, "org.gnome.seahorse", 0, &reply, &err);
    if (!ret) {
        seahorse_util_show_error (GTK_WINDOW (seahorse_widget_get_top (swidget)), 
                                  _("Couldn't start seahorse-daemon"), 
                                  _("The program 'seahorse-daemon' is necessary for the caching of passphrases"));
        g_warning ("Couldn't start seahorse-daemon: %s", err.message);
        return;
    }
    
    /* Show message about seahorse-daemon being started */
    widget = glade_xml_get_widget (swidget->xml, "agent-started");
    g_return_if_fail (widget != NULL);
    gtk_widget_show (widget);
}

static void 
focus_cache_tab (GtkNotebook *notebook, GtkNotebookPage *p, 
                 guint page_num, SeahorseWidget *swidget)
{
    const gchar *name;
    GtkWidget *page;
    
    if (g_object_get_data (G_OBJECT (swidget), "seahorse-daemon-inited"))
        return;

    page = gtk_notebook_get_nth_page (notebook, page_num);
    g_return_if_fail (page != NULL);
    
    name = glade_get_widget_name (page);
    if (name && strcmp (name, "cache-tab") == 0) {
        start_seahorse_daemon (swidget);
        g_object_set_data (G_OBJECT (swidget), "seahorse-daemon-inited", swidget);
    }
}

/* Initialize the cache tab */
void
seahorse_prefs_cache (SeahorseWidget *swidget)
{
    GtkWidget *notebook;
    GtkWidget *w, *w2;
    
    g_return_if_fail (swidget != NULL);
    
    w = seahorse_widget_get_widget (swidget, "no-cache");
    g_return_if_fail (w != NULL);
    g_signal_connect_after (w, "toggled", G_CALLBACK (save_cache_choices), swidget);
    
    w = seahorse_widget_get_widget (swidget, "session-cache");
    g_return_if_fail (w != NULL);
    g_signal_connect_after (w, "toggled", G_CALLBACK (save_cache_choices), swidget);
    
    w = seahorse_widget_get_widget (swidget, "keyring-cache");
    g_return_if_fail (w != NULL);
#ifdef WITH_GNOME_KEYRING    
    g_signal_connect_after (w, "toggled", G_CALLBACK (save_cache_choices), swidget);
#else
    gtk_widget_hide (w);
#endif 
    
    w = seahorse_widget_get_widget (swidget, "ttl");
    g_return_if_fail (w != NULL);
    g_signal_connect_after (w, "value-changed", G_CALLBACK (save_ttl), swidget);
    
    /* Initial values, then listen */
    update_cache_choices (SETTING_CACHE, swidget);
    update_cache_choices (SETTING_METHOD, swidget);
    update_cache_choices (SETTING_TTL, swidget);
    seahorse_gconf_notify_lazy (AGENT_SETTINGS, (GConfClientNotifyFunc)cache_gconf_notify, 
                                swidget, swidget);
                                
    /* Authorize check button */
    w = seahorse_widget_get_widget (swidget, "authorize");
    g_return_if_fail (w != NULL);
    seahorse_check_button_gconf_attach (GTK_CHECK_BUTTON (w), SETTING_AUTH);

    /* Setup daemon button visuals */
    w = seahorse_widget_get_widget (swidget, "session-link");
    g_return_if_fail (w != NULL);
    
    w2 = seahorse_widget_get_widget (swidget, "label-session-properties");
    g_return_if_fail (w2 != NULL);
    
    paint_button_label_as_link (GTK_BUTTON (w), GTK_LABEL(w2));
    g_signal_connect (GTK_WIDGET (w), "realize", G_CALLBACK (set_hand_cursor_on_realize), NULL);

    w = seahorse_widget_get_widget (swidget, "auto-load-ssh");
    g_return_if_fail (w != NULL);
    seahorse_check_button_gconf_attach (GTK_CHECK_BUTTON (w), SETTING_AGENT_SSH);
    
    /* End -- Setup daemon button visuals */
    
    glade_xml_signal_connect_data (swidget->xml, "on_session_link",
                                   G_CALLBACK (show_session_properties), NULL);
                                   
    notebook = seahorse_widget_get_widget (swidget, "notebook");
    g_return_if_fail (notebook != NULL);
    g_signal_connect_after (notebook, "switch-page", G_CALLBACK (focus_cache_tab), swidget);
    
    /* Disable GPG agent prefs if another agent is running or error */
    switch (seahorse_passphrase_detect_agent (SKEY_PGP)) {
    case SEAHORSE_AGENT_UNKNOWN:
    case SEAHORSE_AGENT_OTHER:
        g_warning ("Another GPG agent may be running. Disabling cache preferences.");
        w = seahorse_widget_get_widget (swidget, "pgp-area");
        if (w != NULL)
            gtk_widget_set_sensitive (w, FALSE);
        break;
    default:
        break;
    };
    
    /* Disable SSH agent prefs if no SSH is running or error */
    switch (seahorse_passphrase_detect_agent (SKEY_SSH)) {
    case SEAHORSE_AGENT_UNKNOWN:
    case SEAHORSE_AGENT_NONE:
        g_warning ("No SSH agent is running. Disabling cache preferences.");
        w = seahorse_widget_get_widget (swidget, "ssh-area");
        if (w != NULL)
            gtk_widget_set_sensitive (w, FALSE);
        break;
    default:
        break;
    };
}
