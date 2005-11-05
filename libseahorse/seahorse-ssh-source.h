/*
 * Seahorse
 *
 * Copyright (C) 2005 Nate Nielsen
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

/** 
 * SeahorseSSHSource: A key source for SSH keys. 
 * 
 * - Derived from SeahorseKeySource
 * - Lists all the keys in ~/.ssh/ by searching through every file.
 * - Adds the keys it loads to the SeahorseContext.
 * - Monitors ~/.ssh for changes and reloads the keyring as necessary.
 * 
 * Properties:
 *  ktype: (GQuark) The ktype (ie: SKEY_SSH) of keys originating from this 
           key source.
 *  location: (SeahorseKeyLoc) The location of keys that come from this 
 *         source. (ie: SKEY_LOC_LOCAL)
 */
 
#ifndef __SEAHORSE_SSH_SOURCE_H__
#define __SEAHORSE_SSH_SOURCE_H__

#include "seahorse-key-source.h"

#define SEAHORSE_TYPE_SSH_SOURCE            (seahorse_ssh_source_get_type ())
#define SEAHORSE_SSH_SOURCE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_TYPE_SSH_SOURCE, SeahorseSSHSource))
#define SEAHORSE_SSH_SOURCE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_SSH_SOURCE, SeahorseSSHSourceClass))
#define SEAHORSE_IS_SSH_SOURCE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_TYPE_SSH_SOURCE))
#define SEAHORSE_IS_SSH_SOURCE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_SSH_SOURCE))
#define SEAHORSE_SSH_SOURCE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_TYPE_SSH_SOURCE, SeahorseSSHSourceClass))

typedef struct _SeahorseSSHSource SeahorseSSHSource;
typedef struct _SeahorseSSHSourceClass SeahorseSSHSourceClass;
typedef struct _SeahorseSSHSourcePrivate SeahorseSSHSourcePrivate;

struct _SeahorseSSHSource {
    SeahorseKeySource parent;
    
    /*< private >*/
    SeahorseSSHSourcePrivate *priv;
};

struct _SeahorseSSHSourceClass {
    SeahorseKeySourceClass parent_class;
};

GType                seahorse_ssh_source_get_type       (void);

SeahorseSSHSource*   seahorse_ssh_source_new            (void);

SeahorseOperation*   seahorse_ssh_source_upload         (SeahorseSSHSource *ssrc,
                                                         GList *keys,
                                                         const gchar *username,
                                                         const gchar *hostname);

gchar*               seahorse_ssh_source_execute        (const gchar *command, 
                                                         GError **error);

SeahorseOperation*   seahorse_ssh_operation_new         (const gchar *command, 
                                                         const gchar *input, 
                                                         gint length,
                                                         const gchar *progress_label);

#endif /* __SEAHORSE_SSH_SOURCE_H__ */
