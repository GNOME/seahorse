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
    GObject parent;
    
    /*< public >*/
    gpgme_ctx_t ctx;

} SeahorseKeySource;

/* Flags for the get_state method below */
#define SEAHORSE_KEY_SOURCE_REMOTE  0x00000001
#define SEAHORSE_KEY_SOURCE_LOADING 0x00000010

/* Key types for refresh method below. These could never be fingerprints */
#define SEAHORSE_KEY_SOURCE_NEW     "_new_"
#define SEAHORSE_KEY_SOURCE_ALL     "_all_"

typedef struct _SeahorseKeySourceClass {
    GtkObjectClass parent_class;

    /* signals --------------------------------------------------------- */
    
    /* A key was added to this source */
    void (* added) (SeahorseKeySource *sksrc, SeahorseKey *key);

    /* Removed a key from this source */
    void (* removed) (SeahorseKeySource *sksrc, SeahorseKey *key);
    

    /* virtual methods ------------------------------------------------- */

    /* Refresh our list of keys. If |key| is set to SEAHORSE_KEY_NEW, then looks 
     * for new keys, if |key| is set to SEAHORSE_KEY_ALL refreshes all keys, 
     * otherwise tries to load key with specific fingerprint. */
    SeahorseOperation* (*refresh) (SeahorseKeySource *sksrc, const gchar *key);
    
    /* Get the loading operation */
    SeahorseOperation* (*get_operation) (SeahorseKeySource *sksrc);
    
    /* Stop any loading operation in progress */
    void (*stop) (SeahorseKeySource *sksrc);
    
    /* Get the number of keys in the key source. */
    guint (*get_count) (SeahorseKeySource *sksrc, gboolean secret_only);
    
    /* Get the loaded key with given fingerprint. */
    SeahorseKey* (*get_key) (SeahorseKeySource *sksrc, const gchar *fpr);
    
    /* Get a list of all the keys in the key source. No loading done */
    GList* (*get_keys) (SeahorseKeySource *sksrc, gboolean secret_only);
    
    /* Get a new GPGME context for this source */
    gpgme_ctx_t (*new_context) (SeahorseKeySource *sksrc);
    
    /* Get the flags for this key source */
    guint (*get_state) (SeahorseKeySource *sksrc);
    
    /**
     * import
     * @sksrc: The #SeahorseKeySource to import into.
     * @data: The data to import (not freed).
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
    
} SeahorseKeySourceClass;

GType       seahorse_key_source_get_type      (void);

/* Signal helper functions -------------------------------------------- */

void        seahorse_key_source_added         (SeahorseKeySource *sksrc, 
                                               SeahorseKey *key);
void        seahorse_key_source_removed       (SeahorseKeySource *sksrc, 
                                               SeahorseKey *key);

/* Method helper functions ------------------------------------------- */

void         seahorse_key_source_stop        (SeahorseKeySource *sksrc);

guint        seahorse_key_source_get_state   (SeahorseKeySource *sksrc);

SeahorseKey* seahorse_key_source_get_key     (SeahorseKeySource *sksrc,
                                              const gchar *fpr);

GList*       seahorse_key_source_get_keys    (SeahorseKeySource *sksrc,
                                              gboolean secret_only);

guint        seahorse_key_source_get_count   (SeahorseKeySource *sksrc,
                                              gboolean secret_only);

gpgme_ctx_t  seahorse_key_source_new_context (SeahorseKeySource *sksrc);

SeahorseOperation*  seahorse_key_source_get_operation    (SeahorseKeySource *sksrc);

SeahorseOperation*  seahorse_key_source_refresh          (SeahorseKeySource *sksrc,
                                                          const gchar *key);
                                                          
void                seahorse_key_source_refresh_sync     (SeahorseKeySource *sksrc,
                                                          const gchar *key);                                                          

void                seahorse_key_source_refresh_async    (SeahorseKeySource *sksrc,
                                                          const gchar *key);                                                          

SeahorseOperation*  seahorse_key_source_import           (SeahorseKeySource *sksrc,
                                                          gpgme_data_t data);
                                                          
gboolean            seahorse_key_source_import_sync      (SeahorseKeySource *sksrc,
                                                          gpgme_data_t data,
                                                          GError **err);

SeahorseOperation*  seahorse_key_source_export           (SeahorseKeySource *sksrc,
                                                          GList *keys,
                                                          gboolean complete,
                                                          gpgme_data_t data);                        

#endif /* __SEAHORSE_KEY_SOURCE_H__ */
