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

} SeahorseOperation;

typedef struct _SeahorseOperationClass {
    GtkObjectClass parent_class;

    /* signals --------------------------------------------------------- */
    
    /* Signal that occurs when the operation is complete */
    void (*done) (SeahorseOperation *operation);

    /* virtual methods ------------------------------------------------- */

    /* Cancels a pending operation */
    void (*cancel) (SeahorseOperation *operation);
    
} SeahorseOperationClass;

GType       seahorse_operation_get_type      (void);

/* Methods ------------------------------------------------------------ */

void                seahorse_operation_cancel     (SeahorseOperation *operation);

#define             seahorse_operation_is_done(operation) \
                                                  ((operation)->done) 

/* Helpers for Derived ----------------------------------------------- */

void                seahorse_operation_mark_start (SeahorseOperation *operation);

void                seahorse_operation_mark_done  (SeahorseOperation *operation);

/* Helpers for Multiple Operations ----------------------------------- */

GSList*             seahorse_operation_list_add    (GSList *list,
                                                    SeahorseOperation *operation);

GSList*             seahorse_operation_list_remove (GSList *list,
                                                    SeahorseOperation *operation);

void                seahorse_operation_list_cancel (GSList *list);

GSList*             seahorse_operation_list_purge  (GSList *list);
                                                    
GSList*             seahorse_operation_list_free   (GSList *list);
                           
/* ----------------------------------------------------------------------------
 *  SUBCLASS DECLARATION MACROS 
 */
 
/* 
 * To declare a subclass you need to put in code like this:
 *
 
#define SEAHORSE_TYPE_XX_OPERATION            (seahorse_xx_operation_get_type ())
#define SEAHORSE_XX_OPERATION(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_TYPE_XX_OPERATION, SeahorseXxOperation))
#define SEAHORSE_XX_OPERATION_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_XX_OPERATION, SeahorseXxOperationClass))
#define SEAHORSE_IS_XX_OPERATION(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_TYPE_XX_OPERATION))
#define SEAHORSE_IS_XX_OPERATION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_XX_OPERATION))
#define SEAHORSE_XX_OPERATION_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_TYPE_XX_OPERATION, SeahorseXxOperationClass))

DECLARE_OPERATION (Xx, xx)
   ... member vars here ...
END_DECLARE_OPERATION 

 *
 * And then in your implementation something like this 
 *
 
IMPLEMENT_OPERATION (Xx, xx)
 
static void 
seahorse_load_operation_init (SeahorseLoadOperation *lop)
{

}

static void 
seahorse_load_operation_dispose (GObject *gobject)
{
    G_OBJECT_CLASS (operation_parent_class)->dispose (gobject);  
}

static void 
seahorse_load_operation_finalize (GObject *gobject)
{
    G_OBJECT_CLASS (operation_parent_class)->finalize (gobject);  
}

static void 
seahorse_load_operation_cancel (SeahorseOperation *operation)
{
    seahorse_operation_mark_done (operation);
}
 
 *
 * 
 */
 
#define DECLARE_OPERATION(Opx, opx)                                            \
    typedef struct _Seahorse##Opx##Operation Seahorse##Opx##Operation;              \
    typedef struct _Seahorse##Opx##OperationClass Seahorse##Opx##OperationClass;    \
    GType          seahorse_##opx##_operation_get_type      (void);                 \
    struct _Seahorse##Opx##OperationClass {                                         \
        SeahorseOperationClass parent_class;                                        \
    };                                                                              \
    struct _Seahorse##Opx##Operation {                                              \
        SeahorseOperation parent;                                                   \

#define END_DECLARE_OPERATION                                                       \
    }; 

#define IMPLEMENT_OPERATION(Opx, opx)                                                      \
    static void seahorse_##opx##_operation_class_init (Seahorse##Opx##OperationClass *klass);   \
    static void seahorse_##opx##_operation_init       (Seahorse##Opx##Operation *lop);          \
    static void seahorse_##opx##_operation_dispose    (GObject *gobject);                       \
    static void seahorse_##opx##_operation_finalize   (GObject *gobject);                       \
    static void seahorse_##opx##_operation_cancel     (SeahorseOperation *operation);           \
    GType seahorse_##opx##_operation_get_type (void) {                                          \
        static GType operation_type = 0;                                                        \
        if (!operation_type) {                                                                  \
            static const GTypeInfo operation_info = {                                           \
                sizeof (Seahorse##Opx##OperationClass), NULL, NULL,                             \
                (GClassInitFunc) seahorse_##opx##_operation_class_init, NULL, NULL,             \
                sizeof (Seahorse##Opx##Operation), 0,                                           \
                (GInstanceInitFunc)seahorse_##opx##_operation_init                              \
            };                                                                                  \
            operation_type = g_type_register_static (SEAHORSE_TYPE_OPERATION,                   \
                                "Seahorse" #Opx "Operation", &operation_info, 0);               \
        }                                                                                       \
        return operation_type;                                                                  \
    }                                                                                           \
    static GObjectClass *operation_parent_class = NULL;                                         \
    static void seahorse_##opx##_operation_class_init (Seahorse##Opx##OperationClass *klass) {  \
        GObjectClass *gobject_class  = G_OBJECT_CLASS (klass);                                  \
        SeahorseOperationClass *op_class = SEAHORSE_OPERATION_CLASS (klass);                    \
        operation_parent_class = g_type_class_peek_parent (klass);                              \
        op_class->cancel = seahorse_##opx##_operation_cancel;                                   \
        gobject_class->dispose = seahorse_##opx##_operation_dispose;                            \
        gobject_class->finalize = seahorse_##opx##_operation_finalize;                          \
    }                                                                                           \
    
#endif /* __SEAHORSE_OPERATION_H__ */
