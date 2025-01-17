/*
 * Seahorse
 *
 * Copyright (C) 2006 Stefan Walter
 * Copyright (C) 2011 Collabora Ltd.
 * Copyright (C) 2016 Niels De Graef
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

namespace Seahorse.Ssh {

/**
 * Wraps a command-line executable in its own async method.
 *
 * Since OpenSSH doesn't provide a library (or didn't at the time of writing),
 * we use a small wrapper around the ssh and ssh-keygen executables.
 * So, create a subclass instead and do whatever you want with the output.
 */
public abstract class Operation : GLib.Object {

    private string command_name;

    // These are all fields that will be used in case the user will be prompted.
    protected string? prompt_title;
    protected string? prompt_message;
    protected string? prompt_argument;

    /**
     * Calls a command and returns the output.
     *
     * @param command The command that should be launched.
     * @param input The standard input for the command, or null if none expected.
     * @param cancellable Can be used if you want to cancel. The process will be killed.
     * @return The output of the command.
     */
    protected async string? operation_async(string command,
                                            string? input,
                                            Cancellable? cancellable) throws GLib.Error
            requires(command != "") {

        // Strip the command name for logging purposes
        string[] args;
        try {
            Shell.parse_argv(command, out args);
            this.command_name = args[0];
        } catch (GLib.Error e) {
            critical("Couldn't parse SSH command line %s.", command);
            throw e;
        }

        debug("SSHOP: Executing SSH command: %s", command);

        var flags = SubprocessFlags.STDOUT_PIPE | SubprocessFlags.STDERR_PIPE;
        if (input != null)
            flags |= SubprocessFlags.STDIN_PIPE;
        var launcher = new SubprocessLauncher(flags);

        // Set up SSH_ASKPASS according to expected environment variables
        // Do a sanity check first. If this fails, it's probably a packaging
        var ssh_askpass_path = Path.build_filename(Config.EXECDIR, "ssh-askpass");
        if (!FileUtils.test(ssh_askpass_path, FileTest.IS_EXECUTABLE))
            critical("%s is not an executable file", ssh_askpass_path);

        launcher.setenv("SSH_ASKPASS", ssh_askpass_path, false);
        if (this.prompt_title != null)
            launcher.setenv("SEAHORSE_SSH_ASKPASS_TITLE", prompt_title, true);
        if (this.prompt_message != null)
            launcher.setenv("SEAHORSE_SSH_ASKPASS_MESSAGE", prompt_message, true);

        launcher.set_child_setup(() => {
            // No terminal for this process
            if (Posix.setsid() == -1)
                debug("setsid() unsuccessful: %s", strerror(errno));
        });

        // And off we go to run the program
        var subprocess = launcher.spawnv(args);
        string? std_out = null, std_err = null;
        try {
            yield subprocess.communicate_utf8_async(input, cancellable,
                                                    out std_out, out std_err);
            if (subprocess.get_exit_status() != 0) {
                warning("subprocess %s failed with following stderr:\n%s",
                        this.command_name, std_err);
                throw new Error.GENERAL("Internal error");
            }
            return std_out;
        } catch (GLib.Error e) {
            throw e;
        }
    }
}

public class UploadOperation : Operation {
    /**
     * Uploads a set of keys to a given host.
     *
     * @param keys The keys that should be uploaded.
     * @param username The username to use on the server.
     * @param hostname The URL of the host.
     * @param port The port of the host. If none is specified, the default port is used.
     * @param cancellable Used if you want to cancel the operation.
     */
    public async void upload_async(List<Key> keys,
                                   string username, string hostname, string? port,
                                   Cancellable? cancellable) throws GLib.Error
            requires(keys != null)
            requires(username != "")
            requires(hostname != "") {
        this.prompt_title = _("Remote Host Password");

        if (port == null)
            port = "";

        StringBuilder data = new StringBuilder.sized(1024);
        keys.foreach((key) => {
            KeyData keydata = key.key_data;
            if (keydata != null && keydata.pubfile != null) {
                data.append(keydata.rawdata);
                data.append_c('\n');
            }
        });

        /*
         * This command creates the .ssh directory if necessary (with appropriate permissions)
         * and then appends all input data onto the end of .ssh/authorized_keys
         */
        // TODO: Important, we should handle the host checking properly
        string cmd = "%s '%s@%s' %s %s -o StrictHostKeyChecking=no \"umask 077; test -d .ssh || mkdir .ssh ; cat >> .ssh/authorized_keys\""
                               .printf(Config.SSH_PATH, username, hostname, port != "" ? "-p" : "", port);

        yield operation_async(cmd, data.str, cancellable);
    }
}

public class ChangePassphraseOperation : Operation {

