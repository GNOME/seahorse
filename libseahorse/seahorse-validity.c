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
 * along with this program; if not, write to the
 * Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#include <config.h>
#include <gnome.h>

#include "seahorse-validity.h"

const gchar*
seahorse_validity_get_string (SeahorseValidity validity)
{
	switch (validity) {
		case SEAHORSE_VALIDITY_UNKNOWN:
			return _("Unknown");
		case SEAHORSE_VALIDITY_NEVER:
			return _("Never");
		case SEAHORSE_VALIDITY_MARGINAL:
			return _("Marginal");
		case SEAHORSE_VALIDITY_FULL:
			return _("Full");
		case SEAHORSE_VALIDITY_ULTIMATE:
			return _("Ultimate");
		case SEAHORSE_VALIDITY_DISABLED:
			return _("Disabled");
		case SEAHORSE_VALIDITY_REVOKED:
			return _("Revoked");
		default:
			return NULL;
	}
}

SeahorseValidity    
seahorse_validity_from_gpgme (gpgme_validity_t validity)
{
    switch (validity) {
    case GPGME_VALIDITY_NEVER:
        return SEAHORSE_VALIDITY_NEVER;
    case GPGME_VALIDITY_MARGINAL:
        return SEAHORSE_VALIDITY_MARGINAL;
    case GPGME_VALIDITY_FULL:
        return SEAHORSE_VALIDITY_FULL;
    case GPGME_VALIDITY_ULTIMATE:
        return SEAHORSE_VALIDITY_ULTIMATE;
    case GPGME_VALIDITY_UNDEFINED:
    case GPGME_VALIDITY_UNKNOWN:
    default:
        return SEAHORSE_VALIDITY_UNKNOWN;
    }
}

