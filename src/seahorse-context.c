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

#include <gnome.h>
#include <gconf/gconf-client.h>

#include "seahorse-context.h"

#define PREFERENCES "/apps/seahorse/preferences"
#define ARMOR_KEY PREFERENCES "/ascii_armor"
#define TEXT_KEY PREFERENCES "/text_mode"
#define DEFAULT_KEY PREFERENCES "/default_key_id"

struct _SeahorseContextPrivate
{
	GList *key_ring;
	GConfClient *gclient;
};

enum {
	STATUS,
	ADD,
	LAST_SIGNAL
};

static void	seahorse_context_class_init	(SeahorseContextClass	*klass);
static void	seahorse_context_init		(SeahorseContext	*sctx);
static void	seahorse_context_finalize	(GObject		*gobject);
/* Key signals */
static void	seahorse_context_key_destroyed	(GtkObject		*object,
						 SeahorseContext	*sctx);
static void	seahorse_context_key_changed	(SeahorseKey		*skey,
						 SeahorseKeyChange	change,
						 SeahorseContext	*sctx);

static GtkObjectClass	*parent_class			= NULL;
static guint		context_signals[LAST_SIGNAL]	= { 0 };

GType
seahorse_context_get_type (void)
{
	static GType context_type = 0;
	
	if (!context_type) {
		static const GTypeInfo context_info =
		{
			sizeof (SeahorseContextClass),
			NULL, NULL,
			(GClassInitFunc) seahorse_context_class_init,
			NULL, NULL,
			sizeof (SeahorseContext),
			0,
			(GInstanceInitFunc) seahorse_context_init
		};
		
		context_type = g_type_register_static (GTK_TYPE_OBJECT, "SeahorseContext", &context_info, 0);
	}
	
	return context_type;
}

static void
seahorse_context_class_init (SeahorseContextClass *klass)
{
	GObjectClass *gobject_class;
	
	parent_class = g_type_class_peek_parent (klass);
	gobject_class = G_OBJECT_CLASS (klass);
	
	gobject_class->finalize = seahorse_context_finalize;
	
	klass->status = NULL;
	klass->add = NULL;
	
	context_signals[STATUS] = g_signal_new ("status", G_OBJECT_CLASS_TYPE (gobject_class),
		G_SIGNAL_RUN_LAST,  G_STRUCT_OFFSET (SeahorseContextClass, status),
		NULL, NULL, g_cclosure_marshal_VOID__STRING, G_TYPE_NONE, 1, G_TYPE_STRING);
	context_signals[ADD] = g_signal_new ("add", G_OBJECT_CLASS_TYPE (gobject_class),
		G_SIGNAL_RUN_LAST, G_STRUCT_OFFSET (SeahorseContextClass, add),
		NULL, NULL, g_cclosure_marshal_VOID__OBJECT, G_TYPE_NONE, 1, SEAHORSE_TYPE_KEY);
}

/* Initializes the gpgme context & preferences */
static void
seahorse_context_init (SeahorseContext *sctx)
{
	GpgmeProtocol proto;
	GpgmeError err;
	
	sctx->priv = g_new0 (SeahorseContextPrivate, 1);
	
	sctx->priv->gclient = gconf_client_get_default ();
	
	proto = GPGME_PROTOCOL_OpenPGP;
	err = gpgme_engine_check_version (proto);
	g_return_if_fail (err == GPGME_No_Error);
	
	err = gpgme_new (&(sctx->ctx));
	g_return_if_fail (err == GPGME_No_Error);
	
	err = gpgme_set_protocol (sctx->ctx, proto);
	g_return_if_fail (err == GPGME_No_Error);
	
	gpgme_set_armor (sctx->ctx, gconf_client_get_bool (
		sctx->priv->gclient, ARMOR_KEY, NULL));
	gpgme_set_textmode (sctx->ctx, gconf_client_get_bool (
		sctx->priv->gclient, TEXT_KEY, NULL));
}

/* Destroy all keys & key ring, release gpgme context */
static void
seahorse_context_finalize (GObject *gobject)
{
	SeahorseContext *sctx;
	SeahorseKey *skey;
	
	sctx = SEAHORSE_CONTEXT (gobject);
	
	while (sctx->priv->key_ring != NULL && (
	skey = sctx->priv->key_ring->data) != NULL)
		seahorse_key_destroy (skey);
	
	g_list_free (sctx->priv->key_ring);
	gpgme_release (sctx->ctx);
	
	G_OBJECT_CLASS (parent_class)->finalize (gobject);
}

/* Remove key from key ring */
static void
seahorse_context_key_destroyed (GtkObject *object, SeahorseContext *sctx)
{
	SeahorseKey *skey;
	
	skey = SEAHORSE_KEY (object);
	sctx->priv->key_ring = g_list_remove (sctx->priv->key_ring, skey);
	
	g_signal_handlers_disconnect_by_func (GTK_OBJECT (skey),
		seahorse_context_key_destroyed, sctx);
	g_signal_handlers_disconnect_by_func (skey,
		seahorse_context_key_changed, sctx);
	
	g_object_unref (skey);
}

