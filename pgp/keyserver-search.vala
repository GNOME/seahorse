/*
 * Seahorse
 *
 * Copyright (C) 2004-2005 Stefan Walter
 * Copyright (C) 2011 Collabora Ltd.
 * Copyright (C) 2017 Niels De Graef
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
 */

namespace Seahorse {

/**
 * Contains the UI and functions to start a search for keys on a keyserver.
 **/
public class KeyserverSearch : Gtk.Dialog {

    /**
     * A list of keyserver names
     */
    private string[] names;

    /**
     * A list of keyserver URIs
     */
    private string[] uris;

    /**
     * true if all keyservers are selected
     */
    private bool all;

    // Widget descendants
    private Gtk.Entry search_text;
    private Gtk.Spinner search_where;
    private Gtk.Box key_server_list;
    private Gtk.Box shared_keys_list;

    public KeyserverSearch(Gtk.Window parent) {
        GLib.Object(transient_for: parent);
    }

    /**
     * Shows a remote search window.
     *
     * @param parent the parent window to connect this window to
     */
    void show() {
        GtkWindow *window;
        GtkWidget *widget;

        swidget = seahorse_widget_new ("keyserver-search", parent);
        g_return_val_if_fail (swidget != null, null);

        window = GTK_WINDOW (seahorse_widget_get_widget (swidget, swidget->name));

        Gtk.Entry entry = seahorse_widget_get_widget (swidget, "search-text");
        g_return_val_if_fail (entry != null, window);

        string? last_search = Seahorse.Application.settings().get_string("last-search-text");
        if (search != null) {
            entry.text = last_search;
            entry.select_region(0, -1);
        }

        // The key servers to list
        GLib.Settings settings = Seahorse.Application.pgp_settings();
        on_settings_keyservers_changed (settings, "keyservers", swidget);
        g_signal_connect_object (settings, "changed::keyservers",
                                 G_CALLBACK (on_settings_keyservers_changed), swidget, 0);

        // Any shared keys to list
        Discovery ssd = Pgp.Backend.get().get_discovery();
        refresh_shared_keys(ssd, null, swidget);
        ssd.added.connect(refresh_shared_keys);
        ssd.removed.connect(refresh_shared_keys);
        window.destroy.connect(cleanup_signals);

        select_inital_keyservers();
        on_keyserver_search_control_changed(null, swidget);

        return window;
    }


    /* Selection Retrieval ------------------------------------------------------ */

    /**
     * Extracts all keyservers in the sub-widgets "key-server-list" and
     * "shared-keys-list" and returns the selection
     **/
    private void get_keyserver_selection() {
        /* Key servers */
        Gtk.Widget widget = seahorse_widget_get_widget (swidget, "key-server-list");
        g_return_val_if_fail (widget != null, selection);
        gtk_container_foreach (GTK_CONTAINER (widget), (GtkCallback)get_checks, selection);

        /* Shared Key */
        widget = seahorse_widget_get_widget (swidget, "shared-keys-list");
        g_return_val_if_fail (widget != null, selection);
        gtk_container_foreach (GTK_CONTAINER (widget), (GtkCallback)get_checks, selection);

        return selection;
    }

