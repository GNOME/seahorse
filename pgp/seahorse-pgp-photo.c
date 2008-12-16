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
 * along with this program; if not, write to the
 * Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include "seahorse-pgp.h"
#include "seahorse-pgp-photo.h"

#include <string.h>

#include <glib/gi18n.h>

enum {
	PROP_0,
	PROP_PIXBUF
};

G_DEFINE_TYPE (SeahorsePgpPhoto, seahorse_pgp_photo, G_TYPE_OBJECT);

struct _SeahorsePgpPhotoPrivate {
	GdkPixbuf *pixbuf;         
};

/* -----------------------------------------------------------------------------
 * OBJECT 
 */

static void
seahorse_pgp_photo_init (SeahorsePgpPhoto *self)
{
	self->pv = G_TYPE_INSTANCE_GET_PRIVATE (self, SEAHORSE_TYPE_PGP_PHOTO, SeahorsePgpPhotoPrivate);
}

static void
seahorse_pgp_photo_get_property (GObject *object, guint prop_id,
                                  GValue *value, GParamSpec *pspec)
{
	SeahorsePgpPhoto *self = SEAHORSE_PGP_PHOTO (object);
	
	switch (prop_id) {
	case PROP_PIXBUF:
		g_value_set_object (value, seahorse_pgp_photo_get_pixbuf (self));
		break;
	}
}

static void
seahorse_pgp_photo_set_property (GObject *object, guint prop_id, const GValue *value, 
                                  GParamSpec *pspec)
{
	SeahorsePgpPhoto *self = SEAHORSE_PGP_PHOTO (object);

	switch (prop_id) {
	case PROP_PIXBUF:
		seahorse_pgp_photo_set_pixbuf (self, g_value_get_object (value));
		break;
	}
}

static void
seahorse_pgp_photo_finalize (GObject *gobject)
{
	SeahorsePgpPhoto *self = SEAHORSE_PGP_PHOTO (gobject);

	if (self->pv->pixbuf)
		g_object_unref (self->pv->pixbuf);
	self->pv->pixbuf = NULL;
    
	G_OBJECT_CLASS (seahorse_pgp_photo_parent_class)->finalize (gobject);
}

static void
seahorse_pgp_photo_class_init (SeahorsePgpPhotoClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);    

	seahorse_pgp_photo_parent_class = g_type_class_peek_parent (klass);
	g_type_class_add_private (klass, sizeof (SeahorsePgpPhotoPrivate));
    
	gobject_class->finalize = seahorse_pgp_photo_finalize;
	gobject_class->set_property = seahorse_pgp_photo_set_property;
	gobject_class->get_property = seahorse_pgp_photo_get_property;
    
	g_object_class_install_property (gobject_class, PROP_PIXBUF,
	        g_param_spec_object ("pixbuf", "Pixbuf", "Photo Pixbuf",
	                             GDK_TYPE_PIXBUF, G_PARAM_READWRITE));
}

/* -----------------------------------------------------------------------------
 * PUBLIC 
 */

SeahorsePgpPhoto* 
seahorse_pgp_photo_new (GdkPixbuf *pixbuf) 
{
	return g_object_new (SEAHORSE_TYPE_PGP_PHOTO, "pixbuf", pixbuf, NULL);
}

GdkPixbuf*
seahorse_pgp_photo_get_pixbuf (SeahorsePgpPhoto *self)
{
	g_return_val_if_fail (SEAHORSE_IS_PGP_PHOTO (self), NULL);
	return self->pv->pixbuf;
}

void
seahorse_pgp_photo_set_pixbuf (SeahorsePgpPhoto *self, GdkPixbuf* pixbuf)
{
	g_return_if_fail (SEAHORSE_IS_PGP_PHOTO (self));

	if (self->pv->pixbuf)
		g_object_unref (self->pv->pixbuf);
	self->pv->pixbuf = pixbuf;
	if (self->pv->pixbuf)
		g_object_ref (self->pv->pixbuf);
	
	g_object_notify (G_OBJECT (self), "pixbuf");
}
