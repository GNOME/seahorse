/*
 * Seahorse
 *
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

#include "seahorse-sidebar.h"

#include "seahorse-action.h"
#include "seahorse-actions.h"
#include "seahorse-backend.h"
#include "seahorse-common.h"
#include "seahorse-interaction.h"
#include "seahorse-lockable.h"
#include "seahorse-place.h"
#include "seahorse-util.h"
#include "seahorse-viewable.h"

#include "gkr/seahorse-gkr.h"
#include "pgp/seahorse-pgp.h"
#include "pkcs11/seahorse-pkcs11.h"
#include "ssh/seahorse-ssh.h"

#include <glib/gi18n.h>

struct _SeahorseSidebar {
	GtkScrolledWindow parent;

	GtkTreeView *tree_view;

	GtkListStore *store;
	GPtrArray *backends;
	GcrUnionCollection *objects;

	/* The selection */
	GHashTable *selection;
	gboolean combined;
	gboolean updating;

	/* A set of chosen uris, used with settings */
	GHashTable *chosen;

	/* Action icons */
	GdkPixbuf *pixbuf_lock;
	GdkPixbuf *pixbuf_unlock;
	GdkPixbuf *pixbuf_lock_l;
	GdkPixbuf *pixbuf_unlock_l;
	GtkTreePath *action_highlight_path;
	GtkCellRenderer *action_cell_renderer;
	gint action_button_size;
	GtkAccelGroup *accel_group;

	guint update_places_sig;
};

struct _SeahorseSidebarClass {
	GtkScrolledWindowClass parent_class;
};

#define ACTION_BUTTON_XPAD 6

enum {
	PROP_0,
	PROP_COLLECTION,
	PROP_COMBINED,
	PROP_SELECTED_URIS
};

typedef enum {
	TYPE_BACKEND,
	TYPE_PLACE,
} RowType;

enum {
	SIDEBAR_ROW_TYPE,
	SIDEBAR_ICON,
	SIDEBAR_LABEL,
	SIDEBAR_TOOLTIP,
	SIDEBAR_EDITABLE,
	SIDEBAR_CATEGORY,
	SIDEBAR_COLLECTION,
	SIDEBAR_URI,
	SIDEBAR_N_COLUMNS
};

static GType column_types[] = {
	G_TYPE_UINT,
	0 /* later */,
	G_TYPE_STRING,
	G_TYPE_STRING,
	G_TYPE_BOOLEAN,
	G_TYPE_STRING,
	0 /* later */,
	G_TYPE_STRING
};

enum {
	CONTEXT_MENU,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0, };

G_DEFINE_TYPE (SeahorseSidebar, seahorse_sidebar, GTK_TYPE_SCROLLED_WINDOW);

static void
seahorse_sidebar_init (SeahorseSidebar *self)
{
	g_assert (SIDEBAR_N_COLUMNS == G_N_ELEMENTS (column_types));
	column_types[SIDEBAR_ICON] = G_TYPE_ICON;
	column_types[SIDEBAR_COLLECTION] = GCR_TYPE_COLLECTION;
	self->store = gtk_list_store_newv (SIDEBAR_N_COLUMNS, column_types);

	self->backends = g_ptr_array_new_with_free_func (g_object_unref);
	self->selection = g_hash_table_new (g_direct_hash, g_direct_equal);
	self->objects = GCR_UNION_COLLECTION (gcr_union_collection_new ());
	self->chosen = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

	self->accel_group = gtk_accel_group_new ();
}

static guchar
lighten_component (guchar cur_value)
{
	int new_value = cur_value + 24 + (cur_value >> 3);
	return (new_value > 255) ? (guchar)255 : (guchar)new_value;
}

static GdkPixbuf *
create_spotlight_pixbuf (GdkPixbuf* src)
{
	GdkPixbuf *dest;
	int i, j;
	int width, height, has_alpha, src_row_stride, dst_row_stride;
	guchar *target_pixels, *original_pixels;
	guchar *pixsrc, *pixdest;

	dest = gdk_pixbuf_new (gdk_pixbuf_get_colorspace (src),
	                       gdk_pixbuf_get_has_alpha (src),
	                       gdk_pixbuf_get_bits_per_sample (src),
	                       gdk_pixbuf_get_width (src),
	                       gdk_pixbuf_get_height (src));

	has_alpha = gdk_pixbuf_get_has_alpha (src);
	width = gdk_pixbuf_get_width (src);
	height = gdk_pixbuf_get_height (src);
	dst_row_stride = gdk_pixbuf_get_rowstride (dest);
	src_row_stride = gdk_pixbuf_get_rowstride (src);
	target_pixels = gdk_pixbuf_get_pixels (dest);
	original_pixels = gdk_pixbuf_get_pixels (src);

	for (i = 0; i < height; i++) {
		pixdest = target_pixels + i * dst_row_stride;
		pixsrc = original_pixels + i * src_row_stride;
		for (j = 0; j < width; j++) {
			*pixdest++ = lighten_component (*pixsrc++);
			*pixdest++ = lighten_component (*pixsrc++);
			*pixdest++ = lighten_component (*pixsrc++);
			if (has_alpha) {
				*pixdest++ = *pixsrc++;
			}
		}
	}
	return dest;
}

static void
ensure_sidebar_pixbufs (SeahorseSidebar *self)
{
	GtkIconInfo *icon_info;
	GIcon *icon;
	GtkIconTheme *icon_theme;
	GtkStyleContext *style;
	gint height;

	if (self->pixbuf_lock &&
	    self->pixbuf_lock_l &&
	    self->pixbuf_unlock_l &&
	    self->pixbuf_unlock)
		return;

	icon_theme = gtk_icon_theme_get_default ();
	style = gtk_widget_get_style_context (GTK_WIDGET (self));
	if (!gtk_icon_size_lookup (GTK_ICON_SIZE_MENU, &self->action_button_size, &height))
		self->action_button_size = 16;

	/* Lock icon */
	icon = g_themed_icon_new_with_default_fallbacks ("changes-prevent-symbolic");
	icon_info = gtk_icon_theme_lookup_by_gicon (icon_theme, icon, self->action_button_size, GTK_ICON_LOOKUP_GENERIC_FALLBACK);
	g_return_if_fail (icon_info != NULL);
	if (!self->pixbuf_lock)
		self->pixbuf_lock = gtk_icon_info_load_symbolic_for_context (icon_info, style, NULL, NULL);
	if (!self->pixbuf_lock_l)
		self->pixbuf_lock_l = create_spotlight_pixbuf (self->pixbuf_lock);
#if GTK_CHECK_VERSION(3, 8, 0)
	g_object_unref (icon_info);
#else
	gtk_icon_info_free (icon_info);
#endif
	g_object_unref (icon);

	/* Unlock icon */
	icon = g_themed_icon_new_with_default_fallbacks ("changes-allow-symbolic");
	icon_info = gtk_icon_theme_lookup_by_gicon (icon_theme, icon, self->action_button_size, GTK_ICON_LOOKUP_GENERIC_FALLBACK);
	g_return_if_fail (icon_info != NULL);
	if (!self->pixbuf_unlock)
		self->pixbuf_unlock = gtk_icon_info_load_symbolic_for_context (icon_info, style, NULL, NULL);
	if (!self->pixbuf_unlock_l)
		self->pixbuf_unlock_l = create_spotlight_pixbuf (self->pixbuf_unlock);
#if GTK_CHECK_VERSION(3, 8, 0)
	g_object_unref (icon_info);
#else
	gtk_icon_info_free (icon_info);
#endif
	g_object_unref (icon);
}

