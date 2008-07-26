[CCode (cprefix="", lower_case_prefix="")]
namespace Bugs {

	/* 
	 * BUG: Hacks to get around deficiencies in vala bindings.
	 */
	
	/* ngettext seems to be missing from vala */ 
	[CCode (cname="ngettext", cheader_filename = "glib.h,glib/gi18n-lib.h")]
	public weak string ngettext (string single, string plural, int num);

	/* http://bugzilla.gnome.org/show_bug.cgi?id=540663 */
	[CCode (cname="gtk_selection_data_get_text")]
	public string selection_data_get_text (Gtk.SelectionData sel);
	
	/* http://bugzilla.gnome.org/show_bug.cgi?id=540664 */
	[CCode (cname="gtk_selection_data_get_uris"), NoArrayLength]
	public string[] selection_data_get_uris (Gtk.SelectionData sel);
	
	/* http://bugzilla.gnome.org/show_bug.cgi?id=540713 */
	[CCode (cname="gtk_notebook_page_num")]
	public int notebook_page_num (Gtk.Notebook notebook, Gtk.Widget widget);
	
	/* http://bugzilla.gnome.org/show_bug.cgi?id=544871 */
	[CCode (cname="g_base64_encode")]
	public string base64_encode (uchar* data, size_t length);
}
