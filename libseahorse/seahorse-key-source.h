/*
 * Seahorse
 *
 * Copyright (C) 2004,2005 Nate Nielsen
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
 * SeahorseKeySource: Base class for other key sources. 
 * 
 * - A generic interface for accessing key sources.
 * - Eventually more functionality will be merged from seahorse-op.* into 
 *   this class and derived classes. 
 * - Each SeahorseKey has a weak pointer to the SeahorseKeySource that 
 *   created it.
 * 
 * Properties base classes must implement:
 *  ktype: (GQuark) The ktype (ie: SKEY_PGP) of keys originating from this 
 *         key source.
 *  key-desc: (gchar*) Description for the type of keys originating here.
 *  location: (SeahorseKeyLoc) The location of keys that come from this 
 *         source. (ie: SKEY_LOC_LOCAL, SKEY_LOC_REMOTE)
 *  uri: (gchar*) Only for remote key sources. The full URI of the keyserver 
 *         being used.
 */


#ifndef __SEAHORSE_KEY_SOURCE_H__
#define __SEAHORSE_KEY_SOURCE_H__

#include <gnome.h>
#include <gpgme.h>

#include "seahorse-key.h"
#include "seahorse-operation.h"

#define SEAHORSE_TYPE_KEY_SOURCE            (seahorse_key_source_get_type ())
#define SEAHORSE_KEY_SOURCE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_TYPE_KEY_SOURCE, SeahorseKeySource))
#define SEAHORSE_KEY_SOURCE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_KEY_SOURCE, SeahorseKeySourceClass))
#define SEAHORSE_IS_KEY_SOURCE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_TYPE_KEY_SOURCE))
#define SEAHORSE_IS_KEY_SOURCE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_KEY_SOURCE))
#define SEAHORSE_KEY_SOURCE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_TYPE_KEY_SOURCE, SeahorseKeySourceClass))

typedef struct _SeahorseKeySource {
    GObject             parent;   
} SeahorseKeySource;

/* Flags for the get_state method below */
typedef enum {
    SKSRC_REMOTE  = 0x00000001,
    SKSRC_LOADING = 0x00000010
} SeahorseKeySourceFlags;

/* Operation for the load method below */
typedef enum {
    SKSRC_LOAD_NEW,
    SKSRC_LOAD_ALL,
    SKSRC_LOAD_KEY,
    SKSRC_LOAD_SEARCH
} SeahorseKeySourceLoad;

typedef struct _SeahorseKeySourceClass {
    GtkObjectClass parent_class;

    /* virtual methods ------------------------------------------------- */

    /**
     * load
     * @sksrc: The #SeahorseKeySource.
     * @op: The type of load to do. See SeahorseKeySourceLoad.
     * @match: Match text (valid for SKSRC_LOAD_KEY and SKSRC_LOAD_SEARCH ops).
     * 
     * Loads the requested keys, and add the keys to SeahorseContext. 
     * 
     * Returns: The load operation.
     */
    SeahorseOperation* (*load) (SeahorseKeySource *sksrc, SeahorseKeySourceLoad op,
                                GQuark keyid, const gchar *match);
    
    /**
     * stop
     * @sksrc: The #SeahorseKeySource.
     * 
     * Stops any operations in progress. (ie: load, import, export etc...)
     */
    void (*stop) (SeahorseKeySource *sksrc);
    
    /* Get the flags for this key source */
    guint (*get_state) (SeahorseKeySource *sksrc);
    
    /**
     * import
     * @sksrc: The #SeahorseKeySource to import into.
     * @data: The data to import (takes ownership).
     *
     * Import keys into the key source. When operation is 'done' a GList of 
     * updated keys may be found as the operation result. 
     * 
     * Returns: The import operation
     */
    SeahorseOperation* (*import) (SeahorseKeySource *sksrc, gpgme_data_t data);

    /**
     * export
     * @sksrc: The #SeahorseKeySource to export from.
     * @keys: A list of keys to export.
     * @complete: Whether to export the secret key too.
     * @data: Optional data object to export to (not freed).
     *
     * Import keys into the key source. When operation is 'done' the result
     * of the operation will be a gpgme_data_t 
     * 
     * Returns: The export operation
     */    
    SeahorseOperation* (*export) (SeahorseKeySource *sksrc, GList *keys, 
                                  gboolean complete, gpgme_data_t data);

    /**
     * export_raw
     * @sksrc: The #SeahorseKeySource to export from.
     * @keys: A list of key ids to export.
     * @data: Optional data object to export to (not freed).
     *
     * Import keys into the key source. When operation is 'done' the result
     * of the operation will be a gpgme_data_t 
     * 
     * Returns: The export operation
     */    
    SeahorseOperation* (*export_raw) (SeahorseKeySource *sksrc, GSList *keyids, 
                                      gpgme_data_t data);

    /**
     * remove
     * @sksrc: The #SeahorseKeySource to delete the key from.
     * @key: A key to delete.
     * @name: UID/name to delete, or 0 for main key.
     * @error: Error code when not successful.
     * 
     * Delete the keys from the key source. 
     *
     * Returns: Whether successful or not.
     */
    gboolean (*remove) (SeahorseKeySource *sksrc, SeahorseKey *skey,  
                        guint name, GError **error);
    
} SeahorseKeySourceClass;

GType       seahorse_key_source_get_type      (void);

/* Method helper functions ------------------------------------------- */


guint               seahorse_key_source_get_state        (SeahorseKeySource *sksrc);

SeahorseOperation*  seahorse_key_source_load             (SeahorseKeySource *sksrc,
                                                          SeahorseKeySourceLoad load,
                                                          GQuark keyid,
                                                          const gchar *match);
                                                          
void                seahorse_key_source_load_sync        (SeahorseKeySource *sksrc,
                                                          SeahorseKeySourceLoad load,
                                                          GQuark keyid,
                                                          const gchar *match);

void                seahorse_key_source_load_async       (SeahorseKeySource *sksrc,
                                                          SeahorseKeySourceLoad load,
                                                          GQuark keyid,
                                                          const gchar *match);

/* Takes ownership of |data| */
SeahorseOperation*  seahorse_key_source_import           (SeahorseKeySource *sksrc,
                                                          gpgme_data_t data);

/* Takes ownership of |data| */
gboolean            seahorse_key_source_import_sync      (SeahorseKeySource *sksrc,
                                                          gpgme_data_t data,
                                                          GError **err);

SeahorseOperation*  seahorse_key_source_export_keys      (GList *keys, 
                                                          gpgme_data_t data);

SeahorseOperation*  seahorse_key_source_export           (SeahorseKeySource *sksrc,
                                                          GList *keys,
                                                          gboolean complete,
                                                          gpgme_data_t data);

SeahorseOperation*  seahorse_key_source_export_raw       (SeahorseKeySource *sksrc, 
                                                          GSList *keyids, 
                                                          gpgme_data_t data);

void                seahorse_key_source_stop             (SeahorseKeySource *sksrc);

gboolean            seahorse_key_source_remove           (SeahorseKeySource *sksrc,
                                                          SeahorseKey *skey,
                                                          guint name,
                                                          GError **error);

GQuark              seahorse_key_source_get_ktype        (SeahorseKeySource *sksrc);

SeahorseKeyLoc      seahorse_key_source_get_location     (SeahorseKeySource *sksrc);

GQuark              seahorse_key_source_cannonical_keyid (GQuark ktype, 
                                                          const gchar *keyid);

#endif /* __SEAHORSE_KEY_SOURCE_H__ */
