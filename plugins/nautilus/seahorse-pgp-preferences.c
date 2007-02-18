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

#include <config.h>
#include <gnome.h>
#include <locale.h>

#include "seahorse-context.h"
#include "seahorse-prefs.h"
#include "seahorse-gtkstock.h"

#ifdef WITH_AGENT
static gboolean show_cache = FALSE;
#endif
#ifdef WITH_SHARING
static gboolean show_sharing = FALSE;
#endif
#ifdef WITH_APPLET
static gboolean show_applet = FALSE;
#endif

static const GOptionEntry options[] = {
#ifdef WITH_AGENT    
	{ "cache", 'c', 0, G_OPTION_ARG_NONE, &show_cache,
	    N_("For internal use"), NULL },
#endif 
#ifdef WITH_SHARING   
	{ "sharing", 's', 0, G_OPTION_ARG_NONE, &show_sharing, 
	    N_("For internal use"), NULL },
#endif 
#ifdef WITH_APPLET   
	{ "applet", 's', 0, G_OPTION_ARG_NONE, &show_applet, 
	    N_("For internal use"), NULL },
#endif 
    { NULL }
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
    SeahorseOperation *op;
    GOptionContext *octx = NULL;

    bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
    bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
    textdomain (GETTEXT_PACKAGE);

    octx = g_option_context_new ("");
    g_option_context_add_main_entries (octx, options, GETTEXT_PACKAGE);

    gnome_program_init ("seahorse-preferences", VERSION, LIBGNOMEUI_MODULE, argc, argv,
                GNOME_PARAM_GOPTION_CONTEXT, octx,
                GNOME_PARAM_HUMAN_READABLE_NAME, _("Encryption Preferences"),
                GNOME_PARAM_APP_DATADIR, DATA_DIR, NULL);

    /* Insert Icons into Stock */ 
    seahorse_gtkstock_init();
    
    /* The default SeahorseContext */
    seahorse_context_new (SEAHORSE_CONTEXT_APP, 0);
    op = seahorse_context_load_local_keys (SCTX_APP ());
    
    /* Let operation take care of itself */
    g_object_unref (op);
   
    swidget = seahorse_prefs_new ();
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
#ifdef WITH_APPLET	
    if (show_applet) {
        GtkWidget *tab = glade_xml_get_widget (swidget->xml, "applet-tab");
        seahorse_prefs_select_tab (swidget, tab);
    }
#endif       
	gtk_main();
	return 0;
}
