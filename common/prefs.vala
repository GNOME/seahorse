/*
 * Seahorse
 *
 * Copyright (C) 2004-2005 Stefan Walter
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

public class Seahorse.Prefs : Gtk.Dialog {

    private Gtk.Notebook notebook;

    /**
     * Create a new preferences window.
     *
     * @param parent The Gtk.Window to set as the preferences dialog's parent
     */
    public Prefs(Gtk.Window? parent, string? tabid = null) {
        GLib.Object (
            title: _("Preferences"),
            transient_for: parent,
            modal: true
        );

        var builder = new Gtk.Builder();
        try {
            string path = "/org/gnome/Seahorse/seahorse-prefs.ui";
            builder.add_from_resource(path);
        } catch (GLib.Error err) {
            GLib.critical("%s", err.message);
        }
        Gtk.Box content = (Gtk.Box) builder.get_object("prefs");
        get_content_area().border_width = 0;
        get_content_area().add(content);
        this.notebook = (Gtk.Notebook) builder.get_object("notebook");

#if WITH_KEYSERVER
        var keyservers = new Keyservers(this);
        var label = new Gtk.Label("Keyservers");
        add_tab(label, keyservers);
#endif

        if (tabid != null) {
            Gtk.Widget? tab = builder.get_object(tabid) as Gtk.Widget;
            if (tab != null)
                select_tab(tab);
        }
    }

    public static bool available() {
#if WITH_KEYSERVER
        return true;
#else
        return false;
#endif
    }

    /**
     * Add a tab to the preferences window
     *
     * @param label Label for the tab to be added
     * @param tab Tab to be added
     */
    public void add_tab(Gtk.Widget? label, Gtk.Widget? tab) {
        label.show();
        this.notebook.prepend_page(tab, label);
    }

    /**
     * Sets the input tab to be the active one
     *
     * @param tab The tab to be set active
     */
    public void select_tab(Gtk.Widget? tab) {
        int pos = this.notebook.page_num(tab);
        if (pos != -1)
            this.notebook.set_current_page(pos);
    }

    /**
     * Removes a tab from the preferences window
     *
     * @tab: The tab to be removed
     */
    public void remove_tab(Gtk.Widget? tab) {
        int pos = this.notebook.page_num(tab);
        if (pos != -1)
            this.notebook.remove_page(pos);
    }

}
