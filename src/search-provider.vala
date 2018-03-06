/*
 * Seahorse
 *
 * Copyright (C) 2013 Giovanni Campagna <scampa.giovanni@gmail.com>
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

//#define SECRET_API_SUBJECT_TO_CHANGE

    /* SeahorseShellSearchProvider2SkeletonClass parent_class; */
[DBus (name = "org.gnome.Shell.SearchProvider2")]
public class Seahorse.SearchProvider : GLib.Object {
    private Gcr.UnionCollection union_collection = new Gcr.UnionCollection();

    private Predicate base_predicate = Predicate();
    private Gcr.Collection collection;
    private HashTable<string, weak Object> handles;
    private int n_loading = 0;
    private bool loaded = false;

    public SearchProvider() {
        this.base_predicate.flags = Flags.PERSONAL;
        this.base_predicate.custom = check_object_type;
        this.collection = new Collection.for_predicate(this.union_collection, this.base_predicate, null);
        this.handles = new HashTable<string, weak Object>.full(str_hash, str_equal, free, null);
    }

    ~SearchProvider() {
        this.handles.foreach( (__, obj) => {
            obj.weak_unref(on_object_gone);
        });
    }

    private async void load () {
        // avoid reentering load () from a different request
        while (!loaded && n_loading >= 0) {
            var wait = notify["loaded"].connect (() => {
                load.callback ();
            });

            yield;

            disconnect (wait);
            if (this.loaded)
                return;
        }

        foreach (Backend backend in Backend.get_registered()) {
            // XXX won't this give races?
            this.n_loading++;

            backend.notify["loaded"].connect(on_backend_loaded);
            backend.added.connect(on_place_added);
            backend.removed.connect(on_place_removed);

            foreach (GLib.Object place in backend.get_objects())
                on_place_added(backend, place);
        }
    }

    public async string[] GetInitialResultSet(string[] terms) {
        hold_app();

        if (this.n_loading >= 0)
            yield load();

        Predicate predicate = Predicate () {
            custom = object_matches_search,
            custom_target = terms
        };

        string?[] results = {};
        foreach (GLib.Object obj in this.collection.get_objects()) {
            if (predicate.match(obj)) {
                string str = "%p".printf(obj);

                if (!(str in this.handles)) {
                    this.handles.insert(str, (Object) obj);
                    obj.weak_ref(on_object_gone);
                }
                results += str;
            }
        }

        release_app ();
        results += null;
        return results;
    }

    public async string[] GetSubsearchResultSet(string[] previous_results, string[] new_terms) {
        hold_app ();

        Predicate predicate = Predicate() {
            custom = object_matches_search,
            custom_target = new_terms
        };

        string?[] results = {};
        foreach (string previous_result in previous_results) {
            GLib.Object? object = this.handles.lookup(previous_result);
            if (object == null || !this.collection.contains(object))
                continue; // Bogus value

            if (predicate.match(object))
                results += previous_result;
        }

        release_app ();
        results += null;
        return results;
    }

    public async HashTable<string, Variant>[] GetResultMetas(string[] results) {
        hold_app();

        var metas = new HashTable<string, Variant>[results.length];
        int good_results = 0;
        foreach (string result in results) {
            Seahorse.Object object = this.handles.lookup(result);
            if (object == null || !(object in this.collection))
                continue; // Bogus value

            HashTable<string, Variant> meta = new HashTable<string, Variant>(str_hash, str_equal);

            meta["id"] = result;
            if (object.label != null)
                meta["name"] = object.label;
            if (object.icon != null)
                meta["icon"] = object.icon.serialize();

            string? description = get_description_if_available(object);
            if (description != null)
                meta["name"] = Markup.escape_text(description);

            metas[good_results] = meta;
            good_results++;
        }

        release_app();
        return metas[0:good_results];
    }

    public void ActivateResult(string identifier, string[] terms, uint32 timestamp) {
        hold_app();

        Object? object = null;
        identifier.scanf("%p", &object);
        object = this.handles.lookup(identifier);
        if (object == null || !(object in this.collection) || !(object is Viewable))
            return; // Bogus value

        KeyManager key_manager = new KeyManager((Application)GLib.Application.get_default());
        /* key_manager.show(timestamp); */
        Viewable.view(object, (Gtk.Window) key_manager);

        release_app ();
    }

    public void LaunchSearch (string[] terms, uint32 timestamp) {
        // TODO
    }

    private static bool object_matches_search (GLib.Object? object, void* terms) {
        foreach (string term in ((string[]) terms)) {
            if (!object_contains_filtered_text (object as Object, term))
                return false;
        }

        return true;
    }

    /* Search through row for text */
    private static bool object_contains_filtered_text (Seahorse.Object object, string? text) {
        string? name = object.label;
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

    // We called before loading, we queue GetInitialResultSet, but
    // we drop all other calls, because we don't expect to see them
    // before we reply to GetInitialResultSet

    private void hold_app() {
        GLib.Application.get_default().hold();
    }

    private void release_app() {
        GLib.Application.get_default().release();
    }

    private void on_place_added (Gcr.Collection places, GLib.Object object) {
        Place place = (Place) object;
        if (!this.union_collection.have(place))
            this.union_collection.add(place);
    }

    private void on_place_removed (Gcr.Collection places, GLib.Object object) {
        Place place = (Place) object;
        if (this.union_collection.have(place))
            this.union_collection.remove(place);
    }

    private void on_backend_loaded (GLib.Object? object, ParamSpec pspec) {
        this.n_loading--;
        if (this.n_loading == 0)
            this.loaded = true;
    }

    private static bool check_object_type (GLib.Object? object, void* custom_target) {
        if (!(object is Viewable))
            return false;

        if (object is Secret.Item) {
            string? schema_name = ((Secret.Item) object).get_schema_name ();
            if (schema_name != "org.gnome.keyring.Note")
                return false;
        }

        return true;
    }

    /* public bool dbus_register (DBusConnection connection, string? object_path) throws GLib.Error { */
    /*     return export (connection, object_path, error); */
    /* } */

    /* public void dbus_unregister (DBusConnection connection, string? object_path) { */
    /*     if (has_connection(connection)) */
    /*         unexport_from_connection(connection); */
    /* } */

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
