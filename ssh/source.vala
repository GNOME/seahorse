/*
 * Seahorse
 *
 * Copyright (C) 2004-2006 Stefan Walter
 * Copyright (C) 2011 Collabora Ltd.
 * Copyright (c) 2016 Niels De Graef
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
 * The {@link Place} where SSH keys are stored. By default that is ~/.ssh.
 * Basically, this becomes an interface to the SSH home directory.
 */
public class Seahorse.Ssh.Source : GLib.Object, GLib.ListModel, Seahorse.Place {

    // Paths to the authorized_keys file and one for unauthorized keys
    // The second file is Seahorse-specific, to allow users to retain public
    // keys without trusting them. (this also has the nice side effect that you
    // can undo certain operaions)
    public const string AUTHORIZED_KEYS_FILE = "authorized_keys";
    public const string OTHER_KEYS_FILE = "other_keys.seahorse";

    // The home directory for SSH keys.
    private string ssh_homedir;
    // Source for refresh timeout
    private uint scheduled_refresh_source = 0;
    // For monitoring the .ssh directory
    private FileMonitor? monitor_handle = null;

    // The list of Seahorse.Ssh.Keys
    private GenericArray<Ssh.Key> keys = new GenericArray<Ssh.Key>();

    public string label {
        owned get { return _("OpenSSH keys"); }
        set { }
    }

    public string description {
        owned get { return _("OpenSSH: %s").printf(this.ssh_homedir); }
    }

    public string uri {
        owned get { return _("openssh://%s").printf(this.ssh_homedir); }
    }

    public Place.Category category {
        get { return Place.Category.KEYS; }
    }

    public GLib.ActionGroup? actions {
        owned get { return null; }
    }

    public unowned string? action_prefix {
        get { return null; }
    }

    public MenuModel? menu_model {
        owned get { return null; }
    }

    /**
     * The directory containing the SSH keys.
     */
    public string base_directory {
        get { return ssh_homedir; }
    }

    construct {
        this.ssh_homedir = "%s/.ssh".printf(Environment.get_home_dir());

        // Make the .ssh directory if it doesn't exist
        if (!FileUtils.test(this.ssh_homedir, FileTest.EXISTS)) {
            if (DirUtils.create(this.ssh_homedir, 0700) != 0) {
                warning("couldn't create .ssh directory: %s", this.ssh_homedir);
                return;
            }
        }

        monitor_ssh_homedir();
    }

    private bool check_file_for_ssh(string filename) {
        if(!FileUtils.test(filename, FileTest.IS_REGULAR))
            return false;

        try {
            string contents;
            FileUtils.get_contents(filename, out contents);

            // Check for our signature
            return " PRIVATE KEY-----" in contents;
        } catch (FileError e) {
            warning("Error reading file '%s' to check for SSH key. %s".printf(filename, e.message));
        }

        return false;
    }

    private void cancel_scheduled_refresh () {
        if (this.scheduled_refresh_source != 0) {
            debug("Cancelling scheduled refresh event");
            GLib.Source.remove(this.scheduled_refresh_source);
            this.scheduled_refresh_source = 0;
        }
    }

    private bool scheduled_refresh() {
        debug("Scheduled refresh event ocurring now");
        cancel_scheduled_refresh();
        load.begin(null);
        return false; // don't run again
    }

    private bool scheduled_dummy() {
        debug("Dummy refresh event occurring now");
        this.scheduled_refresh_source = 0;
        return false; // don't run again
    }

    private void monitor_ssh_homedir() {
        File? dot_ssh_dir = File.new_for_path(this.ssh_homedir);
        if (dot_ssh_dir == null)
            return;

        try {
            this.monitor_handle = dot_ssh_dir.monitor_directory(FileMonitorFlags.NONE, null);
            this.monitor_handle.changed.connect((file, other_file, event_type) => {
                if (this.scheduled_refresh_source != 0 ||
                    (event_type != FileMonitorEvent.CHANGED &&
                     event_type != FileMonitorEvent.CHANGES_DONE_HINT &&
                     event_type != FileMonitorEvent.DELETED &&
                     event_type != FileMonitorEvent.CREATED))
                    return;

                string? path = file.get_path();
                if (path == null)
                    return;

                // Filter out any noise
                if (event_type != FileMonitorEvent.DELETED
                        && !path.has_suffix(AUTHORIZED_KEYS_FILE)
                        && !path.has_suffix(OTHER_KEYS_FILE)
                        && !path.has_suffix(".pub")
                        && !SecData.contains_private_key(path))
                    return;

                debug("Scheduling refresh event due to file changes");
                this.scheduled_refresh_source = Timeout.add(500, scheduled_refresh);
            });
        } catch (GLib.Error e) {
            warning("couldn't monitor ssh directory: %s: %s", this.ssh_homedir, e.message);
        }
    }

