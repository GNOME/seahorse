/*
 * Seahorse
 *
 * Copyright (C) 2005 Stefan Walter
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

/**
 * SeahorseHKPSource: A key source which searches HTTP PGP key servers.
 *
 * - Derived from SeahorseServerSource.
 * - Adds found keys to SeahorseContext.
 * - Used by SeahorseServiceDiscovery for retrieving shared keys.
 */

#pragma once

#include "config.h"
#include "seahorse-server-source.h"

#ifdef WITH_HKP

#define SEAHORSE_TYPE_HKP_SOURCE (seahorse_hkp_source_get_type ())
G_DECLARE_FINAL_TYPE (SeahorseHKPSource, seahorse_hkp_source,
                      SEAHORSE, HKP_SOURCE,
                      SeahorseServerSource)

SeahorseHKPSource*    seahorse_hkp_source_new      (const char *uri);

gboolean              seahorse_hkp_is_valid_uri    (const char *uri);

GList *               seahorse_hkp_parse_lookup_response  (const char *response);


#define HKP_ERROR_DOMAIN (seahorse_hkp_error_quark())
GQuark            seahorse_hkp_error_quark       (void);

#endif /* WITH_HKP */
