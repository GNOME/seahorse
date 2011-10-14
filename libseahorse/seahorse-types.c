/*
 * Seahorse
 *
 * Copyright (C) 2008 Stefan Walter
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

#include "config.h"

#include "seahorse-types.h"

GType
seahorse_usage_get_type (void)
{
	static GType seahorse_usage_type_id = 0;
	if (!seahorse_usage_type_id) {
		static const GEnumValue values[] = {
			{ SEAHORSE_USAGE_NONE, "SEAHORSE_USAGE_NONE", "none" },
			{ SEAHORSE_USAGE_SYMMETRIC_KEY, "SEAHORSE_USAGE_SYMMETRIC_KEY", "symmetric-key" }, 
			{ SEAHORSE_USAGE_PUBLIC_KEY, "SEAHORSE_USAGE_PUBLIC_KEY", "public-key" }, 
			{ SEAHORSE_USAGE_PRIVATE_KEY, "SEAHORSE_USAGE_PRIVATE_KEY", "private-key" }, 
			{ SEAHORSE_USAGE_CREDENTIALS, "SEAHORSE_USAGE_CREDENTIALS", "credentials" }, 
			{ SEAHORSE_USAGE_IDENTITY, "SEAHORSE_USAGE_IDENTITY", "identity" }, 
			{ SEAHORSE_USAGE_OTHER, "SEAHORSE_USAGE_OTHER", "other" }, 
			{ 0, NULL, NULL }
		};
		seahorse_usage_type_id = g_enum_register_static ("SeahorseUsage", values);
	}
	return seahorse_usage_type_id;
}

GType
seahorse_flags_get_type (void)
{
	static GType etype = 0;
	if (G_UNLIKELY(etype == 0)) {
		static const GFlagsValue values[] = {
			{ SEAHORSE_FLAG_NONE, "SEAHORSE_FLAG_NONE", "none" },
			{ SEAHORSE_FLAG_IS_VALID, "SEAHORSE_FLAG_IS_VALID", "is-valid" },
			{ SEAHORSE_FLAG_CAN_ENCRYPT, "SEAHORSE_FLAG_CAN_ENCRYPT", "can-encrypt" },
			{ SEAHORSE_FLAG_CAN_SIGN, "SEAHORSE_FLAG_CAN_SIGN", "can-sign" },
			{ SEAHORSE_FLAG_EXPIRED, "SEAHORSE_FLAG_EXPIRED", "expired" },
			{ SEAHORSE_FLAG_REVOKED, "SEAHORSE_FLAG_REVOKED", "revoked" },
			{ SEAHORSE_FLAG_DISABLED, "SEAHORSE_FLAG_DISABLED", "disabled" },
			{ SEAHORSE_FLAG_TRUSTED, "SEAHORSE_FLAG_TRUSTED", "trusted" },
			{ SEAHORSE_FLAG_PERSONAL, "SEAHORSE_FLAG_PERSONAL", "personal" },
			{ SEAHORSE_FLAG_EXPORTABLE, "SEAHORSE_FLAG_EXPORTABLE", "exportable" },
			{ SEAHORSE_FLAG_DELETABLE, "SEAHORSE_FLAG_DELETABLE", "deletable" },
			{ 0, NULL, NULL }
		};
		etype = g_flags_register_static (g_intern_static_string ("SeahorseFlags"), values);
	}
	return etype;
}
