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

namespace Seahorse.Servers {

[CCode (cname = "SeahorseValidUriFunc", has_target = false)]
public delegate bool ValidUriFunc(string uri);

private struct ServerInfo {
    public string type;
    public string description;
    public ValidUriFunc validator;
}

private HashTable<string, ServerInfo?>? server_types = null;

[CCode (array_null_terminated = true, array_length = false)]
public string[] get_types() {
    string[] types = {};

    if (server_types != null) {
        server_types.foreach((uri, server_info) => {
            types += server_info.type;
        });
    }

    return types;
}

public string? get_description(string? type) {
    if (server_types == null)
        return null;

    ServerInfo? info = server_types.lookup(type);
    if (info != null)
        return info.description;

    return null;
}

public void register_type(string? type, string? description, ValidUriFunc validate) {
    ServerInfo info = ServerInfo() {
        description = description,
        type = type,
        validator = validate
    };

    if (server_types == null)
        server_types = new HashTable<string, ServerInfo?>(str_hash, str_equal);

    server_types.replace(info.type, info);
}

public void cleanup() {
    server_types = null;
}

[CCode (array_null_terminated = true, array_length = false)]
public string[] get_uris() {
    string[] servers = Application.pgp_settings().get_strv("keyservers");

    // The values are 'uri name', remove the name part
    string[] uris = {};
    foreach (string server in servers)
        uris += server.strip().split(" ", 2)[0];

    return uris;
}

[CCode (array_null_terminated = true, array_length = false)]
public string[] get_names() {
    string[] servers = Application.pgp_settings().get_strv("keyservers");

    // The values are 'uri name', remove the name part
    string[] names = {};
    foreach (string server in servers)
        names += server.strip().split(" ", 2)[1];

    return names;
}

/**
 * Checks to see if the passed uri is valid against registered validators
 */
public bool is_valid_uri(string? uri) {
    if (uri == null || server_types == null)
        return false;

    string[] parts = uri.split(":", 2);
    if (parts.length > 0) {
        weak ServerInfo? info = server_types.lookup(parts[0]);
        if (info != null && (info.validator(uri)))
            return true;
    }

    return false;
}

}
