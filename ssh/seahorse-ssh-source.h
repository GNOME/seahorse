/*
 * Seahorse
 *
 * Copyright (C) 2005 Stefan Walter
 * Copyright (C) 2011 Collabora Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2 of the licence or (at
 * your option) any later version.
 *
 * See the included COPYING file for more information.
 */

/** 
 * SeahorseSSHSource: A key source for SSH keys. 
 * 
 * - Derived from SeahorseSource
 * - Lists all the keys in ~/.ssh/ by searching through every file.
 * - Loads public keys from ~/.ssh/authorized_keys and ~/.ssh/other_keys.seahorse
 * - Adds the keys it loads to the SeahorseContext.
 * - Monitors ~/.ssh for changes and reloads the key ring as necessary.
 * 
 * Properties:
 *  ktype: (GQuark) The ktype (ie: SEAHORSE_SSH) of keys originating from this 
           key source.
 *  location: (SeahorseLocation) The location of keys that come from this 
 *         source. (ie: SEAHORSE_LOCATION_LOCAL)
 */
 
#ifndef __SEAHORSE_SSH_SOURCE_H__
#define __SEAHORSE_SSH_SOURCE_H__

#include "seahorse-ssh-key.h"

#define SEAHORSE_TYPE_SSH_SOURCE            (seahorse_ssh_source_get_type ())
#define SEAHORSE_SSH_SOURCE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_TYPE_SSH_SOURCE, SeahorseSSHSource))
#define SEAHORSE_SSH_SOURCE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_SSH_SOURCE, SeahorseSSHSourceClass))
#define SEAHORSE_IS_SSH_SOURCE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_TYPE_SSH_SOURCE))
#define SEAHORSE_IS_SSH_SOURCE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_SSH_SOURCE))
#define SEAHORSE_SSH_SOURCE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_TYPE_SSH_SOURCE, SeahorseSSHSourceClass))

struct _SeahorseSSHKey;
typedef struct _SeahorseSSHSource SeahorseSSHSource;
typedef struct _SeahorseSSHSourceClass SeahorseSSHSourceClass;
typedef struct _SeahorseSSHSourcePrivate SeahorseSSHSourcePrivate;

struct _SeahorseSSHSource {
	GObject parent;
	SeahorseSSHSourcePrivate *priv;
};

struct _SeahorseSSHSourceClass {
	GObjectClass parent_class;
};

GType                seahorse_ssh_source_get_type           (void);

SeahorseSSHSource*   seahorse_ssh_source_new                (void);

struct _SeahorseSSHKey*
                     seahorse_ssh_source_key_for_filename   (SeahorseSSHSource *ssrc,
                                                             const gchar *privfile);

gchar*               seahorse_ssh_source_file_for_public    (SeahorseSSHSource *ssrc,
                                                             gboolean authorized);

gchar*               seahorse_ssh_source_file_for_algorithm (SeahorseSSHSource *ssrc,
                                                             guint algo);

void                 seahorse_ssh_source_remove_object      (SeahorseSSHSource *self,
                                                             SeahorseSSHKey *skey);

guchar*              seahorse_ssh_source_export_private     (SeahorseSSHSource *ssrc,
                                                             SeahorseSSHKey *skey,
                                                             gsize *n_results,
                                                             GError **err);

void                 seahorse_ssh_source_import_async       (SeahorseSSHSource *self,
                                                             GInputStream *input,
                                                             GtkWindow *transient_for,
                                                             GCancellable *cancellable,
                                                             GAsyncReadyCallback callback,
                                                             gpointer user_data);

GList *              seahorse_ssh_source_import_finish      (SeahorseSSHSource *self,
                                                             GAsyncResult *result,
                                                             GError **error);

#endif /* __SEAHORSE_SSH_SOURCE_H__ */
