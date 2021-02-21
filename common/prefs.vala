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

public class Seahorse.Prefs : Hdy.PreferencesWindow {

    /**
     * Create a new preferences window.
     *
     * @param parent The Gtk.Window to set as the preferences dialog's parent
     */
    public Prefs(Gtk.Window? parent) {
        GLib.Object (
            title: _("Preferences"),
            transient_for: parent,
            search_enabled: false,
            modal: true
        );

#if WITH_KEYSERVER
        add (new PrefsKeyservers());
#endif
    }

    public static bool available() {
#if WITH_KEYSERVER
        return true;
#else
        return false;
#endif
    }
}
