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
 * along with this program; if not, write to the
 * Free Software Foundation, Inc.,
 * 59 Temple Exporter, Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Stef Walter <stefw@collabora.co.uk>
 */

namespace Seahorse {

public enum ExporterType {
	ANY,
	TEXTUAL
}

public interface Exporter : GLib.Object {
	public abstract string filename { get; }

	public abstract string content_type { get; }

	public abstract Gtk.FileFilter file_filter { get; }

	public abstract unowned GLib.List<weak GLib.Object> get_objects();

	public abstract bool add_object(GLib.Object obj);

	[CCode (array_length_type = "size_t")]
	public abstract async uchar[] export(GLib.Cancellable? cancellable) throws GLib.Error;

	static GLib.File file_increment_unique(GLib.File file,
	                                       ref uint state) {

		string uri = file.get_uri();

		/* Last path component */
		int last = uri.last_index_of_char('/');
		if (last == -1)
			last = 0;

		string prefix;
		string suffix;

		/* A dot in last path component? */
		int index = uri.last_index_of_char('.', last);
		if (index == -1) {
			prefix = uri;
			suffix = "";
		} else {
			prefix = uri.substring(0, index);
			suffix = uri.substring(index);
		}

		state++;
		return GLib.File.new_for_uri("%s-%u%s".printf(prefix, state, suffix));
	}

	public async bool export_to_file(GLib.File file,
	                                 bool overwrite,
	                                 GLib.Cancellable? cancellable) throws GLib.Error {

		uchar[] bytes = yield this.export(cancellable);
		GLib.File out = file;

		/*
		 * When not trying to overwrite we pass an invalid etag. This way
		 * if the file exists, it will not match the etag, and we'll be
		 * able to detect it and try another file name.
		 */

		while (true) {
			uint unique = 0;
			try {
				yield out.replace_contents_async(bytes, overwrite ? null : "invalid etag",
				                                 false, GLib.FileCreateFlags.PRIVATE,
				                                 cancellable, null);
				return true;

			} catch (GLib.IOError err) {
				if (err is GLib.IOError.WRONG_ETAG) {
					out = file_increment_unique(file, ref unique);
					continue;
				}
				throw err;
			}
		}
	}
}

}
