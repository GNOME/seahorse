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

#include <gnome.h>
#include <stdio.h>
#include <stdlib.h>

#include "seahorse-libdialogs.h"
#include "seahorse-secure-memory.h"

int main (int argc, char* argv[])
{
    GtkDialog *dialog;
    const gchar *pass;
    gchar *message;
    int ret = -1;
    
    seahorse_secure_memory_init (65536);

    /* We need to drop privileges completely for security */
#if defined(HAVE_SETRESUID) && defined(HAVE_SETRESGID)

    /* Not in header files for all OSs, even where present */
    int setresuid(uid_t ruid, uid_t euid, uid_t suid);
    int setresgid(gid_t rgid, gid_t egid, gid_t sgid);
  
    if (setresuid (getuid (), getuid (), getuid ()) == -1 ||
        setresgid (getgid (), getgid (), getgid ()) == -1)
#else
    if (setuid (getuid ()) == -1 || setgid (getgid ()) == -1)
#endif
        g_error (_("couldn't drop privileges properly"));

    gtk_init (&argc, &argv);

    /* Non buffered stdout */
    setvbuf(stdout, 0, _IONBF, 0);
    
    if (argc > 1)
        message = g_strjoinv(" ", argv + 1);
    else 
        message = g_strdup("Enter your SSH passphrase:");

    dialog = seahorse_passphrase_prompt_show (_("SSH Passphrase"), message, NULL, NULL);
    g_free (message);
    
    if (gtk_dialog_run (dialog) == GTK_RESPONSE_ACCEPT) {
        pass = seahorse_passphrase_prompt_get (dialog);
        write (1, pass, strlen (pass));
        ret = 0;
    }
    
    gtk_widget_destroy (GTK_WIDGET (dialog));
    return ret;
}
