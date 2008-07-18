
#include <seahorse-types.h>








GType seahorse_location_get_type (void) {
	static GType seahorse_location_type_id = 0;
	if (G_UNLIKELY (seahorse_location_type_id == 0)) {
		static const GEnumValue values[] = {{SEAHORSE_LOCATION_INVALID, "SEAHORSE_LOCATION_INVALID", "invalid"}, {SEAHORSE_LOCATION_MISSING, "SEAHORSE_LOCATION_MISSING", "missing"}, {SEAHORSE_LOCATION_SEARCHING, "SEAHORSE_LOCATION_SEARCHING", "searching"}, {SEAHORSE_LOCATION_REMOTE, "SEAHORSE_LOCATION_REMOTE", "remote"}, {SEAHORSE_LOCATION_LOCAL, "SEAHORSE_LOCATION_LOCAL", "local"}, {0, NULL, NULL}};
		seahorse_location_type_id = g_enum_register_static ("SeahorseLocation", values);
	}
	return seahorse_location_type_id;
}



GType seahorse_usage_get_type (void) {
	static GType seahorse_usage_type_id = 0;
	if (G_UNLIKELY (seahorse_usage_type_id == 0)) {
		static const GEnumValue values[] = {{SEAHORSE_USAGE_NONE, "SEAHORSE_USAGE_NONE", "none"}, {SEAHORSE_USAGE_SYMMETRIC_KEY, "SEAHORSE_USAGE_SYMMETRIC_KEY", "symmetric-key"}, {SEAHORSE_USAGE_PUBLIC_KEY, "SEAHORSE_USAGE_PUBLIC_KEY", "public-key"}, {SEAHORSE_USAGE_PRIVATE_KEY, "SEAHORSE_USAGE_PRIVATE_KEY", "private-key"}, {SEAHORSE_USAGE_CREDENTIALS, "SEAHORSE_USAGE_CREDENTIALS", "credentials"}, {SEAHORSE_USAGE_IDENTITY, "SEAHORSE_USAGE_IDENTITY", "identity"}, {SEAHORSE_USAGE_OTHER, "SEAHORSE_USAGE_OTHER", "other"}, {0, NULL, NULL}};
		seahorse_usage_type_id = g_enum_register_static ("SeahorseUsage", values);
	}
	return seahorse_usage_type_id;
}




