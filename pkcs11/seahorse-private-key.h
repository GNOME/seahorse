/*
 * Seahorse
 *
 * Copyright (C) 2008 Stefan Walter
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#ifndef __SEAHORSE_PRIVATE_KEY_H__
#define __SEAHORSE_PRIVATE_KEY_H__

#include <gck/gck.h>

#include <glib-object.h>

#define SEAHORSE_TYPE_PRIVATE_KEY               (seahorse_private_key_get_type ())
#define SEAHORSE_PRIVATE_KEY(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_TYPE_PRIVATE_KEY, SeahorsePrivateKey))
#define SEAHORSE_PRIVATE_KEY_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_PRIVATE_KEY, SeahorsePrivateKeyClass))
#define SEAHORSE_IS_PRIVATE_KEY(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_TYPE_PRIVATE_KEY))
#define SEAHORSE_IS_PRIVATE_KEY_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_PRIVATE_KEY))
#define SEAHORSE_PRIVATE_KEY_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_TYPE_PRIVATE_KEY, SeahorsePrivateKeyClass))

typedef struct _SeahorsePrivateKey SeahorsePrivateKey;
typedef struct _SeahorsePrivateKeyClass SeahorsePrivateKeyClass;
typedef struct _SeahorsePrivateKeyPrivate SeahorsePrivateKeyPrivate;

struct _SeahorsePrivateKey {
	GckObject parent;
	SeahorsePrivateKeyPrivate *pv;
};

struct _SeahorsePrivateKeyClass {
	GckObjectClass parent_class;
};

GType                 seahorse_private_key_get_type           (void) G_GNUC_CONST;

#endif /* __SEAHORSE_PRIVATE_KEY_H__ */
