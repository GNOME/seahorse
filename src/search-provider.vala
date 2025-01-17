/*
 * Seahorse
 *
 * Copyright (C) 2013 Giovanni Campagna <scampa.giovanni@gmail.com>
 * Copyright (C) 2018 Niels De Graef
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

[DBus (name = "org.gnome.Shell.SearchProvider2")]
public class Seahorse.SearchProvider : GLib.Object {

    private GLib.ListModel backends = Backend.get_registered();
    private Gtk.FilterListModel collection;

    private HashTable<string, weak Seahorse.Item> handles
        = new HashTable<string, weak Seahorse.Item>.full(str_hash, str_equal, free, null);

    private GLib.Application app;
    private int n_loading = 0;
    private RWLock n_loading_lock = RWLock();
    private bool loaded = false;

    public SearchProvider(GLib.Application app) {
        this.app = app;

        var places = new Gtk.FlattenListModel(this.backends);
        var filter = new Gtk.CustomFilter(filter_objects);
        this.collection = new Gtk.FilterListModel(places, filter);
    }

    private static bool filter_objects(GLib.Object obj) {
        unowned var item = obj as Seahorse.Item;
        if (item == null || !(Flags.PERSONAL in item.item_flags))
            return false;

        string? schema_name = ((Secret.Item) obj).get_schema_name ();
        if (schema_name != "org.gnome.keyring.Note")
            return false;

        return true;
    }

    public async void load() throws GLib.Error {
        // avoid reentering load () from a different request
        while (!this.loaded && get_n_loading() > 0) {
            var wait = notify["loaded"].connect (() => {
                load.callback ();
            });

            yield;

            disconnect (wait);
            if (this.loaded)
                return;
        }

        this.n_loading = (int) this.backends.get_n_items();
        for (uint i = 0; i < backends.get_n_items(); i++) {
            var backend = (Backend) backends.get_item(i);
            if (!backend.loaded) {
                backend.notify["loaded"].connect(() => {
                    change_n_loading(-1);
                    if (get_n_loading() == 0)
                        this.loaded = true;
                });
            } else {
                change_n_loading(-1);
            }
        }
    }

    private int get_n_loading() {
        this.n_loading_lock.reader_lock();
        var retval = this.n_loading;
        this.n_loading_lock.reader_unlock();
        return retval;
    }

    private void change_n_loading(int diff) {
        this.n_loading_lock.writer_lock();
        this.n_loading += diff;
        this.n_loading_lock.writer_unlock();
    }

    public async string[] GetInitialResultSet(string[] terms) throws GLib.Error {
        this.app.hold();

        if (get_n_loading() >= 0)
            yield load();

        string?[] results = {};
        for (uint i = 0; i < this.collection.get_n_items(); i++) {
            var item = this.collection.get_item(i) as Seahorse.Item;

            if (item_matches_search(item, terms)) {
                string str = "%p".printf(item);

                if (!(str in this.handles)) {
                    this.handles.insert(str, item);
                    item.weak_ref(on_object_gone);
                }
                results += (owned) str;
            }
        }

        this.app.release ();
        return results;
    }

    public async string[] GetSubsearchResultSet(string[] previous_results, string[] new_terms)
            throws GLib.Error {
        this.app.hold ();

        string?[] results = {};
        foreach (string previous_result in previous_results) {
            unowned var item = this.handles.lookup(previous_result);
            if (item == null)
                continue; // Bogus value

            if (item_matches_search(item, new_terms))
                results += previous_result;
        }

        this.app.release ();
        return results;
    }

    public async HashTable<string, Variant>[] GetResultMetas(string[] results) throws GLib.Error {
        this.app.hold();

        var metas = new HashTable<string, Variant>[results.length];
        int good_results = 0;
        foreach (unowned string result in results) {
            unowned var item = this.handles.lookup(result);
            if (item == null)
                continue; // Bogus value

            HashTable<string, Variant> meta = new HashTable<string, Variant>(str_hash, str_equal);

            meta["id"] = result;
            meta["name"] = item.title;
            unowned var icon = item.icon;
            if (icon != null)
                meta["icon"] = icon.serialize();

            string? subtitle = item.subtitle;
            if (subtitle != null)
                meta["description"] = subtitle;

            metas[good_results] = meta;
            good_results++;
        }

        this.app.release();
        return metas[0:good_results];
    }

    public void ActivateResult(string identifier, string[] terms, uint32 timestamp) throws GLib.Error {
        this.app.hold();

        unowned Seahorse.Item? item = null;
        identifier.scanf("%p", &item);
        item = this.handles.lookup(identifier);
        if (!(item is Seahorse.Item))
            return; // Bogus value

        KeyManager key_manager = new KeyManager(GLib.Application.get_default() as Application);
        item.view((Gtk.Window) key_manager);

        this.app.release ();
    }

    public void LaunchSearch (string[] terms, uint32 timestamp) throws GLib.Error {
        // TODO
    }

    private bool item_matches_search(Seahorse.Item item, string[] terms) {
        foreach (unowned string term in ((string[]) terms)) {
            if (!item_contains_filtered_text(item, term))
                return false;
        }

        return true;
    }

    /* Search through row for text */
    private bool item_contains_filtered_text(Seahorse.Item item, string? text) {
        if (text in item.title.down())
            return true;

        var subtitle = item.subtitle;
        if (subtitle != null && (text in subtitle.down()))
            return true;

        return false;
    }

    private void on_object_gone(GLib.Object? where_the_object_was) {
        this.handles.remove("%p".printf(where_the_object_was));
    }
}
