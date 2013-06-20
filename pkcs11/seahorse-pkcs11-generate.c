/*
 * Seahorse
 *
 * Copyright (C) 2008 Stefan Walter
 * Copyright (C) 2011 Collabora Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#include "config.h"

#include "seahorse-pkcs11-backend.h"
#include "seahorse-pkcs11-generate.h"
#include "seahorse-token.h"

#include "seahorse-common.h"
#include "seahorse-progress.h"
#include "seahorse-interaction.h"
#include "seahorse-util.h"

#include <glib/gi18n.h>

#include <gcr/gcr.h>

#include <gck/gck.h>

#define SEAHORSE_PKCS11_GENERATE(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_TYPE_PKCS11_GENERATE, SeahorsePkcs11Generate))
#define SEAHORSE_IS_PKCS11_GENERATE(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_TYPE_PKCS11_GENERATE))
#define SEAHORSE_PKCS11_GENERATE_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_PKCS11_GENERATE, SeahorsePkcs11GenerateClass))
#define SEAHORSE_IS_PKCS11_GENERATE_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_PKCS11_GENERATE))
#define SEAHORSE_PKCS11_GENERATE_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_TYPE_PKCS11_GENERATE, SeahorsePkcs11GenerateClass))

typedef struct _SeahorsePkcs11Generate SeahorsePkcs11Generate;
typedef struct _SeahorsePkcs11GenerateClass SeahorsePkcs11GenerateClass;

struct _SeahorsePkcs11Generate {
	GtkDialog dialog;

	GtkEntry *label_entry;

	SeahorseToken *token;
	GtkComboBox *token_box;
	GcrCollectionModel *token_model;

	GckMechanism *mechanism;
	GtkListStore *mechanism_store;
	GtkComboBox *mechanism_box;

	GtkSpinButton *bits_entry;

	GCancellable *cancellable;
	GckAttributes *pub_attrs;
	GckAttributes *prv_attrs;
};

struct _SeahorsePkcs11GenerateClass {
	GtkDialogClass dialog_class;
};

G_DEFINE_TYPE (SeahorsePkcs11Generate, seahorse_pkcs11_generate, GTK_TYPE_DIALOG);

enum {
	MECHANISM_LABEL,
	MECHANISM_TYPE,
	MECHANISM_N_COLS
};

static GType MECHANISM_TYPES[] = {
	G_TYPE_STRING,
	G_TYPE_ULONG
};

struct {
	gulong mechanism_type;
	const gchar *label;
} AVAILABLE_MECHANISMS[] = {
	{ CKM_RSA_PKCS_KEY_PAIR_GEN, N_("RSA") },
};

static void
seahorse_pkcs11_generate_init (SeahorsePkcs11Generate *self)
{

}

static void
update_response (SeahorsePkcs11Generate *self)
{
	gtk_dialog_set_response_sensitive (GTK_DIALOG (self), GTK_RESPONSE_OK,
	                                   self->token != NULL && self->mechanism != NULL);
}

static void
complete_generate (SeahorsePkcs11Generate *self,
                   GError **error)
{
	g_assert (error != NULL);

	if (*error != NULL)
		seahorse_util_handle_error (error, NULL, _("Couldn't generate private key"));
	else
		seahorse_place_load (SEAHORSE_PLACE (self->token), self->cancellable, NULL, NULL);

	g_clear_object (&self->cancellable);
	gck_attributes_unref (self->pub_attrs);
	gck_attributes_unref (self->prv_attrs);
	self->pub_attrs = self->prv_attrs = NULL;
}

static void
prepare_generate (SeahorsePkcs11Generate *self)
{
	CK_BYTE rsa_public_exponent[] = { 0x01, 0x00, 0x01 }; /* 65537 in bytes */
	GckBuilder publi = GCK_BUILDER_INIT;
	GckBuilder priva = GCK_BUILDER_INIT;
	const gchar *label;

	g_assert (self->cancellable == NULL);
	g_assert (self->mechanism != NULL);

	gck_builder_add_ulong (&publi, CKA_CLASS, CKO_PUBLIC_KEY);
	gck_builder_add_ulong (&priva, CKA_CLASS, CKO_PRIVATE_KEY);

	gck_builder_add_boolean (&publi, CKA_TOKEN, TRUE);
	gck_builder_add_boolean (&priva, CKA_TOKEN, TRUE);

	gck_builder_add_boolean (&priva, CKA_PRIVATE, TRUE);
	gck_builder_add_boolean (&priva, CKA_SENSITIVE, TRUE);

	label = gtk_entry_get_text (self->label_entry);
	gck_builder_add_string (&publi, CKA_LABEL, label);
	gck_builder_add_string (&priva, CKA_LABEL, label);

	if (self->mechanism->type == CKM_RSA_PKCS_KEY_PAIR_GEN) {
		gck_builder_add_boolean (&publi, CKA_ENCRYPT, TRUE);
		gck_builder_add_boolean (&publi, CKA_VERIFY, TRUE);
		gck_builder_add_boolean (&publi, CKA_WRAP, TRUE);

		gck_builder_add_boolean (&priva, CKA_DECRYPT, TRUE);
		gck_builder_add_boolean (&priva, CKA_SIGN, TRUE);
		gck_builder_add_boolean (&priva, CKA_UNWRAP, TRUE);

		gck_builder_add_data (&publi, CKA_PUBLIC_EXPONENT,
		                      rsa_public_exponent, sizeof (rsa_public_exponent));
		gck_builder_add_ulong (&publi, CKA_MODULUS_BITS,
		                       gtk_spin_button_get_value_as_int (self->bits_entry));

	} else {
		g_warning ("currently no support for this mechanism");
	}

	self->prv_attrs = gck_builder_steal (&priva);
	self->pub_attrs = gck_builder_steal (&publi);

	gck_builder_clear (&publi);
	gck_builder_clear (&priva);
}

