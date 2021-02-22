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
    private Gcr.UnionCollection union_collection = new Gcr.UnionCollection();
    private HashTable<string, weak GLib.Object> handles
        = new HashTable<string, weak GLib.Object>.full(str_hash, str_equal, free, null);

    private Gcr.FilterCollection collection;
    private GLib.Application app;
    private int n_loading = 0;
    private RWLock n_loading_lock = RWLock();
    private bool loaded = false;

    public SearchProvider(GLib.Application app) {
        this.app = app;
        this.collection = new Gcr.FilterCollection.with_callback(this.union_collection, filter_objects);
    }

    private static bool filter_objects (GLib.Object? obj) {
        unowned Object? object = obj as Object;
        if (object == null || !(Flags.PERSONAL in object.object_flags))
            return false;

        if (!(obj is Viewable))
            return false;

        if (obj is Secret.Item) {
            string? schema_name = ((Secret.Item) obj).get_schema_name ();
            if (schema_name != "org.gnome.keyring.Note")
                return false;
        }

        return true;
    }

    ~SearchProvider() {
        this.handles.foreach( (__, obj) => {
            obj.weak_unref(on_object_gone);
        });
    }

    public async void load () throws GLib.Error {
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

        var backends = Backend.get_registered();
        this.n_loading = (int) backends.length();
        foreach (var backend in backends) {
            if (!backend.loaded) {
                backend.notify["loaded"].connect(() => {
                    change_n_loading(-1);
                    if (get_n_loading() == 0)
                        this.loaded = true;
                });
            } else {
                change_n_loading(-1);
            }

            backend.added.connect(on_place_added);
            backend.removed.connect(on_place_removed);

            foreach (GLib.Object place in backend.get_objects())
                on_place_added(backend, place);
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

        Predicate predicate = Predicate () {
            custom = object_matches_search,
            custom_target = terms
        };

        string?[] results = {};
        foreach (unowned GLib.Object obj in this.collection.get_objects()) {
            if (predicate.match(obj)) {
                string str = "%p".printf(obj);

                if (!(str in this.handles)) {
                    this.handles.insert(str, obj);
                    obj.weak_ref(on_object_gone);
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

        Predicate predicate = Predicate() {
            custom = object_matches_search,
            custom_target = new_terms
        };

        string?[] results = {};
        foreach (string previous_result in previous_results) {
            unowned GLib.Object? object = this.handles.lookup(previous_result);
            if (object == null || !this.collection.contains(object))
                continue; // Bogus value

            if (predicate.match(object))
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
            unowned GLib.Object object = this.handles.lookup(result);
            if (object == null || !(object in this.collection))
                continue; // Bogus value

            HashTable<string, Variant> meta = new HashTable<string, Variant>(str_hash, str_equal);

            meta["id"] = result;

            string? label = null;
            object.get("label", out label, null);
            if (label != null)
                meta["name"] = label;

            Icon? icon = null;
            object.get("icon", out icon, null);
            if (icon != null)
                meta["icon"] = icon.serialize();

            string? description = get_description_if_available(object);
            if (description != null)
                meta["description"] = Markup.escape_text(description);

            metas[good_results] = meta;
            good_results++;
        }

        this.app.release();
        return metas[0:good_results];
    }

    public void ActivateResult(string identifier, string[] terms, uint32 timestamp) throws GLib.Error {
        this.app.hold();

        unowned GLib.Object? object = null;
        identifier.scanf("%p", &object);
        object = this.handles.lookup(identifier);
        if (object == null || !(object in this.collection) || !(object is Viewable))
            return; // Bogus value

        KeyManager key_manager = new KeyManager(GLib.Application.get_default() as Application);
        /* key_manager.show(timestamp); */
        Viewable.view(object, (Gtk.Window) key_manager);

        this.app.release ();
    }

    public void LaunchSearch (string[] terms, uint32 timestamp) throws GLib.Error {
        // TODO
    }

    private static bool object_matches_search (GLib.Object? object, void* terms) {
        foreach (unowned string term in ((string[]) terms)) {
            if (!object_contains_filtered_text (object, term))
                return false;
        }

        return true;
    }

    /* Search through row for text */
    private static bool object_contains_filtered_text (GLib.Object object, string? text) {
        string? name = null;
        object.get("label", out name, null);
        if (name != null && (text in name.down()))
            return true;

        string? description = get_description_if_available (object);
        if (description != null && (text in description.down()))
            return true;

        return false;
    }

    private void on_object_gone(GLib.Object? where_the_object_was) {
        this.handles.remove("%p".printf(where_the_object_was));
    }

    private void on_place_added (Gcr.Collection places, GLib.Object object) {
        unowned var place = (Place) object;
        if (!this.union_collection.have(place))
            this.union_collection.add(place);
    }

    private void on_place_removed (Gcr.Collection places, GLib.Object object) {
        unowned var place = (Place) object;
        if (this.union_collection.have(place))
            this.union_collection.remove(place);
    }

    private static string? get_description_if_available (GLib.Object? obj) {
        if (obj == null)
            return null;

        if (obj.get_class().find_property("description") == null)
            return null;

        string? description = null;
        obj.get("description", out description);
        return description;
    }
}
