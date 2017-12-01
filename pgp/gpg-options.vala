/*
 * Seahorse
 *
 * Copyright (C) 2003 Stefan Walter
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

namespace Seahorse.Gpg.Options {

public const string GPG_CONF_HEADER = "# FILE CREATED BY SEAHORSE\n\n";
public const string GPG_VERSION_PREFIX1 = "1.";
public const string GPG_VERSION_PREFIX2 = "2.";

public const string HOME_PREFIX = "\nHome: ";

/* Initializes the gpg-options static info */
private bool gpg_options_init() throws GLib.Error {
    if (gpg_options_inited)
        return true;

    GPG.EngineInfo engine_info;
    GPGError.ErrorCode gerr = GPG.get_engine_info(out engine_info);
    if (seahorse_gpgme_propagate_error (gerr, err))
        return false;

    /* Look for the OpenPGP engine_info */
    while (engine_info != null && engine.protocol != GPGME_PROTOCOL_OpenPGP)
        engine_info = engine.next;

    // Make sure it's the right version for us to be messing
    // around with the configuration file.
    if (engine_info == null || engine.version == null || engine.file_name == null ||
        !(engine_info.version.has_prefix(GPG_VERSION_PREFIX1) ||
          engine_info.version.has_prefix(GPG_VERSION_PREFIX2))) {
        seahorse_gpgme_propagate_error (GPG_E (GPG_ERR_INV_ENGINE), err);
        return false;
    }

    // Read in the home directory
    gpg_homedir = engine_info.home_dir ?? GPG.get_dirinfo("homedir");

    gpg_options_inited = true;
    return true;
}

/**
 * Returns: The home dir that GPG uses for it's keys and configuration
 **/
public string? seahorse_gpg_homedir() {
    if (!Options.init(null))
        return null;
    return gpg_homedir;
}

/**
 * Find the value for a given option in the gpg config file.
 * Values without a value are returned as an empty string.
 * On success be sure to free *value after you're done with it.
 *
 * @param option The option to find
 *
 * @return the option's value, or null when not found
 **/
public string? find(string option) throws GLib.Error {
    string[] values = find_vals ({option}, value, err);
    return (values.length > 0)? values[0] : null;
}

/**
 * Find the values for the given options in the gpg config file.
 *
 * @param option null terminated array of option names
 *
 * @return An array of values. Values without a value are returned as an empty string.
 **/
public string[] find_vals(string[] options) throws GLib.Error {
    string[] values = new string[lines.length];

    Options.init();
    string[] lines = read_config_file();
    foreach (string l in lines) {
        string line = l.strip();

        // Ignore comments and blank lines
        if (line[0] == '#' || line == "")
            continue;

        for (int i = 0; i < options.length; i++) {
            string option = options[i];
            if (line.has_prefix(option)) {
                string t = line.substring(option.length);
                if (t[0] == 0 || t[0].ascii_isspace()) {
                    // NOTE: we don't short-circuit the search because for GPG, options can be
                    // specified multiple times, and the last one wins.
                    values[i] = t.strip();
                    break;
                }
            }
        }
    }

    return values;
}

/* Figure out needed changes to configuration file */
private void process_conf_edits (string[] lines, string[] options, string[] values) {
    for (int i = 0; i < lines.length; i++) {
        string line = line[i];
        assert(line != null);

        // Does this line have an ending?
        // We use this below when appending lines.
        string n = line.strip();

        // Don't use g_strstrip as we don't want to modify the line
        while (*n && g_ascii_isspace (*n))
            n++;

        // Ignore blank lines
        bool comment;
        if (n[0] != 0) {
            comment = false;

            // We look behind comments to see if we need to uncomment them
            if (n[0] == '#') {
                n++;
                comment = true;

                while (*n && n[0].ascii_isspace())
                    n++;
            }

            for (uint j = 0; options[j] != null; j++) {
                string option = options[j];
                if (!n.has_prefix(options[j]))
                    continue;

                string t = n.substring(option.length);
                if (t[0] != 0 && !t.ascii_isspace())
                    continue;

                // Are we setting this value?
                if (values[j]) {
                    // At this point we're rewriting the line, so we
                    // can modify the old line
                    *t = 0;

                    // A line with a value
                    if (values[j][0])
                        n = n + " " + values[j];

                    // We're done with this option, all other instances
                    // of it need to be commented out
                    values[j] = null;
                }

                // Otherwise we're removing the value
                else if (!comment) {
                    n = "# " + n;
                }

                line = n;

                // Done with this line
                break;
            }
        }

        if (lines[j] != line)
            lines[j] = line;
    }

    // Append any that haven't been added but need to */
    for (uint i = 0; i < options.length; i++) {
        // Are we setting this value?
        if (values[i] != null) {
            string n = options[i];
            // A line with a value
            if (values[i][0] != null)
                n = n + " " + values[i];

            lines += n;
        }
    }
}

