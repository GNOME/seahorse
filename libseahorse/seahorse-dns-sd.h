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
 * along with this program; if not, write to the
 * Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 */
 
#ifndef __SEAHORSE_DNS_SD_H__
#define __SEAHORSE_DNS_SD_H__

#include <glib.h>

#define SEAHORSE_TYPE_SERVICE_DISCOVERY             (seahorse_service_discovery_get_type ())
#define SEAHORSE_SERVICE_DISCOVERY(obj)	            (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_TYPE_SERVICE_DISCOVERY, SeahorseServiceDiscovery))
#define SEAHORSE_SERVICE_DISCOVERY_CLASS(klass)	    (G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_SERVICE_DISCOVERY, SeahorseServiceDiscoveryClass))
#define SEAHORSE_IS_SERVICE_DISCOVERY(obj)		    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_TYPE_SERVICE_DISCOVERY))
#define SEAHORSE_IS_SERVICE_DISCOVERY_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_SERVICE_DISCOVERY))
#define SEAHORSE_SERVICE_DISCOVERY_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_TYPE_SERVICE_DISCOVERY, SeahorseServiceDiscoveryClass))

typedef struct _SeahorseServiceDiscovery SeahorseServiceDiscovery;
typedef struct _SeahorseServiceDiscoveryClass SeahorseServiceDiscoveryClass;
typedef struct _SeahorseServiceDiscoveryPriv SeahorseServiceDiscoveryPriv;

/**
 * SeahorseServiceDiscovery:
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

struct _SeahorseServiceDiscovery {
    GObject parent;

    /*< public >*/
    GHashTable  *services;
    
    /*< private >*/
    SeahorseServiceDiscoveryPriv *priv;
};

struct _SeahorseServiceDiscoveryClass {
    GObjectClass parent_class;

    /* A relevant service appeared on the network */
    void (*added) (SeahorseServiceDiscovery *ssd, const gchar* service);

    /* A service disappeared */
    void (*removed) (SeahorseServiceDiscovery *ssd, const gchar* service);
};

GType                       seahorse_service_discovery_get_type  (void);

SeahorseServiceDiscovery*   seahorse_service_discovery_new       (void);

gchar **                    seahorse_service_discovery_list      (SeahorseServiceDiscovery *ssd);

const gchar*                seahorse_service_discovery_get_uri   (SeahorseServiceDiscovery *ssd,
                                                                  const gchar *service);

gchar **                    seahorse_service_discovery_get_uris  (SeahorseServiceDiscovery *ssd,
                                                                  const gchar **services);

#endif /* __SEAHORSE_DNS_SD_H__ */
