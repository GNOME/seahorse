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

#include <string.h>
#include <eel/eel.h>

#include "config.h"
#include "seahorse-gpgmex.h"
#include "seahorse-keyserver-op.h"
#include "seahorse-util.h"

enum {
    PROP_0,
    PROP_KEY_SOURCE,
    PROP_UPDATED_KEYS
};

#define SEAHORSE_TYPE_IMPORT_OPERATION            (seahorse_import_operation_get_type ())
#define SEAHORSE_IMPORT_OPERATION(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_TYPE_IMPORT_OPERATION, SeahorseImportOperation))
#define SEAHORSE_IMPORT_OPERATION_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_IMPORT_OPERATION, SeahorseImportOperationClass))
#define SEAHORSE_IS_IMPORT_OPERATION(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_TYPE_IMPORT_OPERATION))
#define SEAHORSE_IS_IMPORT_OPERATION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_IMPORT_OPERATION))
#define SEAHORSE_IMPORT_OPERATION_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_TYPE_IMPORT_OPERATION, SeahorseImportOperationClass))

DECLARE_OPERATION (Import, import)
    SeahorseKeySource *sksrc;       /* Keysource to import into */
    gpgmex_keyserver_op_t kop;      /* Keyserver operation */
    GList *updated;                 /* List of updated keys */
END_DECLARE_OPERATION 

IMPLEMENT_OPERATION_PROPS (Import, import)

    g_object_class_install_property (gobject_class, PROP_KEY_SOURCE,
        g_param_spec_object ("key-source", "Seahorse Key Source",
                             "Current Seahorse Key Source to use",
                             SEAHORSE_TYPE_KEY_SOURCE, 
                             G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE));
    
    g_object_class_install_property (gobject_class, PROP_UPDATED_KEYS,
        g_param_spec_pointer ("updated-keys", "Updated Keys",
                              "List of updated keys", G_PARAM_READABLE));    
    
END_IMPLEMENT_OPERATION_PROPS

static void 
seahorse_import_operation_init (SeahorseImportOperation *iop)
{
    iop->updated = NULL;
    iop->sksrc = NULL;
}

static void 
seahorse_import_operation_dispose (GObject *gobject)
{
    SeahorseImportOperation *iop;
    iop = SEAHORSE_IMPORT_OPERATION (gobject);    

    if (iop->sksrc) {
        g_object_unref (iop->sksrc);
        iop->sksrc = NULL;
    }
    
    G_OBJECT_CLASS (operation_parent_class)->dispose (gobject);  
}

static void 
seahorse_import_operation_finalize (GObject *gobject)
{
    SeahorseImportOperation *iop;
    iop = SEAHORSE_IMPORT_OPERATION (gobject);    

    g_assert (iop->sksrc == NULL);
    
    /* We don't own any references on the keys */
    g_list_free (iop->updated);
    
    G_OBJECT_CLASS (operation_parent_class)->finalize (gobject);  
}

static void
seahorse_import_operation_set_property (GObject *object, guint prop_id,
                                        const GValue *value, GParamSpec *pspec)
{
    SeahorseImportOperation *iop;
    iop = SEAHORSE_IMPORT_OPERATION (object);
   
    switch (prop_id) {
    case PROP_KEY_SOURCE:
        g_return_if_fail (iop->sksrc == NULL);
        iop->sksrc = g_value_get_object (value);
        g_object_ref (iop->sksrc);
        break;
    default:
        g_return_if_reached ();
        break;
    }
}

static void
seahorse_import_operation_get_property (GObject *object, guint prop_id,
                                        GValue *value, GParamSpec *pspec)
{
    SeahorseImportOperation *iop;
    GList *keys;
    
    iop = SEAHORSE_IMPORT_OPERATION (object);
   
    switch (prop_id) {
    case PROP_KEY_SOURCE:
        g_value_set_object (value, iop->sksrc);
        break;
    case PROP_UPDATED_KEYS:
        keys = g_list_copy (iop->updated);
        g_value_set_pointer (value, keys);
        break;
    default:
        break;
    }
}

