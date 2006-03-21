/*
 * Seahorse
 *
 * Copyright (C) 2006 Nate Nielsen
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
 * SeahorsePGPOperation: A way to track operations done via GPGME. 
 * 
 * - Derived from SeahorseOperation
 * - Uses the arcane gpgme_io_cbs API.
 * - Creates a new context so this doesn't block other operations.
 * 
 * Properties:
 *  gctx:  (pointer) GPGME context.
 */
 
#ifndef __SEAHORSE_PGP_OPERATION_H__
#define __SEAHORSE_PGP_OPERATION_H__

// TODO: Eventually most of the stuff from seahorse-pgp-key-op and 
// seahorse-op should get merged into here.

#include "seahorse-operation.h"

#define SEAHORSE_TYPE_PGP_OPERATION            (seahorse_pgp_operation_get_type ())
#define SEAHORSE_PGP_OPERATION(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_TYPE_PGP_OPERATION, SeahorsePGPOperation))
#define SEAHORSE_PGP_OPERATION_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_PGP_OPERATION, SeahorsePGPOperationClass))
#define SEAHORSE_IS_PGP_OPERATION(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_TYPE_PGP_OPERATION))
#define SEAHORSE_IS_PGP_OPERATION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_PGP_OPERATION))
#define SEAHORSE_PGP_OPERATION_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_TYPE_PGP_OPERATION, SeahorsePGPOperationClass))

DECLARE_OPERATION (PGP, pgp)
    /*< public >*/
    gpgme_ctx_t gctx;
END_DECLARE_OPERATION

SeahorsePGPOperation*   seahorse_pgp_operation_new       (const gchar *message);

#endif /* __SEAHORSE_PGP_OPERATION_H__ */
