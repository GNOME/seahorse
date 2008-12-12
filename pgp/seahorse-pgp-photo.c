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
	PROP_PUBKEY,
	PROP_PIXBUF,
	PROP_INDEX
};

G_DEFINE_TYPE (SeahorsePgpPhoto, seahorse_pgp_photo, G_TYPE_OBJECT);

struct _SeahorsePgpPhotoPrivate {
	gpgme_key_t pubkey;        /* Key that this photo is on */
	GdkPixbuf *pixbuf;         /* The public key that this photo is part of */
	guint index;               /* The GPGME index of the photo */
};

/* -----------------------------------------------------------------------------
 * OBJECT 
 */

static void
seahorse_pgp_photo_init (SeahorsePgpPhoto *self)
{
	self->pv = G_TYPE_INSTANCE_GET_PRIVATE (self, SEAHORSE_TYPE_PGP_PHOTO, SeahorsePgpPhotoPrivate);
	self->pv->index = 0;
}

static GObject*
seahorse_pgp_photo_constructor (GType type, guint n_props, GObjectConstructParam *props)
{
	GObject *obj = G_OBJECT_CLASS (seahorse_pgp_photo_parent_class)->constructor (type, n_props, props);
	SeahorsePgpPhoto *self = NULL;
	
	if (obj) {
		self = SEAHORSE_PGP_PHOTO (obj);
		g_return_val_if_fail (self->pv->pubkey, NULL);
	}
	
	return obj;
}

static void
seahorse_pgp_photo_get_property (GObject *object, guint prop_id,
                                  GValue *value, GParamSpec *pspec)
{
	SeahorsePgpPhoto *self = SEAHORSE_PGP_PHOTO (object);
	
	switch (prop_id) {
	case PROP_PUBKEY:
		g_value_set_boxed (value, seahorse_pgp_photo_get_pubkey (self));
		break;
	case PROP_PIXBUF:
		g_value_set_object (value, seahorse_pgp_photo_get_pixbuf (self));
		break;
	case PROP_INDEX:
		g_value_set_uint (value, seahorse_pgp_photo_get_index (self));
		break;
	}
}

static void
seahorse_pgp_photo_set_property (GObject *object, guint prop_id, const GValue *value, 
                                  GParamSpec *pspec)
{
	SeahorsePgpPhoto *self = SEAHORSE_PGP_PHOTO (object);

	switch (prop_id) {
	case PROP_PUBKEY:
		g_return_if_fail (!self->pv->pubkey);
		self->pv->pubkey = g_value_get_boxed (value);
		if (self->pv->pubkey)
			gpgmex_key_ref (self->pv->pubkey);
		break;		
	case PROP_PIXBUF:
		seahorse_pgp_photo_set_pixbuf (self, g_value_get_object (value));
		break;
	case PROP_INDEX:
		seahorse_pgp_photo_set_index (self, g_value_get_uint (value));
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
    
	gobject_class->constructor = seahorse_pgp_photo_constructor;
	gobject_class->finalize = seahorse_pgp_photo_finalize;
	gobject_class->set_property = seahorse_pgp_photo_set_property;
	gobject_class->get_property = seahorse_pgp_photo_get_property;
    
	g_object_class_install_property (gobject_class, PROP_PUBKEY,
	        g_param_spec_boxed ("pubkey", "Public Key", "GPGME Public Key that this subkey is on",
	                            SEAHORSE_PGP_BOXED_KEY, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property (gobject_class, PROP_PIXBUF,
	        g_param_spec_object ("pixbuf", "Pixbuf", "Photo Pixbuf",
	                             GDK_TYPE_PIXBUF, G_PARAM_READWRITE));

	g_object_class_install_property (gobject_class, PROP_INDEX,
	        g_param_spec_uint ("index", "Index", "Index of photo UID",
	                           0, G_MAXUINT, 0, G_PARAM_READWRITE));
}

/* -----------------------------------------------------------------------------
 * PUBLIC 
 */

SeahorsePgpPhoto* 
seahorse_pgp_photo_new (gpgme_key_t pubkey, GdkPixbuf *pixbuf, guint index) 
{
	return g_object_new (SEAHORSE_TYPE_PGP_PHOTO,
	                     "pubkey", pubkey,
	                     "pixbuf", pixbuf, 
	                     "index", index, NULL);
}

gpgme_key_t
seahorse_pgp_photo_get_pubkey (SeahorsePgpPhoto *self)
{
	g_return_val_if_fail (SEAHORSE_IS_PGP_PHOTO (self), NULL);
	g_return_val_if_fail (self->pv->pubkey, NULL);
	return self->pv->pubkey;
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

guint
seahorse_pgp_photo_get_index (SeahorsePgpPhoto *self)
{
	g_return_val_if_fail (SEAHORSE_IS_PGP_PHOTO (self), 0);
	return self->pv->index;
}

void
seahorse_pgp_photo_set_index (SeahorsePgpPhoto *self, guint index)
{
	g_return_if_fail (SEAHORSE_IS_PGP_PHOTO (self));

	self->pv->index = index;
	g_object_notify (G_OBJECT (self), "index");
}
