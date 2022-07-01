/*
 * Seahorse
 *
 * Copyright (C) 2004,2005 Stefan Walter
 * Copyright (C) 2011 Collabora Ltd.
 * Copyright (C) 2013 Stef Walter
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

/**
 * A SeahorsePlace is a collection of objects (passwords/keys/certificates/...).
 * An example of this is a keyring.
 */
public interface Seahorse.Place : GLib.ListModel {

    /**
     * We generally divide a SeahorsePlace in some high level categories.
     *
     * These are then used to group places in the sidebar.
     */
    public enum Category {
        PASSWORDS,
        KEYS,
        CERTIFICATES;

        public unowned string to_string() {
            switch (this) {
                case Category.PASSWORDS:
                    return _("Passwords");
                case Category.KEYS:
                    return _("Keys");
                case Category.CERTIFICATES:
                    return _("Certificates");
            }

            return_val_if_reached(null);
        }
    }

    public abstract string label { owned get; set; }
    public abstract string description { owned get; }
    public abstract string uri { owned get; }
    public abstract Category category { get; }

    /**
     * Returns the {@link GLib.Action}s that are defined for this Place,
     * or null if none.
     */
    public abstract GLib.ActionGroup? actions { owned get; }

    /**
     * Returns the prefix that is used for the actions in this.menu_model.
     */
    public abstract unowned string? action_prefix { get; }

    /**
     * Returns the menu of basic actions that apply specifically to
     * this Place. Can be used for example to show a context menu.
     */
    public abstract MenuModel? menu_model { owned get; }

    /**
     * Loads the items in this Place so they are available.
     *
     * Returns false if an error occurre
     */
    public abstract async bool load(Cancellable? cancellable) throws GLib.Error;
}
