/*
 * Seahorse
 *
 * Copyright (C) 2005 Stefan Walter
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

public class Seahorse.Collection : Gcr.Collection, GLib.Object {
    private GenericSet<GLib.Object?> objects = new GenericSet<GLib.Object?>(direct_hash, direct_equal);

    private DestroyNotify destroy_func;

    /**
     * Base_collection collection
     */
    public Gcr.Collection base_collection { get; construct set; }

    /**
     * Predicate for matching objects into this set.
     */
    public Predicate? predicate { get; private set; }

    public Collection.for_predicate (Gcr.Collection base_collection, Predicate? pred, DestroyNotify destroy_func) {
        GLib.Object (base_collection: base_collection);
        this.predicate = pred;

        this.destroy_func = destroy_func;

        this.base_collection.added.connect(on_base_collection_added);
        this.base_collection.removed.connect(on_base_collection_removed);

        refresh ();
    }

    ~Collection() {
        SignalHandler.disconnect_by_func((void*) this.base_collection, (void*) on_base_collection_added, this);
        SignalHandler.disconnect_by_func((void*) this.base_collection, (void*) on_base_collection_removed, this);

        foreach (GLib.Object obj in this.objects) {
            SignalHandler.disconnect_by_func((void*) obj, (void*) on_object_changed, this);
            emit_removed(obj);
        }
    }

    public void refresh() {
        // Make note of all the objects we had prior to refresh
        GenericSet<GLib.Object?> check = new GenericSet<GLib.Object?>(direct_hash, direct_equal);
        foreach(GLib.Object? obj in this.objects)
            check.add(obj);

        foreach (weak GLib.Object object in get_objects()) {
            // Make note that we've seen this object
            check.remove(object);

            // This will add to set
            if (!maybe_remove_object(object))
                if (maybe_add_object(object))
                    object.notify.connect(on_object_changed);
        }

        foreach (GLib.Object obj in check) {
            SignalHandler.disconnect_by_func ((void*) obj, (void*) on_object_changed, (void*) this);
            remove_object (obj);
        }
    }

    private void on_object_changed (GLib.Object obj, ParamSpec spec) {
        if (obj in objects)
            maybe_remove_object(obj);
        else
            maybe_add_object(obj);
    }

    public uint get_length() {
        return this.objects.length;
    }

    public List<weak GLib.Object> get_objects() {
        List<GLib.Object> result = new List<GLib.Object>();
        foreach (GLib.Object obj in this.objects)
            result.append(obj);
        return result;
    }

    public bool contains(GLib.Object object) {
        return object in this.objects;
    }

    private void remove_object(GLib.Object object) {
        this.objects.remove(object);
        removed(object);
    }

    private bool maybe_add_object(GLib.Object obj) {
        if (obj in objects)
            return false;

        if (this.predicate == null || !this.predicate.match(obj))
            return false;

        this.objects.add(obj);
        emit_added(obj);
        return true;
    }

    private bool maybe_remove_object(GLib.Object obj) {
        if (!(obj in objects))
            return false;

        if (this.predicate == null || this.predicate.match(obj))
            return false;

        remove_object(obj);
        return true;
    }

    private void on_base_collection_added (Gcr.Collection base_collection, GLib.Object obj) {
        obj.notify.connect(on_object_changed);
        maybe_add_object(obj);
    }

    private void on_base_collection_removed (Gcr.Collection base_collection, GLib.Object object) {
        SignalHandler.disconnect_by_func (object, (void*) on_object_changed, this);

        if (object in objects)
            remove_object (object);
    }
}
