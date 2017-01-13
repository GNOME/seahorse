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

namespace Seahorse {
namespace Ssh {

/**
 * Wraps a command-line executable in its own async method.
 *
 * The basic idea is that we don't want to link to the OpenSSL/LibreSSL libraries.
 * So, create a subclass instead and do whatever you want with the output.
 */
public abstract class Operation : GLib.Object {

    private string command_name;

    private StringBuilder _std_out = new StringBuilder();
    protected StringBuilder std_out { get { return _std_out; } }

    private StringBuilder _std_err = new StringBuilder();
    protected StringBuilder std_err { get { return _std_err; } }

    private Pid pid;

    // These are all fields that will be used in case the user will be prompted.
    protected string? prompt_title;
    protected string? prompt_message;
    protected string? prompt_argument;
    protected ulong prompt_transient_for;

    /**
     * Calls a command and returns the output.
     *
     * @param command The command that should be launched.
     * @param input The standard input for the command, or null if none expected.
     * @param cancellable Can be used if you want to cancel. The process will be killed.
     * @return The output of the command.
     */
    protected async void operation_async(string command,
                                         string? input,
                                         Cancellable? cancellable) throws GLib.Error {
        if (command == null || command == "")
            return;

        string[] args;
        try {
            Shell.parse_argv(command, out args);
            this.command_name = args[0];
        } catch (GLib.Error e) {
            critical("Couldn't parse SSH command line %s.", command);
            throw e;
        }

        debug("SSHOP: Executing SSH command: %s", command);

        // And off we go to run the program
        int? fin, fout, ferr;
        if (input != null)
            fin = null;
        Process.spawn_async_with_pipes(null, args, null,
                                       SpawnFlags.DO_NOT_REAP_CHILD | SpawnFlags.LEAVE_DESCRIPTORS_OPEN,
                                       on_spawn_setup_child, out this.pid, out fin, out fout, out ferr);

        ulong cancelled_sig = 0;
        if (cancellable != null)
            cancelled_sig = cancellable.connect(() =>  { Posix.kill(this.pid, Posix.SIGTERM); });

        // Copy the input for later writing
        if (input != null) {
            StringBuilder std_in = new StringBuilder(input);
            debug("Sending input to '%s': '%s'", this.command_name, std_in.str);
            create_io_channel(std_in, fin, IOCondition.OUT,
                              (io, cond) => on_io_ssh_write(io, cond, std_in));
        }

        // Make all the proper IO Channels for the output/error
        create_io_channel(this.std_out, fout, IOCondition.IN,
                          (io, cond) => on_io_ssh_read(io, cond, this.std_out));
        create_io_channel(this.std_err, ferr, IOCondition.IN,
                          (io, cond) => on_io_ssh_read(io, cond, this.std_err));

        // Process watch
        watch_ssh_process(cancellable, cancelled_sig);
    }

    private void create_io_channel(StringBuilder data,
                                   int handle,
                                   IOCondition io_cond,
                                   IOFunc io_func) throws GLib.Error {
        Posix.fcntl(handle, Posix.F_SETFL, Posix.O_NONBLOCK | Posix.fcntl(handle, Posix.F_GETFL));
        IOChannel io_channel = new IOChannel.unix_new(handle);
        io_channel.set_encoding(null);
        io_channel.set_close_on_unref(true);
        io_channel.add_watch(io_cond, io_func);
    }

    private void watch_ssh_process(Cancellable? cancellable, ulong cancelled_sig) throws GLib.Error {
        ChildWatch.add_full(Priority.DEFAULT, this.pid, (pid, status) => {
            debug("Process '%s' done.", this.command_name);

            // FIXME This is the wrong place to catch that error really
            try {
                Process.check_exit_status(status);
            } catch (GLib.Error e) {
                Seahorse.Util.show_error(null, this.prompt_title, this.std_err.str);
            }
            cancellable.disconnect(cancelled_sig);
            Process.close_pid(pid);
        });
    }

