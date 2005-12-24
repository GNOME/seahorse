/*
 * Seahorse
 *
 * Copyright (C) 2002 Jacob Perkins
 * Copyright (C) 2004 Nate Nielsen
 * Copyright (C) 2005 Adam Schreiber
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

#include "seahorse-libdialogs.h"
#include "seahorse-widget.h"
#include "seahorse-util.h"
#include "seahorse-pgp-key.h"

/* Note that we can't use GTK stock here, as we hand these icons off 
   to other processes in the case of notifications */
#define ICON_PREFIX     DATA_DIR "/pixmaps/seahorse/"

void
seahorse_import_notify (guint keys)
{
    const gchar *icon = NULL;
    const gchar *summary;
    gchar *body;
    gboolean ok = TRUE;

    body = ngettext("Imported %i Key", "Imported %i Keys", keys);
    summary = ngettext("Key Imported", "Keys Imported", keys);
    body = g_strdup_printf(body, keys);
    icon = ICON_PREFIX "seahorse-key.png";

    seahorse_notification_display (summary, body, !ok, icon);
    
    g_free (body);
}
