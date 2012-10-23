/*
 * Seahorse
 *
 * Copyright (C) 2003 Jacob Perkins
 * Copyright (C) 2004, 2005, 2006 Stefan Walter
 * Copyright (C) 2011 Collabora Ltd.
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

#include "seahorse-common.h"
#include "seahorse-place.h"
#include "seahorse-util.h"

#include "seahorse-key-manager-store.h"
#include "seahorse-prefs.h"
#include "seahorse-validity.h"

#include <string.h>
#include <unistd.h>

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#define DEBUG_FLAG SEAHORSE_DEBUG_DRAG
#include "seahorse-debug.h"

#include "eggtreemultidnd.h"

#define KEY_MANAGER_SORT_KEY "/apps/seahorse/listing/sort_by"

enum {
	PROP_0,
	PROP_MODE,
	PROP_FILTER,
	PROP_SETTINGS
};

enum {
	COL_ICON,
	COL_MARKUP,
	COL_LABEL,
	COL_DESCRIPTION,
	N_COLS
};

static GcrColumn columns[] = {
	{ "icon", /* later */ 0, /* later */ 0, NULL,
	  0, NULL, NULL },
	{ "markup", G_TYPE_STRING, G_TYPE_STRING, NULL,
	  0, NULL, NULL },
	{ "label", G_TYPE_STRING, G_TYPE_STRING, NULL,
	  GCR_COLUMN_SORTABLE, NULL, NULL },
	{ "description", G_TYPE_STRING, G_TYPE_STRING, NULL,
	  0, NULL, NULL },
	{ NULL }
};

enum {
	DRAG_INFO_TEXT,
	DRAG_INFO_XDS,
};

#define XDS_FILENAME "xds.txt"
#define MAX_XDS_ATOM_VAL_LEN 4096
#define XDS_ATOM   gdk_atom_intern  ("XdndDirectSave0", FALSE)
#define TEXT_ATOM  gdk_atom_intern  ("text/plain", FALSE)

static GtkTargetEntry store_targets[] = {
	{ "text/plain", 0, DRAG_INFO_TEXT },
	{ "XdndDirectSave0", 0, DRAG_INFO_XDS }
};

struct _SeahorseKeyManagerStorePriv {
	GSettings *settings;

    SeahorseKeyManagerStoreMode    filter_mode;
    gchar*                  filter_text;
    guint                   filter_stag;

	gchar *drag_destination;
	GError *drag_error;
	GList *drag_objects;
};

G_DEFINE_TYPE (SeahorseKeyManagerStore, seahorse_key_manager_store, GCR_TYPE_COLLECTION_MODEL);

/* Search through row for text */
static gboolean
object_contains_filtered_text (GObject *object,
                               const gchar* text)
{
	gchar* name = NULL;
	gchar* description = NULL;
	gchar* lower;
	gboolean ret = FALSE;

	/* Empty search text results in a match */
	if (!text || !text[0])
		return TRUE;

	g_object_get (object, "label", &name, NULL);
	if (name != NULL) {
		lower = g_utf8_strdown (name, -1);
		if (strstr (lower, text))
			ret = TRUE;
		g_free (lower);
		g_free (name);
	}

	if (!ret && g_object_class_find_property (G_OBJECT_GET_CLASS (object), "description")) {
		g_object_get (object, "description", &description, NULL);
		if (description != NULL) {
			lower = g_utf8_strdown (description, -1);
			if (strstr (lower, text))
				ret = TRUE;
			g_free (lower);
			g_free (description);
		}
	}

	return ret;
}

