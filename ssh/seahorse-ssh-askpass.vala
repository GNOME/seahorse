/*
 * Seahorse
 *
 * Copyright (C) 2005 Stefan Walter
 * Copyright (C) 2011 Collabora Ltd.
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
 *
 * Author: Stef Walter <stefw@collabora.co.uk>
 */

extern int setvbuf(Posix.FILE file, char* buffer, int mode, size_t size);
extern const int _IONBF;

public int main(string[] args) {
    Intl.bindtextdomain(Config.GETTEXT_PACKAGE, Config.LOCALEDIR);
    Intl.bind_textdomain_codeset(Config.GETTEXT_PACKAGE, "UTF-8");
    Intl.textdomain(Config.GETTEXT_PACKAGE);

    /* Non buffered stdout */
    setvbuf(Posix.stdout, null, _IONBF, 0);

    var app = new Adw.Application(null, ApplicationFlags.HANDLES_COMMAND_LINE);
    app.command_line.connect(on_app_command_line);

    return app.run(args);
}

private int on_app_command_line(Application app,
                                ApplicationCommandLine cmd_line) {
    var args = cmd_line.get_arguments();

    unowned string title = Environment.get_variable("SEAHORSE_SSH_ASKPASS_TITLE");
    if (title == null || title == "")
        title = _("Enter your Secure Shell passphrase:");

    string message = Environment.get_variable("SEAHORSE_SSH_ASKPASS_MESSAGE") ?? "";
    if (message == "") {
        if (args.length > 1)
            message = string.joinv(" ", args[1:]);
        else
            message = _("Enter your Secure Shell passphrase:");
    }

    unowned var argument = Environment.get_variable("SEAHORSE_SSH_ASKPASS_ARGUMENT") ?? "";

    unowned var flags = Environment.get_variable("SEAHORSE_SSH_ASKPASS_FLAGS") ?? "";
    if ("multiple" in flags) {
        string lower = message.ascii_down();

        /* Need the old passphrase */
        if ("old pass" in lower) {
            title = _("Old Key Passphrase");
            message = _("Enter the old passphrase for: %s").printf(argument);

        /* Look for the new passphrase thingy */
        } else if ("new pass" in lower) {
            title = _("New Key Passphrase");
            message = _("Enter the new passphrase for: %s").printf(argument);

        /* Confirm the new passphrase, just send it again */
        } else if ("again" in lower) {
            title = _("New Key Passphrase");
            message = _("Enter the new passphrase again: %s").printf(argument);
        }
    }

    // Make sure we don't timeout and quit
    app.hold();

    var dialog = new Seahorse.PassphrasePrompt(title, message, _("Password:"), null, false);
    dialog.prompt.begin(null, null, (obj, res) => {
        var pass = dialog.prompt.end(res);
        if (pass != null) {
            size_t len = (pass != null)? pass.length : 0;
            if (Posix.write(1, pass, len) != len) {
                warning("couldn't write out password properly");
            }
        }

        app.release();
    });

    return 0;
}
