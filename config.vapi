[CCode (cprefix="", lower_case_prefix="", cheader_filename="config.h")]
namespace Config {
	[CCode (cname = "VERSION")]
	public const string VERSION;
	
	[CCode (cname = "GETTEXT_PACKAGE")]
	public const string GETTEXT_PACKAGE;

	[CCode (cname = "SEAHORSE_GLADEDIR")]
	public const string SEAHORSE_GLADEDIR;
}
