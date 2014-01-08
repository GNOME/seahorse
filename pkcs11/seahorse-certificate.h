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
 * License along with this program; if not, see
 * <http://www.gnu.org/licenses/>.
 */

#ifndef __SEAHORSE_CERTIFICATE_H__
#define __SEAHORSE_CERTIFICATE_H__

#include <gck/gck.h>

#include <glib-object.h>

#define SEAHORSE_TYPE_CERTIFICATE               (seahorse_certificate_get_type ())
#define SEAHORSE_CERTIFICATE(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_TYPE_CERTIFICATE, SeahorseCertificate))
#define SEAHORSE_CERTIFICATE_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_CERTIFICATE, SeahorseCertificateClass))
#define SEAHORSE_IS_CERTIFICATE(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_TYPE_CERTIFICATE))
#define SEAHORSE_IS_CERTIFICATE_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_CERTIFICATE))
#define SEAHORSE_CERTIFICATE_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_TYPE_CERTIFICATE, SeahorseCertificateClass))

typedef struct _SeahorseCertificate SeahorseCertificate;
typedef struct _SeahorseCertificateClass SeahorseCertificateClass;
typedef struct _SeahorseCertificatePrivate SeahorseCertificatePrivate;
typedef struct _SeahorsePrivateKey SeahorsePrivateKey;

struct _SeahorseCertificate {
	GckObject parent;
	SeahorseCertificatePrivate *pv;
};

struct _SeahorseCertificateClass {
	GckObjectClass parent_class;
};

GType                 seahorse_certificate_get_type               (void) G_GNUC_CONST;

GIcon *               seahorse_certificate_get_icon               (SeahorseCertificate *self);

const gchar *         seahorse_certificate_get_description        (SeahorseCertificate *self);

SeahorsePrivateKey *  seahorse_certificate_get_partner            (SeahorseCertificate *self);

void                  seahorse_certificate_set_partner            (SeahorseCertificate *self,
                                                                   SeahorsePrivateKey *private_key);

#endif /* __SEAHORSE_CERTIFICATE_H__ */
