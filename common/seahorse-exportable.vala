/*
 * Seahorse
 *
 * Copyright (C) 2004,2005 Stefan Walter
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
 * 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 */

namespace Seahorse {

public interface Exportable : GLib.Object {
	public abstract bool exportable { get; }

	public abstract GLib.List<Exporter> create_exporters(ExporterType type);

	public static bool can_export(GLib.Object object) {
		if (object is Exportable)
			return ((Exportable)object).exportable;
		return false;
	}

	public static int export_to_directory_wait(GLib.List<GLib.Object> objects,
	                                           string directory) throws GLib.Error {
		var loop = new GLib.MainLoop (null, false);
		GLib.AsyncResult? result = null;
		int count = 0;

		foreach (var object in objects) {
			if (!Exportable.can_export(object))
				continue;

			var exporters = ((Exportable)object).create_exporters(ExporterType.ANY);
			if (exporters == null)
				continue;

			var exporter = exporters.data;
			string filename = GLib.Path.build_filename (directory, exporter.filename, null);
			var file = GLib.File.new_for_uri(filename);

			exporter.export_to_file.begin(file, false, null, (obj, res) => {
				result = res;
				loop.quit();
			});

			loop.run();

			exporter.export_to_file.end(result);

			count++;
		}

		return count;
	}

	public static uint export_to_text_wait(GLib.List<GLib.Object> objects,
	                                       [CCode (array_length_type = "size_t")] out uchar[] output) throws GLib.Error {
		GLib.List<Exporter> exporters = null;

		foreach (var object in objects) {
			if (!Exportable.can_export(object))
				continue;

			/* If we've already found exporters, then add to those */
			if (exporters != null) {
				foreach (var exporter in exporters)
					exporter.add_object(object);

			/* Otherwise try and create new exporters for this object */
			} else {
				exporters = ((Exportable)object).create_exporters (ExporterType.TEXTUAL);
			}
		}

		/* Find the exporter than has the most objects */
		Exporter? chosen = null;
		uint total = 0;
		foreach (var exporter in exporters) {
			uint count = exporter.get_objects().length();
			if (count > total) {
				total = count;
				chosen = exporter;
			}
		}

		if (chosen != null) {
			var loop = new GLib.MainLoop (null, false);
			GLib.AsyncResult? result = null;

			chosen.export.begin(null, (obj, res) => {
				result = res;
				loop.quit();
			});

			loop.run();

			output = chosen.export.end(result);
			return total;
		}

		output = { };
		return 0;
	}

	public static int export_to_prompt_wait(GLib.List<GLib.Object> objects,
	                                        Gtk.Window? parent) throws GLib.Error {
		int count = 0;

		var pending = new GLib.HashTable<weak GLib.Object, weak GLib.Object>(GLib.direct_hash, GLib.direct_equal);
		foreach (var object in objects)
			pending.add(object);

		foreach (var object in objects) {
			if (pending.lookup(object) == null)
				continue;

			if (!Exportable.can_export(object)) {
				pending.remove(object);
				continue;
			}

			var exporters = ((Exportable)object).create_exporters(ExporterType.ANY);
			if (exporters == null)
				continue;

			foreach (var x in objects) {
				if (x == object)
					continue;
				if (pending.lookup(x) != null) {
					foreach (var exporter in exporters)
						exporter.add_object(x);
				}
			}

			string directory = null;
			GLib.File? file;
			Exporter? exporter;

			/* Now show a prompt choosing between the exporters */
			bool ret = Exportable.prompt(exporters, parent, ref directory,
			                             out file, out exporter);
			if (!ret)
				break;

			var loop = new GLib.MainLoop(null, false);
			GLib.AsyncResult? result = null;

			exporter.export_to_file.begin(file, true, null, (obj, res) => {
				result = res;
				loop.quit();
			});

			exporter.export_to_file.end(result);
			foreach (var e in exporter.get_objects()) {
				pending.remove(e);
				count++;
			}
		}

		return count;
	}

	private static string calculate_basename(GLib.File file,
	                                         string extension) {
		var basename = file.get_basename();
		var dot = basename.last_index_of_char('.');
		if (dot != -1)
			basename = basename.substring(0, dot);
		return "%s%s".printf(basename, extension);
	}

	public static bool prompt(GLib.List<Exporter> exporters,
	                          Gtk.Window? parent,
	                          ref string directory,
	                          out GLib.File chosen_file,
	                          out Exporter chosen_exporter) {
		var chooser = new Gtk.FileChooserDialog(null, parent, Gtk.FileChooserAction.SAVE,
		                                        Gtk.Stock.CANCEL, Gtk.ResponseType.CANCEL,
		                                        _("Export"), Gtk.ResponseType.ACCEPT,
		                                        null);

		chooser.set_default_response(Gtk.ResponseType.ACCEPT);
		chooser.set_local_only(false);
		chooser.set_do_overwrite_confirmation(true);

		if (directory != null)
			chooser.set_current_folder(directory);

		Gtk.FileFilter? first = null;
		var filters = new GLib.HashTable<Gtk.FileFilter, Exporter>(GLib.direct_hash, GLib.direct_equal);
		foreach (var exporter in exporters) {
			var filter = exporter.file_filter;
			filters.replace(filter, exporter);
			chooser.add_filter(filter);
			if (first == null)
				first = filter;
		}

		chooser.notify.connect((obj, prop) => {
			var exporter = filters.lookup(chooser.get_filter());
			var name = exporter.filename;
			var dot = name.last_index_of_char('.');
			if (dot != -1) {
				var file = chooser.get_file();
				if (file != null) {
					var basename = calculate_basename(file, name.substring(dot));
					chooser.set_current_name(basename);
				} else {
					chooser.set_current_name(name);
				}
			}
		});

		chooser.set_filter(first);

		if (chooser.run() == Gtk.ResponseType.ACCEPT) {
			chosen_file = chooser.get_file();
			chosen_exporter = filters.lookup(chooser.get_filter());
			directory = chooser.get_current_folder();
			return true;
		}

		chosen_file = null;
		chosen_exporter = null;
		return false;
	}
}

}
