
[CCode (cheader_filename = "pkcs11.h,pkcs11g.h", cprefix = "", lower_case_cprefix = "")] 
namespace P11 {
	const uint CKA_CLASS;
	const uint CKA_END_DATE;
	const uint CKA_GNOME_IMPORT_OBJECTS;
	const uint CKA_GNOME_IMPORT_TOKEN;
	const uint CKA_GNOME_USER_TRUST;
	const uint CKA_ID;
	const uint CKA_LABEL;
	const uint CKA_START_DATE;
	const uint CKA_TOKEN;
	const uint CKA_VALUE;
	
	const ulong CKO_DATA;
	const ulong CKO_CERTIFICATE;
	const ulong CKO_PRIVATE_KEY;
	const ulong CKO_PUBLIC_KEY;
	
	const uint CKF_RW_SESSION;
	
	const ulong CKR_OBJECT_HANDLE_INVALID;
	const ulong CKR_FUNCTION_CANCELED;
	
	const ulong CKT_GNOME_TRUSTED;
	const ulong CKT_GNOME_UNTRUSTED;
}
