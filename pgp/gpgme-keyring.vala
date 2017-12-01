/*
 * Seahorse
 *
 * Copyright (C) 2004-2006 Stefan Walter
 * Copyright (C) 2011 Collabora Ltd.
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

namespace Seahorse {
namespace GpgME {

public class Keyring : GLib.Object, Gcr.Collection, Seahorse.Place {

    // Amount of keys to load in a batch
    public const string DEFAULT_LOAD_BATCH = 50;

    private HashTable<string, Key> keys;
    // Source for refresh timeout
    private uint scheduled_refresh;
    // For monitoring the .gnupg directory
    private FileMonitor monitor_handle;
    // Orphan secret key
    private List orphan_secret;

    public string label {
        owned get { return _("GnuPG keys"); }
    }

    public string description {
        owned get { return _("GnuPG: default keyring directory"); }
    }

    public string uri {
        owned get { return _("gnupg://"); }
    }

    public Icon icon {
        owned get { return new ThemedIcon(Gcr.ICON_GNUPG); }
    }

    /// XXX Properties
    public Gtk.ActionGroup? actions {
        owned get { return null; }
    }

    static construct {
        debug("init gpgme version %s", gpgme_check_version (null));

        if (Config.ENABLE_NLS) {
            gpgme_set_locale (null, LC_CTYPE, setlocale (LC_CTYPE, null));
            gpgme_set_locale (null, LC_MESSAGES, setlocale (LC_MESSAGES, null));
        }
    }

    [Flags]
    private enum LoadPart {
        LOAD_FULL = 0x01,
        LOAD_PHOTOS = 0x02
    }

    /**
     * Creates a new PGP key source
     **/
    public Keyring() {
        this.keys = new HashTable.full(seahorse_pgp_keyid_hash, seahorse_pgp_keyid_equal);
        this.scheduled_refresh = 0;
        this.monitor_handle = null;

        string? gpg_homedir = seahorse_gpg_homedir ();
        if (gpg_homedir != null) {
            File? file = File.new_for_path(gpg_homedir);
            return_if_fail (file != null);

            GError *err = null;
            this.monitor_handle = file.monitor_directory(G_FILE_MONITOR_NONE, null, &err);
            if (this.monitor_handle) {
                this.monitor_handle.changed.connect(monitor_gpg_homedir);
            } else {
                warning("Couldn't monitor the GPG home directory: %s: %s", gpg_homedir, err && err.message ? err.message : "");
            }
        }
    }

    public GPG.Context new_context (GPGError.ErrorCode *gerr) {
        gpgme_protocol_t proto = GPGME_PROTOCOL_OpenPGP;
        GPGError.ErrorCode error = 0;
        GPG.Context ctx = null;

        error = gpgme_engine_check_version (proto);
        if (error == 0)
            error = GPG.Context.context(out ctx);
        if (error == 0)
            error = gpgme_set_protocol (ctx, proto);

        if (error != 0) {
            message("couldn't initialize gnupg properly: %s",
                       gpgme_strerror (error));
            if (gerr)
                *gerr = error;
            return null;
        }

        ctx.set_passphrase_cb(passphrase_get, null);
        ctx.set_keylist_mode(GPG.KeyListMode.LOCAL);
        if (gerr)
            *gerr = 0;
        return ctx;
    }

    static GPGError.ErrorCode passphrase_get(void* dummy, string? passphrase_hint, string? passphrase_info, int flags, int fd) {
        GPGError.ErrorCode err;
        string pass;

        bool confirm = (passphrase_info != null && passphrase_info.length < 16);
        if (confirm)
            flags |= SEAHORSE_PASS_NEW;

        string[]? split_uid = (passphrase_hint != null)? passphrase_hint.split(" ", 2) : null;
        string? errmsg = (SEAHORSE_PASS_BAD in flags)? _("Wrong passphrase.") : null;

        string? label = null;
        if (split_uid != null && split_uid[0] && split_uid[1]) {
            if (SEAHORSE_PASS_NEW in flags)
                label = _("Enter new passphrase for “%s”").printf(split_uid[1]);
            else
                label = _("Enter passphrase for “%s”").printf(split_uid[1]);
        } else {
            label = (SEAHORSE_PASS_NEW in flags)? _("Enter new passphrase")
                                                : _("Enter passphrase");
        }

        Gtk.Dialog dialog = seahorse_passphrase_prompt_show (_("Passphrase"), errmsg ? errmsg : label,
                                                  null, null, confirm);

        switch (dialog.run()) {
            case Gtk.ResponseType.ACCEPT:
                pass = seahorse_passphrase_prompt_get (dialog);
                seahorse_util_printf_fd (fd, "%s\n", pass);
                err = GPG_OK;
                break;
            default:
                err = GPG_E (GPG_ERR_CANCELED);
                break;
        };

        dialog.destroy();
        return err;
    }

    public Key? lookup(string? keyid) {
        if (keyid == null)
            return null;

        return this.keys.lookup(keyid);
    }

    public void remove_key(Key key) {
        if (this.keys.lookup(key.keyid) != key)
            return;

        this.keys.remove(key.keyid);
        removed(key);
    }

    private struct keyring_list_closure {
        Keyring keyring;
        Cancellable cancellable;
        ulong cancelled_sig;
        GPG.Context gctx;
        HashTable checks;
        int parts;
        int loaded;
    }

    public async void list_async(string[] patterns, int parts, bool secret, Cancellable cancellable) {
        closure = g_new0 (keyring_list_closure, 1);
        int parts = parts;
        GPG.Context? gctx = new_context(&gerr);

        /* Start the key listing */
        GPG.ErrorCode gerr = 0;
        if (gctx != null) {
            if (parts & LOAD_FULL)
                gctx.set_keylist_mode(GPG.KeyListMode.SIGS | gctx.get_keylist_mode());

            if (patterns)
                gerr = Operation.keylist_ext_start(closure.gctx, patterns, secret, 0);
            else
                gerr = Operation.keylist_start(closure.gctx, null, secret);
        }

        if (gerr != 0) {
            seahorse_gpgme_propagate_error (gerr, &error);
            g_simple_async_result_take_error (res, error);
            g_simple_async_result_complete_in_idle (res);
            return;
        }

        // Loading all the keys?
        if (patterns == null) {
            HashTable<string, string> checks = new HashTable(seahorse_pgp_keyid_hash, seahorse_pgp_keyid_equal);
            this.keys.foreach((keyid, key) => {
                if ((secret && key.usage == Seahorse.Usage.PRIVATE_KEY)
                    || (!secret && key.usage == Seahorse.Usage.PUBLIC_KEY)) {
                    checks.insert(keyid, keyid);
                }
            });
        }

        Seahorse.Progress.prep_and_begin(cancellable, res, null);
        if (cancellable)
            closure.cancelled_sig = cancellable.connect(on_keyring_list_cancelled);

        Idle.add(on_idle_list_batch_of_keys, Priority.LOW);
    }

    /* Add a key to the context  */
    private Key add_key_to_context(GPG.Key key) {
        if (key.subkeys == null && key.subkeys.keyid == null)
            return null;

        string keyid = key.subkeys.keyid;
        if (keyid == null)
            return null;

        // Check if we can just replace the key on the object
        Key? prev = lookup(keyid);
        if (prev != null) {
            if (key.secret)
                prev.seckey = key;
            else
                prev.pubkey = key;
            return prev;
        }

        // Create a new key with secret
        Key? pkey = null;
        if (key.secret) {
            pkey = new Key(null, key);

            // Since we don't have a public key yet, save this away
            this.orphan_secret = this.orphan_secret.append(pkey);

            // No key was loaded as far as everyone is concerned
            return null;
        }

        // Just a new public key

        // Check for orphans
        foreach (Key orphan in this.orphan_secret) {
            GPG.Key? seckey = orphan.get_private();
            if (seckey == null || seckey.subkeys == null || seckey.subkeys.keyid == null)
                return;

            // Look for a matching key
            if (keyid == seckey.subkeys.keyid) {

                // Set it up properly
                pkey = orphan;
                pkey.pubkey = key;

                // Remove item from orphan list cleanly
                this.orphan_secret = this.orphan_secret.remove_link(orphan);
                break;
            }
        }

        pkey = pkey ?? new Key(this, key, null);

        // Add to context
        this.keys.insert(keyid, pkey);
        added(pkey);

        return pkey;
    }


    /**
     * Remove the given key from the context
     */
    private void _remove_key(string keyid) {
        Key? key = this.keys.lookup(keyid);
        if (key != null)
            remove_key(key);
    }

    /**
     * Completes one batch of key loading
     */
    static bool on_idle_list_batch_of_keys(gpointer data) {
        keyring_list_closure *closure = g_simple_async_result_get_op_res_gpointer (res);

        /* We load until done if batch is zero */
        uint batch = DEFAULT_LOAD_BATCH;

        while (batch-- > 0) {

            if (!gctx.keylist_next(&key.is_success())) {
                gctx.keylist_end();

                /* If we were a refresh loader, then we remove the keys we didn't find */
                if (closure.checks) {
                    closure.checks.foreach((keyid, derp) => {
                        _remove_key(closure.keyring, keyid);
                    });
                }

                Seahorse.Progress.end(closure.cancellable, res);
                g_simple_async_result_complete (res);
                return false; /* Remove event handler */
            }

            g_return_val_if_fail (key.subkeys && key.subkeys.keyid, false);

            // During a refresh if only new or removed keys
            if (closure.checks != null) {
                // Make note that this key exists in key ring
                closure.checks.remove(key.subkeys.keyid);

            }

            Key pkey = add_key_to_context (closure.keyring, key);

            /* Load additional info */
            if (pkey && closure.parts & LOAD_PHOTOS)
                KeyOperation.photos_load(pkey);

            gpgme_key_unref (key);
            closure.loaded++;
        }

        string detail = ngettext("Loaded %d key", "Loaded %d keys", closure.loaded).printf(closure.loaded);
        Seahorse.Progress.update(closure.cancellable, res, detail);

        return true;
    }

    static void on_keyring_list_cancelled (Cancellable cancellable) {
        keyring_list_closure *closure = g_simple_async_result_get_op_res_gpointer (res);

        cgtx.keylist_end();
    }

    static bool list_finish(Keyring keyring, AsyncResult result) throws GLib.Error {
        g_return_val_if_fail (g_simple_async_result_is_valid (result, G_OBJECT (keyring),
                              seahorse_gpgme_keyring_list_async), false);

        if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (result), error))
            return false;

        return true;
    }

    private void cancel_scheduled_refresh () {
        if (this.scheduled_refresh != 0) {
            g_debug ("cancelling scheduled refresh event");
            g_source_remove (this.scheduled_refresh);
            this.scheduled_refresh = 0;
        }
    }

    public async void load_async(Cancellable? cancellable) throws GLib.Error {
        yield load_full_async(null, 0, cancellable);
    }

    private async void load_full_async(string[]? patterns, int parts, Cancellable *cancellable) {
        // Schedule a dummy refresh. This blocks all monitoring for a while */
        cancel_scheduled_refresh();
        this.scheduled_refresh = Timeout.add (500, () => {
            debug("dummy refresh event occurring now");
            this.scheduled_refresh = 0;
            return false; // don't run again
        });
        debug("scheduled a dummy refresh");

        debug("refreshing keys...");

        // Secret keys
        yield list_async(patterns, 0, true, cancellable);
        // Public keys
        yield list_async(patterns, 0, false, cancellable);
    }

    private struct keyring_import_closure {
        Cancellable cancellable;
        Keyring keyring;
        GPG.Context gctx;
        gpgme_data_t data;
        gchar **patterns;
        List<Key> keys;
    }

    public async void import_async(InputStream input, Cancellable cancellable) {
        keyring_import_closure *closure;
        GPGError.ErrorCode gerr = 0;
        GError *error = null;
        Source? gsource = null;

        closure.gctx = new_context (&gerr);
        closure.data = seahorse_gpgme_data_input (input);
        g_simple_async_result_set_op_res_gpointer (res, closure, keyring_import_free);

        if (gerr == 0) {
            Seahorse.Progress.prep_and_begin(cancellable, res, null);
            gsource = seahorse_gpgme_gsource_new (closure.gctx, cancellable);
            gsource.set_callback(on_keyring_import_complete);
            gerr = Operation.import_start(closure.gctx, closure.data);
        }

        if (seahorse_gpgme_propagate_error (gerr, &error)) {
            g_simple_async_result_take_error (res, error);
            g_simple_async_result_complete_in_idle (res);

        } else {
            gsource.attach (MainContext.default ());
        }
    }

    private void on_keyring_import_loaded (GLib.Object source, GAsyncResult *result) {
        keyring_import_closure *closure = g_simple_async_result_get_op_res_gpointer (res);

        for (uint i = 0; closure.patterns[i] != null; i++) {
            Key? key = this.keys.lookup(closure.patterns[i]);
            if (key == null) {
                warning("imported key but then couldn't find it in keyring: %s", closure.patterns[i]);
                continue;
            }

            closure.keys.prepend(key);
        }

        Seahorse.Progress.end(closure.cancellable, res);
        g_simple_async_result_complete (res);
    }

    static bool on_keyring_import_complete (GPGError.ErrorCode gerr) {
        keyring_import_closure *closure = g_simple_async_result_get_op_res_gpointer (res);
        gpgme_import_result_t results;
        gpgme_import_status_t import;
        GError *error = null;

        if (seahorse_gpgme_propagate_error (gerr, &error)) {
            g_simple_async_result_take_error (res, error);
            g_simple_async_result_complete (res);
            return false; /* don't call again */
        }

        /* Figure out which keys were imported */
        results = gpgme_op_import_result (closure.gctx);
        if (results == null) {
            g_simple_async_result_complete (res);
            return false; /* don't call again */
        }

        /* Dig out all the fingerprints for use as load patterns */
        closure.patterns = new string[results.considered + 1];
        for (int i = 0, import = results.imports; i < results.considered && import; import = import.next) {
            if (import.result.is_success())
                closure.patterns[i++] = g_strdup (import.fpr);
        }

        /* See if we've managed to import any ... */
        if (closure.patterns[0] == null) {

            /* ... try and find out why */
            if (results.considered > 0 && results.no_user_id) {
                string msg = _("Invalid key data (missing UIDs). This may be due to a computer with a date set in the future or a missing self-signature.");
                g_simple_async_result_set_error (res, SEAHORSE_ERROR, -1, "%s", msg);
            }

            g_simple_async_result_complete (res);
            return false; /* don't call again */
        }

        /* Reload public keys */
        seahorse_gpgme_keyring_load_full_async (closure.keyring, (string *)closure.patterns,
                                                LOAD_FULL, closure.cancellable,
                                                on_keyring_import_loaded, g_object_ref (res));

        return false; /* don't call again */
    }

    GList * seahorse_gpgme_keyring_import_finish (Keyring *self, GAsyncResult *result, GError **error) {
        keyring_import_closure *closure;
        GList *results;

        g_return_val_if_fail (g_simple_async_result_is_valid (result, G_OBJECT (self),
                              seahorse_gpgme_keyring_import_async), null);

        if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (result), error))
            return null;

        closure = g_simple_async_result_get_op_res_gpointer (G_SIMPLE_ASYNC_RESULT (result));
        results = closure.keys;
        closure.keys = null;
        return results;
    }

    private void monitor_gpg_homedir(FileMonitor handle, File file, File other_file, FileMonitorEvent event_type) {
        if (event_type == G_FILE_MONITOR_EVENT_CHANGED ||
            event_type == G_FILE_MONITOR_EVENT_DELETED ||
            event_type == G_FILE_MONITOR_EVENT_CREATED) {

            string name = file.get_basename();
            if (name.has_suffix(".gpg")) {
                if (this.scheduled_refresh == 0) {
                    debug("scheduling refresh event due to file changes");
                    this.scheduled_refresh = Timeout.add (500, () => {
                        debug("scheduled refresh event ocurring now");
                        cancel_scheduled_refresh();
                        load_async(null, null, null);
                        return false; /* don't run again */
                    });
                }
            }
        }
    }

    static void
    seahorse_gpgme_keyring_get_property (GLib.Object obj,
                                         guint prop_id,
                                         GValue *value,
                                         GParamSpec *pspec)
    {
        SeahorsePlace *place = SEAHORSE_PLACE (obj);

        switch (prop_id) {
        case PROP_LABEL:
            g_value_take_string (value, seahorse_gpgme_keyring_get_label (place));
            break;
        case PROP_DESCRIPTION:
            g_value_take_string (value, seahorse_gpgme_keyring_get_description (place));
            break;
        case PROP_ICON:
            g_value_take_object (value, seahorse_gpgme_keyring_get_icon (place));
            break;
        case PROP_URI:
            g_value_take_string (value, seahorse_gpgme_keyring_get_uri (place));
            break;
        case PROP_ACTIONS:
            g_value_take_object (value, seahorse_gpgme_keyring_get_actions (place));
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
            break;
        }
    }

    static void
    seahorse_gpgme_keyring_dispose (GLib.Object object)
    {
        Keyring *self = SEAHORSE_GPGME_KEYRING (object);
        GList *l;

        if (this.actions)
            this.actions.set_sensitive(true);
        this.keys.remove_all();

        cancel_scheduled_refresh();
        if (this.monitor_handle) {
            g_object_unref (this.monitor_handle);
            this.monitor_handle = null;
        }

        for (l = this.orphan_secret; l != null; l = g_list_next (l))
            g_object_unref (l.data);
        g_list_free (this.orphan_secret);
        this.orphan_secret = null;

        base.dispose (object);
    }

    public uint get_length() {
        return this.keys.length;
    }

    public List get_objects() {
        return this.keys.get_values();
    }

    public bool contains(GLib.Object object) {
        GpgME.Key? key = object as GpgME.Key;
        if (key == null)
            return false;

        return this.keys.lookup(key.keyid) == key;
    }
}

}
}
