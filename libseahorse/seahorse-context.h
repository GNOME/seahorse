/*
 * Seahorse
 *
 * Copyright (C) 2003 Jacob Perkins
 * Copyright (C) 2004-2005 Stefan Walter
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
 * SeahorseContext: This is where all the action in a Seahorse process comes
 * together. 
 *
 * - Usually there's only one SeahorseContext per process created by passing 
 *   |SEAHORSE_CONTEXT_APP| to |seahorse_context_new|, and accessed via 
 *   the |SCTX_APP| macro.
 * - Retains the list of all valid struct _SeahorseObject objects. 
 * - Has a collection of SeahorseSource objects which add objects to the 
 *   SeahorseContext. 
 * 
 * Signals:
 *   added: A object was added to the context.
 *   removed: A object was removed from the context.
 *   changed: A object changed.
 *   destroy: The context was destroyed.
 */
 
#ifndef __SEAHORSE_CONTEXT_H__
#define __SEAHORSE_CONTEXT_H__

#include <gtk/gtk.h>

#include "seahorse-source.h"
#include "seahorse-dns-sd.h"

#define SEAHORSE_TYPE_CONTEXT			(seahorse_context_get_type ())
#define SEAHORSE_CONTEXT(obj)			(G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_TYPE_CONTEXT, SeahorseContext))
#define SEAHORSE_CONTEXT_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_CONTEXT, SeahorseContextClass))
#define SEAHORSE_IS_CONTEXT(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_TYPE_CONTEXT))
#define SEAHORSE_IS_CONTEXT_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_CONTEXT))
#define SEAHORSE_CONTEXT_GET_CLASS(obj)		(G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_TYPE_CONTEXT, SeahorseContextClass))

struct _SeahorseKey;
struct _SeahorseObject;
struct _SeahorseObjectPredicate;

typedef struct _SeahorseContext SeahorseContext;
typedef struct _SeahorseContextClass SeahorseContextClass;
typedef struct _SeahorseContextPrivate SeahorseContextPrivate;

struct _SeahorseContext {
    GtkObject               parent;
    
    /*< public >*/
    gboolean                is_daemon;
    
    /*< private >*/
    SeahorseContextPrivate  *pv;
};

struct _SeahorseContextClass {
    GtkObjectClass parent_class;
    
    /* signals --------------------------------------------------------- */
    
    /* A object was added to this source */
    void (*added) (SeahorseContext *sctx, struct _SeahorseObject *sobj);

    /* Removed a object from this source */
    void (*removed) (SeahorseContext *sctx, struct _SeahorseObject *sobj);
    
    /* This object has changed */
    void (*changed) (SeahorseContext *sctx, struct _SeahorseObject *sobj);
};

enum SeahorseContextType {
    SEAHORSE_CONTEXT_APP = 1,
    SEAHORSE_CONTEXT_DAEMON = 2,
};

typedef void (*SeahorseObjectFunc) (struct _SeahorseObject *obj, gpointer user_data);

#define             SCTX_APP()                          (seahorse_context_for_app ())

GType               seahorse_context_get_type           (void);

SeahorseContext*    seahorse_context_for_app            (void);

SeahorseContext*    seahorse_context_new                (guint              flags);

void                seahorse_context_destroy            (SeahorseContext    *sctx);

#define             seahorse_context_is_daemon(ctx)     ((ctx)->is_daemon)

void                seahorse_context_add_source         (SeahorseContext    *sctx,
                                                         SeahorseSource  *sksrc);

void                seahorse_context_take_source        (SeahorseContext    *sctx,
                                                         SeahorseSource  *sksrc);

void                seahorse_context_remove_source      (SeahorseContext    *sctx,
                                                         SeahorseSource  *sksrc);

SeahorseSource*     seahorse_context_find_source        (SeahorseContext    *sctx,
                                                         GQuark             ktype,
                                                         SeahorseLocation   location);

GSList*             seahorse_context_find_sources       (SeahorseContext    *sctx,
                                                         GQuark             ktype,
                                                         SeahorseLocation   location);
                                                         

SeahorseSource*     seahorse_context_remote_source      (SeahorseContext    *sctx,
                                                         const gchar        *uri);

void                seahorse_context_add_object         (SeahorseContext    *sctx,
                                                         struct _SeahorseObject     *sobj);

void                seahorse_context_take_object        (SeahorseContext    *sctx, 
                                                         struct _SeahorseObject     *sobj);

guint               seahorse_context_get_count          (SeahorseContext    *sctx);

struct _SeahorseObject*     seahorse_context_get_object  (SeahorseContext    *sctx,
                                                         SeahorseSource  *sksrc,
                                                         GQuark             id);

GList*              seahorse_context_get_objects        (SeahorseContext    *sctx, 
                                                         SeahorseSource  *sksrc);

struct _SeahorseObject*     seahorse_context_find_object (SeahorseContext    *sctx,
                                                         GQuark             id,
                                                         SeahorseLocation   location);

GList*              seahorse_context_find_objects       (SeahorseContext    *sctx,
                                                         GQuark             ktype,
                                                         SeahorseUsage      usage,
                                                         SeahorseLocation   location);

GList*              seahorse_context_find_objects_full  (SeahorseContext *self,
                                                         struct _SeahorseObjectPredicate *skpred);

void                seahorse_context_for_objects_full   (SeahorseContext *self,
                                                         struct _SeahorseObjectPredicate *skpred,
                                                         SeahorseObjectFunc func,
                                                         gpointer user_data);

void                seahorse_context_remove_object      (SeahorseContext *sctx,
                                                         struct _SeahorseObject *sobj);

SeahorseServiceDiscovery*
                    seahorse_context_get_discovery      (SeahorseContext    *sctx);

struct _SeahorseObject*   
                    seahorse_context_get_default_key    (SeahorseContext    *sctx);

SeahorseOperation*  seahorse_context_refresh_local         (SeahorseContext    *sctx);

void                seahorse_context_refresh_local_async   (SeahorseContext *sctx);

SeahorseOperation*  seahorse_context_search_remote         (SeahorseContext    *sctx,
                                                            const gchar        *search);

SeahorseOperation*  seahorse_context_transfer_objects   (SeahorseContext    *sctx, 
                                                         GList              *objs, 
                                                         SeahorseSource  *to);

SeahorseOperation*  seahorse_context_retrieve_objects   (SeahorseContext    *sctx, 
                                                         GQuark             ktype, 
                                                         GSList             *ids, 
                                                         SeahorseSource  *to);

GList*              seahorse_context_discover_objects   (SeahorseContext    *sctx, 
                                                         GQuark             ktype, 
                                                         GSList             *ids);

struct _SeahorseObject*     seahorse_context_object_from_dbus   (SeahorseContext    *sctx,
                                                         const gchar        *dbusid);

gchar*              seahorse_context_object_to_dbus     (SeahorseContext    *sctx,
                                                         struct _SeahorseObject *sobj);

gchar*              seahorse_context_id_to_dbus         (SeahorseContext    *sctx,
                                                         GQuark             id);

#endif /* __SEAHORSE_CONTEXT_H__ */