    public GLib.Type get_item_type() {
        return typeof(Ssh.Key);
    }

    public uint get_n_items() {
        return this.keys.length;
    }

    public GLib.Object? get_item(uint index) {
        if (index >= this.keys.length)
            return null;
        return this.keys[index];
    }

    public void remove_object(Ssh.Key key) {
        uint pos;
        if (this.keys.find(key, out pos)) {
            this.keys.remove_index(pos);
            items_changed(pos, 1, 0);
        }
    }

    public string authorized_keys_path() {
        return Path.build_filename(this.ssh_homedir, AUTHORIZED_KEYS_FILE);
    }

    public string other_keys_path() {
        return Path.build_filename(this.ssh_homedir, OTHER_KEYS_FILE);
    }

    public async Key? add_key_from_filename(string? privfile) throws GLib.Error {
        if (privfile == null)
            return null;

        // Check if it was already loaded once. If not, load it now
        for (uint i = 0; i < this.keys.length; i++) {
            unowned var key = this.keys[i];
            if (key.key_data.privfile == privfile) {
                return key;
            }
        }

        return yield load_key_for_private_file(privfile);
    }

    // Loads the (public) key for a private key.
    private async Key? load_key_for_private_file(string privfile) throws GLib.Error {
        string pubfile = privfile + ".pub";
        Key? key = null;

        // possibly an SSH key?
        if (FileUtils.test(privfile, FileTest.EXISTS)
                && FileUtils.test(pubfile, FileTest.EXISTS)
                && check_file_for_ssh(privfile)) {
            try {
                var result = yield Key.parse_file(pubfile);
                foreach (unowned var keydata in result.public_keys) {
                    key = add_key_from_parsed_data(keydata, pubfile, false, false, privfile);
                }
            } catch (GLib.Error e) {
                throw new Error.GENERAL("Couldn't read SSH file: %s (%s)".printf(pubfile, e.message));
            }
        }

        return key;
    }

    /**
     * Loads the keys from this Source's directory.
     *
     * @param cancellable Use this to cancel the operation.
     */
    public async bool load(GLib.Cancellable? cancellable) throws GLib.Error {
        // Schedule a dummy refresh. This blocks all monitoring for a while
        cancel_scheduled_refresh();
        this.scheduled_refresh_source = Timeout.add(500, scheduled_dummy);
        debug("scheduled a dummy refresh");

        // List the .ssh directory for private keys
        Dir dir = Dir.open(this.ssh_homedir);

        // Load each key file in ~/.ssh
        string? filename = null;
        while ((filename = dir.read_name()) != null) {
            string privfile = Path.build_filename(this.ssh_homedir, filename);
            load_key_for_private_file.begin(privfile, (obj, res) => {
                try {
                    load_key_for_private_file.end(res);
                } catch (GLib.Error err) {
                    warning("Couldn't load key for file: %s", err.message);
                }
            });
        }

        // Now load the authorized keys (if it exists)
        string pubfile = authorized_keys_path();
        if (FileUtils.test(pubfile, FileTest.IS_REGULAR)) {
            var result = yield Key.parse_file(pubfile);
            foreach (unowned var keydata in result.public_keys) {
                add_key_from_parsed_data(keydata, pubfile, true, true, null);
            }
        }

        // Load the "other keys" (public keys without authorization)
        pubfile = other_keys_path();
        if (FileUtils.test(pubfile, FileTest.IS_REGULAR)) {
            var result = yield Key.parse_file(pubfile);
            foreach (unowned var keydata in result.public_keys) {
                add_key_from_parsed_data(keydata, pubfile, true, false, null);
            }
        }

        return true;
    }

