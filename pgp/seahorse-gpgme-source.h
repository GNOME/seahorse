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
 * along with this program; if not, write to the
 * Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/** 
 * SeahorseGpgmeSource: A key source for PGP keys retrieved from GPGME. 
 * 
 * - Derived from SeahorseSource
 * - Since GPGME represents secret keys as seperate from public keys, this
 *   class takes care to combine them into one logical SeahorsePGPKey object. 
 * - Adds the keys it loads to the SeahorseContext.
 * - Eventually a lot of stuff from seahorse-op.* should probably be merged
 *   into this class.
 * - Monitors ~/.gnupg for changes and reloads the key ring as necessary.
 * 
 * Properties:
 *  ktype: (GQuark) The ktype (ie: SEAHORSE_PGP) of keys originating from this 
           key source.
 *  location: (SeahorseLocation) The location of keys that come from this 
 *         source. (ie: SEAHORSE_LOCATION_LOCAL, SEAHORSE_LOCATION_REMOTE)
 */
 
#ifndef __SEAHORSE_GPGME_SOURCE_H__
#define __SEAHORSE_GPGME_SOURCE_H__

#include "seahorse-source.h"

#include <gpgme.h>

#define SEAHORSE_TYPE_GPGME_SOURCE            (seahorse_gpgme_source_get_type ())
#define SEAHORSE_GPGME_SOURCE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_TYPE_GPGME_SOURCE, SeahorseGpgmeSource))
#define SEAHORSE_GPGME_SOURCE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_GPGME_SOURCE, SeahorseGpgmeSourceClass))
#define SEAHORSE_IS_GPGME_SOURCE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_TYPE_GPGME_SOURCE))
#define SEAHORSE_IS_GPGME_SOURCE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_GPGME_SOURCE))
#define SEAHORSE_GPGME_SOURCE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_TYPE_GPGME_SOURCE, SeahorseGpgmeSourceClass))

typedef struct _SeahorseGpgmeSource SeahorseGpgmeSource;
typedef struct _SeahorseGpgmeSourceClass SeahorseGpgmeSourceClass;
typedef struct _SeahorseGpgmeSourcePrivate SeahorseGpgmeSourcePrivate;

struct _SeahorseGpgmeSource {
	GObject parent;
	gpgme_ctx_t gctx;
	SeahorseGpgmeSourcePrivate *pv;
};

struct _SeahorseGpgmeSourceClass {
	GObjectClass parent_class;
};

GType                seahorse_gpgme_source_get_type       (void);

SeahorseGpgmeSource*   seahorse_gpgme_source_new            (void);

gpgme_ctx_t          seahorse_gpgme_source_new_context    (void);

#endif /* __SEAHORSE_GPGME_SOURCE_H__ */
