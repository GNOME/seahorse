/*
 * Seahorse
 *
 * Copyright (C) 2003 Jacob Perkins
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

#include "seahorse-context.h"
#include "seahorse-key-manager.h"
#include "seahorse-preferences.h"

/* Initializes context and preferences, then loads key manager */
int
main (int argc, char **argv)
{
	SeahorseContext *sctx;

#ifdef ENABLE_NLS	
	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);
#endif

	gnome_program_init (PACKAGE, VERSION, LIBGNOMEUI_MODULE,
		argc, argv, GNOME_PARAM_APP_DATADIR, DATA_DIR, NULL);
	
	sctx = seahorse_context_new ();
	seahorse_key_manager_show (sctx);
	gtk_main ();
	
	return 0;
}
