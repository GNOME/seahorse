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

static struct StockIcon
{
	const char *name;
	const char *dir;
	const char *filename;

} const stock_icons[] =
{
    { SEAHORSE_STOCK_KEY,         "seahorse",   "seahorse-key.png"         },
    { SEAHORSE_STOCK_PERSON,      "seahorse",   "seahorse-person.png"      },
    { SEAHORSE_STOCK_SEAHORSE,    NULL,         "seahorse.png"             },
    { SEAHORSE_STOCK_SECRET,      "seahorse",   "seahorse-secret.png"      },
    { SEAHORSE_STOCK_SHARE_KEYS,  "seahorse",   "seahorse-share-keys.png"  }
};

static gchar *
find_file(const char *dir, const char *base)
{
	char *filename;

	if (base == NULL)
		return NULL;

	if (dir == NULL)
		filename = g_build_filename(DATA_DIR, "pixmaps", base, NULL);
	else
	{
		filename = g_build_filename(DATA_DIR, "pixmaps", dir,
									base, NULL);
	}

	if (!g_file_test(filename, G_FILE_TEST_EXISTS))
	{
		g_critical("Unable to load stock pixmap %s\n", base);

		g_free(filename);

		return NULL;
	}

	return filename;
}

void
seahorse_gtk_stock_init(void)
{
	static gboolean stock_initted = FALSE;
	GtkIconFactory *icon_factory;
	int i;
	GtkWidget *win;

	if (stock_initted)
		return;

	stock_initted = TRUE;

	/* Setup the icon factory. */
	icon_factory = gtk_icon_factory_new();

	gtk_icon_factory_add_default(icon_factory);

	/* Er, yeah, a hack, but it works. :) */
	win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_widget_realize(win);

	for (i = 0; i < G_N_ELEMENTS(stock_icons); i++)
	{
		GdkPixbuf *pixbuf;
		GtkIconSet *iconset;
		gchar *filename;

		filename = find_file(stock_icons[i].dir, stock_icons[i].filename);

		if (filename == NULL)
			continue;

		pixbuf = gdk_pixbuf_new_from_file(filename, NULL);

		g_free(filename);

		iconset = gtk_icon_set_new_from_pixbuf(pixbuf);

		g_object_unref(G_OBJECT(pixbuf));

		gtk_icon_factory_add(icon_factory, stock_icons[i].name, iconset);

		gtk_icon_set_unref(iconset);
	}

	gtk_widget_destroy(win);

	g_object_unref(G_OBJECT(icon_factory));
}
