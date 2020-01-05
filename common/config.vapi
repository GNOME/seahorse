[CCode (cprefix = "", lower_case_cprefix = "", cheader_filename = "config.h")]
namespace Config
{
    public const string APPLICATION_ID;

    public const string PROFILE;

	public const string PKGDATADIR;

	public const string EXECDIR;
	public const string LOCALEDIR;

	public const string VERSION;
	public const string PACKAGE;
	public const string PACKAGE_STRING;
	public const string GETTEXT_PACKAGE;

	public const string SSH_PATH;
	public const string SSH_KEYGEN_PATH;

	public const string GNUPG;
	public const int GPG_MAJOR;
	public const int GPG_MINOR;
	public const int GPG_MICRO;
}

/*
 * TODO: Temporary hacks for interfacing with some very simple C code in
 * the current C code base. In general we want to port to vala instead of
 * listing stuff here. Otherwise things will get unmanageable.
 */

namespace Seahorse {

[CCode (cheader_filename = "data/seahorse-resources.h")]
public void register_resource();

[CCode (cheader_filename = "libseahorse/seahorse-util.h")]
public static GLib.HashFunc<ulong?> ulong_hash;

[CCode (cheader_filename = "libseahorse/seahorse-util.h")]
public static GLib.EqualFunc<ulong?> ulong_equal;

[CCode (cheader_filename = "libseahorse/seahorse-progress.h")]
namespace Progress {
	public void show(GLib.Cancellable? cancellable, string title, bool delayed);
}

[CCode (cheader_filename = "pgp/seahorse-pgp-backend.h")]
namespace Pgp.Backend {
	public void initialize();
}

[CCode (cheader_filename = "pkcs11/seahorse-pkcs11-backend.h")]
public class Pkcs11.Backend {
	public static void initialize();
	public static Gcr.Collection get_writable_tokens(Pkcs11.Backend? self, ulong with_mechanism);
}
}

namespace Egg {
[CCode (cheader_filename = "libegg/eggtreemultidnd.h")]
namespace TreeMultiDrag {
	public void add_drag_support(Gtk.TreeView view);
}
}