static void
on_generate_complete (GObject *object,
                      GAsyncResult *result,
                      gpointer user_data)
{
	SeahorsePkcs11Generate *self = SEAHORSE_PKCS11_GENERATE (user_data);
	GError *error = NULL;

	gck_session_generate_key_pair_finish (GCK_SESSION (object), result, NULL, NULL, &error);
	complete_generate (self, &error);

	g_object_unref (self);
}

static void
on_generate_open_session (GObject *object,
                          GAsyncResult *result,
                          gpointer user_data)
{
	SeahorsePkcs11Generate *self = SEAHORSE_PKCS11_GENERATE (user_data);
	GError *error = NULL;
	GckSession *session;

	session = gck_session_open_finish (result, &error);
	if (session) {
		gck_session_generate_key_pair_async (session, self->mechanism,
		                                     self->pub_attrs, self->prv_attrs,
		                                     self->cancellable, on_generate_complete,
		                                     g_object_ref (self));
		g_object_unref (session);

	} else {
		complete_generate (self, &error);
	}

	g_object_unref (self);
}

static const gchar *
get_available_mechanism_label (gulong type)
{
	guint i;

	for (i = 0; i < G_N_ELEMENTS (AVAILABLE_MECHANISMS); i++) {
		if (AVAILABLE_MECHANISMS[i].mechanism_type == type)
			return AVAILABLE_MECHANISMS[i].label;
	}

	return NULL;
}

static void
on_token_changed (GtkComboBox *combo_box,
                  gpointer user_data)
{
	SeahorsePkcs11Generate *self = SEAHORSE_PKCS11_GENERATE (user_data);
	GtkTreeIter iter;
	GArray *mechanisms;
	GObject *object;
	gulong type, otype;
	gboolean valid;
	GtkTreeModel *model;
	const gchar *label;
	guint i;

	g_clear_object (&self->token);

	if (gtk_combo_box_get_active_iter (combo_box, &iter)) {
		object = gcr_collection_model_object_for_iter (self->token_model, &iter);
		self->token = g_object_ref (object);
	}

	model = GTK_TREE_MODEL (self->mechanism_store);
	valid = gtk_tree_model_get_iter_first (model, &iter);
	if (self->token) {
		mechanisms = seahorse_token_get_mechanisms (self->token);
		for (i = 0; mechanisms && i < mechanisms->len; i++) {
			type = g_array_index (mechanisms, gulong, i);
			label = get_available_mechanism_label (type);
			if (label == NULL)
				continue;
			while (valid) {
				gtk_tree_model_get (model, &iter, MECHANISM_TYPE, &otype, -1);
				if (otype == type)
					break;
				valid = gtk_list_store_remove (self->mechanism_store, &iter);
			}
			if (!valid)
				gtk_list_store_append (self->mechanism_store, &iter);
			gtk_list_store_set (self->mechanism_store, &iter,
			                    MECHANISM_TYPE, type,
			                    MECHANISM_LABEL, label,
			                    -1);
			valid = gtk_tree_model_iter_next (model, &iter);
		}
	}
	while (valid)
		valid = gtk_list_store_remove (self->mechanism_store, &iter);

	/* Select first mechanism if none are selected */
	if (!gtk_combo_box_get_active_iter (self->mechanism_box, &iter))
		if (gtk_tree_model_get_iter_first (model, &iter))
			gtk_combo_box_set_active_iter (self->mechanism_box, &iter);

	update_response (self);
}