/* Called to filter each row */
static gboolean
on_filter_visible (GObject *obj,
                   gpointer user_data)
{
	SeahorseKeyManagerStore* self = SEAHORSE_KEY_MANAGER_STORE (user_data);
	GList *children, *l;
	gboolean ret = FALSE;

	/* Check the row requested */
	switch (self->priv->filter_mode) {
	case KEY_STORE_MODE_FILTERED:
		ret = object_contains_filtered_text (obj, self->priv->filter_text);
		break;

	case KEY_STORE_MODE_ALL:
		ret = TRUE;
		break;

	default:
		g_assert_not_reached ();
		break;
	};

	/* If current is not being shown, double check with children */
	if (!ret && GCR_IS_COLLECTION (obj)) {
		children = gcr_collection_get_objects (GCR_COLLECTION (obj));
		for (l = children; !ret && l != NULL; l = g_list_next (l))
			ret = on_filter_visible (l->data, user_data);
	}

	return ret;
}

void
seahorse_key_manager_store_refilter (SeahorseKeyManagerStore* self)
{
	GcrCollection *collection = gcr_collection_model_get_collection (GCR_COLLECTION_MODEL (self));
	seahorse_collection_refresh (SEAHORSE_COLLECTION (collection));
}

/* Refilter the tree */
static gboolean
refilter_now (gpointer user_data)
{
	SeahorseKeyManagerStore* self = SEAHORSE_KEY_MANAGER_STORE (user_data);
	self->priv->filter_stag = 0;
	seahorse_key_manager_store_refilter (self);
	return FALSE;
}

/* Refilter the tree after a timeout has passed */
static void
refilter_later (SeahorseKeyManagerStore* skstore)
{
    if (skstore->priv->filter_stag != 0)
        g_source_remove (skstore->priv->filter_stag);
    skstore->priv->filter_stag = g_timeout_add (200, (GSourceFunc)refilter_now, skstore);
}

/* Update the sort order for a column */
static void
set_sort_to (SeahorseKeyManagerStore *skstore, const gchar *name)
{
    gint i, id = -1;
    GtkSortType ord = GTK_SORT_ASCENDING;
    const gchar* n;

    g_return_if_fail (name != NULL);

    /* Prefix with a minus means descending */
    if (name[0] == '-') {
        ord = GTK_SORT_DESCENDING;
        name++;
    }

    /* Prefix with a plus means ascending */
    else if (name[0] == '+') {
        ord = GTK_SORT_ASCENDING;
        name++;
    }

    /* Find the column sort id */
    for (i = N_COLS - 1; i >= 0 ; i--) {
        n = columns[i].user_data;
        if (n && g_ascii_strcasecmp (name, n) == 0) {
            id = i;
            break;
        }
    }

    if (id != -1)
        gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (skstore), id, ord);
}

/* Called when the column sort is changed */
static void
on_sort_column_changed (GtkTreeSortable *sort,
                        gpointer user_data)
{
	SeahorseKeyManagerStore *self = SEAHORSE_KEY_MANAGER_STORE (user_data);
	GtkSortType ord;
	gchar* value;
	gint column_id;

	if (!self->priv->settings)
		return;

	/* We have a sort so save it */
	if (gtk_tree_sortable_get_sort_column_id (GTK_TREE_SORTABLE (self), &column_id, &ord)) {
		if (column_id >= 0 && column_id < N_COLS) {
			if (columns[column_id].user_data != NULL) {
				value = g_strconcat (ord == GTK_SORT_DESCENDING ? "-" : "",
				                     columns[column_id].user_data, NULL);
				g_settings_set_string (self->priv->settings, "sort-by", value);
				g_free (value);
			}
		}

	/* No sort so save blank */
	} else if (self->priv->settings) {
		g_settings_set_string (self->priv->settings, "sort-by", "");
	}
}

/* The following three functions taken from bugzilla
 * (http://bugzilla.gnome.org/attachment.cgi?id=49362&action=view)
 * Author: Christian Neumair
 * Copyright: 2005 Free Software Foundation, Inc
 * License: GPL */
