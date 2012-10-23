/*
 * Seahorse
 *
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
 *
 * Author: Stef Walter <stefw@collabora.co.uk>
 */

#ifndef __SEAHORSE_CERTIFICATE_DER_EXPORTER_H__
#define __SEAHORSE_CERTIFICATE_DER_EXPORTER_H__

#include "seahorse-common.h"

#include <glib-object.h>

#define SEAHORSE_TYPE_CERTIFICATE_DER_EXPORTER            (seahorse_certificate_der_exporter_get_type ())
#define SEAHORSE_CERTIFICATE_DER_EXPORTER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_TYPE_CERTIFICATE_DER_EXPORTER, SeahorseCertificateDerExporter))
#define SEAHORSE_IS_CERTIFICATE_DER_EXPORTER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_TYPE_CERTIFICATE_DER_EXPORTER))

typedef struct _SeahorseCertificateDerExporter SeahorseCertificateDerExporter;

GType                     seahorse_certificate_der_exporter_get_type     (void) G_GNUC_CONST;

SeahorseExporter *        seahorse_certificate_der_exporter_new          (SeahorseCertificate *certificate);

#endif /* __SEAHORSE_CERTIFICATE_DER_EXPORTER_H__ */