static void
on_mechanism_changed (GtkComboBox *widget,
                      gpointer user_data)
{
	SeahorsePkcs11Generate *self = SEAHORSE_PKCS11_GENERATE (user_data);
	GckMechanismInfo *info;
	GtkTreeIter iter;
	GckSlot *slot;
	guint min;
	guint max;

	g_free (self->mechanism);
	self->mechanism = NULL;

	if (gtk_combo_box_get_active_iter (widget, &iter)) {
		self->mechanism = g_new0 (GckMechanism, 1);
		gtk_tree_model_get (GTK_TREE_MODEL (self->mechanism_store), &iter,
		                    MECHANISM_TYPE, &self->mechanism->type, -1);

		slot = seahorse_token_get_slot (self->token);
		info = gck_slot_get_mechanism_info (slot, self->mechanism->type);
		g_return_if_fail (info != NULL);

		min = info->min_key_size;
		max = info->max_key_size;

		if (min < 512 && max >= 512)
			min = 512;
		if (max > 16384 && min <= 16384)
			max = 16384;
		gtk_spin_button_set_range (self->bits_entry, min, max);

		gck_mechanism_info_free (info);
	}

	gtk_widget_set_sensitive (GTK_WIDGET (self->bits_entry),
	                          self->mechanism != NULL);

	update_response (self);
}

static gint
on_mechanism_sort (GtkTreeModel *model,
                   GtkTreeIter *a,
                   GtkTreeIter *b,
                   gpointer user_data)
{
	gchar *label_a = NULL;
	gchar *label_b = NULL;
	gint ret;

	gtk_tree_model_get (model, a, MECHANISM_LABEL, &label_a, -1);
	gtk_tree_model_get (model, b, MECHANISM_LABEL, &label_b, -1);
	ret = g_strcmp0 (label_a, label_b);
	g_free (label_a);
	g_free (label_b);

	return ret;
}

static void
seahorse_pkcs11_generate_constructed (GObject *obj)
{
	SeahorsePkcs11Generate *self = SEAHORSE_PKCS11_GENERATE (obj);
	GtkCellRenderer *renderer;
	GcrCollection *collection;
	GtkBuilder *builder;
	const gchar *path;
	GError *error = NULL;
	GtkWidget *content;
	GtkWidget *widget;

	G_OBJECT_CLASS (seahorse_pkcs11_generate_parent_class)->constructed (obj);

	builder = gtk_builder_new ();
	path = SEAHORSE_UIDIR "/seahorse-pkcs11-generate.xml";
	gtk_builder_add_from_file (builder, path, &error);
	if (error != NULL) {
		g_warning ("couldn't load ui file: %s", path);
		g_clear_error (&error);
		g_object_unref (builder);
		return;
	}

	gtk_window_set_resizable (GTK_WINDOW (self), FALSE);
	content = gtk_dialog_get_content_area (GTK_DIALOG (self));
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "pkcs11-generate"));
	gtk_container_add (GTK_CONTAINER (content), widget);
	gtk_widget_show (widget);

	self->bits_entry = GTK_SPIN_BUTTON (gtk_builder_get_object (builder, "key-bits"));
	gtk_spin_button_set_range (self->bits_entry, 0, G_MAXINT); /* updated later */
	gtk_spin_button_set_increments (self->bits_entry, 128, 128);
	gtk_spin_button_set_value (self->bits_entry, 2048);

	self->label_entry = GTK_ENTRY (gtk_builder_get_object (builder, "key-label"));

	/* The mechanism */
	self->mechanism_box = GTK_COMBO_BOX (gtk_builder_get_object (builder, "key-mechanism"));
	G_STATIC_ASSERT (MECHANISM_N_COLS == G_N_ELEMENTS (MECHANISM_TYPES));
	self->mechanism_store = gtk_list_store_newv (MECHANISM_N_COLS, MECHANISM_TYPES);
	gtk_tree_sortable_set_default_sort_func (GTK_TREE_SORTABLE (self->mechanism_store),
	                                         on_mechanism_sort, NULL, NULL);
	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (self->mechanism_store),
	                                      GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID, GTK_SORT_ASCENDING);
	gtk_combo_box_set_model (self->mechanism_box, GTK_TREE_MODEL (self->mechanism_store));
	renderer = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (self->mechanism_box), renderer, TRUE);
	gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (self->mechanism_box), renderer, "markup", MECHANISM_LABEL);
	g_signal_connect (self->mechanism_box, "changed", G_CALLBACK (on_mechanism_changed), self);

	/* The tokens */
	self->token_box = GTK_COMBO_BOX (gtk_builder_get_object (builder, "key-token"));
	collection = seahorse_pkcs11_backend_get_writable_tokens (NULL, CKM_RSA_PKCS_KEY_PAIR_GEN);
	self->token_model = gcr_collection_model_new (collection, GCR_COLLECTION_MODEL_LIST,
	                                              "icon", G_TYPE_ICON, "label", G_TYPE_STRING, NULL);
	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (self->token_model), 1, GTK_SORT_ASCENDING);
	gtk_combo_box_set_model (self->token_box, GTK_TREE_MODEL (self->token_model));
	renderer = gtk_cell_renderer_pixbuf_new ();
	g_object_set (renderer, "stock-size", GTK_ICON_SIZE_BUTTON, NULL);
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (self->token_box), renderer, FALSE);
	gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (self->token_box), renderer, "gicon", 0);
	renderer = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (self->token_box), renderer, TRUE);
	gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (self->token_box), renderer, "text", 1);
	g_signal_connect (self->token_box, "changed", G_CALLBACK (on_token_changed), self);
	if (gcr_collection_get_length (collection) > 0)
		gtk_combo_box_set_active (self->token_box, 0);
	g_object_unref (collection);

	/* The buttons */
	gtk_dialog_add_buttons (GTK_DIALOG (self),
	                        GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
	                        _("Create"), GTK_RESPONSE_OK,
	                        NULL);
	gtk_dialog_set_default_response (GTK_DIALOG (self), GTK_RESPONSE_OK);

	update_response (self);
	g_object_unref (builder);
}