/* A key has changed, so refresh the SeahorseKey's GpgmeKey */
static void
seahorse_context_key_changed (SeahorseKey *skey, SeahorseKeyChange change, SeahorseContext *sctx)
{
	GpgmeKey key;
	
	g_return_if_fail (gpgme_op_keylist_start (sctx->ctx,
		seahorse_key_get_keyid (skey, 0), FALSE) == GPGME_No_Error);
	g_return_if_fail (gpgme_op_keylist_next (sctx->ctx, &key) == GPGME_No_Error);
	gpgme_op_keylist_end (sctx->ctx);
	gpgme_key_unref (skey->key);
	skey->key = key;
}

/* Add a new SeahorseKey to the key ring */
static SeahorseKey*
add_key (SeahorseContext *sctx, GpgmeKey key)
{
	SeahorseKey *skey;
		
	skey = seahorse_key_new (key);
	g_object_ref (skey);
	gtk_object_sink (GTK_OBJECT (skey));
	
	g_signal_connect_after (GTK_OBJECT (skey), "destroy",
		G_CALLBACK (seahorse_context_key_destroyed), sctx);
	g_signal_connect (skey, "changed",
		G_CALLBACK (seahorse_context_key_changed), sctx);
	
	return skey;
}

/**
 * seahorse_context_new:
 *
 * Creates a new #SeahorseContext for managing the key ring and preferences.
 *
 * Returns: The new #SeahorseContext
 **/
SeahorseContext*
seahorse_context_new (void)
{
	SeahorseContext *sctx;
	GpgmeKey key; 
	GList *list = NULL;
	gchar *keyid = "";
	SeahorseKey *skey;
	
	sctx = g_object_new (SEAHORSE_TYPE_CONTEXT, NULL);
	gpgme_set_keylist_mode (sctx->ctx, GPGME_KEYLIST_MODE_LOCAL);
	
	g_return_if_fail (gpgme_op_keylist_start (sctx->ctx, NULL, FALSE) == GPGME_No_Error);
	
	keyid = gconf_client_get_string (sctx->priv->gclient, DEFAULT_KEY, NULL);
	if (keyid == NULL)
		keyid = "";
	
	/* Make key ring */
	while (gpgme_op_keylist_next (sctx->ctx, &key) == GPGME_No_Error) {
		skey = add_key (sctx, key);
		list = g_list_append (list, skey);
		
		if (g_str_equal (keyid, seahorse_key_get_keyid (skey, 0)))
			seahorse_context_set_signer (sctx, skey);
	}
	sctx->priv->key_ring = g_list_first (list);
	
	return sctx;
}

/**
 * seahorse_context_destroy:
 * @sctx: #SeahorseContext to destroy
 *
 * Emits the destroy signal for @sctx.
 **/
void
seahorse_context_destroy (SeahorseContext *sctx)
{
	g_return_if_fail (GTK_IS_OBJECT (sctx));
	
	gtk_object_destroy (GTK_OBJECT (sctx));
}

/**
 * seahorse_context_get_keys:
 * @sctx: Current #SeahorseContext
 *
 * Gets the current key ring.
 *
 * Returns: A #GList of #SeahorseKeys
 **/
GList*
seahorse_context_get_keys (const SeahorseContext *sctx)
{
	g_return_val_if_fail (SEAHORSE_IS_CONTEXT (sctx) && sctx->priv != NULL, NULL);
	
	return sctx->priv->key_ring;
}

/**
 * seahorse_context_get_key:
 * @sctx: Current #SeahorseContext
 * @key: GpgmeKey corresponding to the desired #SeahorseKey
 *
 * Gets the #SeahorseKey in the key ring corresponding to @key.
 *
 * Returns: The found #SeahorseKey, or NULL
 **/
SeahorseKey*
seahorse_context_get_key (const SeahorseContext *sctx, GpgmeKey key)
{
	GList *list;
	SeahorseKey *skey = NULL;
	const gchar *id1, *id2;
	
	g_return_val_if_fail (SEAHORSE_IS_CONTEXT (sctx), NULL);
	g_return_val_if_fail (key != NULL, NULL);
	
	list = seahorse_context_get_keys (sctx);
	id1 = gpgme_key_get_string_attr (key, GPGME_ATTR_KEYID, NULL, 0);
	
	/* Search by keyid */
	while (list != NULL && (skey = list->data) != NULL) {
		id2 = gpgme_key_get_string_attr (skey->key, GPGME_ATTR_KEYID, NULL, 0);
		
		if (g_str_equal (id1, id2))
			break;
		
		list = g_list_next (list);
	}
	
	gpgme_key_unref (key);
	return skey;
}

/**
 * seahorse_context_show_status:
 * @sctx: Current #SeahorseContext
 * @op: Operation performed
 * @success: Whether @op was successful
 *
 * Emits the status signal for @op & @success.
 **/
