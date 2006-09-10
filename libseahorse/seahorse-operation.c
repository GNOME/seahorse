/*
 * Seahorse
 *
 * Copyright (C) 2004-2006 Nate Nielsen
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
 
#include <gnome.h>

#include "config.h"
#include "seahorse-util.h"
#include "seahorse-marshal.h"
#include "seahorse-operation.h"

/* Override the DEBUG_OPERATION_ENABLE switch here */
/* #define DEBUG_OPERATION_ENABLE 0 */

#ifndef DEBUG_OPERATION_ENABLE
#if _DEBUG
#define DEBUG_OPERATION_ENABLE 1
#else
#define DEBUG_OPERATION_ENABLE 0
#endif
#endif

#if DEBUG_OPERATION_ENABLE
#define DEBUG_OPERATION(x) g_printerr x
#else
#define DEBUG_OPERATION(x) 
#endif

/* -----------------------------------------------------------------------------
 * SEAHORSE OPERATION
 */

enum {
    PROP_0,
    PROP_PROGRESS,
    PROP_MESSAGE,
    PROP_RESULT
};

enum {
    DONE,
    PROGRESS,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (SeahorseOperation, seahorse_operation, G_TYPE_OBJECT);

/* OBJECT ------------------------------------------------------------------- */

static void
seahorse_operation_init (SeahorseOperation *operation)
{
    /* This is the state of non-started operation. all other flags are zero */
    operation->progress = -1;
}

static void
seahorse_operation_set_property (GObject *object, guint prop_id, 
                                 const GValue *value, GParamSpec *pspec)
{
    SeahorseOperation *op = SEAHORSE_OPERATION (object);
    
    switch (prop_id) {
    case PROP_MESSAGE:
        g_free (op->message);
        op->message = g_value_dup_string (value);
        break;
    }
}

static void
seahorse_operation_get_property (GObject *object, guint prop_id, 
                                 GValue *value, GParamSpec *pspec)
{
    SeahorseOperation *op = SEAHORSE_OPERATION (object);
    
    switch (prop_id) {
    case PROP_PROGRESS:
        g_value_set_double (value, op->progress);
        break;
    case PROP_MESSAGE:
        g_value_set_string (value, op->message);
        break;
    case PROP_RESULT:
        g_value_set_pointer (value, op->result);
        break;
    }
}

/* dispose of all our internal references */
static void
seahorse_operation_dispose (GObject *gobject)
{
    SeahorseOperation *op = SEAHORSE_OPERATION (gobject);
    
    if (op->is_running)
        seahorse_operation_cancel (op);
    g_assert (!op->is_running);

    /* We do this in dispose in case it's a circular reference */
    if (op->result && op->result_destroy)
        (op->result_destroy) (op->result);
    op->result = NULL;
    op->result_destroy = NULL;
    
    G_OBJECT_CLASS (seahorse_operation_parent_class)->dispose (gobject);
}

/* free private vars */
static void
seahorse_operation_finalize (GObject *gobject)
{
    SeahorseOperation *op = SEAHORSE_OPERATION (gobject);
    g_assert (!op->is_running);
    
    if (op->error) {
        g_error_free (op->error);
        op->error = NULL;
    }
        
    g_free (op->message);
    op->message = NULL;
        
    G_OBJECT_CLASS (seahorse_operation_parent_class)->finalize (gobject);
}

static void
seahorse_operation_class_init (SeahorseOperationClass *klass)
{
    GObjectClass *gobject_class;
   
    seahorse_operation_parent_class = g_type_class_peek_parent (klass);
    gobject_class = G_OBJECT_CLASS (klass);
    
    gobject_class->dispose = seahorse_operation_dispose;
    gobject_class->finalize = seahorse_operation_finalize;
    gobject_class->set_property = seahorse_operation_set_property;
    gobject_class->get_property = seahorse_operation_get_property;

    g_object_class_install_property (gobject_class, PROP_PROGRESS,
        g_param_spec_double ("progress", "Progress position", "Current progress position (fraction between 0 and 1)", 
                             0.0, 1.0, 0.0, G_PARAM_READABLE));

    g_object_class_install_property (gobject_class, PROP_MESSAGE,
        g_param_spec_string ("message", "Status message", "Current progress message", 
                             NULL, G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class, PROP_RESULT,
        g_param_spec_pointer ("result", "Operation Result", "Exact value depends on derived class.", 
                              G_PARAM_READABLE));

    
    signals[DONE] = g_signal_new ("done", SEAHORSE_TYPE_OPERATION, 
                G_SIGNAL_RUN_LAST, G_STRUCT_OFFSET (SeahorseOperationClass, done),
                NULL, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

    signals[PROGRESS] = g_signal_new ("progress", SEAHORSE_TYPE_OPERATION, 
                G_SIGNAL_RUN_LAST, G_STRUCT_OFFSET (SeahorseOperationClass, progress),
                NULL, NULL, seahorse_marshal_VOID__STRING_DOUBLE, G_TYPE_NONE, 2, G_TYPE_STRING, G_TYPE_DOUBLE);
}

/* PUBLIC ------------------------------------------------------------------- */

SeahorseOperation*  
seahorse_operation_new_complete (GError *err)
{
    SeahorseOperation *operation;
    
    operation = g_object_new (SEAHORSE_TYPE_OPERATION, NULL);
    seahorse_operation_mark_start (operation);
    seahorse_operation_mark_done (operation, FALSE, err);
    return operation;
}

SeahorseOperation*
seahorse_operation_new_cancelled ()
{
    SeahorseOperation *operation;
    
    operation = g_object_new (SEAHORSE_TYPE_OPERATION, NULL);
    seahorse_operation_mark_start (operation);
    seahorse_operation_mark_done (operation, TRUE, NULL);
    return operation;
}

void
seahorse_operation_cancel (SeahorseOperation *op)
{
    SeahorseOperationClass *klass;

    g_return_if_fail (SEAHORSE_IS_OPERATION (op));
    g_return_if_fail (op->is_running);

    g_object_ref (op);
 
    klass = SEAHORSE_OPERATION_GET_CLASS (op);
    
    /* A derived operation exists */
    if (klass->cancel)
        (*klass->cancel) (op); 
    
    /* No derived operation exists */
    else
        seahorse_operation_mark_done (op, TRUE, NULL);
    
    g_object_unref (op);
}

void
seahorse_operation_copy_error  (SeahorseOperation *op, GError **err)
{
    g_return_if_fail (err == NULL || *err == NULL);
    if (err) 
        *err = op->error ? g_error_copy (op->error) : NULL;
}

const GError*       
seahorse_operation_get_error (SeahorseOperation *op)
{
    return op->error;
}

gpointer
seahorse_operation_get_result (SeahorseOperation *op)
{
    return op->result;
}

void                
seahorse_operation_wait (SeahorseOperation *op)
{
    seahorse_util_wait_until (!seahorse_operation_is_running (op));
}

void
seahorse_operation_watch (SeahorseOperation *operation, GCallback done_callback,
                          GCallback progress_callback, gpointer userdata)
{
    typedef void (*DoneCb) (SeahorseOperation*, gpointer);
    typedef void (*ProgressCb) (SeahorseOperation*, const gchar*, gdouble, gpointer);

    if (!seahorse_operation_is_running (operation)) {
        if (done_callback)
            ((DoneCb)done_callback) (operation, userdata);
        return;
    }
    
    if (done_callback)
        g_signal_connect (operation, "done", done_callback, userdata);
    
    if (progress_callback) {
        ((ProgressCb)progress_callback) (operation, 
                seahorse_operation_get_message (operation),
                seahorse_operation_get_progress (operation), userdata);
        g_signal_connect (operation, "progress", progress_callback, userdata);
    }
}

/* METHODS FOR DERIVED CLASSES ---------------------------------------------- */

void
seahorse_operation_mark_start (SeahorseOperation *op)
{
    g_return_if_fail (SEAHORSE_IS_OPERATION (op));
        
    /* A running operation always refs itself */
    g_object_ref (op);
    op->is_running = TRUE;
    op->is_done = FALSE;
    op->is_cancelled = FALSE;
    op->progress = -1;
    
    g_free (op->message);
    op->message = NULL;
}

void 
seahorse_operation_mark_progress (SeahorseOperation *op, const gchar *message, 
                                  gdouble progress)
{
    gboolean emit = FALSE;
    
    g_return_if_fail (SEAHORSE_IS_OPERATION (op));
    g_return_if_fail (op->is_running);

    if (progress != op->progress) {
        op->progress = progress;
        emit = TRUE;
    }

    if (!seahorse_util_string_equals (op->message, message)) {
        g_free (op->message);
        op->message = message ? g_strdup (message) : NULL;
        emit = TRUE;
    }

    if (emit)    
        g_signal_emit (G_OBJECT (op), signals[PROGRESS], 0, op->message, progress);
}

void
seahorse_operation_mark_progress_full (SeahorseOperation *op, const gchar *message,
                                       gdouble current, gdouble total)
{
    if (current > total)
        current = total;
    seahorse_operation_mark_progress (op, message, total <= 0 ? -1 : current / total);
}


void                
seahorse_operation_mark_done (SeahorseOperation *op, gboolean cancelled,
                              GError *error)
{
    g_return_if_fail (SEAHORSE_IS_OPERATION (op));
    g_return_if_fail (op->is_running);

    /* No message on completed operations */
    g_free (op->message);
    op->message = NULL;
    
    op->progress = cancelled ? -1 : 1.0;
    op->is_running = FALSE;
    op->is_done = TRUE;
    op->is_cancelled = cancelled;
    op->error = error;

    g_signal_emit (op, signals[PROGRESS], 0, NULL, op->progress);
    g_signal_emit (op, signals[DONE], 0);

    /* A running operation always refs itself */
    g_object_unref (op);
}

void
seahorse_operation_mark_result (SeahorseOperation *op, gpointer result,
                                GDestroyNotify destroy_func)
{
    if (op->result && op->result_destroy)
        (op->result_destroy) (op->result);
    op->result = result;
    op->result_destroy = destroy_func;
}

/* -----------------------------------------------------------------------------
 * MULTI OPERATION
 */

/* GObject handlers */
static void seahorse_multi_operation_class_init (SeahorseMultiOperationClass *klass);
static void seahorse_multi_operation_init       (SeahorseMultiOperation *mop);
static void seahorse_multi_operation_dispose    (GObject *gobject);
static void seahorse_multi_operation_finalize   (GObject *gobject);
static void seahorse_multi_operation_cancel     (SeahorseOperation *operation);

G_DEFINE_TYPE (SeahorseMultiOperation, seahorse_multi_operation, SEAHORSE_TYPE_OPERATION);

/* HELPERS ------------------------------------------------------------------ */

static void
multi_operation_progress (SeahorseOperation *operation, const gchar *message, 
                          gdouble fract, SeahorseMultiOperation *mop)
{
    GSList *list;
    
    g_assert (SEAHORSE_IS_MULTI_OPERATION (mop));
    g_assert (SEAHORSE_IS_OPERATION (operation));
    
    if (seahorse_operation_is_cancelled (SEAHORSE_OPERATION (mop)))
        return;
    
    list = mop->operations;
    
    /* One or two operations, simple */
    if (g_slist_length (list) <= 1) {

        DEBUG_OPERATION (("[multi-operation 0x%08X] single progress: %s %lf\n", (guint)mop, 
            seahorse_operation_get_message (operation), operation->progress));
        
        seahorse_operation_mark_progress (SEAHORSE_OPERATION (mop), 
                            seahorse_operation_get_message (operation), 
                            operation->progress);


    /* When more than one operation calculate the fraction */
    } else {
    
        SeahorseOperation *op;
        gdouble total = 0;
        gdouble current = 0;
        gdouble progress;
        
        message = seahorse_operation_get_message (operation);

        /* Get the total progress */
        while (list) {
            op = SEAHORSE_OPERATION (list->data);
            list = g_slist_next (list);

            if (message && !message[0])
                message = NULL;
            
            if (!message && seahorse_operation_is_running (op))
                message = seahorse_operation_get_message (op);
            
            if (!seahorse_operation_is_cancelled (op)) {
                total += 1;
                if (op->progress < 0)
                    current += (seahorse_operation_is_running (op) ? 0 : 1);
                else
                    current += op->progress;
                DEBUG_OPERATION (("    progres is: %lf\n", op->progress));
            }
        } 
        
        progress = total ? current / total : -1;

        DEBUG_OPERATION (("[multi-operation 0x%08X] multi progress: %s %lf\n", (guint)mop, 
                          message, progress));
        
        seahorse_operation_mark_progress (SEAHORSE_OPERATION (mop), message, progress);
    }
}

static void
multi_operation_done (SeahorseOperation *op, SeahorseMultiOperation *mop)
{
    GSList *l;
    gboolean done = TRUE;
    
    g_assert (SEAHORSE_IS_MULTI_OPERATION (mop));
    g_assert (SEAHORSE_IS_OPERATION (op));  
  
    g_signal_handlers_disconnect_by_func (op, multi_operation_done, mop);
    g_signal_handlers_disconnect_by_func (op, multi_operation_progress, mop);
    
    if (!seahorse_operation_is_successful (op) && !SEAHORSE_OPERATION (mop)->error)
        seahorse_operation_copy_error (op, &(SEAHORSE_OPERATION (mop)->error));

    DEBUG_OPERATION (("[multi-operation 0x%08X] part complete (%d): 0x%08X/%s\n", (guint)mop, 
                g_slist_length (mop->operations), (guint)op, seahorse_operation_get_message (op)));
        
    /* Are we done with all of them? */
    for (l = mop->operations; l; l = g_slist_next (l)) {
        if (seahorse_operation_is_running (SEAHORSE_OPERATION (l->data))) 
            done = FALSE;
    }
    
    /* Not done, just update progress. */
    if (!done) {
        multi_operation_progress (SEAHORSE_OPERATION (mop), NULL, -1, mop);
        return;
    }

    DEBUG_OPERATION (("[multi-operation 0x%08X] complete\n", (guint)mop));

    /* Remove all the listeners */
    for (l = mop->operations; l; l = g_slist_next (l)) {
        g_signal_handlers_disconnect_by_func (l->data, multi_operation_done, mop);
        g_signal_handlers_disconnect_by_func (l->data, multi_operation_progress, mop);
    }

    mop->operations = seahorse_operation_list_purge (mop->operations);
    
    if (!seahorse_operation_is_cancelled (SEAHORSE_OPERATION (mop))) 
        seahorse_operation_mark_done (SEAHORSE_OPERATION (mop), FALSE, 
                                      SEAHORSE_OPERATION (mop)->error);
}

/* OBJECT ------------------------------------------------------------------- */

static void
seahorse_multi_operation_init (SeahorseMultiOperation *mop)
{
    mop->operations = NULL;
}

/* dispose of all our internal references */
static void
seahorse_multi_operation_dispose (GObject *gobject)
{
    SeahorseMultiOperation *mop;
    GSList *l;
    
    mop = SEAHORSE_MULTI_OPERATION (gobject);
    
    /* Remove all the listeners */
    for (l = mop->operations; l; l = g_slist_next (l)) {
        g_signal_handlers_disconnect_by_func (l->data, multi_operation_done, mop);
        g_signal_handlers_disconnect_by_func (l->data, multi_operation_progress, mop);
    }
    
    /* Anything remaining, gets released  */
    mop->operations = seahorse_operation_list_free (mop->operations);
        
    G_OBJECT_CLASS (seahorse_multi_operation_parent_class)->dispose (gobject);
}

/* free private vars */
static void
seahorse_multi_operation_finalize (GObject *gobject)
{
    SeahorseMultiOperation *mop;
    mop = SEAHORSE_MULTI_OPERATION (gobject);
    
    g_assert (mop->operations == NULL);
    
    G_OBJECT_CLASS (seahorse_multi_operation_parent_class)->finalize (gobject);
}

static void 
seahorse_multi_operation_cancel (SeahorseOperation *operation)
{
    SeahorseMultiOperation *mop;
    
    g_assert (SEAHORSE_IS_MULTI_OPERATION (operation));
    mop = SEAHORSE_MULTI_OPERATION (operation);

    seahorse_operation_mark_done (operation, TRUE, NULL);

    seahorse_operation_list_cancel (mop->operations);
    mop->operations = seahorse_operation_list_purge (mop->operations);
}

static void
seahorse_multi_operation_class_init (SeahorseMultiOperationClass *klass)
{
    SeahorseOperationClass *op_class = SEAHORSE_OPERATION_CLASS (klass);
    GObjectClass *gobject_class;

    seahorse_multi_operation_parent_class = g_type_class_peek_parent (klass);
    gobject_class = G_OBJECT_CLASS (klass);
    
    op_class->cancel = seahorse_multi_operation_cancel;        
    gobject_class->dispose = seahorse_multi_operation_dispose;
    gobject_class->finalize = seahorse_multi_operation_finalize;
}

/* PUBLIC ------------------------------------------------------------------- */

SeahorseMultiOperation*  
seahorse_multi_operation_new ()
{
    SeahorseMultiOperation *mop = g_object_new (SEAHORSE_TYPE_MULTI_OPERATION, NULL); 
    return mop;
}

void
seahorse_multi_operation_take (SeahorseMultiOperation* mop, SeahorseOperation *op)
{
    g_return_if_fail (SEAHORSE_IS_MULTI_OPERATION (mop));
    g_return_if_fail (SEAHORSE_IS_OPERATION (op));
    
    /* We should never add an operation to itself */
    g_return_if_fail (SEAHORSE_OPERATION (op) != SEAHORSE_OPERATION (mop));
    
    if(mop->operations == NULL) {
        DEBUG_OPERATION (("[multi-operation 0x%08X] start\n", (guint)mop));
        seahorse_operation_mark_start (SEAHORSE_OPERATION (mop));
    }
    
    DEBUG_OPERATION (("[multi-operation 0x%08X] adding part: 0x%08X\n", (guint)mop, (guint)op));
    
    mop->operations = seahorse_operation_list_add (mop->operations, op);
    seahorse_operation_watch (op, G_CALLBACK (multi_operation_done), 
                              G_CALLBACK (multi_operation_progress), mop);
}

/* -----------------------------------------------------------------------------
 * OPERATION LIST FUNCTIONS
 */

GSList*
seahorse_operation_list_add (GSList *list, SeahorseOperation *operation)
{
    /* This assumes the main reference */
    return g_slist_prepend (list, operation);
}

GSList*             
seahorse_operation_list_remove (GSList *list, SeahorseOperation *operation)
{
   GSList *element;
   
   element = g_slist_find (list, operation);
   if (element) {
        g_object_unref (operation);
        list = g_slist_remove_link (list, element);
        g_slist_free (element);
   } 
   
   return list;
}

void             
seahorse_operation_list_cancel (GSList *list)
{
    SeahorseOperation *operation;
    
    while (list) {
        operation = SEAHORSE_OPERATION (list->data);
        list = g_slist_next (list);
        
        if (seahorse_operation_is_running (operation))
            seahorse_operation_cancel (operation);
    }
}

GSList*             
seahorse_operation_list_purge (GSList *list)
{
    GSList *l, *p;
    
    p = list;
    
    while (p != NULL) {

        l = p;
        p = g_slist_next (p);
        
        if (!seahorse_operation_is_running (SEAHORSE_OPERATION (l->data))) {
            g_object_unref (G_OBJECT (l->data));

            list = g_slist_remove_link (list, l);
            g_slist_free (l);
        }         
    }
    
    return list;
}

GSList*
seahorse_operation_list_free (GSList *list)
{
    GSList *l;
    
    for (l = list; l; l = g_slist_next (l)) {
        g_assert (SEAHORSE_IS_OPERATION (l->data));
        g_object_unref (G_OBJECT (l->data));
    }
    
    g_slist_free (list);
    return NULL;
}