static char *
xds_get_atom_value (GdkDragContext *context)
{
	GdkWindow *source_window;
	char *ret;

	g_return_val_if_fail (context != NULL, NULL);

	source_window = gdk_drag_context_get_source_window (context);
	g_return_val_if_fail (source_window != NULL, NULL);

	if (gdk_property_get (source_window,
	                      XDS_ATOM, TEXT_ATOM,
	                      0, MAX_XDS_ATOM_VAL_LEN,
	                      FALSE, NULL, NULL, NULL,
	                      (unsigned char **) &ret))
		return ret;

	return NULL;
}

static gboolean
xds_context_offers_target (GdkDragContext *context, GdkAtom target)
{
	GList *targets = gdk_drag_context_list_targets (context);
	return (g_list_find (targets, target) != NULL);
}

static gboolean
xds_is_dnd_valid_context (GdkDragContext *context)
{
	char *tmp;
	gboolean ret;

	g_return_val_if_fail (context != NULL, FALSE);

	tmp = NULL;
	if (xds_context_offers_target (context, XDS_ATOM)) {
		tmp = xds_get_atom_value (context);
	}

	ret = (tmp != NULL);
	g_free (tmp);

	return ret;
}

static gboolean
drag_begin (GtkWidget *widget, GdkDragContext *context, SeahorseKeyManagerStore *skstore)
{
	GtkTreeView *view = GTK_TREE_VIEW (widget);
	GdkWindow *source_window;

	seahorse_debug ("drag_begin -->");

	g_free (skstore->priv->drag_destination);
	skstore->priv->drag_destination = NULL;

	g_clear_error (&skstore->priv->drag_error);

	g_list_free (skstore->priv->drag_objects);
	skstore->priv->drag_objects = seahorse_key_manager_store_get_selected_objects (view);

	if (skstore->priv->drag_objects) {
		source_window = gdk_drag_context_get_source_window (context);
		gdk_property_change (source_window, XDS_ATOM, TEXT_ATOM,
		                     8, GDK_PROP_MODE_REPLACE, (guchar*)XDS_FILENAME,
		                     strlen (XDS_FILENAME));
	}

	seahorse_debug ("drag_begin <--");
	return skstore->priv->drag_objects ? TRUE : FALSE;
}

typedef struct {
	SeahorseKeyManagerStore *skstore;

	gint exports;
	gboolean failures;
} export_keys_to_output_closure;

static gboolean
export_to_text (SeahorseKeyManagerStore *self,
                GtkSelectionData *selection_data)
{
	guchar *output;
	gsize size;
	gboolean ret;
	guint count;

	g_return_val_if_fail (self->priv->drag_objects, FALSE);
	seahorse_debug ("exporting to text");

	count = seahorse_exportable_export_to_text_wait (self->priv->drag_objects,
	                                                 &output, &size, &self->priv->drag_error);

	/* TODO: Need to print status if only partially exported */

	if (count > 0) {
		seahorse_debug ("setting selection text");
		gtk_selection_data_set_text (selection_data, (gchar *)output, (gint)size);
		ret = TRUE;
	} else if (self->priv->drag_error) {
		g_message ("error occurred on export: %s", self->priv->drag_error->message);
		ret = FALSE;
	} else {
		g_message ("no objects exported");
		ret = FALSE;
	}

	g_free (output);
	return ret;
}

static gboolean
export_to_directory (SeahorseKeyManagerStore *self,
                     const gchar *directory)
{
	seahorse_debug ("exporting to %s", directory);

	return seahorse_exportable_export_to_directory_wait (self->priv->drag_objects,
	                                                     directory,
	                                                     &self->priv->drag_error);
}