void
seahorse_context_show_status (const SeahorseContext *sctx, const gchar *op, gboolean success)
{
	gchar *status;
	
	g_return_if_fail (SEAHORSE_IS_CONTEXT (sctx));
	g_return_if_fail (op != NULL);
	
	/* Construct status message */
	if (success)
		status = g_strdup_printf (_("%s Successful"), op);
	else
		status = g_strdup_printf (_("%s Failed"), op);
	g_print ("%s\n", status);
	
	g_signal_emit (G_OBJECT (sctx), context_signals[STATUS], 0, status);
	
	g_free (status);
}

/**
 * seahorse_context_key_added
 * @sctx: Current #SeahorseContext
 *
 * Causes @sctx to add a new keys to internal key ring.
 **/
void
seahorse_context_key_added (SeahorseContext *sctx)
{
	GpgmeKey key;
	GList *list;
	SeahorseKey *skey;
	
	g_return_if_fail (SEAHORSE_IS_CONTEXT (sctx));
        g_return_if_fail (gpgme_op_keylist_start (sctx->ctx, NULL, FALSE) == GPGME_No_Error);	
	
	list = sctx->priv->key_ring;
	
	/* Go to end of cached list, then add any keys remaining in the gpgme keylist */
	while (gpgme_op_keylist_next (sctx->ctx, &key) == GPGME_No_Error) {
		if (list == NULL) {
			skey = add_key (sctx, key);
			sctx->priv->key_ring = g_list_append (sctx->priv->key_ring, skey);
			g_signal_emit (G_OBJECT (sctx), context_signals[ADD], 0, skey);
			list = g_list_last (sctx->priv->key_ring);
		}
		list = g_list_next (list);
	}
}

/**
 * seahorse_context_key_has_secret:
 * @sctx: Current #SeahorseContext
 * @skey: #SeahorseKey to check
 *
 * Checks if @skey has a secret part.
 *
 * Returns: %TRUE if @skey has a secret part, %FALSE otherwise
 **/
gboolean
seahorse_context_key_has_secret (SeahorseContext *sctx, SeahorseKey *skey)
{
	GpgmeKey key = NULL;
	gboolean found;
	
	g_return_val_if_fail (SEAHORSE_IS_CONTEXT (sctx), FALSE);
	g_return_val_if_fail (SEAHORSE_IS_KEY (skey), FALSE);
	
	/* Look for key by keyid */
	g_return_val_if_fail (gpgme_op_keylist_start (sctx->ctx,
		seahorse_key_get_keyid (skey, 0), TRUE) == GPGME_No_Error, FALSE);
	
	found = ((gpgme_op_keylist_next (sctx->ctx, &key) == GPGME_No_Error) && key != NULL);
	gpgme_key_unref (key);
	
	gpgme_op_keylist_end (sctx->ctx);
	return found;
}

/**
 * seahorse_context_set_ascii_armor:
 * @sctx: Current #SeahorseContext
 * @ascii_armor: New ascii armor value
 *
 * Sets ascii armor attribute of @sctx to @ascii_armor and saves in gconf.
 **/
void
seahorse_context_set_ascii_armor (SeahorseContext *sctx, gboolean ascii_armor)
{
	g_return_if_fail (SEAHORSE_IS_CONTEXT (sctx));
	
	gpgme_set_textmode (sctx->ctx, ascii_armor);
	gconf_client_set_bool (sctx->priv->gclient, ARMOR_KEY, ascii_armor, NULL);
}

/**
 * seahorse_context_set_text_mode:
 * @sctx: Current #SeahorseContext
 * @text_mode: New text mode value
 *
 * Sets text mode attribute of @sctx to @text_mode and saves in gconf.
 **/
void
seahorse_context_set_text_mode (SeahorseContext *sctx, gboolean text_mode)
{
	g_return_if_fail (SEAHORSE_IS_CONTEXT (sctx));
	
	gpgme_set_textmode (sctx->ctx, text_mode);
	gconf_client_set_bool (sctx->priv->gclient, TEXT_KEY, text_mode, NULL);
}

/**
 * seahorse_context_set_signer:
 * @sctx: Current #SeahorseContext
 * @signer: New signer #SeahorseKey
 *
 * Sets signer attribute of @sctx to @signer.
 **/
void
seahorse_context_set_signer (SeahorseContext *sctx, SeahorseKey *signer)
{
	g_return_if_fail (SEAHORSE_IS_CONTEXT (sctx));
	g_return_if_fail (SEAHORSE_IS_KEY (signer));
	
	gpgme_signers_clear (sctx->ctx);
	gpgme_signers_add (sctx->ctx, signer->key);
	
	gconf_client_set_string (sctx->priv->gclient, DEFAULT_KEY, seahorse_key_get_keyid (signer, 0), NULL);
}

/**
 * seahorse_context_get_last_signer:
 * @sctx: Current #SeahorseContext
 *
 * Gets the signing #SeahorseKey for @sctx
 *
 * Returns: The default signing #SeahorseKey
 **/
SeahorseKey*
seahorse_context_get_last_signer (const SeahorseContext *sctx)
{
	GpgmeKey key;
	
	key = gpgme_signers_enum (sctx->ctx, 0);
	return seahorse_context_get_key (sctx, key);
}
