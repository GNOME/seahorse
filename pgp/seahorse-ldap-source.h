/*
 * Seahorse
 *
 * Copyright (C) 2004 Stefan Walter
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
 * SeahorseHKPSource: A key source which searches LDAP PGP key servers.
 *
 * - Derived from SeahorseServerSource.
 * - Adds found keys to SeahorseContext.
 */

#pragma once

#include "seahorse-server-source.h"

#ifdef WITH_LDAP

#define SEAHORSE_TYPE_LDAP_SOURCE (seahorse_ldap_source_get_type ())
G_DECLARE_FINAL_TYPE (SeahorseLDAPSource, seahorse_ldap_source,
                      SEAHORSE, LDAP_SOURCE,
                      SeahorseServerSource)

SeahorseLDAPSource*   seahorse_ldap_source_new     (const char *uri);

gboolean              seahorse_ldap_is_valid_uri   (const char *uri);

#endif /* WITH_LDAP */
