/*
 * Seahorse
 *
 * Copyright (C) 2004-2006 Stefan Walter
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

namespace Seahorse.ComboKeys {

public void attach(Gtk.ComboBox combo, Gcr.Collection collection, string none_option) {
    combo.set_data_full("combo-keys-closure", new ComboClosure(), combo_closure_free);

    /* Setup the None Option */
    Gtk.TreeModel? model = combo.model;
    if (model == null) {
        model = new Gtk.ListStore(N_COLUMNS, typeof(string), typeof(string), typeof(void*));
        combo.set_model(model);

        combo.clear();
        Gtk.CellRenderer renderer = new Gtk.CellRendererText();

        combo.pack_start (renderer, true);
        combo.add_attribute (renderer, "markup", COMBO_MARKUP);
    }

    /* Setup the object list */
    foreach (weak GLib.Object obj in collection.get_objects ())
        on_collection_added (collection, obj, combo);

    collection.added.connect(on_collection_added);
    collection.removed.connect(on_collection_removed);

    if (none_option) {
        model.prepend(&iter);
        model.set(&iter, COMBO_LABEL, null,
                         COMBO_MARKUP, none_option,
                         COMBO_POINTER, null, -1);
    }

    Gtk.TreeIter iter;
    model.get_iter_first (out iter);
    combo.set_active_iter(iter);

    combo.destroy.connect(() => {
        Gcr.Collection collection = GCR_COLLECTION (user_data);
        foreach (GLib.Object object in collection.get_objects())
            SignalHandler.disconnect_by_func((void*) l.data, (void*) on_label_changed, combo);
        SignalHandler.disconnect_by_func((void*) collection, (void*) on_collection_added, combo);
        SignalHandler.disconnect_by_func((void*) collection, (void*) on_collection_removed, combo);
    });
}

public void set_active_id(Gtk.ComboBox combo, string? keyid) {
    Gtk.TreeModel model = combo.model;
    g_return_if_fail (model != null);

    Gtk.TreeIter iter;
    bool valid = model.get_iter_first (out iter);
    uint i = 0;

    while (valid) {
        void* pointer;
        model.get(iter, COMBO_POINTER, out pointer, -1);

        Pgp.Key? key = pointer as Pgp.Key;

        if (keyid == null) {
            if (key == null) {
                combo.set_active_iter(iter);
                break;
            }
        } else if (key != null) {
            if (key.get_keyid() == keyid) {
                combo.set_active_iter(iter);
                break;
            }
        }

        valid = model.iter_next (&iter);
        i++;
    }
}

public void set_active(Gtk.ComboBox combo, Pgp.Key key) {
    set_active_id(combo, key == null ? null : key.keyid);
}

public Pgp.Key get_active(Gtk.ComboBox combo) {
    if (combo == null || combo.model == null)
        return null;

    Gtk.TreeIter iter;
    combo.get_active_iter(out iter);

    void* pointer;
    combo.model.get(iter, COMBO_POINTER, out pointer, -1);

    return (Pgp.Key) pointer;
}

public string get_active_id(Gtk.ComboBox combo) {
    Pgp.Key key = ComboKeys.get_active(combo);
    return (key == null)? 0 : key.get_keyid();
}

private enum Column {
  COMBO_LABEL,
  COMBO_MARKUP,
  COMBO_POINTER,
  N_COLUMNS
}

private class ComboClosure {
    GenericSet<string> labels;
    bool collision;

    public ComboClosure () {
        this.labels = new GenericSet<string>(str_hash, str_equal);
    }
}

private void refresh_all_markup_in_combo(Gtk.ComboBox combo) {
    Gtk.TreeModel model = combo.get_model();
    Gtk.TreeIter iter;
    bool valid = model.get_iter_first (out iter);

    while (valid) {
        void* object;
        model.get(iter, COMBO_POINTER, out object, -1);
        g_object_notify (object, "label");
        valid = model.iter_next(&iter);
    }
}

private string calculate_markup_for_object(Gtk.ComboBox combo, string label, Object object) {
    ComboClosure closure = combo.get_data("combo-keys-closure");

    if (!closure.collision) {
        if (closure.labels.contains(label)) {
            closure.collision = true;
            refresh_all_markup_in_combo (combo);
        } else {
            closure.labels.add(label);
        }
    }

    string markup;
    if (closure.collision && (object is Pgp.Key)) {
        string ident = Pgp.Key.calc_identifier(((Pgp.Key) object).get_keyid());
        markup = Markup.printf_escaped("%s <span size='small'>[%s]</span>", label, ident);
    } else {
        markup = Markup.escape_text(label, -1);
    }

    return markup;
}

private void on_label_changed(GLib.Object obj, ParamSpec param) {
    Object object = SEAHORSE_OBJECT (obj);
    Gtk.ComboBox combo = GTK_COMBO_BOX (user_data);
    ComboClosure closure;
    Gtk.TreeModel? model;
    Gtk.TreeIter iter;

    closure = combo.get_data("combo-keys-closure");
    Gtk.TreeModel model = combo.get_model();

    bool valid = model.get_iter_first(&iter);
    while (valid) {
        string previous;
        void* pntr;
        model.get(&iter, COMBO_POINTER, &pntr, COMBO_LABEL, &previous, -1);
        if (SEAHORSE_OBJECT (pntr) == object) {

            /* Remove this from label collision checks */
            closure.labels.remove(previous);

            /* Calculate markup taking into account label collisions */
            string markup = calculate_markup_for_object (combo, object.label, object);
            model.set(iter, COMBO_LABEL, object.label,
                            COMBO_MARKUP, markup, -1);
            break;
        }

        valid = model.iter_next(&iter);
    }
}

private void on_collection_added(Gcr.Collection collection, GLib.Object obj) {
    Object object = SEAHORSE_OBJECT (obj);
    Gtk.ComboBox combo = GTK_COMBO_BOX (user_data);

    Gtk.ListStore model = GTK_LIST_STORE (combo.get_model());

    string markup = calculate_markup_for_object (combo, object.label, object);

    Gtk.TreeIter iter;
    model.append(out iter);
    model.set(iter, COMBO_LABEL, object.label,
                    COMBO_MARKUP, markup,
                    COMBO_POINTER, object, -1);

    object.notify["label"].connect(on_label_changed, combo);
}

private void on_collection_removed (Gcr.Collection collection, GLib.Object obj) {
    ComboClosure closure = g_object_get_data (user_data, "combo-keys-closure");
    Object object = SEAHORSE_OBJECT (obj);
    Gtk.ComboBox combo = GTK_COMBO_BOX (user_data);
    Gtk.TreeModel? model = combo.get_model();
    Gtk.TreeIter iter;
    bool valid = model.get_iter_first(out iter);
    while (valid) {
        char previous;
        void* pntr;
        model.get(iter, COMBO_LABEL, out previous,
                        COMBO_POINTER, out pntr, -1);

        if (SEAHORSE_OBJECT (pntr) == object) {
            closure.labels.remove(previous);
            model.remove(&iter);
            break;
        }

        valid = model.iter_next(ref iter);
    }

    SignalHandler.disconnect_by_func((void*) object, (void*) on_label_changed, combo);
}


}
