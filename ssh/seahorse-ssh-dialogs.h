#ifndef SEAHORSESSHDIALOGS_H_
#define SEAHORSESSHDIALOGS_H_

#include <gtk/gtk.h>

#include "seahorse-ssh-key.h"
#include "seahorse-ssh-source.h"

void        seahorse_ssh_upload_prompt         (GList *keys,
                                                GtkWindow *parent);

void        seahorse_ssh_key_properties_show   (SeahorseSSHKey *skey,
                                                GtkWindow *parent);

void        seahorse_ssh_generate_show         (SeahorseSSHSource *sksrc,
                                                GtkWindow *parent);

void        seahorse_ssh_generate_register     (void);

#endif /*SEAHORSESSHDIALOGS_H_*/
