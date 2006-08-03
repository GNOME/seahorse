/*
 * Seahorse
 *
 * Copyright (C) 2003 Jacob Perkins
 * Copyright (C) 2004-2005 Nate Nielsen
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
 *   |TRUE| to |seahorse_context_new|, and accessed via the |SCTX_APP| macro.
 * - Retains the list of all valid SeahorseKey objects. 
 * - Has a collection of SeahorseKeySource objects which add keys to the 
 *   SeahorseContext. 
 * 
 * Signals:
 *   added: A key was added to the context.
 *   removed: A key was removed from the context.
 *   changed: A key changed.
 *   destroy: The context was destroyed.
 */
 
#ifndef __SEAHORSE_CONTEXT_H__
#define __SEAHORSE_CONTEXT_H__

#include <gtk/gtk.h>
#include <gpgme.h>

#include "seahorse-key.h"
#include "seahorse-key-source.h"
#include "seahorse-dns-sd.h"

#define SEAHORSE_TYPE_CONTEXT			(seahorse_context_get_type ())
#define SEAHORSE_CONTEXT(obj)			(GTK_CHECK_CAST ((obj), SEAHORSE_TYPE_CONTEXT, SeahorseContext))
#define SEAHORSE_CONTEXT_CLASS(klass)		(GTK_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_CONTEXT, SeahorseContextClass))
#define SEAHORSE_IS_CONTEXT(obj)		(GTK_CHECK_TYPE ((obj), SEAHORSE_TYPE_CONTEXT))
#define SEAHORSE_IS_CONTEXT_CLASS(klass)	(GTK_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_CONTEXT))
#define SEAHORSE_CONTEXT_GET_CLASS(obj)		(GTK_CHECK_GET_CLASS ((obj), SEAHORSE_TYPE_CONTEXT, SeahorseContextClass))

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
    
    /* A key was added to this source */
    void (*added) (SeahorseContext *sctx, SeahorseKey *key);

    /* Removed a key from this source */
    void (*removed) (SeahorseContext *sctx, SeahorseKey *key);
    
    /* This key has changed */
    void (*changed) (SeahorseContext *sctx, SeahorseKey *skey, SeahorseKeyChange change);
};

enum SeahorseContextType {
    SEAHORSE_CONTEXT_APP = 1,
    SEAHORSE_CONTEXT_DAEMON = 2,
};

#define             SCTX_APP()                          (seahorse_context_app ())

GType               seahorse_context_get_type           (void);

SeahorseContext*    seahorse_context_app                (void);

SeahorseContext*    seahorse_context_new                (guint              flags,
                                                         guint              ktype);

void                seahorse_context_destroy            (SeahorseContext    *sctx);

#define             seahorse_context_is_daemon(ctx)     ((ctx)->is_daemon)

void                seahorse_context_add_key_source     (SeahorseContext    *sctx,
                                                         SeahorseKeySource  *sksrc);

void                seahorse_context_take_key_source    (SeahorseContext    *sctx,
                                                         SeahorseKeySource  *sksrc);

void                seahorse_context_remove_key_source  (SeahorseContext    *sctx,
                                                         SeahorseKeySource  *sksrc);

SeahorseKeySource*  seahorse_context_find_key_source    (SeahorseContext    *sctx,
                                                         GQuark             ktype,
                                                         SeahorseKeyLoc     location);

GSList*             seahorse_context_find_key_sources   (SeahorseContext    *sctx,
                                                         GQuark             ktype,
                                                         SeahorseKeyLoc     location);
                                                         

SeahorseKeySource*  seahorse_context_remote_key_source  (SeahorseContext    *sctx,
                                                         const gchar        *uri);

void                seahorse_context_add_key            (SeahorseContext    *sctx,
                                                         SeahorseKey        *skey);

void                seahorse_context_take_key           (SeahorseContext    *sctx, 
                                                         SeahorseKey        *skey);

guint               seahorse_context_get_count          (SeahorseContext    *sctx);

SeahorseKey*        seahorse_context_get_key            (SeahorseContext    *sctx,
                                                         SeahorseKeySource  *sksrc,
                                                         GQuark             keyid);

GList*              seahorse_context_get_keys           (SeahorseContext    *sctx, 
                                                         SeahorseKeySource  *sksrc);

SeahorseKey*        seahorse_context_find_key           (SeahorseContext    *sctx,
                                                         GQuark             keyid,
                                                         SeahorseKeyLoc     location);

GList*              seahorse_context_find_keys          (SeahorseContext    *sctx,
                                                         GQuark             ktype,
                                                         SeahorseKeyEType   etype,
                                                         SeahorseKeyLoc     location);

GList*              seahorse_context_find_keys_full     (SeahorseContext    *sctx,
                                                         SeahorseKeyPredicate *skpred);
                                                         
void                seahorse_context_remove_key         (SeahorseContext    *sctx,
                                                         SeahorseKey        *skey);

SeahorseServiceDiscovery*
                    seahorse_context_get_discovery      (SeahorseContext    *sctx);

SeahorseKey*        seahorse_context_get_default_key    (SeahorseContext    *sctx);

SeahorseOperation*  seahorse_context_load_local_keys    (SeahorseContext    *sctx);

SeahorseOperation*  seahorse_context_load_remote_key    (SeahorseContext    *sctx);

SeahorseOperation*  seahorse_context_load_remote_keys   (SeahorseContext    *sctx,
                                                         const gchar        *search);

SeahorseOperation*  seahorse_context_transfer_keys      (SeahorseContext    *sctx, 
                                                         GList              *keys, 
                                                         SeahorseKeySource  *to);

SeahorseOperation*  seahorse_context_retrieve_keys      (SeahorseContext    *sctx, 
                                                         GQuark             ktype, 
                                                         GSList             *keyids, 
                                                         SeahorseKeySource  *to);

GList*              seahorse_context_discover_keys      (SeahorseContext    *sctx, 
                                                         GQuark             ktype, 
                                                         GSList             *keyids);

SeahorseKey*        seahorse_context_key_from_dbus      (SeahorseContext    *sctx,
                                                         const gchar        *dbusid,
                                                         guint              *uid);

gchar*              seahorse_context_key_to_dbus        (SeahorseContext    *sctx,
                                                         SeahorseKey        *skey,
                                                         guint              uid);

gchar*              seahorse_context_keyid_to_dbus      (SeahorseContext    *sctx,
                                                         GQuark             keyid, 
                                                         guint              uid);

#endif /* __SEAHORSE_CONTEXT_H__ */
