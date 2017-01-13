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
public class Seahorse.Ssh.Source : GLib.Object, Gcr.Collection, Seahorse.Place {

    public const string AUTHORIZED_KEYS_FILE = "authorized_keys";
    public const string OTHER_KEYS_FILE = "other_keys.seahorse";

    // The home directory for SSH keys.
    private string ssh_homedir;
    // Source for refresh timeout
    private uint scheduled_refresh_source;
    // For monitoring the .ssh directory
    private FileMonitor monitor_handle;

    // Maps filenames (of the public parts) to keys
    private HashTable<string, Key> keys;

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

    public Icon icon {
        owned get { return new ThemedIcon(Gcr.ICON_HOME_DIRECTORY); }
    }

    public Gtk.ActionGroup? actions {
        owned get { return null; }
    }

    /**
     * The directory containing the SSH keys.
     */
    public string base_directory {
        get { return ssh_homedir; }
    }

    construct {
        this.keys = new HashTable<string, Key>(str_hash, str_equal);

        this.scheduled_refresh_source = 0;
        this.monitor_handle = null;

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

    public uint get_length() {
        return this.keys.size();
    }

    public List<weak GLib.Object> get_objects() {
        return this.keys.get_values();
    }

    public bool contains(GLib.Object object) {
        Key? key = (object as Key);
        if (key == null)
            return false;

        string filename = key.get_location();
        return this.keys.lookup(filename) == object;
    }

    public void remove_object(GLib.Object object) {
        Key? key = object as Key;
        if (key == null)
            return;

        string? filename = key.get_location();
        if (filename == null || this.keys.lookup(filename) != key)
            return;

        this.keys.remove(filename);
        removed(key);
    }

    public string file_for_public(bool authorized) {
        return Path.build_filename(this.ssh_homedir,
                                   authorized ? AUTHORIZED_KEYS_FILE : OTHER_KEYS_FILE);
    }

    public async Key? add_key_from_filename(string? privfile) throws GLib.Error {
        if (privfile == null)
            return null;

        // Check if it was already loaded once. If not, load it now
        Key? key = this.keys.lookup(privfile);
        if (key != null)
            return key;

        return yield load_key_for_private_file(privfile);
    }

    // Loads the (public) key for a private key.
    private async Key? load_key_for_private_file(string privfile) throws GLib.Error {
        string pubfile = privfile + ".pub";
        Source src = this;
        Key? key = null;

        // possibly an SSH key?
        if (FileUtils.test(privfile, FileTest.EXISTS)
                && FileUtils.test(pubfile, FileTest.EXISTS)
                && check_file_for_ssh(privfile)) {
            try {
                yield Key.parse_file(pubfile, (keydata) => {
                    key = Source.add_key_from_parsed_data(src, keydata, pubfile, false, false, privfile);
                    return true;
                });
            } catch (GLib.Error e) {
                throw new Error.GENERAL("Couldn't read SSH file: %s (%s)".printf(pubfile, e.message));
            }
        }

        return key;
    }

    public string? export_private(Key key) throws GLib.Error {
        KeyData? keydata = key.key_data;
        if (keydata == null)
            return null;

        if (keydata.privfile == null)
            throw new Error.GENERAL(_("No private key file is available for this key."));

        // And then the data itself
        string results;
        if (!FileUtils.get_contents(keydata.privfile, out results))
            return null;

        return results;
    }

    /**
     * Loads the keys from this Source's directory.
     *
     * @param cancellable Use this to cancel the operation.
     */
    public async bool load(GLib.Cancellable? cancellable) throws GLib.Error {
        Source src = this;
        // Since we can find duplicate keys, limit them with this hash
        GenericSet<string> loaded = new GenericSet<string>(str_hash, str_equal);
        // Keys that currently exist, so we can remove any that disappeared
        GenericSet<string> checks = load_present_keys();

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
            load_key_for_private_file.begin(privfile);
        }

        // Now load the authorized keys file
        string pubfile = file_for_public(true);
        Key.parse_file.begin(pubfile, (keydata) => {
            Source.add_key_from_parsed_data(src, keydata, pubfile, true, true, null, checks, loaded);
            return true;
        });

        // Load the other keys file
        pubfile = file_for_public(false);
        Key.parse_file.begin(pubfile, (keydata) => {
            Source.add_key_from_parsed_data(src, keydata, pubfile, true, false, null, checks, loaded);
            return true;
        });

        return true;
    }

    private GenericSet<string> load_present_keys() {
        GenericSet<string> checks = new GenericSet<string>(str_hash, str_equal);
        this.keys.foreach((filename, key) => checks.add(filename));
        return checks;
    }

    public static Key? add_key_from_parsed_data(Source src,
                                                KeyData? keydata,
                                                string pubfile,
                                                bool partial,
                                                bool authorized,
                                                string? privfile = null,
                                                GenericSet<string>? checks = null,
                                                GenericSet<string>? loaded = null) {
        if (keydata == null || !keydata.is_valid())
            return null;

        keydata.pubfile = pubfile;
        keydata.partial = partial;
        keydata.authorized = authorized;
        if (privfile != null)
            keydata.privfile = privfile;

        string? location = keydata.get_location();
        if (location == null)
            return null;

        // Does src key exist in the context?
        Key? prev = src.keys.lookup(location);

        // Mark src key as seen
        if (checks != null)
            checks.remove(location);

        // See if we've already gotten a key like src in this load batch
        if (loaded != null) {
            if (location in loaded) {
                if (prev != null)
                    prev.merge_keydata(keydata);
                return null;
            }

            // Mark src key as loaded
            loaded.add(location);
        }

        // If we already have src key then just transfer ownership of keydata
        if (prev != null) {
            prev.key_data = keydata;
            return prev;
        }

        // Create a new key
        Key key = new Key(src, keydata);
        src.keys.insert(location, key);
        src.added(key);

        return key;
    }


    /**
     * Parse an inputstream into a list of keys and import those keys.
     */
    public async List<Key>? import_async(InputStream input, Gtk.Window transient_for,
                                         Cancellable? cancellable = null) throws GLib.Error {
        uint8[] buffer = new uint8[4096];
        size_t bytes_read;
        input.read_all(buffer, out bytes_read, cancellable);

        string fullpath = file_for_public(false);
        Source src = this;

        Key.parse.begin((string) buffer,
                        (keydata) => {
                            import_public_async.begin(keydata, fullpath, cancellable);
                            return true;
                        },
                        (secdata) => {
                            new PrivateImportOperation().import_private_async.begin(src, secdata, null, cancellable);
                            return true;
                        });

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

        Thread<void*> thread = new Thread<void*>("authorize-async", () => {
            try {
                KeyData? keydata = key.key_data;
                if (keydata == null)
                    throw new Error.GENERAL("Can't authorize empty key.");

                if (!authorize) {
                    // Take it out of the file for authorized keys
                    string from = file_for_public(true);
                    KeyData.filter_file(from, null, keydata);
                }

                // Put it into the correct file
                string to = file_for_public(authorize);
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
}
