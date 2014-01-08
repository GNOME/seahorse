/*
 * Seahorse
 *
 * Copyright (C) 2003 Jacob Perkins
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
 * along with this program; if not, see
 * <http://www.gnu.org/licenses/>.
 */
#include <config.h>

#include <glib/gi18n.h>

#include <string.h>

#include "seahorse-validity.h"

/**
 * seahorse_validity_get_string:
 * @validity: The validity ID
 *
 * Returns: A string describing the validity. Must not be freed
 */
const gchar*
seahorse_validity_get_string (SeahorseValidity validity)
{
	switch (validity) {
		case SEAHORSE_VALIDITY_UNKNOWN:
			return C_("Validity", "Unknown");
		case SEAHORSE_VALIDITY_NEVER:
			return C_("Validity", "Never");
		case SEAHORSE_VALIDITY_MARGINAL:
			return C_("Validity", "Marginal");
		case SEAHORSE_VALIDITY_FULL:
			return C_("Validity", "Full");
		case SEAHORSE_VALIDITY_ULTIMATE:
			return C_("Validity", "Ultimate");
		case SEAHORSE_VALIDITY_DISABLED:
			return C_("Validity", "Disabled");
		case SEAHORSE_VALIDITY_REVOKED:
			return C_("Validity", "Revoked");
		default:
			return NULL;
	}
}