/**
 * Changes the given option in the gpg config file.
 *
 * @param option The option to change
 * @param value The value to change it to. If value is null, the option will be deleted.
 *              If you want an empty value, set value to an empty string.
 */
public void change(string option, string? value) throws Glib.Error {
    change_vals({option}, {value});
}

/**
 * Changes the given option in the gpg config file.
 * If a value is null, the option will be deleted. If you want
 * an empty value, set value to an empty string.
 *
 * @param option null-terminated array of option names to change
 * @param value The values to change respective option to
 *
 * @return true if success, false if not XXX change to void
 **/
public void change_vals(string[] options, string[] values) throws GLib.Error {
    gpg_options_init();

    string[] lines = read_config_file();
    process_conf_edits(lines, options, values);
    write_config_file(lines);
}

private string? gpg_homedir;
private bool gpg_options_inited = false;

private bool create_file(string file, mode_t mode) throws GLib.Error {
    int fd;

    if ((fd = open (file, O_CREAT | O_TRUNC | O_WRONLY, mode)) == -1) {
        g_set_error (err, G_IO_CHANNEL_ERROR, g_io_channel_error_from_errno (errno),
                     "%s", g_strerror (errno));
        return false;
    }

    /* Write the header when we make a new file */
    if (write (fd, GPG_CONF_HEADER, strlen (GPG_CONF_HEADER)) == -1) {
        g_set_error (err, G_IO_CHANNEL_ERROR, g_io_channel_error_from_errno (errno),
                     "%s", strerror (errno));
    }

    close (fd);
    return *err ? false : true;
}

/* Finds relevant configuration file, creates if not found */
private string? find_config_file(bool read) throws GLib.Error {
    if (gpg_options_inited == null || gpg_homedir == null)
        return null;

    // Check for and open ~/.gnupg/gpg.conf
    string conf = gpg_homedir + "/gpg.conf";
    if (FileUtils.test(conf, FileTest.IS_REGULAR | FileTest.EXISTS))
        return conf;

    // Check for and open ~/.gnupg/options */
    conf = gpg_homedir + "/options";
    if (FileUtils.test(conf, FileTest.IS_REGULAR | FileTest.EXISTS))
        return conf;

    // Make sure directory exists
    if (!FileUtils.test(gpg_homedir, FileTest.EXISTS)) {
        if (Posix.mkdir(gpg_homedir, 0700) == -1) {
            g_set_error (err, G_IO_CHANNEL_ERROR,
                         g_io_channel_error_from_errno (errno),
                         "%s", strerror (errno));
            return null;
        }
    }

    // For writers just return the file name
    conf = gpg_homedir + "/gpg.conf";
    if (!read)
        return conf;

    // ... for readers we create ~/.gnupg/gpg.conf
    if (create_file (conf, 0600, err))
        return conf;

    return null;
}

private string[] read_config_file() throws GLib.Error {
    string? conf = find_config_file(true);
    if (conf == null)
        return null;

    string contents;
    FileUtils.get_contents(conf, out contents);

    // We took ownership of the individual lines
    return contents.split("\n");
}

private bool write_config_file(string[] lines) throws GLib.Error {
    string? conf = find_config_file(false);
    if (conf == null)
        return false;

    string[] contents = string.joinv("\n", lines);
    Util.write_file_private(conf, contents, err);

    return *err ? false : true;
}

}
