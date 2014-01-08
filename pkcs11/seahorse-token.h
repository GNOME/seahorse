/*
 * Seahorse
 *
 * Copyright (C) 2008 Stefan Walter
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
 * along with this program; if not, see
 * <http://www.gnu.org/licenses/>.
 */


#ifndef __SEAHORSE_TOKEN_H__
#define __SEAHORSE_TOKEN_H__

#include <gck/gck.h>

#define SEAHORSE_TYPE_TOKEN            (seahorse_token_get_type ())
#define SEAHORSE_TOKEN(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_TYPE_TOKEN, SeahorseToken))
#define SEAHORSE_TOKEN_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_TOKEN, SeahorseTokenClass))
#define SEAHORSE_IS_TOKEN(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_TYPE_TOKEN))
#define SEAHORSE_IS_TOKEN_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_TOKEN))
#define SEAHORSE_TOKEN_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_TYPE_TOKEN, SeahorseTokenClass))

typedef struct _SeahorseToken SeahorseToken;
typedef struct _SeahorseTokenClass SeahorseTokenClass;
typedef struct _SeahorseTokenPrivate SeahorseTokenPrivate;

struct _SeahorseToken {
	GObject parent;
	SeahorseTokenPrivate *pv;
};

struct _SeahorseTokenClass {
	GObjectClass parent_class;
};

GType                  seahorse_token_get_type          (void);

SeahorseToken *        seahorse_token_new               (GckSlot *slot);

GckTokenInfo *         seahorse_token_get_info          (SeahorseToken *self);

GckSlot *              seahorse_token_get_slot          (SeahorseToken *self);

GckSession *           seahorse_token_get_session       (SeahorseToken *self);

void                   seahorse_token_set_session       (SeahorseToken *self,
                                                         GckSession *session);

GArray *               seahorse_token_get_mechanisms    (SeahorseToken *self);

gboolean               seahorse_token_has_mechanism     (SeahorseToken *self,
                                                         gulong mechanism);

void                   seahorse_token_remove_object     (SeahorseToken *self,
                                                         GckObject *object);

gboolean               seahorse_token_is_deletable      (SeahorseToken *self,
                                                         GckObject *object);

#endif /* __SEAHORSE_TOKEN_H__ */
