/*
 * Seahorse
 *
 * Copyright (C) 2008 Stefan Walter
 * Copyright (C) 2011 Collabora Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2 of the licence or (at
 * your option) any later version.
 *
 * See the included COPYING file for more information.
 */

#ifndef __SEAHORSE_KEYSERVER_RESULTS_H__
#define __SEAHORSE_KEYSERVER_RESULTS_H__

#include <glib.h>
#include <glib-object.h>
#include <gtk/gtk.h>

#include "seahorse-common.h"
#include "seahorse-object.h"

G_BEGIN_DECLS


#define SEAHORSE_TYPE_KEYSERVER_RESULTS              (seahorse_keyserver_results_get_type ())
#define SEAHORSE_KEYSERVER_RESULTS(obj)              (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_TYPE_KEYSERVER_RESULTS, SeahorseKeyserverResults))
#define SEAHORSE_KEYSERVER_RESULTS_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_KEYSERVER_RESULTS, SeahorseKeyserverResultsClass))
#define SEAHORSE_IS_KEYSERVER_RESULTS(obj)           (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_TYPE_KEYSERVER_RESULTS))
#define SEAHORSE_IS_KEYSERVER_RESULTS_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_KEYSERVER_RESULTS))
#define SEAHORSE_KEYSERVER_RESULTS_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_TYPE_KEYSERVER_RESULTS, SeahorseKeyserverResultsClass))

typedef struct _SeahorseKeyserverResults SeahorseKeyserverResults;
typedef struct _SeahorseKeyserverResultsClass SeahorseKeyserverResultsClass;
typedef struct _SeahorseKeyserverResultsPrivate SeahorseKeyserverResultsPrivate;

struct _SeahorseKeyserverResults {
	SeahorseCatalog parent_instance;
	SeahorseKeyserverResultsPrivate *pv;
};

struct _SeahorseKeyserverResultsClass {
	SeahorseCatalogClass parent_class;
};

GType            seahorse_keyserver_results_get_type         (void);

void             seahorse_keyserver_results_show             (const gchar *search_text);

const gchar*     seahorse_keyserver_results_get_search       (SeahorseKeyserverResults* self);


G_END_DECLS

#endif
