/*
 * Seahorse
 *
 * Copyright (C) 2004 Nate Nielsen
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

/**
 * SeahorseHKPSource: A key source which searches LDAP PGP key servers. 
 * 
 * - Derived from SeahorseServerSource.
 * - Adds found keys to SeahorseContext. 
 */
 
#ifndef __SEAHORSE_LDAP_SOURCE_H__
#define __SEAHORSE_LDAP_SOURCE_H__

#include "config.h"
#include "seahorse-server-source.h"

#ifdef WITH_LDAP

#define SEAHORSE_TYPE_LDAP_SOURCE            (seahorse_ldap_source_get_type ())
#define SEAHORSE_LDAP_SOURCE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_TYPE_LDAP_SOURCE, SeahorseLDAPSource))
#define SEAHORSE_LDAP_SOURCE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_LDAP_SOURCE, SeahorseLDAPSourceClass))
#define SEAHORSE_IS_LDAP_SOURCE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_TYPE_LDAP_SOURCE))
#define SEAHORSE_IS_LDAP_SOURCE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_LDAP_SOURCE))
#define SEAHORSE_LDAP_SOURCE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_TYPE_LDAP_SOURCE, SeahorseLDAPSourceClass))

typedef struct _SeahorseLDAPSource SeahorseLDAPSource;
typedef struct _SeahorseLDAPSourceClass SeahorseLDAPSourceClass;

struct _SeahorseLDAPSource {
    SeahorseServerSource parent;
    
    /*< private >*/
};

struct _SeahorseLDAPSourceClass {
    SeahorseServerSourceClass parent_class;
};

GType                 seahorse_ldap_source_get_type (void);

SeahorseLDAPSource*   seahorse_ldap_source_new     (const gchar *uri, 
                                                    const gchar *host);

gboolean              seahorse_ldap_is_valid_uri   (const gchar *uri);

#endif /* WITH_LDAP */

#endif /* __SEAHORSE_SERVER_SOURCE_H__ */
