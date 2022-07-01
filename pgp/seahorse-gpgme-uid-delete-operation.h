/*
 * Seahorse
 *
 * Copyright (C) 2011 Collabora Ltd.
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
 *
 * Author: Stef Walter <stefw@collabora.co.uk>
 */

#pragma once

#include <glib-object.h>

#include "seahorse-common.h"
#include "seahorse-gpgme-uid.h"

#define SEAHORSE_GPGME_TYPE_UID_DELETE_OPERATION (seahorse_gpgme_uid_delete_operation_get_type ())
G_DECLARE_FINAL_TYPE (SeahorseGpgmeUidDeleteOperation,
                      seahorse_gpgme_uid_delete_operation,
                      SEAHORSE_GPGME, UID_DELETE_OPERATION,
                      SeahorseDeleteOperation)

SeahorseGpgmeUidDeleteOperation *
                      seahorse_gpgme_uid_delete_operation_new       (SeahorseGpgmeUid *uid);

void                  seahorse_gpgme_uid_delete_operation_add_uid   (SeahorseGpgmeUidDeleteOperation *self,
                                                                     SeahorseGpgmeUid                *uid);
