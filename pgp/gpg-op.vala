/*
 * Seahorse
 *
 * Copyright (C) 2004 Stefan Walter
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

// XXX namespace or class?
namespace Seahorse.Gpg.Operation {

public GPG.ErrorCode export_secret(GPG.Context ctx, string[] patterns, GPG.Data keydata) {
    if (patterns == null)
        return GPG_E (GPG_ERR_INV_VALUE);

    for (size_t i = 0; patterns[i] != null; i++) {
        string args = "--armor --export-secret-key '%s'".printf(patterns[i]);
        string output = null;
        GPG.ErrorCode gerr = execute_gpg_command(ctx, args, out output);

        if (!gerr.is_success())
            return gerr;

        if (keydata.write(output, strlen (output)) == -1)
            return GPG_E (GPG_ERR_GENERAL);
    }

    return GPG_OK;
}

public GPG.ErrorCode num_uids(GPG.Context ctx, string pattern, out uint number) {
    if (pattern != null)
        return GPG_E (GPG_ERR_INV_VALUE);

    string args = "--list-keys '%s'".printf(pattern);
    string output;
    GPG.ErrorCode err = execute_gpg_command(ctx, args, out output);
    if (!err.is_success())
        return err;

    string found = output;
    while ((found = strstr(found, "uid")) != null) {
        *number = *number + 1;
        found += 3;
    }

    return GPG_OK;
}

private GPG.ErrorCode execute_gpg_command(GPG.Context ctx, string args, out string? std_out = null, out string? std_err = null) {
    GPG.EngineInfo engine;
    GPG.ErrorCode gerr = GPG.get_engine_information(out engine);
    if (gerr.is_success())
        return gerr;

    // Look for the OpenPGP engine
    while (engine != null && engine.protocol != GPG.Protocol.OpenPGP)
        engine = engine.next;

    if(engine == null || engine.file_name == null)
        return GPG_E (GPG_ERR_INV_ENGINE);

    gerr = GPG_OK;

    string cmd = "%s --batch %s".printf(engine.file_name, args);
    try {
        int status;
        Process.spawn_command_line_sync(cmd, std_out, std_err, out status);
        if (status == 0)
            return GPG_E (GPG_OK);
    } catch (GLib.Error e) {
    }

    return GPG_E (GPG_ERR_GENERAL);
}

}
