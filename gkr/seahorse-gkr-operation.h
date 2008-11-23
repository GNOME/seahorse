/*
 * Seahorse
 *
 * Copyright (C) 2006 Stefan Walter
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
 * SeahorseGkrOperation: Operations to be done on gnome-keyring items
 * 
 * - Derived from SeahorseOperation
 *
 * Properties:
 *  TODO: 
 */
 
#ifndef __SEAHORSE_GKR_OPERATION_H__
#define __SEAHORSE_GKR_OPERATION_H__

#include "seahorse-operation.h"
#include "seahorse-gkr-source.h"
#include "seahorse-gkr-item.h"

#define SEAHORSE_TYPE_GKR_OPERATION            (seahorse_gkr_operation_get_type ())
#define SEAHORSE_GKR_OPERATION(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_TYPE_GKR_OPERATION, SeahorseGkrOperation))
#define SEAHORSE_GKR_OPERATION_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_GKR_OPERATION, SeahorseGkrOperationClass))
#define SEAHORSE_IS_GKR_OPERATION(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_TYPE_GKR_OPERATION))
#define SEAHORSE_IS_GKR_OPERATION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_GKR_OPERATION))
#define SEAHORSE_GKR_OPERATION_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_TYPE_GKR_OPERATION, SeahorseGkrOperationClass))

DECLARE_OPERATION (Gkr, gkr)

    /*< public >*/
    SeahorseGkrSource *gsrc;
    
END_DECLARE_OPERATION

gboolean             seahorse_gkr_operation_parse_error       (GnomeKeyringResult result, 
                                                               GError **err);

/* result: nothing */
SeahorseOperation*   seahorse_gkr_operation_update_info       (SeahorseGkrItem *git,
                                                               GnomeKeyringItemInfo *info);

/* result: nothing */
SeahorseOperation*   seahorse_gkr_operation_update_acl        (SeahorseGkrItem *git,
                                                               GList *acl);

#endif /* __SEAHORSE_GKR_OPERATION_H__ */
