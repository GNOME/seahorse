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
 
#include "config.h"

#include <stdlib.h>

#include "seahorse-transfer-operation.h"
#include "seahorse-util.h"

#define DEBUG_FLAG SEAHORSE_DEBUG_OPERATION
#include "seahorse-debug.h"

/* -----------------------------------------------------------------------------
 * DEFINITIONS
 */
 
typedef struct _SeahorseTransferOperationPrivate {
    SeahorseOperation *operation;   /* The current operation in progress */
    gchar *message;                 /* A progress message to display */
    GMemoryOutputStream *output;    /* Hold onto the data */
    gboolean individually;          /* Individually export keys, not as one block */
} SeahorseTransferOperationPrivate;

enum {
    PROP_0,
    PROP_FROM_KEY_SOURCE,
    PROP_TO_KEY_SOURCE,
    PROP_MESSAGE
};

#define SEAHORSE_TRANSFER_OPERATION_GET_PRIVATE(obj)  \
    (G_TYPE_INSTANCE_GET_PRIVATE ((obj), SEAHORSE_TYPE_TRANSFER_OPERATION, SeahorseTransferOperationPrivate))

/* TODO: This is just nasty. Gotta get rid of these weird macros */
IMPLEMENT_OPERATION_PROPS(Transfer, transfer)

    g_object_class_install_property (gobject_class, PROP_FROM_KEY_SOURCE,
        g_param_spec_object ("from-key-source", "From key source", "Key source keys are being transferred from",
                             SEAHORSE_TYPE_SOURCE, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

    g_object_class_install_property (gobject_class, PROP_TO_KEY_SOURCE,
        g_param_spec_object ("to-key-source", "To key source", "Key source keys are being transferred to",
                             SEAHORSE_TYPE_SOURCE, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

    g_object_class_install_property (gobject_class, PROP_MESSAGE,
        g_param_spec_string ("message", "Progress Message", "Progress message that overrides whatever GPGME gives us", 
                             NULL, G_PARAM_READABLE | G_PARAM_WRITABLE));   

    g_type_class_add_private (gobject_class, sizeof (SeahorseTransferOperationPrivate));

END_IMPLEMENT_OPERATION_PROPS


/* -----------------------------------------------------------------------------
 * HELPERS
 */

static void
import_progress (SeahorseOperation *op, const gchar *message, 
                 gdouble fract, SeahorseTransferOperation *top)
{
    SeahorseTransferOperationPrivate *pv = SEAHORSE_TRANSFER_OPERATION_GET_PRIVATE (top);
    
    g_assert (op == pv->operation);

    seahorse_debug ("[transfer] import progress: %lf", fract);

    if (seahorse_operation_is_running (SEAHORSE_OPERATION (top))) {
        message = pv->message ? pv->message : message;
        fract = fract <= 0 ? 0.5 : (0.5 + (fract / 2.0));
        seahorse_operation_mark_progress (SEAHORSE_OPERATION (top), 
                                          message, fract);
    }
}

static void
import_done (SeahorseOperation *op, SeahorseTransferOperation *top)
{
    SeahorseTransferOperationPrivate *pv = SEAHORSE_TRANSFER_OPERATION_GET_PRIVATE (top);
    GError *err = NULL;
    
    g_assert (op == pv->operation);
    seahorse_debug ("[transfer] import done");
    
    /* A release guard */
    g_object_ref (top);
    
    if (seahorse_operation_is_running (SEAHORSE_OPERATION (top))) {
        if (seahorse_operation_is_cancelled (op)) {
            seahorse_operation_mark_done (SEAHORSE_OPERATION (top), TRUE, NULL);
        
        } else if (!seahorse_operation_is_successful (op)) {
            seahorse_operation_copy_error (op, &err);
            seahorse_operation_mark_done (SEAHORSE_OPERATION (top), FALSE, err);
        }
    }
    
    g_object_unref (pv->operation);
    pv->operation = NULL;

    if (seahorse_operation_is_running (SEAHORSE_OPERATION (top)))
        seahorse_operation_mark_done (SEAHORSE_OPERATION (top), FALSE, NULL);

    /* End release guard */
    g_object_unref (top);
}

static void
export_progress (SeahorseOperation *op, const gchar *message, 
                 gdouble fract, SeahorseTransferOperation *top)
{
    SeahorseTransferOperationPrivate *pv = SEAHORSE_TRANSFER_OPERATION_GET_PRIVATE (top);
    
    g_assert (op == pv->operation);
    seahorse_debug ("[transfer] export progress: %lf", fract);

    if (seahorse_operation_is_running (SEAHORSE_OPERATION (top)))
        seahorse_operation_mark_progress (SEAHORSE_OPERATION (top), 
                                          pv->message ? pv->message : message, 
                                          fract / 2);
}

static void
export_done (SeahorseOperation *op, SeahorseTransferOperation *top)
{
    SeahorseTransferOperationPrivate *pv = SEAHORSE_TRANSFER_OPERATION_GET_PRIVATE (top);
    GError *err = NULL;
    gboolean done = FALSE;
    GInputStream *input;
    gpointer data;
    gsize size;
    
    g_assert (op == pv->operation);
    seahorse_debug ("[transfer] export done");

    /* A release guard */
    g_object_ref (top);
    
    if (seahorse_operation_is_cancelled (op)) {
        seahorse_operation_mark_done (SEAHORSE_OPERATION (top), TRUE, NULL);
        done = TRUE;
        
    } else if (!seahorse_operation_is_successful (op)) {
        seahorse_operation_copy_error (op, &err);
        seahorse_operation_mark_done (SEAHORSE_OPERATION (top), FALSE, err);
        done = TRUE;
    }

    g_object_unref (pv->operation);
    pv->operation = NULL;
    
    /* End release guard */
    g_object_unref (top);

	if (done) {
		seahorse_debug ("[transfer] stopped after export");
		return;
	}

	/* Build an input stream for this */
	data = g_memory_output_stream_get_data (pv->output);
	size = g_memory_output_stream_get_data_size (pv->output);
	
	if(!size) {
		seahorse_operation_mark_done (SEAHORSE_OPERATION (top), FALSE, NULL);
		return;
	}

	seahorse_debug ("[transfer] starting import");

	g_return_if_fail (data);

	input = g_memory_input_stream_new_from_data (g_memdup (data, size), size, g_free);
	g_return_if_fail (input);

	/* This frees data */
	pv->operation = seahorse_source_import (top->to, input);
	g_return_if_fail (pv->operation != NULL);

	g_object_unref (input);

	/* And mark us as started */
	seahorse_operation_mark_start (SEAHORSE_OPERATION (top));
	seahorse_operation_mark_progress (SEAHORSE_OPERATION (top), pv->message ? pv->message : 
					  seahorse_operation_get_message (pv->operation), 0.5);

	seahorse_operation_watch (pv->operation, (SeahorseDoneFunc)import_done, top,
				  (SeahorseProgressFunc)import_progress, top);
}


static gboolean 
start_transfer (SeahorseTransferOperation *top)
{
    SeahorseTransferOperationPrivate *pv = SEAHORSE_TRANSFER_OPERATION_GET_PRIVATE (top);
    SeahorseSource *from;
    GList *keyids;

    g_assert (pv->operation == NULL);

    keyids = (GList*)g_object_get_data (G_OBJECT (top), "transfer-key-ids");
    g_object_get (top, "from-key-source", &from, NULL);
    g_assert (keyids && from);
    
    pv = SEAHORSE_TRANSFER_OPERATION_GET_PRIVATE (top);
    pv->operation = seahorse_source_export_raw (from, keyids, G_OUTPUT_STREAM (pv->output));
    g_return_val_if_fail (pv->operation != NULL, FALSE);
    
    seahorse_operation_watch (pv->operation, (SeahorseDoneFunc) export_done, top,
                              (SeahorseProgressFunc)export_progress, top);
    return FALSE;
};

/* -----------------------------------------------------------------------------
 * OBJECT 
 */

static void 
seahorse_transfer_operation_init (SeahorseTransferOperation *top)
{
	SeahorseTransferOperationPrivate *pv = SEAHORSE_TRANSFER_OPERATION_GET_PRIVATE (top);
	pv->output = G_MEMORY_OUTPUT_STREAM (g_memory_output_stream_new (NULL, 0, g_realloc, g_free));
}

static void 
seahorse_transfer_operation_set_property (GObject *gobject, guint prop_id, 
                                          const GValue *value, GParamSpec *pspec)
{
    SeahorseTransferOperation *top = SEAHORSE_TRANSFER_OPERATION (gobject);
    SeahorseTransferOperationPrivate *pv = SEAHORSE_TRANSFER_OPERATION_GET_PRIVATE (top);
    
    switch (prop_id) {
    case PROP_FROM_KEY_SOURCE:
        g_return_if_fail (top->from == NULL);
        top->from = g_value_get_object (value);
        g_object_ref (top->from);
        break;
    case PROP_TO_KEY_SOURCE:
        g_return_if_fail (top->to == NULL);
        top->to = g_value_get_object (value);
        g_object_ref (top->to);
        break;
    case PROP_MESSAGE:
        g_free (pv->message);
        pv->message = g_strdup (g_value_get_string (value));
        break;
    }
}

static void 
seahorse_transfer_operation_get_property (GObject *gobject, guint prop_id, 
                                          GValue *value, GParamSpec *pspec)
{
    SeahorseTransferOperation *top = SEAHORSE_TRANSFER_OPERATION (gobject);
    SeahorseTransferOperationPrivate *pv = SEAHORSE_TRANSFER_OPERATION_GET_PRIVATE (top);
    
    switch (prop_id) {
    case PROP_FROM_KEY_SOURCE:
        g_value_set_object (value, top->from);
        break;
    case PROP_TO_KEY_SOURCE:
        g_value_set_object (value, top->to);
        break;
    case PROP_MESSAGE:
        g_value_set_string (value, pv->message);
        break;
    }
}

static void 
seahorse_transfer_operation_dispose (GObject *gobject)
{
    SeahorseTransferOperation *top = SEAHORSE_TRANSFER_OPERATION (gobject);
    SeahorseTransferOperationPrivate *pv = SEAHORSE_TRANSFER_OPERATION_GET_PRIVATE (top);

    /* Cancel any events in progress */
    if (pv->operation) {
        seahorse_operation_cancel (pv->operation);
        g_object_unref (pv->operation);
    }
    pv->operation = NULL;
    
    G_OBJECT_CLASS (transfer_operation_parent_class)->dispose (gobject);
}

static void 
seahorse_transfer_operation_finalize (GObject *gobject)
{
    SeahorseTransferOperation *top = SEAHORSE_TRANSFER_OPERATION (gobject);
    SeahorseTransferOperationPrivate *pv = SEAHORSE_TRANSFER_OPERATION_GET_PRIVATE (top);
    
    g_assert (!pv->operation);
    
    if (top->from)
        g_object_unref (top->from);
    top->from = NULL;
    
    if (top->to)
        g_object_unref (top->to);
    top->to = NULL;
    
	if (pv->output)
	    g_object_unref (pv->output);
	pv->output = NULL;
    
    g_free (pv->message);
    pv->message = NULL;
    
    G_OBJECT_CLASS (transfer_operation_parent_class)->finalize (gobject);
}

static void 
seahorse_transfer_operation_cancel (SeahorseOperation *operation)
{
    SeahorseTransferOperation *top = SEAHORSE_TRANSFER_OPERATION (operation);
    SeahorseTransferOperationPrivate *pv = SEAHORSE_TRANSFER_OPERATION_GET_PRIVATE (top);
    
    g_return_if_fail (seahorse_operation_is_running (operation));
    g_assert (pv->operation != NULL);
    
    /* This should call through to our event handler, and cancel us */
    if (pv->operation)
        seahorse_operation_cancel (pv->operation);
    g_assert (pv->operation == NULL);
    
    if (seahorse_operation_is_running (operation))
        seahorse_operation_mark_done (operation, TRUE, NULL);
}

/* -----------------------------------------------------------------------------
 * PUBLIC METHODS
 */

SeahorseOperation*
seahorse_transfer_operation_new (const gchar *message, SeahorseSource *from,
                                 SeahorseSource *to, GList *keyids)
{
    SeahorseTransferOperation *top;
    
    g_return_val_if_fail (from != NULL, NULL);
    g_return_val_if_fail (to != NULL, NULL);
    
    if (!keyids)
        return seahorse_operation_new_complete (NULL);
    
    top = g_object_new (SEAHORSE_TYPE_TRANSFER_OPERATION, "message", message, 
                        "from-key-source", from, "to-key-source", to, NULL);

    seahorse_debug ("[transfer] starting export");

    /* A list of quarks, so a deep copy is not necessary */
    g_object_set_data_full (G_OBJECT (top), "transfer-key-ids", g_list_copy (keyids),
                            (GDestroyNotify)g_list_free);

    /* And mark us as started */
    seahorse_operation_mark_start (SEAHORSE_OPERATION (top));
    seahorse_operation_mark_progress (SEAHORSE_OPERATION (top), message, 0.0);
    
    /* We delay and continue from a callback */
    g_timeout_add_seconds (0, (GSourceFunc)start_transfer, top);
    
    return SEAHORSE_OPERATION (top);
}
