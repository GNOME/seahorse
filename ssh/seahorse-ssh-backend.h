/*
 * Seahorse
 *
 * Copyright (C) 2008 Stefan Walter
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2 of the licence or (at
 * your option) any later version.
 *
 * See the included COPYING file for more information.
 */

#ifndef SEAHORSE_SSH_BACKEND_H_
#define SEAHORSE_SSH_BACKEND_H_

#include "seahorse-ssh.h"
#include "seahorse-ssh-source.h"

G_BEGIN_DECLS

#define SEAHORSE_SSH_STR                     "openssh"

#define SEAHORSE_TYPE_SSH_BACKEND            (seahorse_ssh_backend_get_type ())
#define SEAHORSE_SSH_BACKEND(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_TYPE_SSH_BACKEND, SeahorseSshBackend))
#define SEAHORSE_SSH_BACKEND_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_SSH_BACKEND, SeahorseSshBackendClass))
#define SEAHORSE_IS_SSH_BACKEND(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_TYPE_SSH_BACKEND))
#define SEAHORSE_IS_SSH_BACKEND_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_SSH_BACKEND))
#define SEAHORSE_SSH_BACKEND_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_TYPE_SSH_BACKEND, SeahorseSshBackendClass))

typedef struct _SeahorseSshBackend SeahorseSshBackend;
typedef struct _SeahorseSshBackendClass SeahorseSshBackendClass;

GType                 seahorse_ssh_backend_get_type      (void) G_GNUC_CONST;

SeahorseSshBackend *  seahorse_ssh_backend_get           (void);

SeahorseSSHSource *   seahorse_ssh_backend_get_dot_ssh   (SeahorseSshBackend *self);

#endif /*SEAHORSE_SSH_BACKEND_H_*/
