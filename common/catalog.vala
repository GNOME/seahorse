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

public abstract class Catalog : Gtk.ApplicationWindow {

    /* Set by the derived classes */
    public string ui_name { construct; get; }

    protected MenuModel context_menu;
    private bool _disposed;
    private GLib.Settings _settings;

    public abstract GLib.List<weak Backend> get_backends();
    public abstract GLib.List<GLib.Object> get_selected_objects();

    private const ActionEntry[] ACTION_ENTRIES = {
        { "file-export",         on_key_export_file },
        { "copy",                on_key_export_clipboard },
        { "edit-delete",         on_object_delete },
        { "properties-object",   on_properties_object },
    };

    construct {
        /* Load window size for windows that aren't dialogs */
        var key = "/apps/seahorse/windows/%s/".printf(this.ui_name);
        this._settings = new GLib.Settings.with_path("org.gnome.seahorse.window", key);
        var width = this._settings.get_int("width");
        var height = this._settings.get_int("height");
        if (width > 0 && height > 0)
            this.resize (width, height);

        Gtk.Builder builder = new Gtk.Builder.from_resource(
            "/org/gnome/Seahorse/seahorse-%s-widgets.ui".printf(this.ui_name)
        );
        this.context_menu = (MenuModel) builder.get_object("context_menu");

        add_action_entries (ACTION_ENTRIES, this);
    }

    public override void dispose() {
        if (!this._disposed) {
            this._disposed = true;

            int width, height;
            this.get_size(out width, out height);
            this._settings.set_int("width", width);
            this._settings.set_int("height", height);
        }

        base.dispose();
    }

    public virtual signal void selection_changed() {
        bool can_properties = false;
        bool can_delete = false;
        bool can_export = false;

        var objects = this.get_selected_objects();
        foreach (var object in objects) {
            if (Exportable.can_export(object))
                can_export = true;
            if (Deletable.can_delete(object))
                can_delete = true;
            if (Viewable.can_view(object))
                can_properties = true;
            if (can_export && can_delete && can_properties)
                break;
        }

        ((SimpleAction) lookup_action("properties-object")).set_enabled(can_properties);
        ((SimpleAction) lookup_action("edit-delete")).set_enabled(can_delete);;
        ((SimpleAction) lookup_action("copy")).set_enabled(can_export);
        ((SimpleAction) lookup_action("file-export")).set_enabled(can_export);
    }

    public void show_properties(GLib.Object obj) {
        Viewable.view(obj, this);
    }

    public void show_context_menu(Gdk.Event? event) {
        Gtk.Menu menu = new Gtk.Menu.from_model(this.context_menu);
        menu.insert_action_group("win", this);
        foreach (weak Backend backend in get_backends()) {
            ActionGroup actions = backend.actions;
            menu.insert_action_group(actions.prefix, actions);
        }
        menu.popup_at_pointer(event);
        menu.show();
    }

    private void on_object_delete(SimpleAction action, Variant? param) {
        try {
            var objects = this.get_selected_objects();
            Deletable.delete_with_prompt_wait(objects, this);
        } catch (GLib.Error err) {
            Util.show_error(this, _("Cannot delete"), err.message);
        }
    }

    private void on_properties_object(SimpleAction action, Variant? param) {
        var objects = get_selected_objects();
        if (objects.length() > 0)
            show_properties(objects.data);
    }

    private void on_key_export_file(SimpleAction action, Variant? param) {
        try {
            Exportable.export_to_prompt_wait(get_selected_objects(), this);
        } catch (GLib.Error err) {
            Util.show_error(this, _("Couldn’t export keys"), err.message);
        }
    }

    private void on_key_export_clipboard (SimpleAction action, Variant? param) {
        uint8[] output;
        try {
            var objects = this.get_selected_objects ();
            Exportable.export_to_text_wait (objects, out output);
        } catch (GLib.Error err) {
            Util.show_error(this, _("Couldn’t export data"), err.message);
            return;
        }

        /* TODO: Print message if only partially exported */

        var board = Gtk.Clipboard.get(Gdk.SELECTION_CLIPBOARD);
        board.set_text ((string)output, output.length);
    }
}

}