    public Key? add_key_from_parsed_data(KeyData? keydata,
                                         string pubfile,
                                         bool partial,
                                         bool authorized,
                                         string? privfile = null) {
        return_val_if_fail (keydata != null && keydata.is_valid(), null);

        keydata.pubfile = pubfile;
        keydata.partial = partial;
        keydata.authorized = authorized;
        if (privfile != null)
            keydata.privfile = privfile;

        // Does src key exist in the context?
        Key? prev = find_key_by_fingerprint(keydata.fingerprint);
        if (prev != null)
            prev.merge_keydata(keydata);

        // Don't create a new key if we already got one earlier
        if (prev != null)
            return prev;

        // Create a new key
        Key key = new Key(this, keydata);
        this.keys.add(key);
        items_changed(this.keys.length - 1, 0, 1);

        return key;
    }

    /**
     * Parse an inputstream into a list of keys and import those keys.
     */
    public async List<Key>? import_async(InputStream input,
                                         Gtk.Window? transient_for,
                                         Cancellable? cancellable = null) throws GLib.Error {
        var result = yield Key.parse(input, cancellable);

        string fullpath = other_keys_path();
        foreach (unowned var keydata in result.public_keys) {
            yield import_public_async(keydata, fullpath, cancellable);
        }
        foreach (unowned var secdata in result.secret_keys) {
            var op = new PrivateImportOperation();
            yield op.import_private_async(this, secdata, null, cancellable);
        }

        // TODO: The list of keys imported?
        return null;
    }

    /**
     * Import a public key into a file.
     *
     * @param data The public key that needs to be imported.
     * @param filename The name of the file in which the key should be imported.
     * @param cancellable Allows the operation to be cancelled.
     */
    public async string import_public_async(KeyData data,
                                            string filename,
                                            Cancellable? cancellable) throws GLib.Error {
        if (data == null || !data.is_valid() || data.rawdata == null)
            throw new Error.GENERAL("Trying to import an invalid public key.");

        KeyData.filter_file(filename, data);

        return data.fingerprint;
    }

    /**
     * Returns a possible filename for a key.
     *
     * @param algo The type of the key
     */
    public string? new_filename_for_algorithm(Algorithm algo) {
        string type = algo.to_string() ?? "unk";
        string basename = "id_%s".printf(type.down());

        string? filename = null;
        for (int i = 0; i < int.MAX; i++) {
            string t = (i == 0) ? basename : "%s.%d".printf(basename, i);
            filename = Path.build_filename(this.base_directory, t);

            if (!FileUtils.test(filename, FileTest.EXISTS))
                break;

            filename = null;
        }

        return filename;
    }

    /**
     * Adds/Removes a public key to/from the authorized keys.
     *
     * @param key The key that needs to be authorized.
     * @param authorize Whether the specified key needs to be (de-)authorized.
     */
    public async void authorize_async(Key key, bool authorize) throws GLib.Error {
        SourceFunc callback = authorize_async.callback;
        GLib.Error? err = null;

        // Authorized key → add to authorized_keys
        // otherwise      → add to other_keys.seahorse
        string from, to;
        if (authorize) {
            from = other_keys_path();
            to = authorized_keys_path();
        } else {
            from = authorized_keys_path();
            to = other_keys_path();
        }

        Thread<void*> thread = new Thread<void*>("authorize-async", () => {
            try {
                KeyData? keydata = key.key_data;
                if (keydata == null)
                    throw new Error.GENERAL("Can't authorize empty key.");

                // Filter the public key out of the file, if it exists
                if (FileUtils.test(from, FileTest.EXISTS)) {
                    KeyData.filter_file(from, null, keydata);
                }

                // Now put it into the correct one (make sure that it exists at this point)
                ensure_file_exists(to);
                KeyData.filter_file(to, keydata, null);

                // Just reload that one key
                key.refresh();
            } catch (GLib.Error e) {
                err = e;
            }

            Idle.add((owned)callback);
            return null;
        });

        yield;

        thread.join();

        if (err != null)
            throw err;
    }

    private void ensure_file_exists(string filename) {
        int fd = Posix.open(filename, Posix.O_WRONLY | Posix.O_CREAT, 0644);
        Posix.close(fd);
    }

    public Key? find_key_by_fingerprint(string fingerprint) {
        for (uint i = 0; i < this.keys.length; i++) {
            if (this.keys[i].fingerprint == fingerprint)
                return this.keys[i];
        }

        return null;
    }
}
