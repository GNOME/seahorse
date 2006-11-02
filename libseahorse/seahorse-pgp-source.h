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
 * SeahorsePGPSource: A key source for PGP keys retrieved from GPGME. 
 * 
 * - Derived from SeahorseKeySource
 * - Since GPGME represents secret keys as seperate from public keys, this
 *   class takes care to combine them into one logical SeahorsePGPKey object. 
 * - Adds the keys it loads to the SeahorseContext.
 * - Eventually a lot of stuff from seahorse-op.* should probably be merged
 *   into this class.
 * - Monitors ~/.gnupg for changes and reloads the keyring as necessary.
 * 
 * Properties:
 *  ktype: (GQuark) The ktype (ie: SKEY_PGP) of keys originating from this 
           key source.
 *  location: (SeahorseKeyLoc) The location of keys that come from this 
 *         source. (ie: SKEY_LOC_LOCAL, SKEY_LOC_REMOTE)
 */
 
#ifndef __SEAHORSE_PGP_SOURCE_H__
#define __SEAHORSE_PGP_SOURCE_H__

#include "seahorse-key-source.h"

#define SEAHORSE_TYPE_PGP_SOURCE            (seahorse_pgp_source_get_type ())
#define SEAHORSE_PGP_SOURCE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_TYPE_PGP_SOURCE, SeahorsePGPSource))
#define SEAHORSE_PGP_SOURCE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_PGP_SOURCE, SeahorsePGPSourceClass))
#define SEAHORSE_IS_PGP_SOURCE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_TYPE_PGP_SOURCE))
#define SEAHORSE_IS_PGP_SOURCE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_PGP_SOURCE))
#define SEAHORSE_PGP_SOURCE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_TYPE_PGP_SOURCE, SeahorsePGPSourceClass))

typedef struct _SeahorsePGPSource SeahorsePGPSource;
typedef struct _SeahorsePGPSourceClass SeahorsePGPSourceClass;
typedef struct _SeahorsePGPSourcePrivate SeahorsePGPSourcePrivate;

struct _SeahorsePGPSource {
    SeahorseKeySource parent;
    
    /*< public >*/
    gpgme_ctx_t gctx;
    
    /*< private >*/
    SeahorsePGPSourcePrivate *pv;
};

struct _SeahorsePGPSourceClass {
    SeahorseKeySourceClass parent_class;
};

GType                seahorse_pgp_source_get_type       (void);

SeahorsePGPSource*   seahorse_pgp_source_new            (void);

gpgme_ctx_t          seahorse_pgp_source_new_context    ();

#endif /* __SEAHORSE_PGP_SOURCE_H__ */