static void
invalidate_sidebar_pixbufs (SeahorseSidebar *self)
{
	g_clear_object (&self->pixbuf_lock);
	g_clear_object (&self->pixbuf_unlock);
	g_clear_object (&self->pixbuf_lock_l);
	g_clear_object (&self->pixbuf_unlock_l);
}

static void
next_or_append_row (GtkListStore *store,
                    GtkTreeIter *iter,
                    const gchar *category,
                    GcrCollection *collection)
{
	GcrCollection *row_collection;
	gchar *row_category;
	gboolean found;

	/*
	 * We try to keep the same row in order to preserve checked state
	 * and selections. So if the next row matches the values we want to
	 * set on it, then just keep that row.
	 *
	 * This is complicated by the fact that the first row being inserted
	 * doesn't have a valid iter, and we don't have a standard way to
	 * detect that an iter isn't valid.
	 */

	/* A marker that tells us the iter is not yet valid */
	if (iter->stamp == GPOINTER_TO_INT (iter) && iter->user_data3 == iter &&
	    iter->user_data2 == iter && iter->user_data == iter) {
		if (!gtk_tree_model_get_iter_first (GTK_TREE_MODEL (store), iter))
			gtk_list_store_append (store, iter);
		return;
	}

	if (!gtk_tree_model_iter_next (GTK_TREE_MODEL (store), iter)) {
		gtk_list_store_append (store, iter);
		return;
	}

	for (;;) {
		gtk_tree_model_get (GTK_TREE_MODEL (store), iter,
		                    SIDEBAR_CATEGORY, &row_category,
		                    SIDEBAR_COLLECTION, &row_collection,
		                    -1);

		found = (g_strcmp0 (row_category, category) == 0 && row_collection == collection);

		g_free (row_category);
		g_clear_object (&row_collection);

		if (found)
			return;

		if (!gtk_list_store_remove (store, iter)) {
			gtk_list_store_append (store, iter);
			return;
		}
	}
}

static void
update_backend (SeahorseSidebar *self,
                GcrCollection *backend,
                GtkTreeIter *iter)
{
	GList *collections, *l;
	GtkActionGroup *actions;
	GParamSpec *spec;
	gchar *category;
	gchar *tooltip;
	gchar *label;
	GIcon *icon = NULL;
	gchar *uri;

	collections = gcr_collection_get_objects (backend);

	/* Ignore categories that have nothing */
	if (collections == NULL)
		return;

	g_object_get (backend,
	              "name", &category,
	              "label", &label,
	              "description", &tooltip,
	              NULL);

	next_or_append_row (self->store, iter, category, GCR_COLLECTION (backend));
	gtk_list_store_set (self->store, iter,
	                    SIDEBAR_ROW_TYPE, TYPE_BACKEND,
	                    SIDEBAR_CATEGORY, category,
	                    SIDEBAR_LABEL, label,
	                    SIDEBAR_TOOLTIP, tooltip,
	                    SIDEBAR_EDITABLE, FALSE,
	                    SIDEBAR_COLLECTION, backend,
	                    -1);

	g_free (label);
	g_free (tooltip);

	for (l = collections; l != NULL; l = g_list_next (l)) {
		label = tooltip = NULL;
		g_object_get (l->data,
		              "label", &label,
		              "description", &tooltip,
		              "icon", &icon,
		              "uri", &uri,
		              "actions", &actions,
		              NULL);

		spec = g_object_class_find_property (G_OBJECT_GET_CLASS (l->data), "label");
		g_return_if_fail (spec != NULL);

		next_or_append_row (self->store, iter, category, l->data);
		gtk_list_store_set (self->store, iter,
		                    SIDEBAR_ROW_TYPE, TYPE_PLACE,
		                    SIDEBAR_CATEGORY, category,
		                    SIDEBAR_LABEL, label,
		                    SIDEBAR_TOOLTIP, tooltip,
		                    SIDEBAR_ICON, icon,
		                    SIDEBAR_EDITABLE, (spec->flags & G_PARAM_WRITABLE) ? TRUE : FALSE,
		                    SIDEBAR_COLLECTION, l->data,
		                    SIDEBAR_URI, uri,
		                    -1);
		g_clear_object (&icon);
		g_free (label);
		g_free (tooltip);
		g_free (uri);
	}

	g_free (category);
	g_list_free (collections);
}

static void
update_objects_in_collection (SeahorseSidebar *self,
                              gboolean update_chosen)
{
	GList *collections;
	gboolean include;
	gboolean have;
	gboolean changed = FALSE;
	gchar *uri;
	GList *l;
	guint i;

	/* Updating collection is blocked */
	if (self->updating)
		return;

	for (i = 0; i < self->backends->len; i++) {
		collections = gcr_collection_get_objects (self->backends->pdata[i]);
		for (l = collections; l != NULL; l = g_list_next (l)) {

			include = g_hash_table_lookup (self->selection, l->data) != NULL;

			if (update_chosen) {
				g_object_get (l->data, "uri", &uri, NULL);
				have = g_hash_table_lookup (self->chosen, uri) != NULL;
				if (include && !have) {
					g_hash_table_insert (self->chosen, g_strdup (uri), "");
					changed = TRUE;
				} else if (!include && have) {
					g_hash_table_remove (self->chosen, uri);
					changed = TRUE;
				}
				g_free (uri);
			}

			/* Combined overrides and shows all objects */
			if (self->combined)
				include = TRUE;

			have = gcr_union_collection_have (self->objects, l->data);
			if (include && !have)
				gcr_union_collection_add (self->objects, l->data);
			else if (!include && have)
				gcr_union_collection_remove (self->objects, l->data);
		}
		g_list_free (collections);
	}

	if (update_chosen && changed)
		g_object_notify (G_OBJECT (self), "selected-uris");
}