static void
seahorse_pkcs11_generate_finalize (GObject *obj)
{
	SeahorsePkcs11Generate *self = SEAHORSE_PKCS11_GENERATE (obj);

	g_clear_object (&self->token);
	g_clear_object (&self->token_model);

	g_free (self->mechanism);
	g_clear_object (&self->mechanism_store);

	g_clear_object (&self->cancellable);
	gck_attributes_unref (self->pub_attrs);
	gck_attributes_unref (self->prv_attrs);

	G_OBJECT_CLASS (seahorse_pkcs11_generate_parent_class)->finalize (obj);
}

static void
seahorse_pkcs11_generate_response (GtkDialog *dialog,
                                   gint response_id)
{
	SeahorsePkcs11Generate *self = SEAHORSE_PKCS11_GENERATE (dialog);
	GTlsInteraction *interaction;
	GtkWindow *parent;

	if (response_id == GTK_RESPONSE_OK) {
		g_return_if_fail (self->token);

		prepare_generate (self);
		parent = gtk_window_get_transient_for (GTK_WINDOW (self));
		interaction = seahorse_interaction_new (parent);

		gck_session_open_async (seahorse_token_get_slot (self->token),
		                        GCK_SESSION_READ_WRITE | GCK_SESSION_LOGIN_USER,
		                        interaction, self->cancellable,
		                        on_generate_open_session, g_object_ref (self));

		seahorse_progress_show (self->cancellable, _("Generating key"), FALSE);
		g_object_unref (interaction);
	}

	gtk_widget_hide (GTK_WIDGET (dialog));
}

static void
seahorse_pkcs11_generate_class_init (SeahorsePkcs11GenerateClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	GtkDialogClass *dialog_class = GTK_DIALOG_CLASS (klass);

	gobject_class->constructed = seahorse_pkcs11_generate_constructed;
	gobject_class->finalize = seahorse_pkcs11_generate_finalize;

	dialog_class->response = seahorse_pkcs11_generate_response;
}

void
seahorse_pkcs11_generate_prompt (GtkWindow *parent)
{
	GtkDialog *dialog;

	g_return_if_fail (GTK_IS_WINDOW (parent));

	dialog = g_object_new (SEAHORSE_TYPE_PKCS11_GENERATE,
	                       "transient-for", parent,
	                       NULL);
	g_object_ref_sink (dialog);

	gtk_dialog_run (dialog);

	g_object_unref (dialog);
}

static void
on_generate_activate (GtkAction *action,
                      gpointer user_data)
{
	GtkWindow *parent;

	parent = seahorse_action_get_window (action);
	seahorse_pkcs11_generate_prompt (parent);
}

static const GtkActionEntry ACTION_ENTRIES[] = {
	{ "pkcs11-generate-key", GCR_ICON_KEY_PAIR, N_ ("Private key"), "",
	  N_("Used to request a certificate"), G_CALLBACK (on_generate_activate) }
};

void
seahorse_pkcs11_generate_register (void)
{
	GtkActionGroup *actions;

	actions = gtk_action_group_new ("pkcs11-generate");
	gtk_action_group_set_translation_domain (actions, GETTEXT_PACKAGE);
	gtk_action_group_add_actions (actions, ACTION_ENTRIES, G_N_ELEMENTS (ACTION_ENTRIES), NULL);
	seahorse_registry_register_object (G_OBJECT (actions), "generator");
}
