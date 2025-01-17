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
public class Pgp.Backend : GLib.Object, GLib.ListModel, Place {
    public static void initialize(string? gpg_homedir);
    public static unowned Pgp.Backend get();

    public unowned GLib.ListModel get_remotes();
    public void add_remote(string uri, bool persist);
    public void remove_remote(string uri);

    public Seahorse.Item create_key_for_parsed(Gcr.Parsed parsed);
}

[CCode (cheader_filename = "pgp/seahorse-server-source.h")]
public class ServerSource : GLib.Object, GLib.ListModel, Place {
}

#if WITH_LDAP
[CCode (cheader_filename = "pgp/seahorse-ldap-source.h")]
public class LdapSource : ServerSource {
}

[CCode (cheader_filename = "pgp/seahorse-ldap-source.h")]
public static bool ldap_is_valid_uri(string uri);
#endif // WITH_LDAP

#if WITH_HKP
[CCode (cheader_filename = "pgp/seahorse-hkp-source.h")]
public class HkpSource : ServerSource {
}

[CCode (cheader_filename = "pgp/seahorse-hkp-source.h")]
public static bool hkp_is_valid_uri(string uri);
#endif // WITH_HKP

[CCode (cheader_filename = "pkcs11/seahorse-pkcs11-backend.h")]
public class Pkcs11.Backend : GLib.Object, GLib.ListModel {
    public static void initialize();
    public static unowned Pkcs11.Backend get();
}
}