static void
for_each_selected_place_row (GtkTreeModel *model,
                             GtkTreePath *path,
                             GtkTreeIter *iter,
                             gpointer user_data)
{
	GcrCollection *collection;
	GHashTable *selected = user_data;

	gtk_tree_model_get (model, iter, SIDEBAR_COLLECTION, &collection, -1);
	if (collection != NULL) {
		g_hash_table_insert (selected, collection, collection);
		g_object_unref (collection);
	}
}

static void
update_objects_for_selection (SeahorseSidebar *self,
                              GtkTreeSelection *selection)
{
	GHashTable *selected;

	if (self->updating)
		return;

	selected = g_hash_table_new (g_direct_hash, g_direct_equal);
	gtk_tree_selection_selected_foreach (selection, for_each_selected_place_row, selected);

	g_hash_table_destroy (self->selection);
	self->selection = selected;

	if (!self->combined)
		update_objects_in_collection (self, TRUE);
}

static void
update_objects_for_combine (SeahorseSidebar *self,
                            gboolean combine)
{
	if (self->combined != combine) {
		self->combined = combine;
		update_objects_in_collection (self, FALSE);
	}
}

static void
update_objects_for_chosen (SeahorseSidebar *self,
                           GHashTable *chosen)
{
	GtkTreeIter iter;
	GtkTreeModel *model;
	GcrCollection *collection;
	gchar *uri;
	GtkTreeSelection *selection;

	self->updating = TRUE;

	model = GTK_TREE_MODEL (self->store);
	selection = gtk_tree_view_get_selection (self->tree_view);

	/* Update the display */
	if (gtk_tree_model_get_iter_first (model, &iter)) {
		do {
			gtk_tree_model_get (model, &iter,
					    SIDEBAR_COLLECTION, &collection,
					    SIDEBAR_URI, &uri,
					    -1);

			if (collection && uri) {
				if (g_hash_table_lookup (chosen, uri))
					gtk_tree_selection_select_iter (selection, &iter);
				else
					gtk_tree_selection_unselect_iter (selection, &iter);
			}

			g_clear_object (&collection);
			g_free (uri);
		} while (gtk_tree_model_iter_next (model, &iter));
	}

	self->updating = FALSE;
	update_objects_for_selection (self, selection);
}

static void
update_places (SeahorseSidebar *self)
{
	GtkTreeIter iter;
	guint i;

	/* A marker that tells us the iter is not yet valid */
	iter.stamp = GPOINTER_TO_INT (&iter);
	iter.user_data3 = iter.user_data2 = iter.user_data = &iter;

	for (i = 0; i < self->backends->len; i++)
		update_backend (self, GCR_COLLECTION (self->backends->pdata[i]), &iter);

	/* Update selection */
	update_objects_for_chosen (self, self->chosen);

	if (self->combined)
		update_objects_in_collection (self, FALSE);
}

static gboolean
on_idle_update_places (gpointer user_data)
{
	SeahorseSidebar *self = SEAHORSE_SIDEBAR (user_data);

	self->update_places_sig = 0;
	update_places (self);

	return FALSE; /* don't call again */
}

static void
update_places_later (SeahorseSidebar *self)
{
	if (!self->update_places_sig)
		self->update_places_sig = g_idle_add (on_idle_update_places, self);
}

static SeahorseLockable *
lookup_lockable_for_iter (GtkTreeModel *model,
                          GtkTreeIter *iter)
{
	GcrCollection *collection;

	gtk_tree_model_get (model, iter,
	                    SIDEBAR_COLLECTION, &collection,
	                    -1);

	if (collection == NULL)
		return NULL;

	if (!SEAHORSE_IS_LOCKABLE (collection)) {
		g_object_unref (collection);
		return NULL;
	}

	return SEAHORSE_LOCKABLE (collection);
}

static void
on_cell_renderer_action_icon (GtkTreeViewColumn *column,
                              GtkCellRenderer *cell,
                              GtkTreeModel *model,
                              GtkTreeIter *iter,
                              gpointer user_data)
{
	SeahorseSidebar *self = SEAHORSE_SIDEBAR (user_data);
	gboolean highlight = FALSE;
	GtkTreePath *path;
	GdkPixbuf *pixbuf = NULL;
	SeahorseLockable *lockable;
	gboolean can_lock = FALSE;
	gboolean can_unlock = FALSE;

	lockable = lookup_lockable_for_iter (model, iter);

	if (lockable) {
		can_lock = seahorse_lockable_can_lock (lockable);
		can_unlock = seahorse_lockable_can_unlock (lockable);
	}

	if (can_lock || can_unlock) {
		ensure_sidebar_pixbufs (self);

		pixbuf = NULL;
		highlight = FALSE;

		if (self->action_highlight_path) {
			path = gtk_tree_model_get_path (model, iter);
			highlight = gtk_tree_path_compare (path, self->action_highlight_path) == 0;
			gtk_tree_path_free (path);
		}

		if (can_lock)
			pixbuf = highlight ? self->pixbuf_unlock : self->pixbuf_unlock_l;
		else
			pixbuf = highlight ? self->pixbuf_lock : self->pixbuf_lock_l;

		g_object_set (cell,
		              "visible", TRUE,
		              "pixbuf", pixbuf,
		              NULL);
	} else {
		g_object_set (cell,
		              "visible", FALSE,
		              "pixbuf", NULL,
		              NULL);
	}

	g_clear_object (&lockable);
}

static void
on_cell_renderer_heading_visible (GtkTreeViewColumn *column,
                                  GtkCellRenderer *cell,
                                  GtkTreeModel *model,
                                  GtkTreeIter *iter,
                                  gpointer user_data)
{
	RowType type;
	gtk_tree_model_get (model, iter,
	                    SIDEBAR_ROW_TYPE, &type,
	                    -1);
	g_object_set (cell,
	              "visible", (type == TYPE_BACKEND),
	              NULL);
}

static void
on_padding_cell_renderer (GtkTreeViewColumn *column,
                          GtkCellRenderer *cell,
                          GtkTreeModel *model,
                          GtkTreeIter *iter,
                          gpointer user_data)
{
	RowType type;
	gtk_tree_model_get (model, iter,
	                    SIDEBAR_ROW_TYPE, &type,
	                    -1);

	if (type == TYPE_BACKEND) {
		g_object_set (cell,
		              "visible", FALSE,
		              "xpad", 0,
		              "ypad", 0,
		              NULL);
	} else {
		g_object_set (cell,
		              "visible", TRUE,
		              "xpad", 3,
		              "ypad", 3,
		              NULL);
	}
}

