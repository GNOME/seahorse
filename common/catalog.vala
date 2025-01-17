/*
 * Seahorse
 *
 * Copyright (C) 2008 Stefan Walter
 * Copyright (C) 2011 Collabora Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

namespace Seahorse {

public abstract class Catalog : Adw.ApplicationWindow {

    /* Set by the derived classes */
    public string ui_name { construct; get; }

    protected MenuModel context_menu;
    private GLib.Settings _settings;

    public abstract GLib.List<Seahorse.Item> get_selected_items();

    private const ActionEntry[] ACTION_ENTRIES = {
        { "file-export", on_key_export_file },
        { "copy", on_key_export_clipboard },
        { "edit-delete", on_item_delete },
        { "properties-item", on_properties_item },
    };

    construct {
        /* Load window size for windows that aren't dialogs */
        var key = "/apps/seahorse/windows/%s/".printf(this.ui_name);
        this._settings = new GLib.Settings.with_path("org.gnome.seahorse.window", key);
        var width = this._settings.get_int("width");
        var height = this._settings.get_int("height");
        if (width > 0 && height > 0)
            set_default_size (width, height);

        Gtk.Builder builder = new Gtk.Builder.from_resource(
            "/org/gnome/Seahorse/seahorse-%s-widgets.ui".printf(this.ui_name)
        );
        this.context_menu = (MenuModel) builder.get_object("context_menu");

        add_action_entries (ACTION_ENTRIES, this);
    }

    public override bool close_request() {
        this._settings.set_int("width", this.default_width);
        this._settings.set_int("height", this.default_height);

        return base.close_request();
    }

    public virtual signal void selection_changed() {
        bool can_delete = false;
        bool can_export = false;

        var items = this.get_selected_items();
        foreach (var item in items) {
            if (Exportable.can_export(item))
                can_export = true;
            if (Deletable.can_delete(item))
                can_delete = true;
            if (can_export && can_delete)
                break;
        }

        ((SimpleAction) lookup_action("edit-delete")).set_enabled(can_delete);;
        ((SimpleAction) lookup_action("copy")).set_enabled(can_export);

        // FIXME: for now, we only allow exporting a single item at a time
        ((SimpleAction) lookup_action("file-export")).set_enabled(can_export && items.length() == 1);
    }

    public void show_properties(Seahorse.Item item) {
        item.view(this);
    }

    private void on_item_delete(SimpleAction action, Variant? param) {
        var items = this.get_selected_items();
        if (items.length() == 0)
            return;

        // Sanity check
        unowned Deletable first = null;
        foreach (unowned var item in items) {
            if (!(item is Deletable)) {
                items.remove(item);
                continue;
            }
            if (first == null)
                first = (Deletable) item;
        }

        var delete_op = first.create_delete_operation();
        // XXX
        // foreach (unowned var obj in items.next()) {
        //     delete_op.add_item();
        // }

        delete_op.execute_interactively.begin(this, null, (obj, res) => {
            try {
                delete_op.execute_interactively.end(res);
            } catch (GLib.IOError.CANCELLED e) {
                debug("Deletion of %u items cancelled by user", delete_op.get_n_items());
            } catch (GLib.Error e) {
                Util.show_error(this, _("Couldn’t delete items"), e.message);
            }
        });
    }

    private void on_properties_item(SimpleAction action, Variant? param) {
        var items = get_selected_items();
        if (items.length() > 0)
            show_properties(items.data);
    }

    private void on_key_export_file(SimpleAction action, Variant? param) {
        export_file_async.begin();
    }

    private async void export_file_async() {
        var exportable = get_selected_items().data as Exportable;
        var export_op = exportable.create_export_operation();

        try {
            var prompted = yield export_op.prompt_for_file(this);
            if (!prompted) {
                debug("no file picked by user");
                return;
            }

            yield export_op.execute(null);
        } catch (GLib.IOError.CANCELLED e) {
            debug("Exporting of item cancelled by user");
        } catch (Error e) {
            Util.show_error(this, _("Couldn’t export item"), e.message);
        }
    }

    private void on_key_export_clipboard(SimpleAction action, Variant? param) {
        key_export_clipboard_async.begin();
    }

    private async void key_export_clipboard_async() {
        var output = new MemoryOutputStream.resizable();

        // Do the export
        var exportable = get_selected_items().data as Exportable;
        var export_op = exportable.create_export_operation();
        export_op.output = output;

        try {
            yield export_op.execute(null);

            output.write ("\0".data);
            output.close();

            var val = Value(typeof(string));
            val.set_string((string) output.get_data());

            /* TODO: Print message if only partially exported */

            var board = get_clipboard();
            board.set_value(val);
        } catch (GLib.IOError.CANCELLED e) {
            debug("Exporting of item cancelled by user");
        } catch (Error e) {
            Util.show_error(this, _("Couldn’t export item"), e.message);
        }
    }
}

}
