/*
 * Seahorse
 *
 * Copyright (C) 2013 Giovanni Campagna <scampa.giovanni@gmail.com>
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

#include <string.h>

#include <gcr/gcr.h>
#define SECRET_API_SUBJECT_TO_CHANGE
#include <libsecret/secret.h>

#include "seahorse-predicate.h"
#include "seahorse-common.h"
#include "seahorse-widget.h"
#include "seahorse-collection.h"

#include "seahorse-shell-search-provider-generated.h"
#include "seahorse-search-provider.h"

struct _SeahorseSearchProvider {
	SeahorseShellSearchProvider2Skeleton parent;

	GcrUnionCollection *union_collection;

	SeahorsePredicate base_predicate;
	GcrCollection *collection;
};

struct _SeahorseSearchProviderClass {
	SeahorseShellSearchProvider2SkeletonClass parent_class;
};

static void seahorse_shell_search_provider2_iface_init (SeahorseShellSearchProvider2Iface *iface);

G_DEFINE_TYPE_WITH_CODE (SeahorseSearchProvider, seahorse_search_provider, SEAHORSE_TYPE_SHELL_SEARCH_PROVIDER2_SKELETON,
                         G_IMPLEMENT_INTERFACE (SEAHORSE_TYPE_SHELL_SEARCH_PROVIDER2, seahorse_shell_search_provider2_iface_init))

/* Search through row for text */
static gboolean
object_contains_filtered_text (GObject     *object,
			       const gchar *text)
{
	gchar* name = NULL;
	gchar* description = NULL;
	gchar* lower;
	gboolean ret = FALSE;

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

static gboolean
object_matches_search (GObject     *object,
		       gpointer     user_data)
{
	char **terms = user_data;
	int i;

	for (i = 0; terms[i]; i++) {
		if (!object_contains_filtered_text (object, terms[i]))
			return FALSE;
	}

	return TRUE;
}

static void
init_predicate (SeahorsePredicate  *predicate,
                char              **terms)
{
	memset(predicate, 0, sizeof(SeahorsePredicate));

	predicate->custom = object_matches_search;
	predicate->custom_target = terms;
}

static gboolean
handle_get_initial_result_set (SeahorseShellSearchProvider2 *skeleton,
                               GDBusMethodInvocation        *invocation,
                               const char * const           *terms)
{
	SeahorseSearchProvider *self = SEAHORSE_SEARCH_PROVIDER (skeleton);
	SeahorsePredicate   predicate;
	GPtrArray *array;
	GList *objects, *l;
	char **results;

	init_predicate (&predicate, (char **) terms);

	array = g_ptr_array_new ();
	objects = gcr_collection_get_objects (self->collection);

	for (l = objects; l; l = l->next) {
		if (seahorse_predicate_match (&predicate, l->data)) {
			char *str = g_strdup_printf("%p", l->data);

			g_ptr_array_add (array, str);
		}
	}

	g_list_free (objects);
	g_ptr_array_add (array, NULL);
	results = (char **) g_ptr_array_free (array, FALSE);

	seahorse_shell_search_provider2_complete_get_initial_result_set (skeleton,
	                                                                 invocation,
	                                                                 (const char* const*) results);

	g_strfreev (results);
	return TRUE;
}

static gboolean
handle_get_subsearch_result_set (SeahorseShellSearchProvider2 *skeleton,
                                 GDBusMethodInvocation        *invocation,
                                 const char * const           *previous_results,
                                 const char * const           *terms)
{
	SeahorseSearchProvider *self = SEAHORSE_SEARCH_PROVIDER (skeleton);
	SeahorsePredicate   predicate;
	GPtrArray *array;
	int i;
	char **results;

	init_predicate (&predicate, (char **) terms);

	array = g_ptr_array_new ();

	for (i = 0; previous_results[i]; i++) {
		GObject *object;

		sscanf (previous_results[i], "%p", &object);

		if (!gcr_collection_contains (self->collection, object)) {
			/* Bogus value */
			continue;
		}

		if (seahorse_predicate_match (&predicate, object)) {
			g_ptr_array_add (array, (char*) previous_results[i]);
		}
	}

	g_ptr_array_add (array, NULL);
	results = (char **) g_ptr_array_free (array, FALSE);

	seahorse_shell_search_provider2_complete_get_subsearch_result_set (skeleton,
	                                                                   invocation,
	                                                                   (const char* const*) results);

	/* g_free, not g_strfreev, because we don't duplicate result strings */
	g_free (results);
	return TRUE;
}

static gboolean
handle_get_result_metas (SeahorseShellSearchProvider2 *skeleton,
                         GDBusMethodInvocation        *invocation,
                         const char * const           *results)
{
	SeahorseSearchProvider *self = SEAHORSE_SEARCH_PROVIDER (skeleton);
	int i;
	GVariantBuilder builder;
	char *name, *description, *escaped_description, *icon_string;
	GIcon *icon;

	g_variant_builder_init (&builder, G_VARIANT_TYPE ("aa{sv}"));

	for (i = 0; results[i]; i++) {
		GObject *object;

		sscanf (results[i], "%p", &object);

		if (!gcr_collection_contains (self->collection, object)) {
			/* Bogus value */
			continue;
		}

		g_object_get (object,
		              "label", &name,
		              "icon", &icon,
		              "description", &description,
		              NULL);

		g_variant_builder_open (&builder, G_VARIANT_TYPE ("a{sv}"));
		g_variant_builder_add (&builder, "{sv}",
		                       "id", g_variant_new_string (results[i]));
		if (name) {
			g_variant_builder_add (&builder, "{sv}",
			                       "name", g_variant_new_string (name));
			g_free (name);
		}
		if (icon) {
			icon_string = g_icon_to_string (icon);
			g_variant_builder_add (&builder, "{sv}",
			                       "gicon",
			                       icon_string ? g_variant_new_string (icon_string) : NULL);
			g_free (icon_string);
			g_object_unref (icon);
		}
		if (description) {
			escaped_description = description ? g_markup_escape_text (description, -1) : NULL;
			g_variant_builder_add (&builder, "{sv}",
			                       "description",
			                       escaped_description ? g_variant_new_string (description) : NULL);
			g_free (escaped_description);
			g_free (description);
		}

		g_variant_builder_close (&builder);
	}

	seahorse_shell_search_provider2_complete_get_result_metas (skeleton,
	                                                           invocation,
	                                                           g_variant_builder_end (&builder));
	return TRUE;
}

static gboolean
handle_activate_result (SeahorseShellSearchProvider2 *skeleton,
                        GDBusMethodInvocation        *invocation,
                        const char                   *identifier,
                        const char * const           *results,
                        guint                         timestamp)
{
	SeahorseSearchProvider *self = SEAHORSE_SEARCH_PROVIDER (skeleton);
	GObject *object;
	SeahorseWidget *key_manager;
	GtkWindow *window;

	sscanf (identifier, "%p", &object);

	if (!gcr_collection_contains (self->collection, object) ||
	    !SEAHORSE_IS_VIEWABLE (object)) {
		/* Bogus value */
		return TRUE;
	}

	key_manager = seahorse_widget_find ("key-manager");
	if (key_manager == NULL) {
		GtkApplication *app;

		app = seahorse_application_get ();
		g_application_activate (G_APPLICATION (app));

		key_manager = seahorse_widget_find ("key-manager");
	}

	window = GTK_WINDOW (seahorse_widget_get_widget (key_manager, key_manager->name));
	gtk_window_present_with_time (window, timestamp);

	seahorse_viewable_view (object, window);

	seahorse_shell_search_provider2_complete_activate_result (skeleton,
	                                                          invocation);

	return TRUE;
}

static gboolean
handle_launch_search (SeahorseShellSearchProvider2  *skeleton,
                      GDBusMethodInvocation         *invocation,
                      const char * const            *terms,
                      guint                          timestamp)
{
	/* TODO */
	return FALSE;
}

static void
on_place_added (GcrCollection *places,
                GObject       *place,
                gpointer       user_data)
{
	SeahorseSearchProvider *self = SEAHORSE_SEARCH_PROVIDER (user_data);

	if (!gcr_union_collection_have (self->union_collection,
	                                GCR_COLLECTION (place)))
		gcr_union_collection_add (self->union_collection,
		                          GCR_COLLECTION (place));
}

static void
on_place_removed (GcrCollection *places,
                  GObject       *place,
                  gpointer       user_data)
{
	SeahorseSearchProvider *self = SEAHORSE_SEARCH_PROVIDER (user_data);

	if (gcr_union_collection_have (self->union_collection,
	                               GCR_COLLECTION (place)))
		gcr_union_collection_remove (self->union_collection,
		                             GCR_COLLECTION (place));
}

void
seahorse_search_provider_initialize (SeahorseSearchProvider *self)
{
	GList *backends, *l;
	GList *places, *p;

	backends = seahorse_backend_get_registered ();
	for (l = backends; l != NULL; l = g_list_next (l)) {
		g_signal_connect_object (l->data, "added", G_CALLBACK (on_place_added), self, 0);
		g_signal_connect_object (l->data, "removed", G_CALLBACK (on_place_removed), self, 0);

		places = gcr_collection_get_objects (l->data);
		for (p = places; p != NULL; p = g_list_next (p))
			on_place_added (l->data, p->data, self);
		g_list_free (places);
	}
	g_list_free (backends);
}

static gboolean
check_object_type (GObject  *object,
		   gpointer  user_data)
{
	if (!SEAHORSE_IS_VIEWABLE (object))
		return FALSE;

	if (SECRET_IS_ITEM (object)) {
		const char *schema_name;

		schema_name = secret_item_get_schema_name (SECRET_ITEM (object));
		if (g_strcmp0 (schema_name, "org.gnome.keyring.Note") != 0)
			return FALSE;
	}

	return TRUE;
}

static void
seahorse_shell_search_provider2_iface_init (SeahorseShellSearchProvider2Iface *iface)
{
	iface->handle_get_initial_result_set = handle_get_initial_result_set;
	iface->handle_get_subsearch_result_set = handle_get_subsearch_result_set;
	iface->handle_get_result_metas = handle_get_result_metas;
	iface->handle_activate_result = handle_activate_result;
	iface->handle_launch_search = handle_launch_search;
}

static void
seahorse_search_provider_init (SeahorseSearchProvider *self)
{
	SeahorseCollection *filtered;
	GcrCollection *base;

	base = gcr_union_collection_new ();
	self->union_collection = GCR_UNION_COLLECTION (base);

	self->base_predicate.flags = SEAHORSE_FLAG_PERSONAL;
	self->base_predicate.custom = check_object_type;

	filtered = seahorse_collection_new_for_predicate (base,
	                                                  &self->base_predicate, NULL);
	self->collection = GCR_COLLECTION (filtered);
}

gboolean
seahorse_search_provider_dbus_register (SeahorseSearchProvider  *self,
					GDBusConnection         *connection,
					const gchar             *object_path,
					GError                 **error)
{
	GDBusInterfaceSkeleton *skeleton;

	skeleton = G_DBUS_INTERFACE_SKELETON (self);

	return g_dbus_interface_skeleton_export (skeleton, connection, object_path, error);
}

void
seahorse_search_provider_dbus_unregister (SeahorseSearchProvider *self,
					  GDBusConnection        *connection,
					  const gchar            *object_path)
{
	GDBusInterfaceSkeleton *skeleton;

	skeleton = G_DBUS_INTERFACE_SKELETON (self);

	if (g_dbus_interface_skeleton_has_connection (skeleton, connection))
		g_dbus_interface_skeleton_unexport_from_connection (skeleton, connection);
}

static void
seahorse_search_provider_dispose (GObject *object)
{
	SeahorseSearchProvider *self;

	self = SEAHORSE_SEARCH_PROVIDER (object);

	g_clear_object (&self->collection);

	G_OBJECT_CLASS (seahorse_search_provider_parent_class)->dispose (object);
}

static void
seahorse_search_provider_class_init (SeahorseSearchProviderClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->dispose = seahorse_search_provider_dispose;
}

SeahorseSearchProvider *
seahorse_search_provider_new (void)
{
	return g_object_new (SEAHORSE_TYPE_SEARCH_PROVIDER, NULL);
}

