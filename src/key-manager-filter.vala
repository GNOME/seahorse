/*
 * Seahorse
 *
 * Copyright (C) 2022 Niels De Graef
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
 * License along with this program; if not, see
 * <http://www.gnu.org/licenses/>.
 */

public class Seahorse.KeyManagerFilter : Gtk.Filter {

    public enum ShowFilter {
        ANY,
        PERSONAL,
        TRUSTED;

        public unowned string? to_string() {
            switch (this) {
                case ShowFilter.ANY:
                    return "";
                case ShowFilter.PERSONAL:
                    return "personal";
                case ShowFilter.TRUSTED:
                    return "trusted";
                default:
                    assert_not_reached();
            }
        }

        public static ShowFilter from_string(string? str) {
            switch (str) {
                case null:
                case "":
                case "any":
                    return ShowFilter.ANY;
                case "personal":
                    return ShowFilter.PERSONAL;
                case "trusted":
                    return ShowFilter.TRUSTED;
                default:
                    critical ("Got unknown ShowFilter string: %s", str);
                    assert_not_reached();
            }
        }
    }

    public ShowFilter show_filter {
        get { return this._show_filter; }
        set {
            if (value != this._show_filter) {
                this._show_filter = value;
                changed(Gtk.FilterChange.DIFFERENT);
            }
        }
    }
    private ShowFilter _show_filter = ShowFilter.ANY;

    public string filter_text {
        get { return this._filter_text; }
        set {
            if (value.casefold() == this._filter_text)
                return;
            this._filter_text = value.casefold();
            changed(Gtk.FilterChange.DIFFERENT);
        }
    }
    private string _filter_text = "";

    public override bool match(GLib.Object? object) {
        unowned var item = (Seahorse.Item) object;
        return matches_showfilter(item)
            && object_contains_filtered_text(item, this._filter_text);
    }

    private bool matches_showfilter(Seahorse.Item item) {
        switch (this.show_filter) {
            case ShowFilter.PERSONAL:
                return Seahorse.Flags.PERSONAL in item.item_flags;
            case ShowFilter.TRUSTED:
                return Seahorse.Flags.TRUSTED in item.item_flags;
            case ShowFilter.ANY:
                return true;
        }

        return false;
    }

    // Search through row for text
    private bool object_contains_filtered_text(Seahorse.Item item, string? text) {
        // Empty search text results in a match
        if (text == null || text == "")
            return true;

        if (text in item.title.down())
            return true;

        var description = item.description;
        if (description != null && (text in description.down()))
            return true;

        return false;
    }

    public override Gtk.FilterMatch get_strictness () {
        return Gtk.FilterMatch.SOME;
    }
}
