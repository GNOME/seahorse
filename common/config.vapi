[CCode (prefix = "", lower_case_cprefix = "", cheader_filename = "config.h")]
namespace Config
{
	public const string PKGDATADIR;
	public const string UIDIR;

	public const string VERSION;
	public const string PACKAGE;
	public const string GETTEXT_PACKAGE;
}

/*
 * TODO: Temporary hacks for interfacing with some very simple C code in
 * the current C code base. In general we want to port to vala instead of
 * listing stuff here. Otherwise things will get unmanageable.
 */
namespace Seahorse {

namespace Prefs {
	public void show(Gtk.Window window, string? tabid);
	public bool available();
}

namespace Application {
	public unowned Gtk.Application @get();
}

}
