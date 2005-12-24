/*
 * Copyright (C) 2005 Adam Schreiber <sadam@clemson.edu>
 * Copyright (C) 1999 Dave Camp <dave@davec.dhs.org>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *  
 */

#ifndef __SEAHORSE_APPLET_H__
#define __SEAHORSE_APPLET_H__

#include <glib.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gtk/gtktooltips.h>
#include <gtk/gtkwidget.h>
#include <panel-applet.h>

#define SEAHORSE_TYPE_APPLET           (seahorse_applet_get_type ())
#define SEAHORSE_APPLET(obj)           (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_TYPE_APPLET, SeahorseApplet))
#define SEAHORSE_APPLET_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST    ((klass), SEAHORSE_TYPE_APPLET, SeahorseAppletClass))
#define IS_SEAHORSE_APPLET(obj)        (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TYPE_SEAHORSE_APPLET))
#define IS_SEAHORSE_APPLET_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE    ((klass), SEAHORSE_TYPE_APPLET))
#define SEAHORSE_APPLET_GET_CLASS(klass) (G_TYPE_INSTANCE_GET_CLASS  ((klass), SEAHORSE_TYPE_APPLET, SeahorseAppletClass))

typedef struct _SeahorseApplet      SeahorseApplet;
typedef struct _SeahorseAppletClass SeahorseAppletClass;

struct _SeahorseApplet {
  PanelApplet parent;
};

struct _SeahorseAppletClass {
  PanelAppletClass parent_class;
};

GType seahorse_applet_get_type (void);

SeahorseApplet* seahorse_applet_new (void);

void setup_applet   (SeahorseApplet *seahorse_applet);

void destroy_applet (SeahorseApplet *seahorse_applet);

void properties_cb(BonoboUIComponent *uic,
                    SeahorseApplet    *seahorse_applet,
		            const gchar       *verbname);
		            
#endif