static gboolean
drag_data_get (GtkWidget *widget, GdkDragContext *context, GtkSelectionData *selection_data,
               guint info, guint time, SeahorseKeyManagerStore *skstore)
{
	gchar *destination;
	gboolean ret;

	seahorse_debug ("drag_data_get %d -->", info);

	g_return_val_if_fail (skstore->priv->drag_objects, FALSE);

	/* The caller wants plain text */
	if (info == DRAG_INFO_TEXT) {
		seahorse_debug ("returning object text");
		export_to_text (skstore, selection_data);

	/* The caller wants XDS */
	} else if (info == DRAG_INFO_XDS) {

		if (xds_is_dnd_valid_context (context)) {
			destination = xds_get_atom_value (context);
			g_return_val_if_fail (destination, FALSE);
			skstore->priv->drag_destination = g_path_get_dirname (destination);
			g_free (destination);

			gtk_selection_data_set (selection_data, gtk_selection_data_get_target (selection_data), 8, (guchar*)"S", 1);
			ret = TRUE;
		}

	/* Unrecognized format */
	} else {
		seahorse_debug ("Unrecognized format: %d", info);
	}

	seahorse_debug ("drag_data_get <--");
	return ret;
}

static void
drag_end (GtkWidget *widget, GdkDragContext *context, SeahorseKeyManagerStore *skstore)
{
	seahorse_debug ("drag_end -->");

	if (skstore->priv->drag_destination && !skstore->priv->drag_error)
		export_to_directory (skstore, skstore->priv->drag_destination);

	if (skstore->priv->drag_error) {
		g_dbus_error_strip_remote_error (skstore->priv->drag_error);
		seahorse_util_show_error (widget, _("Couldn't export keys"),
		                          skstore->priv->drag_error->message);
	}

	g_clear_error (&skstore->priv->drag_error);
	g_list_free (skstore->priv->drag_objects);
	skstore->priv->drag_objects = NULL;
	g_free (skstore->priv->drag_destination);
	skstore->priv->drag_destination = NULL;

	seahorse_debug ("drag_end <--");
}

static gint
compare_pointers (gconstpointer a, gconstpointer b)
{
    if (a == b)
        return 0;
    return a > b ? 1 : -1;
}

/* -----------------------------------------------------------------------------
 * OBJECT
 */

static void
seahorse_key_manager_store_init (SeahorseKeyManagerStore *skstore)
{
    /* init private vars */
    skstore->priv = g_new0 (SeahorseKeyManagerStorePriv, 1);
}

static void
seahorse_key_manager_store_get_property (GObject *gobject, guint prop_id,
                                         GValue *value, GParamSpec *pspec)
{
    SeahorseKeyManagerStore *skstore = SEAHORSE_KEY_MANAGER_STORE (gobject);

    switch (prop_id) {

    /* The filtering mode */
    case PROP_MODE:
        g_value_set_uint (value, skstore->priv->filter_mode);
        break;

    case PROP_SETTINGS:
        g_value_set_object (value, skstore->priv->settings);
        break;

    /* The filter text. Note that we act as if we don't have any
     * filter text when not in filtering mode */
    case PROP_FILTER:
        g_value_set_string (value,
            skstore->priv->filter_mode == KEY_STORE_MODE_FILTERED ? skstore->priv->filter_text : "");
        break;

    default:
        break;
    }
}

static void
seahorse_key_manager_store_set_property (GObject *gobject, guint prop_id,
                                         const GValue *value, GParamSpec *pspec)
{
    SeahorseKeyManagerStore *skstore = SEAHORSE_KEY_MANAGER_STORE (gobject);
    const gchar* t;

    switch (prop_id) {

    /* The filtering mode */
    case PROP_MODE:
        if (skstore->priv->filter_mode != g_value_get_uint (value)) {
            skstore->priv->filter_mode = g_value_get_uint (value);
            refilter_later (skstore);
        }
        break;

    case PROP_SETTINGS:
        skstore->priv->settings = g_value_dup_object (value);
        break;

    /* The filter text */
    case PROP_FILTER:
        t = g_value_get_string (value);

        /*
         * If we're not in filtered mode and there is text OR
         * we're in filtered mode (regardless of text or not)
         * then update the filter
         */
        if ((skstore->priv->filter_mode != KEY_STORE_MODE_FILTERED && t && t[0]) ||
            (skstore->priv->filter_mode == KEY_STORE_MODE_FILTERED)) {
            skstore->priv->filter_mode = KEY_STORE_MODE_FILTERED;
            g_free (skstore->priv->filter_text);

            /* We always use lower case text (see filter_callback) */
            skstore->priv->filter_text = g_utf8_strdown (t, -1);
            refilter_later (skstore);
        }
        break;

    default:
        break;
    }
}

