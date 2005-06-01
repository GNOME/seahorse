/*
 * Seahorse
 *
 * Copyright (C) 2004 Nate Nielsen
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
#include <config.h>
#include <locale.h>

#include "config.h"
#include "seahorse-context.h"
#include "seahorse-prefs.h"

#ifdef WITH_AGENT
static gboolean show_cache = FALSE;
#endif
#ifdef WITH_SHARING
static gboolean show_sharing = FALSE;
#endif

static const struct poptOption options[] = {
#ifdef WITH_AGENT    
	{ "cache", 'c', POPT_ARG_NONE | POPT_ARG_VAL, &show_cache, TRUE,
	    N_("For internal use"), NULL },
#endif 
#ifdef WITH_SHARING   
	{ "sharing", 's', POPT_ARG_NONE | POPT_ARG_VAL, &show_sharing, TRUE,
	    N_("For internal use"), NULL },
#endif 
        
	POPT_AUTOHELP
	POPT_TABLEEND
};

static void
destroyed (GtkObject *object, gpointer data)
{
	gtk_exit (0);
}

int
main (int argc, char **argv)
{
	SeahorseWidget *swidget;
	SeahorseContext *sctx;

    gnome_program_init (PACKAGE, VERSION, LIBGNOMEUI_MODULE, argc, argv,
                GNOME_PARAM_POPT_TABLE, options,
                GNOME_PARAM_HUMAN_READABLE_NAME, _("Encryption Preferences"),
                GNOME_PARAM_APP_DATADIR, DATA_DIR, NULL);
	
	sctx = seahorse_context_new ();
    seahorse_context_load_keys (sctx, TRUE);
   
    swidget = seahorse_prefs_new (sctx);
	g_signal_connect (seahorse_widget_get_top (swidget), "destroy", 
                      G_CALLBACK (destroyed), NULL);

#ifdef WITH_AGENT	
    if (show_cache) {
        GtkWidget *tab = glade_xml_get_widget (swidget->xml, "cache-tab");
        seahorse_prefs_select_tab (swidget, tab);
    }
#endif
#ifdef WITH_SHARING	
    if (show_sharing) {
        GtkWidget *tab = glade_xml_get_widget (swidget->xml, "sharing-tab");
        seahorse_prefs_select_tab (swidget, tab);
    }
#endif
    
	gtk_main();
	return 0;
}
