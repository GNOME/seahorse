/*
 * Seahorse
 *
 * Copyright (C) 2008 Stefan Walter
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

/**
 * Describes a category of servers
 */
public class Seahorse.ServerCategory : GLib.Object {
    [CCode (cname = "SeahorseValidUriFunc", has_target = false)]
    public delegate bool ValidUriFunc(string uri);

    /** The scheme of servers (ie the scheme) */
    public string scheme { get; construct set; }

    /** User-facing string to describe it */
    public string description { get; construct set; }

    /** Function to validate a given URI for this category */
    public ValidUriFunc validator;

    private static GenericArray<ServerCategory> categories = null;

    public ServerCategory(string scheme, string description, ValidUriFunc validator) {
        GLib.Object(scheme: scheme,
                    description: description);
        this.validator = validator;
    }

    public static void register(string? scheme, string? description, ValidUriFunc validate) {
        if (categories == null)
            categories = new GenericArray<ServerCategory>();

        // Don't register double (if this happens, it's probably a programmer error)
        if (find_category(scheme) != null) {
            warning("Scheme '%s' is already registered", scheme);
            return;
        }

        var category = new ServerCategory(scheme, description, validate);
        categories.add(category);
    }

    /**
     * Returns a null-terminated string array of supported schemes
     */
    [CCode (array_null_terminated = true, array_length = false)]
    public static string[] get_types() {
        string[] types = {};

        for (uint i = 0; i < categories.length; i++)
            types += categories[i].scheme;

        return types;
    }

    public static unowned ServerCategory? find_category(string? scheme) {
        for (uint i = 0; i < categories.length; i++) {
            if (scheme == categories[i].scheme)
                return categories[i];
        }

        return null;
    }

    /**
     * Checks to see if the passed uri is valid against registered validators
     */
    public static bool is_valid_uri(string uri) {
        string[] parts = uri.split(":", 2);
        if (parts.length <= 0)
            return false;

        for (uint i = 0; i < categories.length; i++) {
            if ((parts[0] == categories[i].scheme) && categories[i].validator(uri))
                return true;
        }

        return false;
    }
}