    /**
     * Escapes a command-line argument so it can be safely passed on.
     *
     * @param arg The argument that should be escaped.
     * @return The escaped argument.
     */
    protected string escape_shell_arg (string arg) {
        string escaped = arg.replace("'", "\'");
        return "\'%s\'".printf(escaped);
    }

    private bool on_io_ssh_read(IOChannel source, IOCondition condition, StringBuilder data) {
        try {
            IOStatus status = IOStatus.NORMAL;
            while (status != IOStatus.EOF) {
                string buf;
                status = source.read_line(out buf, null, null);

                if (status == IOStatus.NORMAL && buf != null) {
                    data.append(buf);
                    debug("%s", data.str.substring(buf.length));
                }
            }
        } catch (GLib.Error e) {
            critical("Couldn't read output of SSH command. Error: %s", e.message);
            Posix.kill(this.pid, Posix.SIGTERM);
            return false;
        }

        return true;
    }

    private bool on_io_ssh_write(IOChannel source, IOCondition condition, StringBuilder input) {
        debug("SSHOP: SSH ready for input");
        try {
            size_t written = 0;
            IOStatus status = source.write_chars(input.str.to_utf8(), out written);
            if (status != IOStatus.AGAIN) {
                debug("SSHOP: Wrote %u bytes to SSH", (uint) written);
                input.erase(0, (ssize_t) written);
            }
        } catch (GLib.Error e) {
            critical("Couldn't write to STDIN of SSH command. Error: %s", e.message);
            Posix.kill(this.pid, Posix.SIGTERM);
            return false;
        }

        return true;
    }

    private void on_spawn_setup_child() {
        // No terminal for this process
        Posix.setsid();

        Environment.set_variable("SSH_ASKPASS", "%sseahorse-ssh-askpass".printf(Config.EXECDIR), false);

        // We do screen scraping so we need locale C
        if (Environment.get_variable("LC_ALL") != null)
            Environment.set_variable("LC_ALL", "C", true);
        Environment.set_variable("LANG", "C", true);

        if (this.prompt_title != null)
            Environment.set_variable("SEAHORSE_SSH_ASKPASS_TITLE", prompt_title, true);
        if (this.prompt_message != null)
            Environment.set_variable("SEAHORSE_SSH_ASKPASS_MESSAGE", prompt_message, true);
        if (this.prompt_transient_for != 0) {
            string parent = "%lu".printf(prompt_transient_for);
            Environment.set_variable("SEAHORSE_SSH_ASKPASS_PARENT", parent, true);
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
                                   Cancellable? cancellable) throws GLib.Error {
        this.prompt_title = _("Remote Host Password");

        if (keys == null
                || username == null || username == ""
                || hostname == null || hostname == "")
            return;

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
                                              Cancellable? cancellable) throws GLib.Error {
        if (key.key_data == null || key.key_data.privfile == null)
            return;

        this.prompt_title = _("Enter Key Passphrase");
        this.prompt_argument = key.label;

        string cmd = "%s -p -f '%s'".printf(Config.SSH_KEYGEN_PATH, key.key_data.privfile);
        yield operation_async(cmd, null, cancellable);
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

        string comment = escape_shell_arg(email);
        string algo = type.to_string().down();

        // Default number of bits
        if (bits == 0)
            bits = 2048;

        string cmd = "%s -b '%u' -t '%s' -C %s -f '%s'".printf(Config.SSH_KEYGEN_PATH, bits, algo, comment, filename);

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
        yield operation_async(cmd, null, cancellable);

        // Only use the first line of the output
        int pos = int.max(this.std_out.str.index_of_char('\n'), std_out.str.index_of_char('\r'));
        if (pos != -1)
            std_out.erase(pos);

        // Parse the data so we can get the fingerprint
        KeyData? keydata = KeyData.parse_line(std_out.str);

        // Add the comment to the output
        if (data.comment != null) {
            std_out.append_c(' ');
            std_out.append(data.comment);
        }

        // The file to write to
        string pubfile = "%s.pub".printf(file);
        FileUtils.set_contents(pubfile, std_out.str);

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
}
