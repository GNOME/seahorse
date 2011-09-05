/*
 * Seahorse
 *
 * Copyright (C) 2003 Jacob Perkins
 * Copyright (C) 2004-2005 Stefan Walter
 * Copyright (C) 2011 Collabora Ltd.
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
struct _SeahorsePredicate;

typedef struct _SeahorseContext SeahorseContext;
typedef struct _SeahorseContextClass SeahorseContextClass;
typedef struct _SeahorseContextPrivate SeahorseContextPrivate;

/**
 * SeahorseContext:
 * @parent: The parent #GObject
 * @is_daemon: a #gboolean indicating whether the context is being used in a
 *             program that is daemonized
 *
 * This is where all the action in a Seahorse process comes together.
 *
 * - Usually there's only one #SeahorseContext per process created by passing
 *   %SEAHORSE_CONTEXT_APP to seahorse_context_new(), and accessed via
 *   the %SCTX_APP macro.
 * - Retains the list of all valid struct _SeahorseObject objects.
 * - Has a collection of #SeahorseSource objects which add objects to the
 *   #SeahorseContext.
 *
 * Signals:
 *   added: A object was added to the context.
 *   removed: A object was removed from the context.
 *   changed: A object changed.
 *   destroy: The context was destroyed.
 */

struct _SeahorseContext {
	GObject parent;

	/*< public >*/
	gboolean is_daemon;

	/*< private >*/
	SeahorseContextPrivate  *pv;
};

struct _SeahorseContextClass {
	GObjectClass parent_class;

	/* signals --------------------------------------------------------- */

	/* A object was added to this source */
	void     (*added)              (SeahorseContext *self,
	                                struct _SeahorseObject *sobj);

	/* Removed a object from this source */
	void     (*removed)            (SeahorseContext *self,
	                                struct _SeahorseObject *sobj);

	/* This object has changed */
	void     (*changed)            (SeahorseContext *self,
	                                struct _SeahorseObject *sobj);

	void     (*destroy)            (SeahorseContext *self);
};

typedef void       (*SeahorseObjectFunc)                     (struct _SeahorseObject *obj,
                                                              gpointer user_data);

#define             SCTX_APP()                               (seahorse_context_instance ())

GType               seahorse_context_get_type                (void);

SeahorseContext*    seahorse_context_instance                (void);

void                seahorse_context_create                  (void);

void                seahorse_context_destroy                 (SeahorseContext *self);

#define             seahorse_context_is_daemon(ctx)          ((ctx)->is_daemon)

void                seahorse_context_add_source              (SeahorseContext *self,
                                                              SeahorseSource *sksrc);

void                seahorse_context_take_source             (SeahorseContext *self,
                                                              SeahorseSource *sksrc);

void                seahorse_context_remove_source           (SeahorseContext *self,
                                                              SeahorseSource *sksrc);

SeahorseSource*     seahorse_context_find_source             (SeahorseContext *self,
                                                              GQuark ktype,
                                                              SeahorseLocation location);

GList*             seahorse_context_find_sources             (SeahorseContext    *sctx,
                                                              GQuark             ktype,
                                                              SeahorseLocation   location);

SeahorseSource*     seahorse_context_remote_source           (SeahorseContext *self,
                                                              const gchar *uri);

void                seahorse_context_add_object              (SeahorseContext *self,
                                                              struct _SeahorseObject *sobj);

void                seahorse_context_take_object             (SeahorseContext *self,
                                                              struct _SeahorseObject *sobj);

guint               seahorse_context_get_count               (SeahorseContext *self);

struct _SeahorseObject*     seahorse_context_get_object      (SeahorseContext *self,
                                                              SeahorseSource *sksrc,
                                                              GQuark id);

GList*              seahorse_context_get_objects             (SeahorseContext *self,
                                                              SeahorseSource *sksrc);

struct _SeahorseObject*     seahorse_context_find_object     (SeahorseContext *self,
                                                              GQuark id,
                                                              SeahorseLocation location);

GList*              seahorse_context_find_objects            (SeahorseContext *self,
                                                              GQuark ktype,
                                                              SeahorseUsage usage,
                                                              SeahorseLocation location);

GList*              seahorse_context_find_objects_full       (SeahorseContext *self,
                                                              struct _SeahorsePredicate *skpred);

void                seahorse_context_for_objects_full        (SeahorseContext *self,
                                                              struct _SeahorsePredicate *skpred,
                                                              SeahorseObjectFunc func,
                                                              gpointer user_data);

void                seahorse_context_remove_object           (SeahorseContext *self,
                                                              struct _SeahorseObject *sobj);

SeahorseServiceDiscovery*
                    seahorse_context_get_discovery           (SeahorseContext    *self);

struct _SeahorseObject*   
                    seahorse_context_get_default_key         (SeahorseContext    *self);

void                seahorse_context_refresh_auto            (SeahorseContext *self);

void                seahorse_context_search_remote_async     (SeahorseContext *self,
                                                              const gchar *search,
                                                              GCancellable *cancellable,
                                                              GAsyncReadyCallback callback,
                                                              gpointer user_data);

GList *             seahorse_context_search_remote_finish    (SeahorseContext *self,
                                                              GAsyncResult *result,
                                                              GError **error);

void                seahorse_context_transfer_objects_async  (SeahorseContext *self,
                                                              GList *objects,
                                                              SeahorseSource *to,
                                                              GCancellable *cancellable,
                                                              GAsyncReadyCallback callback,
                                                              gpointer user_data);

gboolean            seahorse_context_transfer_objects_finish (SeahorseContext *self,
                                                              GAsyncResult *result,
                                                              GError **error);

void                seahorse_context_retrieve_objects_async  (SeahorseContext *self,
                                                              GQuark ktype,
                                                              GList *ids,
                                                              SeahorseSource *to,
                                                              GCancellable *cancellable,
                                                              GAsyncReadyCallback callback,
                                                              gpointer user_data);

gboolean            seahorse_context_retrieve_objects_finish (SeahorseContext *self,
                                                              GAsyncResult *result,
                                                              GError **error);

GList*              seahorse_context_discover_objects        (SeahorseContext *self,
                                                              GQuark ktype,
                                                              GList *rawids,
                                                              GCancellable *cancellable);

typedef GQuark     (*SeahorseCanonizeFunc)                   (const gchar *id);

GQuark              seahorse_context_canonize_id             (GQuark ktype,
                                                              const gchar *id);

GSettings *         seahorse_context_settings           (SeahorseContext *self);

GSettings *         seahorse_context_pgp_settings       (SeahorseContext *self);

#endif /* __SEAHORSE_CONTEXT_H__ */
