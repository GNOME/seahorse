/*
 * Seahorse
 *
 * Copyright (C) 2011 Collabora Ltd.
 * Copyright (C) 2012 Stefan Walter
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Stef Walter <stefw@collabora.co.uk>
 */

public abstract class Seahorse.ExportOperation : GLib.Object {

    public GLib.OutputStream output { get; set; }

    public abstract async bool execute(Cancellable? cancellable = null)
                                       throws GLib.Error;

    /**
     * A helper method to set the output stream to a file picked interactively
     * by the user.
     *
     * Note that this function may return false without throwing an error, in
     * case the user didn't pick any file.
     */
    public async bool prompt_for_file(Gtk.Window? parent,
                                      Cancellable? cancellable = null)
                                      throws GLib.Error {
        // Ask the usr to pick the file
        var dialog = create_file_dialog(parent);
        var file = yield dialog.save(parent, cancellable);
        if (file == null)
            return false;

        // Create the output stream for it
        this.output = yield file.create_async(FileCreateFlags.NONE,
                                              Priority.DEFAULT,
                                              cancellable);
        return true;
    }

    /**
     * Creates a {@link Gtk.FileDialog} that can be used to select a file to
     * export to.
     */
    protected abstract Gtk.FileDialog create_file_dialog(Gtk.Window? parent);
}
