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

#pragma once

#include <glib.h>
#include <glib-object.h>

#define SEAHORSE_TYPE_DISCOVERY             (seahorse_discovery_get_type ())
#define SEAHORSE_DISCOVERY(obj)	            (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_TYPE_DISCOVERY, SeahorseDiscovery))
#define SEAHORSE_DISCOVERY_CLASS(klass)	    (G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_DISCOVERY, SeahorseDiscoveryClass))
#define SEAHORSE_IS_DISCOVERY(obj)		    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_TYPE_DISCOVERY))
#define SEAHORSE_IS_DISCOVERY_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_DISCOVERY))
#define SEAHORSE_DISCOVERY_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_TYPE_DISCOVERY, SeahorseDiscoveryClass))

typedef struct _SeahorseDiscovery SeahorseDiscovery;
typedef struct _SeahorseDiscoveryClass SeahorseDiscoveryClass;
typedef struct _SeahorseDiscoveryPriv SeahorseDiscoveryPriv;

/**
 * SeahorseDiscovery:
 * @parent: The parent #GObject
 * @services: A #GHashTable of known services
 *
 * Listens for DNS-SD shared keys on the network and
 * adds SeahorseKeySoruce objects to the SeahorseContext as necessary.
 *
 * Signals:
 *   added: A new shared key source was found.
 *   removed: A shared key source went away.
 */

struct _SeahorseDiscovery {
	GObject parent;

	GHashTable  *services;
	SeahorseDiscoveryPriv *priv;
};

struct _SeahorseDiscoveryClass {
	GObjectClass parent_class;

	/* A relevant service appeared on the network */
	void    (*added)    (SeahorseDiscovery *ssd,
	                     const gchar* service);

	/* A service disappeared */
	void    (*removed)  (SeahorseDiscovery *ssd,
	                     const gchar* service);
};

GType                       seahorse_discovery_get_type  (void);

SeahorseDiscovery*          seahorse_discovery_new       (void);

gchar **                    seahorse_discovery_list      (SeahorseDiscovery *ssd);

const gchar*                seahorse_discovery_get_uri   (SeahorseDiscovery *ssd,
                                                          const gchar *service);

gchar **                    seahorse_discovery_get_uris  (SeahorseDiscovery *ssd,
                                                          const gchar **services);
