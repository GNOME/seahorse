/*
 * Seahorse
 *
 * Copyright (C) 2004 Stefan Walter
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
 * GPG CONF
 */

static const char* gpg_confs[] = {
    "use-agent",
    NULL
};

/* Loads the main 'use-agent' gpg.conf into gconf 'cache_enabled */
static void 
load_gpg_conf ()
{
    GError *error = NULL;
    char *values[2];

    if (!seahorse_gpg_options_find_vals (gpg_confs, values, &error)) {
        g_warning ("couldn't read gpg 'use-agent' configuration: %s", error ? error->message : "");
	  	g_clear_error (&error);
        return;
    }

    seahorse_gconf_set_boolean (SETTING_CACHE, values[0] ? TRUE : FALSE);
}

/* Saves the gconf 'cache_enabled' setting into 'use-agent' in gpg.conf */
static void 
save_gpg_conf ()
{
    GError *error = NULL;
    gboolean set;
    char *values[2];

    set = seahorse_gconf_get_boolean (SETTING_CACHE);

    if (!seahorse_gpg_options_find_vals (gpg_confs, values, &error)) {
        g_warning ("couldn't read gpg 'use-agent' configuration: %s", error ? error->message : "");
	  	g_clear_error (&error);
        return;
    }

    /* Don't modify needlessly */
    if ((values[0] ? TRUE : FALSE) == set)
        return;

    values[0] = set ? "" : NULL;
    values[1] = NULL; /* null teriminate */

    if (!seahorse_gpg_options_change_vals (gpg_confs, values, &error)) {
        g_warning ("couldn't modify gpg 'use-agent' configuration: %s", error ? error->message : "");
        g_clear_error (&error);
    }
}

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
        save_gpg_conf ();        
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
        save_gpg_conf ();
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
        save_gpg_conf ();
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

/* Startup our agent */ 	 
static void 	 
start_agent (GtkWidget *widget, gpointer data) 	 
{ 	 
    GError *err = NULL; 	 
    gint status; 	 
 	 
    g_spawn_command_line_sync ("seahorse-agent", NULL, NULL, &status, &err); 	 
 	 
    if (err) 	 
        seahorse_util_handle_error (err, _("Couldn't start the 'seahorse-agent' program")); 	 
    else if (!(WIFEXITED (status) && WEXITSTATUS (status) == 0)) 	 
        seahorse_util_handle_error (NULL, _("The 'seahorse-agent' program exited unsuccessfully.")); 	 
    else { 	 
        /* Show the next message about starting up automatically */ 	 
        gtk_widget_hide (gtk_widget_get_parent (gtk_widget_get_parent (widget))); 	 
        gtk_widget_show (GTK_WIDGET (data)); 	 
    }
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

/* Initialize the cache tab */
void
seahorse_prefs_cache (SeahorseWidget *swidget)
{
    GtkWidget *w, *w2;
    gboolean display_start = FALSE;
    
    g_return_if_fail (swidget != NULL);

    /* Update gconf from gpg.conf */
    load_gpg_conf ();

    /* Initial values, then listen */
    update_cache_choices (SETTING_CACHE, swidget);
    update_cache_choices (SETTING_METHOD, swidget);
    update_cache_choices (SETTING_TTL, swidget);
    seahorse_gconf_notify_lazy (AGENT_SETTINGS, (GConfClientNotifyFunc)cache_gconf_notify, 
                                swidget, swidget);
    
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
                                   
    /* Disable GPG agent prefs if another agent is running or error */
    switch (seahorse_passphrase_detect_agent (SKEY_PGP)) {
    case SEAHORSE_AGENT_NONE:
        display_start = TRUE;
        break;
        
    case SEAHORSE_AGENT_UNKNOWN:
    case SEAHORSE_AGENT_OTHER:
        g_warning ("Another GPG agent may be running. Disabling cache preferences.");
        w = seahorse_widget_get_widget (swidget, "pgp-area");
        if (w != NULL)
            gtk_widget_hide (w);
        w = seahorse_widget_get_widget (swidget, "pgp-message");
        if (w != NULL)
            gtk_widget_show (w);
        break;

    default:
        break;
    };
    
    /* Disable SSH agent prefs if no SSH is running or error */
    switch (seahorse_passphrase_detect_agent (SKEY_SSH)) {
    case SEAHORSE_AGENT_OTHER:
        display_start = TRUE;
        break;

    case SEAHORSE_AGENT_UNKNOWN:
    case SEAHORSE_AGENT_NONE:
        g_warning ("No SSH agent is running. Disabling cache preferences.");
        w = seahorse_widget_get_widget (swidget, "ssh-area");
        if (w != NULL)
            gtk_widget_hide (w);
        w = seahorse_widget_get_widget (swidget, "ssh-message");
        if (w != NULL)
            gtk_widget_show (w);
        break;

    default:
        break;
    };

    /* Show the start link */
    if (display_start) {
        w = seahorse_widget_get_widget (swidget, "agent-start");
        g_return_if_fail (w != NULL);
        gtk_widget_show (w);
        w = seahorse_widget_get_widget (swidget, "agent-started");
        g_return_if_fail (w != NULL);
        glade_xml_signal_connect_data (swidget->xml, "on_start_link",
                                       G_CALLBACK (start_agent), w);
    }
}
