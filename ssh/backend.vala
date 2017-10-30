/*
 * Seahorse
 *
 * Copyright (C) 2008 Stefan Walter
 * Copyright (C) 2016 Niels De Graef
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

public class Seahorse.Ssh.Backend : GLib.Object, Gcr.Collection, Seahorse.Backend {

    private Source dot_ssh;

    public string name { get { return SEAHORSE_SSH_NAME; } }
    public string label { get { return _("Secure Shell"); } }
    public string description { get { return _("Keys used to connect securely to other computers"); } }
    public Gtk.ActionGroup? actions { owned get { return null; } }

    private bool _loaded;
    public bool loaded { get { return _loaded; } }

    construct {
        this.dot_ssh = new Source();
        this.dot_ssh.load.begin(null, (obj, res) => {
            try {
                this._loaded = dot_ssh.load.end(res);
            } catch (GLib.Error e) {
                warning("Failed to initialize SSH backend: %s", e.message);
            }
        });
    }

    public Backend() {
        register();
    }

    public static Backend? instance { get; internal set; default = null; }

    public uint get_length() {
        return 1;
    }

    public List<weak GLib.Object> get_objects() {
        List<GLib.Object> list = new List<GLib.Object>();
        list.append(this.dot_ssh);
        return list;
    }

    public bool contains(GLib.Object object) {
        Source? src = object as Source;

        return (src != null) && (this.dot_ssh == src);
    }

    public Seahorse.Place? lookup_place(string uri) {
        if (this.dot_ssh != null && this.dot_ssh.uri != null && this.dot_ssh.uri == uri)
            return this.dot_ssh;

        return null;
    }

    public static void initialize() {
        instance = new Backend();
        Generate.register();
    }

    public Source get_dot_ssh() {
        return this.dot_ssh;
    }
}
