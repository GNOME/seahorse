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

#ifndef __SEAHORSE_OPERATION_H__
#define __SEAHORSE_OPERATION_H__

#include <gnome.h>

#define SEAHORSE_TYPE_OPERATION            (seahorse_operation_get_type ())
#define SEAHORSE_OPERATION(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_TYPE_OPERATION, SeahorseOperation))
#define SEAHORSE_OPERATION_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_OPERATION, SeahorseOperationClass))
#define SEAHORSE_IS_OPERATION(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_TYPE_OPERATION))
#define SEAHORSE_IS_OPERATION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_OPERATION))
#define SEAHORSE_OPERATION_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_TYPE_OPERATION, SeahorseOperationClass))

struct _SeahorseOperation;

typedef void (*SeahorseOperationDone) (struct _SeahorseOperation *operation, 
                                       gboolean cancelled, 
                                       gpointer userdata);

typedef struct _SeahorseOperation {
    GObject parent;
    
    /*< public >*/
    gboolean done;
    
    /*< private >*/
    SeahorseOperationDone donefunc;
    gpointer userdata;

} SeahorseOperation;

typedef struct _SeahorseOperationClass {
    GtkObjectClass parent_class;

    /* Signal that occurs when the operation is complete */
    void (*done) (SeahorseOperation *operation);
    
} SeahorseOperationClass;

GType       seahorse_operation_get_type      (void);

/* Methods ------------------------------------------------------------ */

SeahorseOperation*  seahorse_operation_new        (SeahorseOperationDone done, 
                                                   gpointer userdata);

void                seahorse_operation_cancel     (SeahorseOperation *operation);

#define             seahorse_operation_is_done(operation) \
                                                  ((operation)->done) 

#define             seahorse_operation_get_data(type, operation) \
                                                  ((type*)((operation)->userdata))

void                seahorse_operation_done       (SeahorseOperation *operation);

/* Helpers for Multiple Operations ----------------------------------- */

GSList*             seahorse_operation_list_add    (GSList *list,
                                                    SeahorseOperation *operation);

GSList*             seahorse_operation_list_remove (GSList *list,
                                                    SeahorseOperation *operation);

void                seahorse_operation_list_cancel (GSList *list);

GSList*             seahorse_operation_list_purge  (GSList *list);
                                                    
GSList*             seahorse_operation_list_free   (GSList *list);
                                                                                                       
#endif /* __SEAHORSE_OPERATION_H__ */