static void
on_cell_renderer_heading_not_visible (GtkTreeViewColumn *column,
                                      GtkCellRenderer *cell,
                                      GtkTreeModel *model,
                                      GtkTreeIter *iter,
                                      gpointer user_data)
{
	RowType type;
	gtk_tree_model_get (model, iter,
	                    SIDEBAR_ROW_TYPE, &type,
	                    -1);
	g_object_set (cell,
	              "visible", (type != TYPE_BACKEND),
	              NULL);
}

static gboolean
on_tree_selection_validate (GtkTreeSelection *selection,
                            GtkTreeModel *model,
                            GtkTreePath *path,
                            gboolean path_currently_selected,
                            gpointer user_data)
{
	GtkTreeIter iter;
	RowType row_type;

	gtk_tree_model_get_iter (model, &iter, path);
	gtk_tree_model_get (model, &iter,
	                    SIDEBAR_ROW_TYPE, &row_type,
	                    -1);
	if (row_type == TYPE_BACKEND)
		return FALSE;

	return TRUE;
}

static void
on_tree_selection_changed (GtkTreeSelection *selection,
                           gpointer user_data)
{
	SeahorseSidebar *self = SEAHORSE_SIDEBAR (user_data);
	update_objects_for_selection (self, selection);
}

static void
on_place_changed (GObject *obj,
                  GParamSpec *spec,
                  gpointer user_data)
{
	SeahorseSidebar *self = SEAHORSE_SIDEBAR (user_data);
	update_places_later (self);
}

static void
on_backend_changed (GObject *obj,
                    GParamSpec *spec,
                    gpointer user_data)
{
	SeahorseSidebar *self = SEAHORSE_SIDEBAR (user_data);
	update_places_later (self);
}

static void
on_place_added (GcrCollection *places,
                GObject *place,
                gpointer user_data)
{
	SeahorseSidebar *self = SEAHORSE_SIDEBAR (user_data);
	g_signal_connect (place, "notify", G_CALLBACK (on_place_changed), self);
	update_places_later (self);
}

static void
on_place_removed (GcrCollection *places,
                  GObject *object,
                  gpointer user_data)
{
	SeahorseSidebar *self = SEAHORSE_SIDEBAR (user_data);
	g_signal_handlers_disconnect_by_func (object, on_place_changed, self);
	update_places_later (self);
}

static gint
order_from_backend (GObject *backend)
{
	gchar *name;
	gint order;

	g_object_get (backend, "name", &name, NULL);

	if (name == NULL)
		order = 10;
	else if (g_str_equal (name, SEAHORSE_GKR_NAME))
		order = 0;
	else if (g_str_equal (name, SEAHORSE_PGP_NAME))
		order = 1;
	else if (g_str_equal (name, SEAHORSE_PKCS11_NAME))
		order = 2;
	else if (g_str_equal (name, SEAHORSE_SSH_NAME))
		order = 3;
	else
		order = 10;

	g_free (name);
	return order;
}

static gint
on_sort_backends (gconstpointer a,
                  gconstpointer b)
{
	gint ordera = order_from_backend (G_OBJECT (*((gpointer *)a)));
	gint orderb = order_from_backend (G_OBJECT (*((gpointer *)b)));
	return ordera - orderb;
}

static void
load_backends (SeahorseSidebar *self)
{
	GList *backends, *l;
	GList *places, *p;

	backends = seahorse_backend_get_registered ();
	for (l = backends; l != NULL; l = g_list_next (l)) {
		g_ptr_array_add (self->backends, l->data);
		g_signal_connect (l->data, "added", G_CALLBACK (on_place_added), self);
		g_signal_connect (l->data, "removed", G_CALLBACK (on_place_removed), self);
		g_signal_connect (l->data, "notify", G_CALLBACK (on_backend_changed), self);

		places = gcr_collection_get_objects (l->data);
		for (p = places; p != NULL; p = g_list_next (p))
			on_place_added (l->data, p->data, self);
		g_list_free (places);
	}
	g_ptr_array_sort (self->backends, on_sort_backends);
	g_list_free (backends);
}

static void
on_place_locked (GObject *source,
                 GAsyncResult *result,
                 gpointer user_data)
{
	GtkWindow *parent = GTK_WINDOW (user_data);
	GError *error = NULL;

	if (!seahorse_lockable_lock_finish (SEAHORSE_LOCKABLE (source), result, &error))
		seahorse_util_handle_error (&error, parent, _("Couldn't lock"));

	g_object_unref (parent);
}

static void
place_lock (SeahorseLockable *lockable,
            GtkWindow *window)
{
	GCancellable *cancellable = g_cancellable_new ();
	GTlsInteraction *interaction = seahorse_interaction_new (window);

	seahorse_lockable_lock_async (lockable, interaction, cancellable,
	                              on_place_locked, g_object_ref (window));

	g_object_unref (cancellable);
	g_object_unref (interaction);
}

static void
on_place_lock (GtkMenuItem *item,
               gpointer user_data)
{
	SeahorseLockable *lockable = SEAHORSE_LOCKABLE (user_data);
	GtkWidget *window = gtk_widget_get_toplevel (GTK_WIDGET (item));
	place_lock (lockable, GTK_WINDOW (window));
}

static void
on_place_unlocked (GObject *source,
                 GAsyncResult *result,
                 gpointer user_data)
{
	GtkWindow *parent = GTK_WINDOW (user_data);
	GError *error = NULL;

	if (!seahorse_lockable_unlock_finish (SEAHORSE_LOCKABLE (source), result, &error))
		seahorse_util_handle_error (&error, parent, _("Couldn't unlock"));

	g_object_unref (parent);
}



static void
place_unlock (SeahorseLockable *lockable,
              GtkWindow *window)
{
	GCancellable *cancellable = g_cancellable_new ();
	GTlsInteraction *interaction = seahorse_interaction_new (window);

	seahorse_lockable_unlock_async (lockable, interaction, cancellable,
	                                on_place_unlocked, g_object_ref (window));

	g_object_unref (cancellable);
	g_object_unref (interaction);
}

static void
on_place_unlock (GtkMenuItem *item,
                 gpointer user_data)
{
	SeahorseLockable *lockable = SEAHORSE_LOCKABLE (user_data);
	GtkWidget *window = gtk_widget_get_toplevel (GTK_WIDGET (item));
	place_unlock (lockable, GTK_WINDOW (window));
}

