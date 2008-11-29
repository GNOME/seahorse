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

GType seahorse_location_get_type (void) 
{
	static GType seahorse_location_type_id = 0;
	if (!seahorse_location_type_id) {
		static const GEnumValue values[] = {
			{ SEAHORSE_LOCATION_INVALID, "SEAHORSE_LOCATION_INVALID", "invalid" }, 
			{ SEAHORSE_LOCATION_MISSING, "SEAHORSE_LOCATION_MISSING", "missing" }, 
			{ SEAHORSE_LOCATION_SEARCHING, "SEAHORSE_LOCATION_SEARCHING", "searching" }, 
			{ SEAHORSE_LOCATION_REMOTE, "SEAHORSE_LOCATION_REMOTE", "remote" }, 
			{ SEAHORSE_LOCATION_LOCAL, "SEAHORSE_LOCATION_LOCAL", "local" }, 
			{ 0, NULL, NULL }
		};
		seahorse_location_type_id = g_enum_register_static ("SeahorseLocation", values);
	}
	return seahorse_location_type_id;
}



GType seahorse_usage_get_type (void) 
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
