/*
 * Seahorse
 *
 * Copyright (C) 2005-2006 Stefan Walter
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

#ifndef __SEAHORSE_SET_H__
#define __SEAHORSE_SET_H__

#include "seahorse-context.h"
#include "seahorse-object.h"
#include "seahorse-source.h"

#define SEAHORSE_TYPE_SET               (seahorse_set_get_type ())
#define SEAHORSE_SET(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_TYPE_SET, SeahorseSet))
#define SEAHORSE_SET_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_SET, SeahorseSetClass))
#define SEAHORSE_IS_SET(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_TYPE_SET))
#define SEAHORSE_IS_SET_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_SET))
#define SEAHORSE_SET_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_TYPE_SET, SeahorseSetClass))

typedef struct _SeahorseSet SeahorseSet;
typedef struct _SeahorseSetClass SeahorseSetClass;
typedef struct _SeahorseSetPrivate SeahorseSetPrivate;

/**
 * SeahorseSet:
 * @parent: The parent #GtkObject
 *
 * A subset list of the keys in the SeahorseContext.
 *
 * - Used all over by various views to narrow in on the keys that they're
 *   interested in.
 * - Originally was going to be called SeahorseView (database parlance) but
 *   that's far too confusing with overloaded terminology.
 * - Uses a SeahorseObjectPredicate to match keys.
 * - Sends out events when keys get added and removed from it's view. Or a
 *   key in the view changes etc...
 * - Supports per key event 'closures'. When a closure is set for a key, it's
 *   then passed as an argument to the 'changed' and 'removed' events.
 *
 * Signals:
 *   added: A key was added to this keyset.
 *   removed: A key disappeared from this keyset.
 *   changed: A key in the keyset changed.
 *   set-changed: The number of keys in the keyset changed
 *
 * Properties:
 *   count: The number of keys
 *   predicate: (SeahorseObjectPredicate) The predicate used for matching.
 */

struct _SeahorseSet {
	GObject parent;

	/*<private>*/
	SeahorseSetPrivate   *pv;
};

struct _SeahorseSetClass {
	GObjectClass parent_class;

    /* signals --------------------------------------------------------- */
    
    /* A key was added to this view */
    void (*added)   (SeahorseSet *skset, SeahorseObject *sobj);

    /* Removed a key from this view */
    void (*removed) (SeahorseSet *skset, SeahorseObject *sobj, gpointer closure);
    
    /* One of the key's attributes has changed */
    void (*changed) (SeahorseSet *skset, SeahorseObject *sobj, gpointer closure);
    
    /* The set of keys changed */
    void (*set_changed) (SeahorseSet *skset);
};

GType               seahorse_set_get_type              (void);

SeahorseSet*        seahorse_set_new                   (GQuark             ktype,
                                                        SeahorseUsage      usage,
                                                        SeahorseLocation   location,
                                                        guint              flags,
                                                        guint              nflags);

SeahorseSet*        seahorse_set_new_full               (SeahorseObjectPredicate *pred);

gboolean            seahorse_set_has_object             (SeahorseSet *skset,
                                                         SeahorseObject *sobj);

GList*              seahorse_set_get_objects            (SeahorseSet *skset);

guint               seahorse_set_get_count              (SeahorseSet *skset);

void                seahorse_set_refresh                (SeahorseSet *skset);

gpointer            seahorse_set_get_closure            (SeahorseSet *skset,
                                                         SeahorseObject *sobj);

void                seahorse_set_set_closure            (SeahorseSet *skset,
                                                         SeahorseObject *sobj,
                                                         gpointer closure);

#endif /* __SEAHORSE_SET_H__ */
