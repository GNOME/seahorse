/*
 * Seahorse
 *
 * Copyright (C) 2008 Stefan Walter
 * Copyright (C) 2017 Niels De Graef
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

public class Seahorse.Pgp.Backend : GLib.Object, Gcr.Collection, Seahorse.Backend {
    private Gpgme.Keyring keyring;
    private Discovery discovery;
    private UnknownSource unknown;
    private HashTable<string, ServerSource> remotes;

    public string name { get { return SEAHORSE_PGP_NAME; } }
    public string label { get { return _("PGP Keys"); } }
    public string description { get { return _("PGP keys are for encrypting email or files"); } }
    public Gtk.ActionGroup? actions { owned get { return Actions.instance; } }

    private bool _loaded;
    public bool loaded { get { return _loaded; } }

    public static Backend pgp_backend = null;

    construct {
        this.remotes = new HashTable<string, ServerSource>();
    }

    public Backend() {
        Gpgme.Generate.register();

        this.keyring = new Seahorse.GpgME.Keyring();
        this.keyring.load.begin (null, (obj, res) => {
            try {
                this.keyring.load.end(res);
            } catch (Error e) {
                warning ("Failed to initialize PGP backend: %s", error.message);
            }
            this.loaded = true;
            this.notify_property("loaded");
        });

        this.discovery = new Seahorse.Discovery();
        this.unknown = new Seahorse.UnknownSource();

#if WITH_KEYSERVER
        Application.pgp_settings().changed["keyservers"].connect(on_settings_keyservers_changed);

        // Initial loading
        on_settings_keyservers_changed(Application.pgp_settings(), "keyservers", self);
#endif
    }

#if WITH_KEYSERVER
    private void on_settings_keyservers_changed(GLib.Settings settings, string key) {
        GHashTableIter iter;

        GenericSet<string> check = new GenericSet<string>();
        this.remotes.foreach((uri, source) => {
            check.insert(uri);
        });

        // Load and strip names from keyserver list
        foreach (string uri in Seahorse.Servers.get_uris()) {
            // If we don't have a keysource then add it
            if (this.remotes.lookup(uri) == null) {
                ServerSource source = new ServerSource(uri);
                if (source != null)
                    add_remote(uri, source);
            }

            // Mark this one as present
            check.remove(uri);
        }

        // Now remove any extras
        check.foreach((uri) => {
            remove_remote(uri);
        });
    }
#endif

    public uint get_length() {
        return 1;
    }

    public List<GLib.Object> get_objects() {
        List<Keyring> list = new List<GLib.Object>();
        list.append(this.keyring);
        return list;
    }

    public bool contains(GLib.Object object) {
        Keyring? kr = object as Keyring;
        return (kr != null) && (kr == this.keyring);
    }

    public Seahorse.Place? lookup_place(string uri) {
        if (uri == "gnupg://")
            return Backend.get_default_keyring();

        return null;
    }

    public static Backend get() {
        return pgp_backend;
    }

    public void initialize() {
        register();
        gpgme_set_engine_info (GPGME_PROTOCOL_OpenPGP, null, null);
    }

    public GpgME.Keyring get_default_keyring() {
        return this.keyring;
    }

    public Pgp.Key? get_default_key() {
        Pgp.Key? key = null;

        GLib.Settings? settings = Application.pgp_settings();
        if (settings != null) {
            string val = settings.get_string("default-key");
            if (val != null && val[0]) {
                string keyid;
                if (val.has_prefix("openpgp:"))
                    keyid = val + strlen ("openpgp:");
                else
                    keyid = val;
                key = this.keyring.lookup(keyid);
            }
        }

        return key;
    }

    public Seahorse.Discovery? get_discovery() {
        return this.discovery;
    }

    public Seahorse.ServerSource lookup_remote(string uri) {
        return this.remotes.lookup(uri);
    }

    public void add_remote(string uri, Seahorse.ServerSource source) {
        if (uri == null || this.remotes.lookup(uri) != null || source == null)
            return;

        this.remotes.insert(uri, source);
    }

    public void remove_remote(string uri) {
       if (uri == null)
           return;

        this.remotes.remove(uri);
    }

    struct search_remote_closure {
        Cancellable cancellable;
        gint num_searches;
        GList *objects;
    }

    public async void search_remote_async(string search,
                                          Gcr.SimpleCollection results,
                                          Cancellable cancellable) {
        search_remote_closure *closure;
        GSimpleAsyncResult *res;
        SeahorseServerSource *source;

        // Get a list of all selected key servers
        GenericSet<string> servers = new GenericSet<string>();
        string[] names = Seahorse.Application.settings(null).get_strv("last-search-servers");
        foreach (string name in names)
            servers.add(name);

        this.remotes.foreach((uri, source) => {
            if (servers.lookup(source.uri) == null)
                return;

            Seahorse.Progress.prep_and_begin(closure.cancellable, GINT_TO_POINTER (closure.num_searches), null);
            source.search_async(search, results, closure.cancellable, on_source_search_ready, g_object_ref (res));
            closure.num_searches++;
        });

        if (closure.num_searches == 0)
            g_simple_async_result_complete_in_idle (res);
    }

    private void on_source_search_ready(GObject *source, GAsyncResult *result) {
        GSimpleAsyncResult *res = G_SIMPLE_ASYNC_RESULT (user_data);
        search_remote_closure *closure = g_simple_async_result_get_op_res_gpointer (res);
        GError *error = null;

        g_return_if_fail (closure.num_searches > 0);

        if (!seahorse_server_source_search_finish (SEAHORSE_SERVER_SOURCE (source),
                                                   result, &error))
            g_simple_async_result_take_error (res, error);

        closure.num_searches--;
        Seahorse.Progress.end(closure.cancellable, GINT_TO_POINTER (closure.num_searches));

        if (closure.num_searches == 0)
            g_simple_async_result_complete (res);
    }

    bool seahorse_pgp_backend_search_remote_finish(AsyncResult *result, GError **error) {
        g_return_val_if_fail (g_simple_async_result_is_valid (result, G_OBJECT (self),
                              seahorse_pgp_backend_search_remote_async), false);

        res = G_SIMPLE_ASYNC_RESULT (result);

        if (g_simple_async_result_propagate_error (res, error))
            return false;

        return true;
    }

    struct transfer_closure {
        Cancellable cancellable;
        int num_transfers;
    }

    public async void transfer_async(List keys, SeahorsePlace to, Cancellable cancellable) throws GLib.Error {
        SeahorseObject *object;
        GSimpleAsyncResult *res;
        SeahorsePlace *from;
        GList *next;

        if (cancellable)
            closure.cancellable = g_object_ref (cancellable);

        keys = g_list_copy (keys);

        // Sort by key place
        keys = Util.objects_sort_by_place(keys);

        foreach (Key key in keys) {
            // break off one set (same keysource)
            next = Util.objects_splice_by_place(keys);

            g_assert (SEAHORSE_IS_OBJECT (keys.data));
            object = SEAHORSE_OBJECT (keys.data);

            // Export from this key place
            from = seahorse_object_get_place (object);
            g_return_if_fail (SEAHORSE_IS_PLACE (from));

            if (from != to) {
                // Start a new transfer operation between the two places
                Seahorse.Progress.prep_and_begin(cancellable, GINT_TO_POINTER (closure.num_transfers), null);
                Transfer.transfer_keys_async.begin(from, to, keys, cancellable, (obj, res) => {
                    if (num_transfer <= 0)
                        return;
                    GError *error = null;

                    try {
                        Transfer.transfer_keys_async.end(res);
                    } catch (Error e) {
                        g_simple_async_result_take_error (res, error);
                    }

                    closure.num_transfers--;
                    Seahorse.Progress.end(closure.cancellable, GINT_TO_POINTER (closure.num_transfers));
                });
                closure.num_transfers++;
            }

            keys = next;
        }
    }

    public List<Key> discover_keys(string[] keyids, Cancellable cancellable) {
        // Our return value
        List<Key> robjects = new List<Key>();

        // Check all the ids
        string[] todiscover = new string[keyids.length];
        foreach (string keyid in keyids) {
            // Do we know about this object?
            Key? key = this.keyring.lookup(keyid);

            // No such object anywhere, discover it
            if (key == null) {
                todiscover.add(keyid);
                continue;
            }

            robjects.prepend(key);
        }

        if (todiscover.length > 0) {
            // Start a discover process on all todiscover
            if (Config.WITH_KEYSERVER && Seahorse.Application.settings().get_boolean("server-auto-retrieve"))
                retrieve_async(todiscover, this.keyring, cancellable, null, null);

            // Add unknown objects for all these
            // XXX watch out, you're using keyid twice
            foreach (string keyid in todiscover) {
                SeahorseObject object = this.unknown.add_object(keyid[i], cancellable);
                robjects = robjects.prepend(object);
            }
        }

        return robjects;
    }

    public async void retrieve_async(string[] keyids, Seahorse.Place to, Cancellable cancellable) throws GLib.Error {
        int num_transfers = 0;
        this.remotes.foreach((uri, place) => {
            // Start a new transfer operation between the two places
            Seahorse.Progress.prep_and_begin(cancellable, num_transfers, null);
            Seahorse.Transfer.transfer_keyids_async.begin(place, to, keyids, cancellable, (obj, res) => {
                if (num_transfer <= 0)
                    return;
                GError *error = null;

                try {
                    Transfer.transfer_keys_async.end(res);
                } catch (Error e) {
                    g_simple_async_result_take_error (res, error);
                }

                closure.num_transfers--;
                Seahorse.Progress.end(closure.cancellable, GINT_TO_POINTER (closure.num_transfers));
            });
            num_transfers++;
        });
    }
}
