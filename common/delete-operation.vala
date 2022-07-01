/*
 * Seahorse
 *
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Stef Walter <stefw@collabora.co.uk>
 */

/**
 * A special object that takes care of deleting items.
 */
public abstract class Seahorse.DeleteOperation : GLib.Object, GLib.ListModel {

    protected GenericArray<Deletable> items = new GenericArray<Deletable>();

    /**
     * Deletes the objects
     */
    public abstract async bool execute(Cancellable? cancellable = null) throws GLib.Error;

    /** Similar to execute, but asks the user to confirm first */
    public async bool execute_interactively(Gtk.Window? parent,
                                            Cancellable? cancellable = null)
                                            throws GLib.Error {
        var dialog = create_confirm_dialog();
        unowned var response = yield dialog.choose(parent, cancellable);
        if (response != "delete")
            throw new GLib.IOError.CANCELLED("Delete operation was cancelled by the user");

        return yield execute(cancellable);
    }

    /** Subclasses can override this for a custom confirmation dialog */
    protected virtual Adw.AlertDialog create_confirm_dialog() {
        var title = ngettext("Are you sure you want to permanently delete %u item?",
                             "Are you sure you want to permanently delete %u items?",
                             get_n_items()).printf(get_n_items());
        // Try to do better if we have one item only
        if (get_n_items() == 1) {
            if (this.items[0].get_class().find_property("label") != null) {
                string label;
                this.items[0].get("label", out label);
                title = _("Are you sure you want to permanently delete “%s”?").printf(label);
            }
        }
        var body = _("If you delete an item, it will be permanently lost");
        var dialog = new Adw.AlertDialog(title, body);

        dialog.add_response("cancel", _("_Cancel"));
        dialog.set_default_response("cancel");

        dialog.add_response("delete", _("_Delete"));
        dialog.set_response_appearance("delete", Adw.ResponseAppearance.DESTRUCTIVE);

        return dialog;
    }

    public GLib.Type get_item_type() {
        return typeof(Deletable);
    }

    public uint get_n_items() {
        return this.items.length;
    }

    public GLib.Object? get_item(uint index) {
        if (index >= this.items.length)
            return null;
        return this.items[index];
    }

    public bool contains(Deletable item) {
        foreach (unowned var it in this.items) {
            if (it == item)
                return true;
        }
        return false;
    }
}
