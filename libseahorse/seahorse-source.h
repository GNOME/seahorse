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
 * SeahorseSource: Base class for other sources. 
 * 
 * - A generic interface for accessing sources.
 * - Eventually more functionality will be merged from seahorse-op.* into 
 *   this class and derived classes. 
 * - Each SeahorseObject has a weak pointer to the SeahorseSource that 
 *   created it.
 * 
 * Properties base classes must implement:
 *  ktype: (GQuark) The ktype (ie: SEAHORSE_PGP) of objects originating from this 
 *         object source.
 *  key-desc: (gchar*) Description for the type of keys originating here.
 *  location: (SeahorseLocation) The location of objects that come from this 
 *         source. (ie: SEAHORSE_LOCATION_LOCAL, SEAHORSE_LOCATION_REMOTE)
 *  uri: (gchar*) Only for remote object sources. The full URI of the keyserver 
 *         being used.
 */


#ifndef __SEAHORSE_SOURCE_H__
#define __SEAHORSE_SOURCE_H__

#include "seahorse-operation.h"
#include "seahorse-types.h"

#include <gio/gio.h>
#include <glib-object.h>

#define SEAHORSE_TYPE_SOURCE            (seahorse_source_get_type ())
#define SEAHORSE_SOURCE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_TYPE_SOURCE, SeahorseSource))
#define SEAHORSE_SOURCE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_SOURCE, SeahorseSourceClass))
#define SEAHORSE_IS_SOURCE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_TYPE_SOURCE))
#define SEAHORSE_IS_SOURCE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_SOURCE))
#define SEAHORSE_SOURCE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_TYPE_SOURCE, SeahorseSourceClass))

struct _SeahorseObject;

typedef struct _SeahorseSource {
    GObject             parent;   
} SeahorseSource;

typedef struct _SeahorseSourceClass {
    GtkObjectClass parent_class;
    
    /* class props ----------------------------------------------------- */
    const GQuark obj_tag;
    const gchar *display_name;
    const gchar **mime_types;

    /* class methods --------------------------------------------------- */
    
    GQuark (*canonize_id) (const gchar *id);
    
    /* virtual methods ------------------------------------------------- */

    /**
     * load
     * @sksrc: The #SeahorseSource.
     * @id: The object to load or 0 for all the objects.
     * 
     * Loads the requested objects, and add the objects to SeahorseContext. 
     * 
     * Returns: The load operation.
     */
    SeahorseOperation* (*load) (SeahorseSource *sksrc, GQuark id);

    /**
     * search
     * @sksrc: The #SeahorseSource 
     * @match: Match text
     *
     * Searches for objects in the source.
     *
     * Returns: The search operation.
     */
    SeahorseOperation* (*search) (SeahorseSource *sksrc, const gchar *match);

    
    /**
     * import
     * @sksrc: The #SeahorseSource to import into.
     * @input: The data to import.
     *
     * Import objects into the source. When operation is 'done' a GList of 
     * updated objects may be found as the operation result. 
     * 
     * Returns: The import operation
     */
    SeahorseOperation* (*import) (SeahorseSource *sksrc, GInputStream *input);

    /**
     * export
     * @sksrc: The #SeahorseSource to export from.
     * @objects: A list of objects to export.
     * @complete: Whether to export the secret objects too.
     * @data: Output stream to export to.
     *
     * Import objects into the object source. When operation is 'done' the result
     * of the operation will be a GOutputStream
     * 
     * Returns: The export operation
     */    
    SeahorseOperation* (*export) (SeahorseSource *sksrc, GList *objects, 
                                  gboolean complete, GOutputStream *output);

    /**
     * export_raw
     * @sksrc: The #SeahorseSource to export from.
     * @objects: A list of ids to export.
     * @data: output stream to export to.
     *
     * Import objects into the source. When operation is 'done' the result
     * of the operation will be a GOutputStream
     * 
     * Returns: The export operation
     */    
    SeahorseOperation* (*export_raw) (SeahorseSource *sksrc, GSList *ids, 
                                      GOutputStream *output);

    /**
     * remove
     * @sksrc: The #SeahorseSource to delete the object from.
     * @object: A object to delete.
     * @name: UID/name to delete, or 0 for main object.
     * @error: Error code when not successful.
     * 
     * Delete the objects from the object source. 
     *
     * Returns: Whether successful or not.
     */
    gboolean (*remove) (SeahorseSource *sksrc, struct _SeahorseObject *sobj,  
                        guint name, GError **error);
    
} SeahorseSourceClass;

GType       seahorse_source_get_type      (void);

/* Method helper functions ------------------------------------------- */


SeahorseOperation*  seahorse_source_load                  (SeahorseSource *sksrc,
                                                           GQuark id);
                                                          
void                seahorse_source_load_sync             (SeahorseSource *sksrc,
                                                           GQuark keyid);

void                seahorse_source_load_async            (SeahorseSource *sksrc,
                                                           GQuark id);

SeahorseOperation*  seahorse_source_search                (SeahorseSource *sksrc,
                                                           const gchar *match);

/* Takes ownership of |data| */
SeahorseOperation*  seahorse_source_import                (SeahorseSource *sksrc,
                                                           GInputStream *input);

/* Takes ownership of |data| */
gboolean            seahorse_source_import_sync           (SeahorseSource *sksrc,
                                                           GInputStream *input,
                                                           GError **err);

SeahorseOperation*  seahorse_source_export_objects        (GList *objects, 
                                                           GOutputStream *output);

gboolean            seahorse_source_delete_objects        (GList *objects, 
                                                           GError **error);

SeahorseOperation*  seahorse_source_export                (SeahorseSource *sksrc,
                                                           GList *objects,
                                                           gboolean complete,
                                                           GOutputStream *output);

SeahorseOperation*  seahorse_source_export_raw            (SeahorseSource *sksrc, 
                                                           GSList *ids, 
                                                           GOutputStream *output);

gboolean            seahorse_source_remove                (SeahorseSource *sksrc,
                                                           struct _SeahorseObject *sobj,
                                                           guint name,
                                                           GError **error);

GQuark              seahorse_source_get_ktype             (SeahorseSource *sksrc);

SeahorseLocation    seahorse_source_get_location          (SeahorseSource *sksrc);

GQuark              seahorse_source_canonize_id           (GQuark ktype, 
                                                           const gchar *id);

GQuark              seahorse_source_mime_to_ktype         (const gchar *mimetype);

const gchar*        seahorse_source_type_get_description  (GType type);

#endif /* __SEAHORSE_SOURCE_H__ */