static void
on_place_deleted (GObject *source,
                  GAsyncResult *result,
                  gpointer user_data)
{
	GtkWindow *parent = GTK_WINDOW (user_data);
	GError *error = NULL;

	if (!seahorse_deleter_delete_finish (SEAHORSE_DELETER (source), result, &error))
		seahorse_util_handle_error (&error, parent, _("Couldn't delete"));

	g_object_unref (parent);
}

static void
on_place_delete (GtkMenuItem *item,
                 gpointer user_data)
{
	SeahorseDeletable *deletable = SEAHORSE_DELETABLE (user_data);
	GtkWidget *window = gtk_widget_get_toplevel (GTK_WIDGET (item));
	SeahorseDeleter *deleter;

	deleter = seahorse_deletable_create_deleter (deletable);

	if (seahorse_deleter_prompt (deleter, GTK_WINDOW (window)))
		seahorse_deleter_delete (deleter, NULL, on_place_deleted,
		                         g_object_ref (window));

	g_object_unref (deleter);
}

static void
on_place_properties (GtkMenuItem *item,
                     gpointer user_data)
{
	SeahorseViewable *viewable = SEAHORSE_VIEWABLE (user_data);
	GtkWidget *window = gtk_widget_get_toplevel (GTK_WIDGET (item));
	seahorse_viewable_show_viewer (viewable, GTK_WINDOW (window));
}

static void
check_widget_visible (GtkWidget *widget,
                      gpointer data)
{
	gboolean *visible = data;
	if (gtk_widget_get_visible (widget))
		*visible = TRUE;
}

static void
popup_menu_for_place (SeahorseSidebar *self,
                      SeahorsePlace *place,
                      guint button,
                      guint32 activate_time)
{
	GtkActionGroup *actions = NULL;
	GtkMenu *menu;
	GList *list, *l;
	GtkWidget *item;
	gboolean visible;

	menu = GTK_MENU (gtk_menu_new ());

	/* First add all the actions from the collection */
	g_object_get (place, "actions", &actions, NULL);
	list = actions ? gtk_action_group_list_actions (actions) : NULL;
	if (list) {
		for (l = list; l != NULL; l = g_list_next (l)) {
			gtk_action_set_accel_group (l->data, self->accel_group);
			item = gtk_action_create_menu_item (l->data);
			gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
		}
		g_list_free (list);
	}
	g_clear_object (&actions);

	/* Lock and unlock items */
	if (SEAHORSE_IS_LOCKABLE (place)) {
		item = gtk_menu_item_new_with_mnemonic (_("_Lock"));
		g_signal_connect (item, "activate", G_CALLBACK (on_place_lock), place);
		g_object_bind_property (place, "lockable", item, "visible", G_BINDING_SYNC_CREATE);
		gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

		item = gtk_menu_item_new_with_mnemonic (_("_Unlock"));
		g_signal_connect (item, "activate", G_CALLBACK (on_place_unlock), place);
		g_object_bind_property (place, "unlockable", item, "visible", G_BINDING_SYNC_CREATE);
		gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
	}

	/* Delete item */
	if (SEAHORSE_IS_DELETABLE (place)) {
		item = gtk_image_menu_item_new_with_mnemonic (_("_Delete"));
		gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item),
		                               gtk_image_new_from_stock (GTK_STOCK_DELETE, GTK_ICON_SIZE_MENU));
		g_signal_connect (item, "activate", G_CALLBACK (on_place_delete), place);
		g_object_bind_property (place, "deletable", item, "sensitive", G_BINDING_SYNC_CREATE);
		gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
		gtk_widget_show (item);
	}

	/* Properties item */
	if (SEAHORSE_IS_VIEWABLE (place)) {
		item = gtk_image_menu_item_new_with_mnemonic (_("_Properties"));
		gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item),
		                               gtk_image_new_from_stock (GTK_STOCK_PROPERTIES, GTK_ICON_SIZE_MENU));
		g_signal_connect (item, "activate", G_CALLBACK (on_place_properties), place);
		gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
		gtk_widget_show (item);
	}

	visible = FALSE;
	gtk_container_foreach (GTK_CONTAINER (menu), check_widget_visible, &visible);

	if (visible) {
		gtk_menu_popup (menu, NULL, NULL, NULL, NULL, button, activate_time);
		gtk_menu_attach_to_widget (menu, GTK_WIDGET (self), NULL);
		gtk_widget_show (GTK_WIDGET (menu));
	} else {
		gtk_widget_destroy (GTK_WIDGET (menu));
	}
}

static void
on_tree_view_popup_menu (GtkWidget *widget,
                         gpointer user_data)
{
	SeahorseSidebar *self = SEAHORSE_SIDEBAR (user_data);
	GtkTreeModel *model = GTK_TREE_MODEL (self->store);
	GcrCollection *collection;
	GtkTreePath *path;
	GtkTreeIter iter;

	gtk_tree_view_get_cursor (self->tree_view, &path, NULL);
	if (path == NULL)
		return;

	if (!gtk_tree_model_get_iter (model, &iter, path))
		g_return_if_reached ();
	gtk_tree_path_free (path);

	gtk_tree_model_get (model, &iter,
	                    SIDEBAR_COLLECTION, &collection,
	                    -1);

	if (SEAHORSE_IS_PLACE (collection))
		popup_menu_for_place (self, SEAHORSE_PLACE (collection), 0, gtk_get_current_event_time ());

	g_clear_object (&collection);
}

static void
update_action_buttons_take_path (SeahorseSidebar *self,
                                 GtkTreePath *path)
{
	GtkTreeIter iter;
	GtkTreePath *old_path;
	GtkTreeModel *model;

	if (path == self->action_highlight_path) {
		gtk_tree_path_free (path);
		return;
	}

	if (path && self->action_highlight_path &&
	    gtk_tree_path_compare (self->action_highlight_path, path) == 0) {
		gtk_tree_path_free (path);
		return;
	}

	old_path = self->action_highlight_path;
	self->action_highlight_path = path;

	model = GTK_TREE_MODEL (self->store);
	if (self->action_highlight_path) {
		if (gtk_tree_model_get_iter (model, &iter, self->action_highlight_path))
			gtk_tree_model_row_changed (model, self->action_highlight_path, &iter);
	}

	if (old_path) {
		if (gtk_tree_model_get_iter (model, &iter, old_path))
			gtk_tree_model_row_changed (model, old_path, &iter);
		gtk_tree_path_free (old_path);
	}
}

