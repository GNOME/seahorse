/**
 * @file seahorse-gtkstock.c GTK+ Stock resources
 *
 * Seahorse
 *
 * Gaim is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with the gaim
 * source distribution.
 *
 * Also (c) 2005 Adam Schreiber <sadam@clemson.edu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <gtk/gtk.h>
 
#include "seahorse-gtkstock.h"

static const SeahorseStockIcon seahorse_icons[] = {
    { SEAHORSE_STOCK_KEY,         "seahorse",   "seahorse-key.png"         },
    { SEAHORSE_STOCK_PERSON,      "seahorse",   "seahorse-person.png"      },
    { SEAHORSE_STOCK_SIGN,        "seahorse",   "seahorse-sign.svg"        },
    { SEAHORSE_STOCK_SEAHORSE,    NULL,         "seahorse.png"             },
    { SEAHORSE_STOCK_SECRET,      "seahorse",   "seahorse-secret.png"      },
    { SEAHORSE_STOCK_SHARE_KEYS,  "seahorse",   "seahorse-share-keys.png"  },
    { SEAHORSE_STOCK_KEY_SSH,     "seahorse",   "seahorse-key-ssh.png"     },
    { SEAHORSE_STOCK_KEY_SSH_LRG, "seahorse",   "seahorse-key-ssh-large.png" },
    { NULL }
};

static gchar *
find_file(const char *dir, const char *base)
{
    char *filename;

    if (base == NULL)
        return NULL;

    if (dir == NULL)
        filename = g_build_filename (DATA_DIR, "pixmaps", base, NULL);
    else 
        filename = g_build_filename (DATA_DIR, "pixmaps", dir, base, NULL);

    if (!g_file_test (filename, G_FILE_TEST_EXISTS)) {
        g_critical ("Unable to load stock pixmap %s\n", base);
        g_free (filename);
        return NULL;
    }

    return filename;
}

void
seahorse_gtkstock_init(void)
{
    static gboolean stock_initted = FALSE;

    if (stock_initted)
        return;

    stock_initted = TRUE;
    seahorse_gtkstock_add_icons (seahorse_icons);
}

void
seahorse_gtkstock_add_icons (const SeahorseStockIcon *icons)
{
    GtkIconFactory *factory;
    GtkIconSource *source;
    GtkIconSet *iconset;
    GtkWidget *win;
    gchar *filename;

    /* Setup the icon factory. */
    factory = gtk_icon_factory_new ();
    gtk_icon_factory_add_default (factory);
    
    /* Er, yeah, a hack, but it works. :) */
    win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_widget_realize(win);
    
    /* TODO: Implement differently sized icons here */

    for ( ; icons->id; icons++) {
    
        filename = find_file(icons->dir, icons->filename);
        if (filename == NULL)
            continue;
        
        source = gtk_icon_source_new ();
        gtk_icon_source_set_filename (source, filename);
        gtk_icon_source_set_direction_wildcarded (source, TRUE);
        gtk_icon_source_set_size_wildcarded (source, TRUE);
        gtk_icon_source_set_state_wildcarded (source, TRUE);
            
        g_free(filename);
        
        iconset = gtk_icon_set_new ();
        gtk_icon_set_add_source (iconset, source);
        gtk_icon_source_free (source);
        
        gtk_icon_factory_add (factory, icons->id, iconset);
        gtk_icon_set_unref (iconset);
    }
    
    gtk_widget_destroy (win);
    g_object_unref (factory);
}