    /**
     * Changes the passphrase of the given key.
     *
     * @param key The key of which to change the passphrase.
     * @param cancellable Used if you want to cancel the operation.
     */
    public async void change_passphrase_async(Key key,
                                              Cancellable? cancellable) throws GLib.Error
            requires(key.key_data != null)
            requires(key.key_data.privfile != null) {

        this.prompt_title = _("Enter Key Passphrase");
        this.prompt_argument = key.title;

        var cmd = "%s -p -f '%s'".printf(Config.SSH_KEYGEN_PATH, key.key_data.privfile);

        debug("Changing passphrase of key %s", key.fingerprint);
        yield operation_async(cmd, null, cancellable);
        debug("Successfully changed passphrase of key %s", key.fingerprint);
    }
}

public class GenerateOperation : Operation {
    /**
     * Generates an SSH key pair.
     *
     * @param filename The filename of the new key.
     * @param email The e-mail address for which the key should be made.
     * @param type The type of key, i.e. which algorithm should be used;
     * @param bits The amount of bits that should be used.
     * @param cancellable Used if you want to cancel the operation.
     */
    public async void generate_async(string filename,
                                     string email,
                                     Algorithm type,
                                     uint bits,
                                     Cancellable cancellable) throws GLib.Error {
        if (type == Algorithm.UNKNOWN)
            throw new Error.GENERAL("Can't generate key for an unknown algorithm");

        this.prompt_title = _("Passphrase for New Secure Shell Key");

        string algo = type.to_string().down();
        string bits_str = (bits != 0)? "-b '%u'".printf(bits) : "";
        string comment = GLib.Shell.quote(email);

        string cmd = "%s %s -t '%s' -C %s -f '%s'".printf(Config.SSH_KEYGEN_PATH, bits_str, algo, comment, filename);

        yield operation_async(cmd, null, cancellable);
    }
}

public class PrivateImportOperation : Operation {
    /**
     * Imports a private key into a file.
     *
     * @param source The source that will decide the location of the private key.
     * @param data The public key that needs to be imported.
     * @param filename The name of the file in which the key should be imported.
     * @param cancellable Allows the operation to be cancelled.
     */
    public async string? import_private_async(Source source,
                                              SecData data,
                                              string? filename,
                                              Cancellable cancellable) throws GLib.Error {
        if (data == null || data.rawdata == null)
            throw new Error.GENERAL("Trying to import private key that is empty.");

        // No filename specified, make one up
        string file = filename ?? source.new_filename_for_algorithm(data.algo);

        // Add the comment to the output
        string message = (data.comment != null) ?
            _("Importing key: %s").printf(data.comment) : _("Importing key. Enter passphrase");

        // The prompt
        this.prompt_title = _("Import Key");
        this.prompt_message = message;

        // Write the private key into the file
        FileUtils.set_contents(file, data.rawdata);

        // Start command to generate public key
        string cmd = "%s -y -f '%s'".printf(Config.SSH_KEYGEN_PATH, file);
        string std_out = yield operation_async(cmd, null, cancellable);

        // We'll build the key string from the output
        var key_str = new StringBuilder(std_out);

        // Only use the first line of the output
        int pos = int.max(key_str.str.index_of_char('\n'), key_str.str.index_of_char('\r'));
        if (pos != -1)
            key_str.erase(pos);

        // Parse the data so we can get the fingerprint
        KeyData? keydata = KeyData.parse_line(std_out);

        // Add the comment to the output
        if (data.comment != null) {
            key_str.append_c(' ');
            key_str.append(data.comment);
        }

        // The file to write to
        string pubfile = "%s.pub".printf(file);
        FileUtils.set_contents(pubfile, key_str.str);

        if (keydata != null && keydata.is_valid())
            return keydata.fingerprint;

        throw new Error.GENERAL("Couldn't parse imported private key fingerprint");
    }
}

public class RenameOperation : Operation {
    /**
     * Renames/Deletes the comment in a key.
     *
     * @param key The key that should have its comment renamed/deleted.
     * @param new_comment The new comment of the key (or null if the previous
     *                    comment should be deleted)
     */
    public async void rename_async(Key key, string? new_comment, Gtk.Window transient_for) throws GLib.Error {
        KeyData keydata = key.key_data;

        if (new_comment == null)
            new_comment = "";

        if (!change_raw_comment (keydata, new_comment))
            return;

        debug("Renaming key to: %s", new_comment);

        assert (keydata.pubfile != null);
        if (keydata.partial) // Just part of a file for this key
            KeyData.filter_file (keydata.pubfile, keydata, keydata);
        else // A full file for this key
            FileUtils.set_contents(keydata.pubfile, keydata.rawdata);
    }

    private bool change_raw_comment(KeyData keydata, string new_comment) {
        assert(keydata.rawdata != null);

        string no_leading = keydata.rawdata.chug();
        string[] parts = no_leading.split_set(" ", 3);
        if (parts.length < 3)
            return false;

        keydata.rawdata = parts[0] + " " + parts[1] + " " + new_comment;

        return true;
    }
}

}
