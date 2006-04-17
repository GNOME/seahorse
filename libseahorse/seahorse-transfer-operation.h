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
 * SeahorseTransferOperation: Transfer a set of keys from one key source
 * to another. 
 * 
 * - Derived from SeahorseOperation
 * - Exports all keys from 'from' source key source. 
 * - Import them into the 'to' key source.
 *
 * Properties:
 *  from-key-source: (SeahorseKeySource) From key source 
 *  to-key-source: (SeahorseKeySource) To key source
 */
 
#ifndef __SEAHORSE_TRANSFER_OPERATION_H__
#define __SEAHORSE_TRANSFER_OPERATION_H__

#include "seahorse-operation.h"
#include "seahorse-key-source.h"

#define SEAHORSE_TYPE_TRANSFER_OPERATION            (seahorse_transfer_operation_get_type ())
#define SEAHORSE_TRANSFER_OPERATION(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_TYPE_TRANSFER_OPERATION, SeahorseTransferOperation))
#define SEAHORSE_TRANSFER_OPERATION_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_TRANSFER_OPERATION, SeahorseTransferOperationClass))
#define SEAHORSE_IS_TRANSFER_OPERATION(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_TYPE_TRANSFER_OPERATION))
#define SEAHORSE_IS_TRANSFER_OPERATION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_TRANSFER_OPERATION))
#define SEAHORSE_TRANSFER_OPERATION_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_TYPE_TRANSFER_OPERATION, SeahorseTransferOperationClass))

DECLARE_OPERATION (Transfer, transfer)
    /*< public >*/
    SeahorseKeySource *from;
    SeahorseKeySource *to;
END_DECLARE_OPERATION

SeahorseOperation*      seahorse_transfer_operation_new     (const gchar *message,
                                                             SeahorseKeySource *from,
                                                             SeahorseKeySource *to,
                                                             GSList *keyids);

#endif /* __SEAHORSE_TRANSFER_OPERATION_H__ */