    /**
     * widget: CHECK_BUTTON widget to read
     * selection: will be updated depending on the state of the widget
     *
     * Adds the name/uri of the checked widget to the selection
     *
     **/
    private void get_checks (GtkWidget *widget, KeyserverSelection *selection) {
        if (!GTK_IS_CHECK_BUTTON (widget))
            return;

        /* Pull in the selected name and uri */
        if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget))) {
            string value = gtk_button_get_label (GTK_BUTTON (widget));
            g_ptr_array_add (selection->names, g_strdup (value));
            value = g_object_get_data (G_OBJECT (widget), "keyserver-uri");
            g_ptr_array_add (selection->uris, g_strdup (value));

        /* Note that not all checks are selected */
        } else {
            selection->all = false;
        }
    }

    /**
     * returns true if at least one of the CHECK_BUTTONS in "key-server-list" or
     * "shared-keys-list" is true
     **/
    private bool have_keyserver_selection() {
        /* Key servers */
        Gtk.Widget w = GTK_WIDGET (seahorse_widget_get_widget (swidget, "key-server-list"));
        g_return_val_if_fail (w != null, false);
        foreach (child in w) {
            Gtk.CheckButton? btn = child as Gtk.CheckButton;
            if (btn != null && btn.active)
                return true;
        }

        /* Shared keys */
        w = GTK_WIDGET (seahorse_widget_get_widget (swidget, "shared-keys-list"));
        g_return_val_if_fail (w != null, false);
        foreach (child in w) {
            Gtk.CheckButton? btn = child as Gtk.CheckButton;
            if (btn != null && btn.active)
                return true;
        }

        return false;
    }

    /**
     * Enables the "search" button if the edit-field contains text and at least a
     * server is selected
     */
    G_MODULE_EXPORT void on_keyserver_search_control_changed (GtkWidget *widget) {
        bool enabled = true;
        GtkWidget *w;

        /* Need to have at least one key server selected ... */
        if (!have_keyserver_selection (swidget))
            enabled = false;

        /* ... and some search text */
        else {
            w = GTK_WIDGET (seahorse_widget_get_widget (swidget, "search-text"));
            string text = gtk_editable_get_chars (GTK_EDITABLE (w), 0, -1);
            if (!text || !text[0])
                enabled = false;
        }

        w = GTK_WIDGET (seahorse_widget_get_widget (swidget, "search"));
        w.sensitive = enabled;
    }

    /* Initial Selection -------------------------------------------------------- */

    /**
     * Reads key servers from settings and updates the UI content.
     */
    private void select_inital_keyservers(SeahorseWidget *swidget) {
        string[] names = Seahorse.Application.settings().get_strv("last-search-servers");

        /* Close the expander if all servers are selected */
        Gtk.Widget widget = seahorse_widget_get_widget (swidget, "search-where");
        g_return_if_fail (widget != null);
        gtk_expander_set_expanded (GTK_EXPANDER (widget), names != null && names[0] != null);

        /* We do case insensitive matches */
        for (uint i = 0; names[i] != null; i++) {
            names[i] = g_utf8_casefold (names[i], -1);
        }

        widget = seahorse_widget_get_widget (swidget, "key-server-list");
        g_return_if_fail (widget != null);
        gtk_container_foreach (GTK_CONTAINER (widget), foreach_child_select_checks, names);

        widget = seahorse_widget_get_widget (swidget, "shared-keys-list");
        g_return_if_fail (widget != null);
        gtk_container_foreach (GTK_CONTAINER (widget), foreach_child_select_checks, names);
    }

    private void foreach_child_select_checks (GtkWidget *widget, string[] names) {
        if (GTK_IS_CHECK_BUTTON (widget)) {key_server
            string name = g_utf8_casefold (gtk_button_get_label (GTK_BUTTON (widget)), -1);
            bool checked = names != null && names[0] != null ? false : true;
            for (uint i = 0; names && names[i] != null; i++) {
                if (g_utf8_collate (names[i], name) == 0) {
                    checked = true;
                    break;
                }
            }
            gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), checked);
        }
    }

    /* Populating Lists --------------------------------------------------------- */

    /**
     * Updates the box and adds checkboxes containing names/uris. The check-status
     * of already existing check boxes is not changed.
     *
     * box: the GTK_CONTAINER with the checkboxes
     * uris: the uri list of the keyservers
     * names: the keyserver names
     **/
    private void populate_keyserver_list(Gtk.Box box, string[] uris, string[] names) {
        // Remove all checks, and note which ones were unchecked
        HashTable<string, ?> unchecked = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, null);
        gtk_container_foreach (box, remove_checks, unchecked);

        bool any = false;
        // Now add the new ones back
        for (uint i = 0; uris && uris[i] && names && names[i]; i++) {
            any = true;

            // A new checkbox with this the name as the label
            Gtk.CheckButton check = new Gtk.CheckButton.with_label(names[i]);
            check.active = (unchecked.lookup(names[i]) == null);
            check.toggled.connect(on_keyserver_search_control_changed);
            check.show();

            // Save URI and set it as the tooltip
            check.set_data<string>("keyserver-uri", uris[i]);
            check.tooltip_text = uris[i];

            box.add(check);
        }

        // Only display the container if we had some checks
        box.visible = any;
    }

    /**
     * widget: a check button
     * unchecked: a hash table containing the state of the servers
     *
     * If the button is not checked, the hash table entry associate with it will be
     * replaced with ""
     *
     **/
    private void remove_checks(GtkWidget *widget, HashTable *unchecked) {
        if (GTK_IS_CHECK_BUTTON (widget)) {

            if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)))
                unchecked.replace(gtk_button_get_label (GTK_BUTTON (widget)), "");

            widget.destroy();
        }
    }

    private void on_settings_keyservers_changed (GSettings *settings, const gchar *key) {
        GtkWidget *widget;

        widget = seahorse_widget_get_widget (swidget, "key-server-list");
        g_return_if_fail (widget != null);

        string[] keyservers = seahorse_servers_get_uris ();
        string[] names = seahorse_servers_get_names ();
        populate_keyserver_list (swidget, widget, keyservers, names);
    }

    /**
     * refreshes the "shared-keys-list"
     *
     * ssd: the SeahorseServiceDiscovery. List-data is read from there
     * name: ignored
     */
    private void refresh_shared_keys(Seahorse.Discovery ssd, string name) {
        GtkWidget widget = seahorse_widget_get_widget (swidget, "shared-keys-list");
        g_return_if_fail (widget != null);

        string[] names = ssd.list();
        string[] keyservers = ssd.get_uris(names);
        populate_keyserver_list(widget, keyservers, names);
    }

    /* -------------------------------------------------------------------------- */

    /**
     * Extracts data, stores it in settings and starts a search using the entered
     * search data.
     *
     * This function gets the things done
     */
    G_MODULE_EXPORT void on_keyserver_search_ok_clicked(GtkButton *button) {
        KeyserverSelection *selection;
        GtkWidget *widget;
        GtkWindow *parent;

        widget = seahorse_widget_get_widget (swidget, "search-text");
        g_return_if_fail (widget != null);

        GLib.Settings app_settings = Seahorse.Application.settings();

        /* Get search text and save it for next time */
        string search = gtk_entry_get_text (GTK_ENTRY (widget));
        g_return_if_fail (search != null && search[0] != 0);
        app_settings.set_string("last-search-text", search);

        /* The keyservers to search, and save for next time */
        selection = get_keyserver_selection (swidget);
        g_return_if_fail (selection->uris != null);
        app_settings.set_strv("last-search-servers", selection->all ? null : selection->uris);

        // Open the new result window; its transient parent is *our* transient
        // parent (Seahorse's primary window), not ourselves, as *we* will
        // disappear when "OK" is clicked.
        seahorse_keyserver_results_show (search, this.transient_for);

        free_keyserver_selection (selection);
        swidget.destroy();
    }

    /**
     * Disconnects the added/removed signals
     **/
    private void cleanup_signals (GtkWidget *widget, SeahorseWidget *swidget) {
        Seahorse.Discovery ssd = Backend.get().get_discovery();
        g_signal_handlers_disconnect_by_func (ssd, refresh_shared_keys, swidget);
    }

}

}
