/*
 * Seahorse
 *
 * Copyright (C) 2013 Giovanni Campagna <scampa.giovanni@gmail.com>
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

#ifndef __SEAHORSE_SEARCH_PROVIDER_H__
#define __SEAHORSE_SEARCH_PROVIDER_H__

#include <gio/gio.h>

#define SEAHORSE_TYPE_SEARCH_PROVIDER                   (seahorse_search_provider_get_type ())
#define SEAHORSE_SEARCH_PROVIDER(obj)                   (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_TYPE_SEARCH_PROVIDER, SeahorseSearchProvider))
#define SEAHORSE_SEARCH_PROVIDER_CLASS(klass)           (G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_SEARCH_PROVIDER, SeahorseSearchProviderClass))
#define SEAHORSE_IS_SEARCH_PROVIDER(obj)                (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_TYPE_SEARCH_PROVIDER))
#define SEAHORSE_IS_SEARCH_PROVIDER_CLASS(klass)        (G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_SEARCH_PROVIDER))
#define SEAHORSE_SEARCH_PROVIDER_GET_CLASS(obj)         (G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_TYPE_SEARCH_PROVIDER, SeahorseSearchProviderClass))

typedef struct _SeahorseSearchProvider SeahorseSearchProvider;
typedef struct _SeahorseSearchProviderClass SeahorseSearchProviderClass;

GType                    seahorse_search_provider_get_type           (void);

SeahorseSearchProvider * seahorse_search_provider_new                (void);

gboolean                 seahorse_search_provider_dbus_register      (SeahorseSearchProvider  *provider,
                                                                      GDBusConnection         *connection,
                                                                      const char              *object_path,
                                                                      GError                 **error);
void                     seahorse_search_provider_dbus_unregister    (SeahorseSearchProvider  *provider,
                                                                      GDBusConnection         *connection,
                                                                      const char              *object_path);

void                     seahorse_search_provider_initialize         (SeahorseSearchProvider  *provider);

#endif /* __SEAHORSE_SEARCH_PROVIDER_H__ */