static void 
seahorse_import_operation_cancel (SeahorseOperation *operation)
{
    SeahorseImportOperation *iop;
    
    g_return_if_fail (SEAHORSE_IS_IMPORT_OPERATION (operation));
    iop = SEAHORSE_IMPORT_OPERATION (operation);
    
    if (iop->kop) {
        gpgmex_keyserver_op_t kop = iop->kop;
        iop->kop = NULL;
        gpgmex_keyserver_cancel (kop);
    }
        
    seahorse_operation_mark_done (operation, TRUE, NULL);
}

static gpgme_error_t
import_data (gpgme_ctx_t ctx, gpgmex_keyserver_op_t op, gpgme_data_t data, 
             void *userdata)
{
    SeahorseImportOperation *iop;
    gpgme_import_result_t results;
    gpgme_import_status_t import;
    gpgme_error_t gerr;
    SeahorseKey *skey;
             
    g_return_val_if_fail (SEAHORSE_IS_IMPORT_OPERATION (userdata), GPG_E (GPG_ERR_INV_VALUE));
    iop = SEAHORSE_IMPORT_OPERATION (userdata);
     
    gerr = gpgme_op_import (ctx, data);
    
    if (!GPG_IS_OK (gerr)) 
        return gerr;
    
    /* Figure out which keys were imported */
    results = gpgme_op_import_result (ctx);
    if (results) {
        for (import = results->imports; import; import = import->next) {
            if (!GPG_IS_OK (import->result))
                continue;
                
            skey = seahorse_key_source_get_key (iop->sksrc, import->fpr, 
                                                SKEY_INFO_NORMAL);            
            if (skey != NULL)
                iop->updated = g_list_append (iop->updated, skey);
        }
    }
    
    return GPG_OK;
}

static void
import_done (gpgme_ctx_t ctx, gpgmex_keyserver_op_t op, gpgme_error_t status,   
             const char *message, void *userdata)
{
    SeahorseImportOperation *iop;
    GError *err = NULL;

    g_return_if_fail (SEAHORSE_IS_IMPORT_OPERATION (userdata));
    iop = SEAHORSE_IMPORT_OPERATION (userdata);
     
    iop->kop = NULL;
    
    /* We always update, because it could be partial failure */
    seahorse_key_source_refresh (iop->sksrc, FALSE);
    
    /* Unsuccessful completion */
    if (!GPG_IS_OK (status)) 
        seahorse_util_gpgme_to_error (status, &err);
        
    /* TODO: We need to preserve the error message we got */
    seahorse_operation_mark_done (SEAHORSE_OPERATION (iop), FALSE, err);
}

/**
 * seahorse_keyserver_op_import
 * @sksrc: The SeahorseKeySource to import into 
 * @keys: A list of SeahorseKeys to import
 * @server: The server to import from
 * 
 * Imports keys from a key server
 * 
 * Returns: The import operation handle
 **/                                                  
SeahorseOperation*  
seahorse_keyserver_op_import (SeahorseKeySource *sksrc, GList *keys, 
                              const gchar *server)
{
    SeahorseImportOperation *iop;
    const gchar **fprs;
    int i;
    
    g_return_val_if_fail (SEAHORSE_IS_KEY_SOURCE (sksrc), NULL);
    g_return_val_if_fail (keys != NULL, NULL);
    g_return_val_if_fail (server && server[0], NULL);
    
    iop = g_object_new (SEAHORSE_TYPE_IMPORT_OPERATION, "key-source", sksrc, NULL);
    
    fprs = g_new0 (const gchar*, g_list_length (keys) + 1);
    
    for (i = 0; keys != NULL; keys = g_list_next (keys), i++) {
        g_return_val_if_fail (SEAHORSE_IS_KEY (keys->data), NULL);
        fprs[i] = seahorse_key_get_id (SEAHORSE_KEY (keys->data)->key);
    }

    seahorse_operation_mark_start (SEAHORSE_OPERATION (iop));
    
    gpgmex_keyserver_start_get (sksrc->ctx, server, fprs, import_data, import_done,
                                iop, &(iop->kop));

    g_free (fprs);
    return SEAHORSE_OPERATION (iop); 
}