static void
seahorse_key_manager_store_finalize (GObject *gobject)
{
    SeahorseKeyManagerStore *skstore = SEAHORSE_KEY_MANAGER_STORE (gobject);

    g_signal_handlers_disconnect_by_func (skstore, on_sort_column_changed, skstore);

    /* Allocated in property setter */
    g_free (skstore->priv->filter_text);

    G_OBJECT_CLASS (seahorse_key_manager_store_parent_class)->finalize (gobject);
}

static void
seahorse_key_manager_store_class_init (SeahorseKeyManagerStoreClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

	columns[COL_ICON].column_type = G_TYPE_ICON;
	columns[COL_ICON].property_type = G_TYPE_ICON;

	gobject_class->finalize = seahorse_key_manager_store_finalize;
	gobject_class->set_property = seahorse_key_manager_store_set_property;
	gobject_class->get_property = seahorse_key_manager_store_get_property;

	g_object_class_install_property (gobject_class, PROP_MODE,
	        g_param_spec_uint ("mode", "Key Store Mode", "Key store mode controls which keys to display",
	                           0, KEY_STORE_MODE_FILTERED, KEY_STORE_MODE_ALL, G_PARAM_READWRITE));

	g_object_class_install_property (gobject_class, PROP_FILTER,
	        g_param_spec_string ("filter", "Key Store Filter", "Key store filter for when in filtered mode",
	                             "", G_PARAM_READWRITE));

	g_object_class_install_property (gobject_class, PROP_SETTINGS,
	        g_param_spec_object ("settings", "Settings", "Manager Settings",
	                             G_TYPE_SETTINGS, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

	/* Couldn't initialize this earlier */
	g_assert (N_COLS + 1 == G_N_ELEMENTS (columns));
	columns[0].property_type = G_TYPE_ICON;
	columns[0].column_type = G_TYPE_ICON;
}

/* -----------------------------------------------------------------------------
 * PUBLIC
 */

SeahorseKeyManagerStore*
seahorse_key_manager_store_new (GcrCollection *collection,
                                GtkTreeView *view,
                                SeahorsePredicate *pred,
                                GSettings *settings)
{
	SeahorseKeyManagerStore *self;
	GtkTreeViewColumn *col;
	SeahorseCollection *filtered;
	GtkCellRenderer *renderer;
	gchar *sort_by;
	guint last;

	filtered = seahorse_collection_new_for_predicate (collection, pred, NULL);
	pred->custom = on_filter_visible;

	self = g_object_new (SEAHORSE_TYPE_KEY_MANAGER_STORE,
	                     "collection", filtered,
	                     "settings", settings,
	                     "mode", GCR_COLLECTION_MODEL_LIST,
	                     NULL);
	pred->custom_target = self;
	g_object_unref (filtered);

	last = gcr_collection_model_set_columns (GCR_COLLECTION_MODEL (self), columns);
	g_return_val_if_fail (last == N_COLS, NULL);

	/* The sorted model is the top level model */
	gtk_tree_view_set_model (view, GTK_TREE_MODEL (self));

	/* add the icon column */
	renderer = gtk_cell_renderer_pixbuf_new ();
	g_object_set (renderer, "stock-size", GTK_ICON_SIZE_DND, NULL);
	g_object_set (renderer, "ypad", 6, NULL);
	g_object_set (renderer, "yalign", 0.0, NULL);
	col = gtk_tree_view_column_new_with_attributes ("", renderer, "gicon", COL_ICON, NULL);
	gtk_tree_view_column_set_resizable (col, FALSE);
	gtk_tree_view_append_column (view, col);

	/* Name column */
	col = gtk_tree_view_column_new ();
	renderer = gtk_cell_renderer_text_new ();
	g_object_set (renderer,
	              "ypad", 6,
	              "yalign", 0.0,
	              "ellipsize", PANGO_ELLIPSIZE_END,
	              NULL);
	gtk_tree_view_column_pack_start (col, renderer, TRUE);
	gtk_tree_view_column_set_attributes (col, renderer,
	                                     "markup", COL_MARKUP,
	                                     NULL);
	renderer = gtk_cell_renderer_text_new ();
	g_object_set (renderer,
	              "ypad", 6,
	              "xpad", 3,
	              "yalign", 0.0,
	              "xalign", 1.0,
	              "scale", PANGO_SCALE_SMALL,
	              "alignment", PANGO_ALIGN_RIGHT,
	              NULL);
	gtk_tree_view_column_pack_start (col, renderer, FALSE);
	gtk_tree_view_column_set_attributes (col, renderer,
	                                     "markup", COL_DESCRIPTION,
	                                     NULL);
	gtk_tree_view_column_set_resizable (col, TRUE);
	gtk_tree_view_column_set_expand (col, TRUE);
	gtk_tree_view_append_column (view, col);
	gtk_tree_view_column_set_sort_column_id (col, COL_LABEL);

	/* Use predicate to figure out which columns to add */
	if (SEAHORSE_IS_COLLECTION (collection))
		pred = seahorse_collection_get_predicate (SEAHORSE_COLLECTION (collection));
	else
		pred = NULL;

	/* Also watch for sort-changed on the store */
	g_signal_connect (self, "sort-column-changed", G_CALLBACK (on_sort_column_changed), self);

	/* Update sort order in case the sorted column was added */
	if ((sort_by = g_settings_get_string (settings, "sort-by")) != NULL) {
		set_sort_to (self, sort_by);
		g_free (sort_by);
	}

	gtk_tree_view_set_enable_search (view, FALSE);
	gtk_tree_view_set_show_expanders (view, FALSE);
	gtk_tree_view_set_rules_hint (view, TRUE);
	gtk_tree_view_set_headers_visible (view, FALSE);

	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (self), COL_LABEL,
	                                      GTK_SORT_ASCENDING);

	/* Tree drag */
	egg_tree_multi_drag_add_drag_support (view);

	g_signal_connect (G_OBJECT (view), "drag_data_get", G_CALLBACK (drag_data_get), self);
	g_signal_connect (G_OBJECT (view), "drag_begin",  G_CALLBACK (drag_begin), self);
	g_signal_connect (G_OBJECT (view), "drag_end",  G_CALLBACK (drag_end), self);

	gtk_drag_source_set (GTK_WIDGET (view), GDK_BUTTON1_MASK,
	                     store_targets, G_N_ELEMENTS (store_targets), GDK_ACTION_COPY);

	return self;
}

GObject *
seahorse_key_manager_store_get_object_from_path (GtkTreeView *view, GtkTreePath *path)
{
    GtkTreeModel *model;
    GtkTreeIter iter;

    g_return_val_if_fail (GTK_IS_TREE_VIEW (view), NULL);
    g_return_val_if_fail (path != NULL, NULL);

    model = gtk_tree_view_get_model (view);
    g_return_val_if_fail (gtk_tree_model_get_iter (model, &iter, path), NULL);
    return gcr_collection_model_object_for_iter (GCR_COLLECTION_MODEL (model), &iter);
}

GList*
seahorse_key_manager_store_get_all_objects (GtkTreeView *view)
{
    SeahorseKeyManagerStore* skstore;

    g_return_val_if_fail (GTK_IS_TREE_VIEW (view), NULL);
    skstore = SEAHORSE_KEY_MANAGER_STORE (gtk_tree_view_get_model (view));
    g_return_val_if_fail (SEAHORSE_IS_KEY_MANAGER_STORE (skstore), NULL);
    return gcr_collection_get_objects (gcr_collection_model_get_collection (GCR_COLLECTION_MODEL (skstore)));
}

GList*
seahorse_key_manager_store_get_selected_objects (GtkTreeView *view)
{
    GObject *obj;
    GList *l, *objects = NULL;
    SeahorseKeyManagerStore* skstore;
    GList *list, *paths = NULL;
    GtkTreeSelection *selection;

    g_return_val_if_fail (GTK_IS_TREE_VIEW (view), NULL);
    skstore = SEAHORSE_KEY_MANAGER_STORE (gtk_tree_view_get_model (view));
    g_return_val_if_fail (SEAHORSE_IS_KEY_MANAGER_STORE (skstore), NULL);

    selection = gtk_tree_view_get_selection (view);
    paths = gtk_tree_selection_get_selected_rows (selection, NULL);

    /* make object list */
    for (list = paths; list != NULL; list = g_list_next (list)) {
        obj = seahorse_key_manager_store_get_object_from_path (view, list->data);
        if (obj != NULL)
            objects = g_list_append (objects, obj);
    }

    /* free selected paths */
    g_list_foreach (paths, (GFunc)gtk_tree_path_free, NULL);
    g_list_free (paths);

    /* Remove duplicates */
    objects = g_list_sort (objects, compare_pointers);
    for (l = objects; l; l = g_list_next (l)) {
        while (l->next && l->data == l->next->data)
            objects = g_list_delete_link (objects, l->next);
    }

    return objects;
}

void
seahorse_key_manager_store_set_selected_objects (GtkTreeView *view, GList* objects)
{
	SeahorseKeyManagerStore* skstore;
	GtkTreeSelection* selection;
	gboolean first = TRUE;
	GtkTreePath *path;
	GList *l;
	GtkTreeIter iter;

	g_return_if_fail (GTK_IS_TREE_VIEW (view));
	selection = gtk_tree_view_get_selection (view);
	gtk_tree_selection_unselect_all (selection);

	skstore = SEAHORSE_KEY_MANAGER_STORE (gtk_tree_view_get_model (view));
	g_return_if_fail (SEAHORSE_IS_KEY_MANAGER_STORE (skstore));

	for (l = objects; l; l = g_list_next (l)) {
		if (gcr_collection_model_iter_for_object (GCR_COLLECTION_MODEL (skstore),
		                                          l->data, &iter)) {
			gtk_tree_selection_select_iter (selection, &iter);

			/* Scroll the first row selected into view */
			if (first) {
				path = gtk_tree_model_get_path (gtk_tree_view_get_model (view), &iter);
				gtk_tree_view_scroll_to_cell (view, path, NULL, FALSE, 0.0, 0.0);
				gtk_tree_path_free (path);
				first = FALSE;
			}
		}
	}
}

GObject *
seahorse_key_manager_store_get_selected_object (GtkTreeView *view)
{
	SeahorseKeyManagerStore* skstore;
	GObject *obj = NULL;
	GList *paths = NULL;
	GtkTreeSelection *selection;

	g_return_val_if_fail (GTK_IS_TREE_VIEW (view), NULL);
	skstore = SEAHORSE_KEY_MANAGER_STORE (gtk_tree_view_get_model (view));
	g_return_val_if_fail (SEAHORSE_IS_KEY_MANAGER_STORE (skstore), NULL);

	selection = gtk_tree_view_get_selection (view);
	paths = gtk_tree_selection_get_selected_rows (selection, NULL);

	/* choose first object */
	if (paths != NULL) {
		obj = seahorse_key_manager_store_get_object_from_path (view, paths->data);

		/* free selected paths */
		g_list_foreach (paths, (GFunc)gtk_tree_path_free, NULL);
		g_list_free (paths);
	}

	return obj;
}