static gboolean
over_action_button (SeahorseSidebar *self,
                    gint x,
                    gint y,
                    GtkTreePath **path)
{
	GtkTreeViewColumn *column;
	gint width, x_offset, hseparator;
	GtkTreeIter iter;
	GtkTreeModel *model;

	*path = NULL;

	if (gtk_tree_view_get_path_at_pos (self->tree_view, x, y, path,
	                                   &column, NULL, NULL)) {

		model = GTK_TREE_MODEL (self->store);
		gtk_tree_model_get_iter (model, &iter, *path);

		gtk_widget_style_get (GTK_WIDGET (self->tree_view),
		                      "horizontal-separator", &hseparator,
		                      NULL);

		/* Reload cell attributes for this particular row */
		gtk_tree_view_column_cell_set_cell_data (column,
		                                         model, &iter, FALSE, FALSE);

		gtk_tree_view_column_cell_get_position (column,
		                                        self->action_cell_renderer,
		                                        &x_offset, &width);

		/* This is kinda weird, but we have to do it to workaround gtk+ expanding
		 * the eject cell renderer (even thought we told it not to) and we then
		 * had to set it right-aligned */
		x_offset += width - hseparator - ACTION_BUTTON_XPAD - self->action_button_size;

		if (x - x_offset >= 0 && x - x_offset <= self->action_button_size)
			return TRUE;
	}

	if (*path != NULL) {
		gtk_tree_path_free (*path);
		*path = NULL;
	}

	return FALSE;
}

static gboolean
on_tree_view_motion_notify_event (GtkWidget *widget,
                                  GdkEventMotion *event,
                                  gpointer user_data)
{
	SeahorseSidebar *self = SEAHORSE_SIDEBAR (user_data);
	GtkTreePath *path = NULL;

	if (over_action_button (self, event->x, event->y, &path)) {
		update_action_buttons_take_path (self, path);
		return TRUE;
	}

	update_action_buttons_take_path (self, NULL);
	return FALSE;
}

static gboolean
on_tree_view_button_press_event (GtkWidget *widget,
                                 GdkEventButton *event,
                                 gpointer user_data)
{
	SeahorseSidebar *self = SEAHORSE_SIDEBAR (user_data);
	GtkTreeModel *model = GTK_TREE_MODEL (self->store);
	GcrCollection *collection;
	GtkTreePath *path;
	GtkTreeIter iter;

	if (event->button != 3 || event->type != GDK_BUTTON_PRESS)
		return FALSE;

	if (!gtk_tree_view_get_path_at_pos (self->tree_view, event->x, event->y,
	                                    &path, NULL, NULL, NULL))
		return FALSE;

	gtk_tree_view_set_cursor (self->tree_view, path, NULL, FALSE);
	if (!gtk_tree_model_get_iter (model, &iter, path))
		g_return_val_if_reached (FALSE);
	gtk_tree_path_free (path);

	gtk_tree_model_get (model, &iter,
	                    SIDEBAR_COLLECTION, &collection,
	                    -1);

	if (SEAHORSE_IS_PLACE (collection))
		popup_menu_for_place (self, SEAHORSE_PLACE (collection), event->button, event->time);

	g_clear_object (&collection);
	return TRUE;
}

static gboolean
on_tree_view_button_release_event (GtkWidget *widget,
                                   GdkEventButton *event,
                                   gpointer user_data)
{
	SeahorseSidebar *self = SEAHORSE_SIDEBAR (user_data);
	SeahorseLockable *lockable;
	GtkTreePath *path;
	GtkTreeModel *model;
	GtkWidget *window;
	GtkTreeIter iter;
	gboolean ret;

	if (event->type != GDK_BUTTON_RELEASE)
		return TRUE;

	if (!over_action_button (self, event->x, event->y, &path))
		return FALSE;

	model = GTK_TREE_MODEL (self->store);

	ret = gtk_tree_model_get_iter (model, &iter, path);
	gtk_tree_path_free (path);

	if (!ret)
		return FALSE;

	window = gtk_widget_get_toplevel (widget);
	ret = FALSE;

	lockable = lookup_lockable_for_iter (model, &iter);
	if (lockable) {
		if (seahorse_lockable_can_lock (lockable))
			place_lock (lockable, GTK_WINDOW (window));
		else if (seahorse_lockable_can_unlock (lockable))
			place_unlock (lockable, GTK_WINDOW (window));
	}

	return TRUE;
}

