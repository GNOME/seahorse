/*
 * Seahorse
 *
 * Copyright (C) 2003 Jacob Perkins
 * Copyright (C) 2004-2005 Stefan Walter
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2 of the licence or (at
 * your option) any later version.
 *
 * See the included COPYING file for more information.
 */

/*
 * Various UI elements and dialogs used in pgp component.
 */
 
#ifndef __SEAHORSE_PGP_DIALOGS_H__
#define __SEAHORSE_PGP_DIALOGS_H__

#include <gtk/gtk.h>

#include "pgp/seahorse-pgp-key.h"

SeahorsePgpKey* seahorse_signer_get                 (GtkWindow *parent);

GtkWindow *     seahorse_pgp_key_properties_show    (SeahorsePgpKey *pkey,
                                                     GtkWindow *parent);

#endif /* __SEAHORSE_PGP_DIALOGS_H__ */
