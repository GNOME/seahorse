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
 
#include "config.h"
#include <gnome.h>
#include <stdio.h>
#include <stdlib.h>

static FILE* seahorse_link = NULL;
 
static gchar* 
askpass_command (const gchar *cmd, const gchar *arg)
{
    const gchar* env;
    gchar *t;
    int fd;
        
    /* Try an open the connection with seahorse */
    if (!seahorse_link) {
        env = g_getenv ("SEAHORSE_SSH_ASKPASS_FD");
        if (env == NULL)
            return NULL;
        g_unsetenv ("SEAHORSE_SSH_ASKPASS_FD");
        
        fd = strtol (env, &t, 10);
        if (*t) {
            g_warning ("fd received from seahorse was not valid: %s", env);
            return NULL;
        }
        
        seahorse_link = fdopen (fd, "r+b");
        if (!seahorse_link) {
            g_warning ("couldn't open fd %d: %s", fd, strerror (errno));
            return NULL;
        }
        
        setvbuf(seahorse_link, 0, _IONBF, 0);
    }
    
    /* Request a setting be sent */
    fprintf (seahorse_link, "%s %s\n", cmd, arg ? arg : "");
    fflush (seahorse_link);
    
    /* Read the setting */
    t = g_new0 (gchar, 512);
    fgets (t, 512, seahorse_link);
    
    /* Make sure it worked */
    if (ferror (seahorse_link)) {
        g_warning ("error reading from seahorse");
        fclose (seahorse_link);
        seahorse_link = NULL;
        g_free (t);
        return NULL;
    }
    
    return t;
}

int main (int argc, char* argv[])
{
    gchar *pass, *message, *p;
    
    gtk_init (&argc, &argv);

    /* Non buffered stdout */
    setvbuf(stdout, 0, _IONBF, 0);

    if (argc > 1)
        message = g_strjoinv (" ", argv + 1);
    else 
        message = g_strdup ("Enter your Secure Shell passphrase:");

    /* Check if we're being handed a password from seahorse */
    pass = askpass_command ("PASSWORD", message);
    g_free (message);
    
    if (pass == NULL)
        return 1;
    
    write (1, pass, strlen (pass));
    for (p = pass; *p; p++) 
        *p = 0;
    g_free (pass);

    if (seahorse_link)
        fclose (seahorse_link);
    
    return 0;
}