static void
seahorse_sidebar_constructed (GObject *obj)
{
	SeahorseSidebar *self = SEAHORSE_SIDEBAR (obj);
	GtkTreeSelection *selection;
	GtkTreeViewColumn *col;
	GtkTreeView *tree_view;
	GtkCellRenderer *cell;

	G_OBJECT_CLASS (seahorse_sidebar_parent_class)->constructed (obj);

	gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (self)),
	                             GTK_STYLE_CLASS_SIDEBAR);

	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (self),
	                                GTK_POLICY_NEVER,
	                                GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_hadjustment (GTK_SCROLLED_WINDOW (self), NULL);
	gtk_scrolled_window_set_vadjustment (GTK_SCROLLED_WINDOW (self), NULL);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (self), GTK_SHADOW_OUT);
	gtk_style_context_set_junction_sides (gtk_widget_get_style_context (GTK_WIDGET (self)),
	                                      GTK_JUNCTION_RIGHT | GTK_JUNCTION_LEFT);

	/* tree view */
	tree_view = GTK_TREE_VIEW (gtk_tree_view_new ());
	col = gtk_tree_view_column_new ();

	/* initial padding */
	cell = gtk_cell_renderer_text_new ();
	gtk_tree_view_column_pack_start (col, cell, FALSE);
	g_object_set (cell,
	              "xpad", 6,
	              NULL);

	/* headings */
	cell = gtk_cell_renderer_text_new ();
	gtk_tree_view_column_pack_start (col, cell, FALSE);
	gtk_tree_view_column_set_attributes (col, cell,
	                                     "text", SIDEBAR_LABEL,
	                                     NULL);
	g_object_set (cell,
	              "weight", PANGO_WEIGHT_BOLD,
	              "weight-set", TRUE,
	              "ypad", 6,
	              "xpad", 0,
	              NULL);
	gtk_tree_view_column_set_cell_data_func (col, cell,
	                                         on_cell_renderer_heading_visible,
	                                         self, NULL);

	/* icon padding */
	cell = gtk_cell_renderer_text_new ();
	gtk_tree_view_column_pack_start (col, cell, FALSE);
	gtk_tree_view_column_set_cell_data_func (col, cell,
	                                         on_padding_cell_renderer,
	                                         self, NULL);

	/* icon renderer */
	cell = gtk_cell_renderer_pixbuf_new ();
	gtk_tree_view_column_pack_start (col, cell, FALSE);
	gtk_tree_view_column_set_attributes (col, cell,
	                                     "gicon", SIDEBAR_ICON,
	                                     NULL);
	gtk_tree_view_column_set_cell_data_func (col, cell,
	                                         on_cell_renderer_heading_not_visible,
	                                         self, NULL);

	/* normal text renderer */
	cell = gtk_cell_renderer_text_new ();
	gtk_tree_view_column_pack_start (col, cell, TRUE);
	g_object_set (G_OBJECT (cell), "editable", FALSE, NULL);
	gtk_tree_view_column_set_attributes (col, cell,
	                                     "text", SIDEBAR_LABEL,
	                                     NULL);
	gtk_tree_view_column_set_cell_data_func (col, cell,
	                                         on_cell_renderer_heading_not_visible,
	                                         self, NULL);
	g_object_set (cell,
	              "ellipsize", PANGO_ELLIPSIZE_END,
	              "ellipsize-set", TRUE,
	              NULL);

	/* lock/unlock icon renderer */
	cell = gtk_cell_renderer_pixbuf_new ();
	self->action_cell_renderer = cell;
	g_object_set (cell,
	              "mode", GTK_CELL_RENDERER_MODE_ACTIVATABLE,
	              "stock-size", GTK_ICON_SIZE_MENU,
	              "xpad", ACTION_BUTTON_XPAD,
	              "xalign", 1.0,
	              NULL);
	gtk_tree_view_column_pack_start (col, cell, FALSE);
	gtk_tree_view_column_set_cell_data_func (col, cell,
	                                         on_cell_renderer_action_icon,
	                                         self, NULL);

	gtk_tree_view_column_set_max_width (GTK_TREE_VIEW_COLUMN (col), 24);
	gtk_tree_view_append_column (tree_view, col);

	gtk_tree_view_set_headers_visible (tree_view, FALSE);
	gtk_tree_view_set_tooltip_column (tree_view, SIDEBAR_TOOLTIP);
	gtk_tree_view_set_search_column (tree_view, SIDEBAR_LABEL);
	gtk_tree_view_set_model (tree_view, GTK_TREE_MODEL (self->store));
	g_signal_connect (tree_view, "popup-menu", G_CALLBACK (on_tree_view_popup_menu), self);
	g_signal_connect (tree_view, "button-press-event", G_CALLBACK (on_tree_view_button_press_event), self);
	g_signal_connect (tree_view, "motion-notify-event", G_CALLBACK (on_tree_view_motion_notify_event), self);
	g_signal_connect (tree_view, "button-release-event", G_CALLBACK (on_tree_view_button_release_event), self);
	gtk_container_add (GTK_CONTAINER (self), GTK_WIDGET (tree_view));
	gtk_widget_show (GTK_WIDGET (tree_view));
	self->tree_view = tree_view;

	selection = gtk_tree_view_get_selection (tree_view);
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_MULTIPLE);
	gtk_tree_selection_set_select_function (selection, on_tree_selection_validate, self, NULL);
	g_signal_connect (selection, "changed", G_CALLBACK (on_tree_selection_changed), self);


	load_backends (self);
}

static void
seahorse_sidebar_get_property (GObject *obj,
                               guint prop_id,
                               GValue *value,
                               GParamSpec *pspec)
{
	SeahorseSidebar *self = SEAHORSE_SIDEBAR (obj);

	switch (prop_id) {
	case PROP_COLLECTION:
		g_value_set_object (value, seahorse_sidebar_get_collection (self));
		break;
	case PROP_COMBINED:
		g_value_set_boolean (value, seahorse_sidebar_get_combined (self));
		break;
	case PROP_SELECTED_URIS:
		g_value_take_boxed (value, seahorse_sidebar_get_selected_uris (self));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
		break;
	}
}

