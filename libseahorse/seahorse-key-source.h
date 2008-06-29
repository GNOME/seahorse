/*
 * Seahorse
 *
 * Copyright (C) 2004,2005 Stefan Walter
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
 *  ktype: (GQuark) The ktype (ie: SEAHORSE_PGP) of keys originating from this 
 *         key source.
 *  key-desc: (gchar*) Description for the type of keys originating here.
 *  location: (SeahorseKeyLoc) The location of keys that come from this 
 *         source. (ie: SKEY_LOC_LOCAL, SKEY_LOC_REMOTE)
 *  uri: (gchar*) Only for remote key sources. The full URI of the keyserver 
 *         being used.
 */


#ifndef __SEAHORSE_KEY_SOURCE_H__
#define __SEAHORSE_KEY_SOURCE_H__

#include "seahorse-key.h"
#include "seahorse-operation.h"

#include <gio/gio.h>
#include <glib-object.h>

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

typedef struct _SeahorseKeySourceClass {
    GtkObjectClass parent_class;
    
    /* class props ----------------------------------------------------- */
    const GQuark key_type;
    const gchar *display_name;
    const gchar **mime_types;

    /* class methods --------------------------------------------------- */
    
    GQuark (*canonize_keyid) (const gchar *keyid);
    
    /* virtual methods ------------------------------------------------- */

    /**
     * load
     * @sksrc: The #SeahorseKeySource.
     * @keyid: The key to load or 0 for all the keys.
     * 
     * Loads the requested keys, and add the keys to SeahorseContext. 
     * 
     * Returns: The load operation.
     */
    SeahorseOperation* (*load) (SeahorseKeySource *sksrc, GQuark keyid);

    /**
     * search
     * @sksrc: The #SeahorseKeySource 
     * @match: Match text
     *
     * Searches for keys in the key source.
     *
     * Returns: The search operation.
     */
    SeahorseOperation* (*search) (SeahorseKeySource *sksrc, const gchar *match);

    
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
     * @input: The data to import.
     *
     * Import keys into the key source. When operation is 'done' a GList of 
     * updated keys may be found as the operation result. 
     * 
     * Returns: The import operation
     */
    SeahorseOperation* (*import) (SeahorseKeySource *sksrc, GInputStream *input);

    /**
     * export
     * @sksrc: The #SeahorseKeySource to export from.
     * @keys: A list of keys to export.
     * @complete: Whether to export the secret key too.
     * @data: Output stream to export to.
     *
     * Import keys into the key source. When operation is 'done' the result
     * of the operation will be a GOutputStream
     * 
     * Returns: The export operation
     */    
    SeahorseOperation* (*export) (SeahorseKeySource *sksrc, GList *keys, 
                                  gboolean complete, GOutputStream *output);

    /**
     * export_raw
     * @sksrc: The #SeahorseKeySource to export from.
     * @keys: A list of key ids to export.
     * @data: output stream to export to.
     *
     * Import keys into the key source. When operation is 'done' the result
     * of the operation will be a GOutputStream
     * 
     * Returns: The export operation
     */    
    SeahorseOperation* (*export_raw) (SeahorseKeySource *sksrc, GSList *keyids, 
                                      GOutputStream *output);

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
                                                          GQuark keyid);
                                                          
void                seahorse_key_source_load_sync        (SeahorseKeySource *sksrc,
                                                          GQuark keyid);

void                seahorse_key_source_load_async       (SeahorseKeySource *sksrc,
                                                          GQuark keyid);

SeahorseOperation*  seahorse_key_source_search           (SeahorseKeySource *sksrc,
                                                          const gchar *match);

/* Takes ownership of |data| */
SeahorseOperation*  seahorse_key_source_import           (SeahorseKeySource *sksrc,
                                                          GInputStream *input);

/* Takes ownership of |data| */
gboolean            seahorse_key_source_import_sync      (SeahorseKeySource *sksrc,
                                                          GInputStream *input,
                                                          GError **err);

SeahorseOperation*  seahorse_key_source_export_keys      (GList *keys, 
                                                          GOutputStream *output);

gboolean            seahorse_key_source_delete_keys      (GList *keys, 
                                                          GError **error);

SeahorseOperation*  seahorse_key_source_export           (SeahorseKeySource *sksrc,
                                                          GList *keys,
                                                          gboolean complete,
                                                          GOutputStream *output);

SeahorseOperation*  seahorse_key_source_export_raw       (SeahorseKeySource *sksrc, 
                                                          GSList *keyids, 
                                                          GOutputStream *output);

void                seahorse_key_source_stop             (SeahorseKeySource *sksrc);

gboolean            seahorse_key_source_remove           (SeahorseKeySource *sksrc,
                                                          SeahorseKey *skey,
                                                          guint name,
                                                          GError **error);

GQuark              seahorse_key_source_get_ktype        (SeahorseKeySource *sksrc);

SeahorseKeyLoc      seahorse_key_source_get_location     (SeahorseKeySource *sksrc);

GQuark              seahorse_key_source_canonize_keyid   (GQuark ktype, 
                                                          const gchar *keyid);

GQuark              seahorse_key_source_mime_to_ktype    (const gchar *mimetype);

const gchar*        seahorse_key_source_type_get_description (GType type);

#endif /* __SEAHORSE_KEY_SOURCE_H__ */
