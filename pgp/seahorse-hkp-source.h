/*
 * Seahorse
 *
 * Copyright (C) 2005 Stefan Walter
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2 of the licence or (at
 * your option) any later version.
 *
 * See the included COPYING file for more information.
 */

/**
 * SeahorseHKPSource: A key source which searches HTTP PGP key servers. 
 * 
 * - Derived from SeahorseServerSource.
 * - Adds found keys to SeahorseContext. 
 * - Used by SeahorseServiceDiscovery for retrieving shared keys.
 */
 
#ifndef __SEAHORSE_HKP_SOURCE_H__
#define __SEAHORSE_HKP_SOURCE_H__

#include "config.h"
#include "seahorse-server-source.h"

#ifdef WITH_HKP

#define SEAHORSE_TYPE_HKP_SOURCE            (seahorse_hkp_source_get_type ())
#define SEAHORSE_HKP_SOURCE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_TYPE_HKP_SOURCE, SeahorseHKPSource))
#define SEAHORSE_HKP_SOURCE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_HKP_SOURCE, SeahorseHKPSourceClass))
#define SEAHORSE_IS_HKP_SOURCE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_TYPE_HKP_SOURCE))
#define SEAHORSE_IS_HKP_SOURCE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_HKP_SOURCE))
#define SEAHORSE_HKP_SOURCE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_TYPE_HKP_SOURCE, SeahorseHKPSourceClass))

typedef struct _SeahorseHKPSource SeahorseHKPSource;
typedef struct _SeahorseHKPSourceClass SeahorseHKPSourceClass;

struct _SeahorseHKPSource {
    SeahorseServerSource parent;
    
    /*< private >*/
};

struct _SeahorseHKPSourceClass {
    SeahorseServerSourceClass parent_class;
};

GType                 seahorse_hkp_source_get_type (void);

SeahorseHKPSource*    seahorse_hkp_source_new      (const gchar *uri, 
                                                    const gchar *host);

gboolean              seahorse_hkp_is_valid_uri    (const gchar *uri);

#endif /* WITH_HKP */

#endif /* __SEAHORSE_HKP_SOURCE_H__ */
