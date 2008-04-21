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
#include <gnome.h>

#include "seahorse-transfer-operation.h"
#include "seahorse-util.h"

#include "pgp/seahorse-gpgmex.h"
#include "pgp/seahorse-pgp-source.h"

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
 * DEFINITIONS
 */
 
typedef struct _SeahorseTransferOperationPrivate {
    SeahorseOperation *operation;   /* The current operation in progress */
    gchar *message;                 /* A progress message to display */
    gpgme_data_t data;              /* Hold onto the data */
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
                             SEAHORSE_TYPE_KEY_SOURCE, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

    g_object_class_install_property (gobject_class, PROP_TO_KEY_SOURCE,
        g_param_spec_object ("to-key-source", "To key source", "Key source keys are being transferred to",
                             SEAHORSE_TYPE_KEY_SOURCE, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

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
    
    DEBUG_OPERATION (("[transfer] import progress: %lf\n", fract));
    
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
    DEBUG_OPERATION (("[transfer] import done\n"));
    
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
    DEBUG_OPERATION (("[transfer] export progress: %lf\n", fract));
    
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
    gpgme_data_t data;
    gpgme_error_t gerr;
    size_t len;
    gchar *raw;
    
    g_assert (op == pv->operation);
    DEBUG_OPERATION (("[transfer] export done\n"));

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
        DEBUG_OPERATION (("[transfer] stopped after export\n"));
        return;
    }
    
    DEBUG_OPERATION (("[transfer] starting import\n"));
    
    /* 
     * HACK: Strange bug in GPGME. It won't accept our gpgme_data_t unless
     * we rebuild it. 
     */
    
    raw = gpgme_data_release_and_get_mem (pv->data, &len);
    pv->data = NULL;
    
    gerr = gpgme_data_new_from_mem (&data, raw, len, 0);
    g_return_if_fail (GPG_IS_OK (gerr));

    /* This frees data */
    pv->operation = seahorse_key_source_import (top->to, data);
    g_return_if_fail (pv->operation != NULL);
    
    /* Free this data with the operation */
    g_object_set_data_full (G_OBJECT (pv->operation), "raw-data", 
                            raw, (GDestroyNotify)free);

    /* And mark us as started */
    seahorse_operation_mark_start (SEAHORSE_OPERATION (top));
    seahorse_operation_mark_progress (SEAHORSE_OPERATION (top), pv->message ? pv->message : 
                                      seahorse_operation_get_message (pv->operation), 0.5);
    
    seahorse_operation_watch (pv->operation, G_CALLBACK (import_done), 
                              G_CALLBACK (import_progress), top);
}


static gboolean 
start_transfer (SeahorseTransferOperation *top)
{
    SeahorseTransferOperationPrivate *pv = SEAHORSE_TRANSFER_OPERATION_GET_PRIVATE (top);
    SeahorseKeySource *from;
    GSList *keyids;
    
    g_assert (pv->operation == NULL);

    keyids = (GSList*)g_object_get_data (G_OBJECT (top), "transfer-key-ids");
    g_object_get (top, "from-key-source", &from, NULL);
    g_assert (keyids && from);
    
    pv = SEAHORSE_TRANSFER_OPERATION_GET_PRIVATE (top);
    pv->operation = seahorse_key_source_export_raw (from, keyids, pv->data);
    g_return_val_if_fail (pv->operation != NULL, FALSE);
    
    seahorse_operation_watch (pv->operation, G_CALLBACK (export_done), 
                              G_CALLBACK (export_progress), top);
    return FALSE;
};

/* -----------------------------------------------------------------------------
 * OBJECT 
 */

static void 
seahorse_transfer_operation_init (SeahorseTransferOperation *top)
{
    SeahorseTransferOperationPrivate *pv = SEAHORSE_TRANSFER_OPERATION_GET_PRIVATE (top);
    pv->data = gpgmex_data_new ();
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
    
    G_OBJECT_CLASS (operation_parent_class)->dispose (gobject);
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
    
    if (pv->data)
        gpgme_data_release (pv->data);
    pv->data = NULL;
    
    g_free (pv->message);
    pv->message = NULL;
    
    G_OBJECT_CLASS (operation_parent_class)->finalize (gobject);
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
seahorse_transfer_operation_new (const gchar *message, SeahorseKeySource *from,
                                 SeahorseKeySource *to, GSList *keyids)
{
    SeahorseTransferOperation *top;
    
    g_return_val_if_fail (from != NULL, NULL);
    g_return_val_if_fail (to != NULL, NULL);
    
    if (!keyids)
        return seahorse_operation_new_complete (NULL);
    
    top = g_object_new (SEAHORSE_TYPE_TRANSFER_OPERATION, "message", message, 
                        "from-key-source", from, "to-key-source", to, NULL);
    
    DEBUG_OPERATION (("[transfer] starting export\n"));

    /* A list of quarks, so a deep copy is not necessary */
    g_object_set_data_full (G_OBJECT (top), "transfer-key-ids", g_slist_copy (keyids), 
                            (GDestroyNotify)g_slist_free);
    
    /* And mark us as started */
    seahorse_operation_mark_start (SEAHORSE_OPERATION (top));
    seahorse_operation_mark_progress (SEAHORSE_OPERATION (top), message, 0.0);
    
    /* We delay and continue from a callback */
    g_timeout_add (0, (GSourceFunc)start_transfer, top);
    
    return SEAHORSE_OPERATION (top);
}