static void
seahorse_sidebar_set_property (GObject *obj,
                               guint prop_id,
                               const GValue *value,
                               GParamSpec *pspec)
{
	SeahorseSidebar *self = SEAHORSE_SIDEBAR (obj);

	switch (prop_id) {
	case PROP_COMBINED:
		seahorse_sidebar_set_combined (self, g_value_get_boolean (value));
		break;
	case PROP_SELECTED_URIS:
		seahorse_sidebar_set_selected_uris (self, g_value_get_boxed (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
		break;
	}
}

static void
seahorse_sidebar_dispose (GObject *obj)
{
	SeahorseSidebar *self = SEAHORSE_SIDEBAR (obj);
	GList *places, *l;
	gint i;

	for (i = 0; i < self->backends->len; i++) {
		g_signal_handlers_disconnect_by_func (self->backends->pdata[i], on_place_added, self);
		g_signal_handlers_disconnect_by_func (self->backends->pdata[i], on_place_removed, self);
		g_signal_handlers_disconnect_by_func (self->backends->pdata[i], on_backend_changed, self);

		places = gcr_collection_get_objects (self->backends->pdata[i]);
		for (l = places; l != NULL; l = g_list_next (l))
			on_place_removed (self->backends->pdata[i], l->data, self);
		g_list_free (places);
	}

	invalidate_sidebar_pixbufs (self);

	G_OBJECT_CLASS (seahorse_sidebar_parent_class)->dispose (obj);
}

static void
seahorse_sidebar_finalize (GObject *obj)
{
	SeahorseSidebar *self = SEAHORSE_SIDEBAR (obj);

	g_hash_table_destroy (self->selection);
	g_hash_table_destroy (self->chosen);
	g_object_unref (self->objects);

	if (self->update_places_sig)
		g_source_remove (self->update_places_sig);

	g_ptr_array_unref (self->backends);
	g_object_unref (self->store);

	if (self->action_highlight_path)
		gtk_tree_path_free (self->action_highlight_path);

	g_object_unref (self->accel_group);

	G_OBJECT_CLASS (seahorse_sidebar_parent_class)->finalize (obj);
}

static void
seahorse_sidebar_class_init (SeahorseSidebarClass *klass)
{
	GObjectClass *gobject_class= G_OBJECT_CLASS (klass);

	gobject_class->constructed = seahorse_sidebar_constructed;
	gobject_class->dispose = seahorse_sidebar_dispose;
	gobject_class->finalize = seahorse_sidebar_finalize;
	gobject_class->get_property = seahorse_sidebar_get_property;
	gobject_class->set_property = seahorse_sidebar_set_property;

	g_object_class_install_property (gobject_class, PROP_COLLECTION,
	        g_param_spec_object ("collection", "Collection", "Collection of objects sidebar represents",
	                             GCR_TYPE_COLLECTION, G_PARAM_READABLE));

	g_object_class_install_property (gobject_class, PROP_COMBINED,
	        g_param_spec_boolean ("combined", "Combined", "Collection shows all objects combined",
	                              FALSE, G_PARAM_READWRITE));

	g_object_class_install_property (gobject_class, PROP_SELECTED_URIS,
	        g_param_spec_boxed ("selected-uris", "Selected URIs", "URIs selected by the user",
	                            G_TYPE_STRV, G_PARAM_READWRITE));

	signals[CONTEXT_MENU] = g_signal_new ("context-menu", SEAHORSE_TYPE_SIDEBAR,
	                                      G_SIGNAL_RUN_FIRST, 0, NULL, NULL, NULL,
	                                      G_TYPE_NONE, 1, GCR_TYPE_COLLECTION);
}

SeahorseSidebar *
seahorse_sidebar_new (void)
{
	return g_object_new (SEAHORSE_TYPE_SIDEBAR,
	                     NULL);
}

GcrCollection *
seahorse_sidebar_get_collection (SeahorseSidebar *self)
{
	g_return_val_if_fail (SEAHORSE_IS_SIDEBAR (self), NULL);
	return GCR_COLLECTION (self->objects);
}

gboolean
seahorse_sidebar_get_combined (SeahorseSidebar *self)
{
	g_return_val_if_fail (SEAHORSE_IS_SIDEBAR (self), FALSE);
	return self->combined;
}

void
seahorse_sidebar_set_combined (SeahorseSidebar *self,
                               gboolean combined)
{
	g_return_if_fail (SEAHORSE_IS_SIDEBAR (self));
	update_objects_for_combine (self, combined);
	g_object_notify (G_OBJECT (self), "combined");
}

gchar **
seahorse_sidebar_get_selected_uris (SeahorseSidebar *self)
{
	GHashTableIter iter;
	GPtrArray *results;
	gchar *uri;

	g_return_val_if_fail (SEAHORSE_IS_SIDEBAR (self), NULL);

	results = g_ptr_array_new ();
	g_hash_table_iter_init (&iter, self->chosen);
	while (g_hash_table_iter_next (&iter, (gpointer *)&uri, NULL))
		g_ptr_array_add (results, g_strdup (uri));
	g_ptr_array_add (results, NULL);

	return (gchar **)g_ptr_array_free (results, FALSE);
}

void
seahorse_sidebar_set_selected_uris (SeahorseSidebar *self,
                                    const gchar **value)
{
	GHashTable *chosen;
	gint i;

	g_return_if_fail (SEAHORSE_IS_SIDEBAR (self));

	/* For quick lookups */
	chosen = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
	for (i = 0; value != NULL && value[i] != NULL; i++)
		g_hash_table_insert (chosen, g_strdup (value[i]), "");

	update_objects_for_chosen (self, chosen);
	g_hash_table_destroy (self->chosen);
	self->chosen = chosen;

	g_object_notify (G_OBJECT (self), "selected-uris");
}

GList *
seahorse_sidebar_get_selected_places (SeahorseSidebar *self)
{
	GcrCollection *collection;
	GtkTreePath *path;
	RowType row_type;
	GtkTreeIter iter;
	GList *places;

	g_return_val_if_fail (SEAHORSE_IS_SIDEBAR (self), NULL);

	places = gcr_union_collection_elements (self->objects);

	gtk_tree_view_get_cursor (self->tree_view, &path, NULL);
	if (path != NULL) {
		if (!gtk_tree_model_get_iter (GTK_TREE_MODEL (self->store), &iter, path))
			g_return_val_if_reached (NULL);
		gtk_tree_model_get (GTK_TREE_MODEL (self->store), &iter,
		                    SIDEBAR_ROW_TYPE, &row_type,
		                    SIDEBAR_COLLECTION, &collection,
		                    -1);

		if (collection != NULL) {
			if (row_type == TYPE_PLACE) {
				places = g_list_remove (places, collection);
				places = g_list_prepend (places, collection);
			}
			g_object_unref (collection);
		}

		gtk_tree_path_free (path);
	}

	return places;
}

SeahorsePlace *
seahorse_sidebar_get_focused_place (SeahorseSidebar *self)
{
	GcrCollection *collection;
	GtkTreePath *path;
	GtkTreeIter iter;
	RowType row_type;

	g_return_val_if_fail (SEAHORSE_IS_SIDEBAR (self), NULL);

	gtk_tree_view_get_cursor (self->tree_view, &path, NULL);
	if (path != NULL) {
		if (!gtk_tree_model_get_iter (GTK_TREE_MODEL (self->store), &iter, path))
			g_return_val_if_reached (NULL);
		gtk_tree_path_free (path);

		gtk_tree_model_get (GTK_TREE_MODEL (self->store), &iter,
		                    SIDEBAR_ROW_TYPE, &row_type,
		                    SIDEBAR_COLLECTION, &collection,
		                    -1);

		if (row_type == TYPE_PLACE)
			return SEAHORSE_PLACE (collection);
	}

	return NULL;
}

GList *
seahorse_sidebar_get_backends (SeahorseSidebar *self)
{
	GList *backends = NULL;
	GcrCollection *collection;
	GtkTreePath *path;
	RowType row_type;
	GtkTreeIter iter;
	guint i;

	g_return_val_if_fail (SEAHORSE_IS_SIDEBAR (self), NULL);

	for (i = 0; i < self->backends->len; i++)
		backends = g_list_prepend (backends, self->backends->pdata[i]);

	backends = g_list_reverse (backends);

	gtk_tree_view_get_cursor (self->tree_view, &path, NULL);
	if (path != NULL) {
		if (!gtk_tree_model_get_iter (GTK_TREE_MODEL (self->store), &iter, path))
			g_return_val_if_reached (NULL);
		gtk_tree_model_get (GTK_TREE_MODEL (self->store), &iter,
		                    SIDEBAR_ROW_TYPE, &row_type,
		                    SIDEBAR_COLLECTION, &collection,
		                    -1);

		if (collection != NULL) {
			if (row_type == TYPE_BACKEND) {
				backends = g_list_remove (backends, collection);
				backends = g_list_prepend (backends, collection);
			}
			g_object_unref (collection);
		}

		gtk_tree_path_free (path);
	}

	return backends;
}
