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
#include "seahorse-key-pair.h"
#include "seahorse-marshal.h"

#define PREFERENCES "/apps/seahorse/preferences"
#define ARMOR_KEY PREFERENCES "/ascii_armor"
#define TEXT_KEY PREFERENCES "/text_mode"
#define DEFAULT_KEY PREFERENCES "/default_key_id"

struct _SeahorseContextPrivate
{
	GConfClient *gclient;
	GList *key_pairs;
	GList *single_keys;
};

enum {
	STATUS,
	ADD,
	PROGRESS,
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

static void	gpgme_progress			(gpointer		data,
						 const gchar		*what,
						 gint			type,
						 gint			current,
						 gint			total);

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
	context_signals[PROGRESS] = g_signal_new ("progress", G_OBJECT_CLASS_TYPE (gobject_class),
		G_SIGNAL_RUN_LAST, G_STRUCT_OFFSET (SeahorseContextClass, progress),
		NULL, NULL, seahorse_marshal_VOID__STRING_DOUBLE, G_TYPE_NONE, 2, G_TYPE_STRING, G_TYPE_DOUBLE);
}

/* Initializes the gpgme context & preferences */
static void
seahorse_context_init (SeahorseContext *sctx)
{
	GpgmeProtocol proto = GPGME_PROTOCOL_OpenPGP;
	gchar *keyid;
	GpgmeKey key;
	
	g_return_if_fail (gpgme_engine_check_version (proto) == GPGME_No_Error);
	g_return_if_fail (gpgme_new (&sctx->ctx) == GPGME_No_Error);
	g_return_if_fail (gpgme_set_protocol (sctx->ctx, proto) == GPGME_No_Error);
	
	/* init private vars */
	sctx->priv = g_new0 (SeahorseContextPrivate, 1);
	sctx->priv->single_keys = NULL;
	sctx->priv->key_pairs = NULL;
	sctx->priv->gclient = gconf_client_get_default ();
	
	/* init listing */
	keyid = gconf_client_get_string (sctx->priv->gclient, DEFAULT_KEY, NULL);
	gpgme_set_keylist_mode (sctx->ctx, GPGME_KEYLIST_MODE_LOCAL);
	
	/* set signer */
	g_return_if_fail (gpgme_op_keylist_start (sctx->ctx, keyid, TRUE) == GPGME_No_Error);
	if (gpgme_op_keylist_next (sctx->ctx, &key) == GPGME_No_Error)
		gpgme_signers_add (sctx->ctx, key);
	g_return_if_fail (gpgme_op_keylist_end (sctx->ctx) == GPGME_No_Error);
	gpgme_key_unref (key);
	
	/* set prefs */
	gpgme_set_armor (sctx->ctx, gconf_client_get_bool (
		sctx->priv->gclient, ARMOR_KEY, NULL));
	gpgme_set_textmode (sctx->ctx, gconf_client_get_bool (
		sctx->priv->gclient, TEXT_KEY, NULL));
	
	gpgme_set_progress_cb (sctx->ctx, gpgme_progress, sctx);
}

/* Destroy all keys & key ring, release gpgme context */
static void
seahorse_context_finalize (GObject *gobject)
{
	SeahorseContext *sctx;
	SeahorseKey *skey;
	GList *list = NULL;
	
	sctx = SEAHORSE_CONTEXT (gobject);
	list = seahorse_context_get_keys (sctx);
	
	while (list != NULL && (skey = list->data) != NULL) {
		seahorse_key_destroy (skey);
		list = g_list_next (list);
	}
	
	g_list_free (sctx->priv->key_pairs);
	g_list_free (sctx->priv->single_keys);
	g_free (sctx->priv->gclient);
	gpgme_release (sctx->ctx);
	
	G_OBJECT_CLASS (parent_class)->finalize (gobject);
}

/* Remove key from key ring */
static void
seahorse_context_key_destroyed (GtkObject *object, SeahorseContext *sctx)
{
	SeahorseKey *skey;
	
	skey = SEAHORSE_KEY (object);
	
	if (SEAHORSE_IS_KEY_PAIR (skey))
		sctx->priv->key_pairs = g_list_remove (sctx->priv->key_pairs, skey);
	else
		sctx->priv->single_keys = g_list_remove (sctx->priv->single_keys, skey);
	
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
	SeahorseKeyPair *skpair;
	
	/* get key */
	g_return_if_fail (gpgme_op_keylist_start (sctx->ctx,
		gpgme_key_get_string_attr (skey->key, GPGME_ATTR_FPR, NULL, 0), FALSE) == GPGME_No_Error);
	g_return_if_fail (gpgme_op_keylist_next (sctx->ctx, &key) == GPGME_No_Error);
	gpgme_op_keylist_end (sctx->ctx);
	/* set key */
	gpgme_key_unref (skey->key);
	skey->key = key;
	
	/* if is a pair */
	if (SEAHORSE_IS_KEY_PAIR (skey)) {
		skpair = SEAHORSE_KEY_PAIR (skey);
		/* get key */
		g_return_if_fail (gpgme_op_keylist_start (sctx->ctx,
			gpgme_key_get_string_attr (skey->key, GPGME_ATTR_FPR, NULL, 0), TRUE) == GPGME_No_Error);
		g_return_if_fail (gpgme_op_keylist_next (sctx->ctx, &key) == GPGME_No_Error);
		gpgme_op_keylist_end (sctx->ctx);
		/* set key */
		gpgme_key_unref (skpair->secret);
		skpair->secret = key;
	}
}

static void
gpgme_progress (gpointer data, const gchar *what, gint type, gint current, gint total)
{
	gdouble fract;
	
	if (total == 0)
		fract = 0;
	else if (current == 100 || total == current)
		fract = -1;
	else
		fract = (gdouble)current / (gdouble)total;
	
	seahorse_context_show_progress (SEAHORSE_CONTEXT (data), what, fract);
}

/* Add a new SeahorseKey to the key ring */
static SeahorseKey*
add_key (GpgmeKey key, SeahorseContext *sctx)
{
	SeahorseKey *skey;
	GpgmeKey secret;
	
	g_return_if_fail (gpgme_op_keylist_start (sctx->ctx,
		gpgme_key_get_string_attr (key, GPGME_ATTR_FPR, NULL, 0), TRUE) == GPGME_No_Error);
	
	seahorse_context_show_progress (sctx, _("Processing Keys"), 0);
	
	/* check if has secret, then do new pair */
	if (gpgme_op_keylist_next (sctx->ctx, &secret) == GPGME_No_Error) {
		skey = seahorse_key_pair_new (key, secret);
		sctx->priv->key_pairs = g_list_append (sctx->priv->key_pairs, skey);
		gpgme_key_unref (secret);
	}
	else {
		skey = seahorse_key_new (key);
		sctx->priv->single_keys = g_list_append (sctx->priv->single_keys, skey);
	}
	gpgme_op_keylist_end (sctx->ctx);
	
	g_object_ref (skey);
	gtk_object_sink (GTK_OBJECT (skey));
	gpgme_key_unref (key);
	
	g_signal_connect_after (GTK_OBJECT (skey), "destroy",
		G_CALLBACK (seahorse_context_key_destroyed), sctx);
	g_signal_connect (skey, "changed",
		G_CALLBACK (seahorse_context_key_changed), sctx);
	
	return skey;
}

static void
add_each_key (GpgmeKey key, SeahorseContext *sctx)
{
	add_key (key, sctx);
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
	return g_object_new (SEAHORSE_TYPE_CONTEXT, NULL);
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

static gboolean
do_lists (SeahorseContext *sctx)
{
	static gboolean init_list = FALSE;
	GpgmeKey key;
	GList *keys = NULL;
	guint count = 1;
	
	if (!init_list) {
		g_return_if_fail (gpgme_op_keylist_start (sctx->ctx, NULL, FALSE) == GPGME_No_Error);
		while (gpgme_op_keylist_next (sctx->ctx, &key) == GPGME_No_Error) {
			if (!(count % 10)) {
				seahorse_context_show_progress (sctx,
					g_strdup_printf (_("Loading key %d"), count), 0);
			}
			
			keys = g_list_append (keys, key);
			count++;
		}
		gpgme_op_keylist_end (sctx->ctx);
	
		g_list_foreach (keys, (GFunc)add_each_key, sctx);
		
		init_list = TRUE;
	}
	
	seahorse_context_show_progress (sctx, _("Loaded all keys"), -1);
	
	return init_list;
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
seahorse_context_get_keys (SeahorseContext *sctx)
{
	static gboolean init_list = FALSE;
	
	if (!init_list)
		init_list = do_lists (sctx);
	
	/* copy key_pairs since will append single_keys to list & key_pairs is small */
	return g_list_concat (g_list_copy (sctx->priv->key_pairs), sctx->priv->single_keys);
}

GList*
seahorse_context_get_key_pairs (SeahorseContext *sctx)
{
	static gboolean init_list = FALSE;
	
	if (!init_list)
		init_list = do_lists (sctx);
	
	return sctx->priv->key_pairs;
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
seahorse_context_get_key (SeahorseContext *sctx, GpgmeKey key)
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
seahorse_context_show_status (SeahorseContext *sctx, const gchar *op, gboolean success)
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
	
	seahorse_context_show_progress (sctx, status, -1);
	//g_signal_emit (G_OBJECT (sctx), context_signals[STATUS], 0, status);
	
	g_free (status);
}

void
seahorse_context_show_progress (SeahorseContext *sctx, const gchar *op, gdouble fract)
{
	g_return_if_fail (sctx != NULL && SEAHORSE_IS_CONTEXT (sctx));
	
	g_signal_emit (G_OBJECT (sctx), context_signals[PROGRESS], 0, op, fract);
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
	GList *list, *keys = NULL;
	SeahorseKey *skey;
	
	g_return_if_fail (sctx != NULL && SEAHORSE_IS_CONTEXT (sctx));
        g_return_if_fail (gpgme_op_keylist_start (sctx->ctx, NULL, FALSE) == GPGME_No_Error);	
	
	list = seahorse_context_get_keys (sctx);
	
	while (gpgme_op_keylist_next (sctx->ctx, &key) == GPGME_No_Error) {
		if (list == NULL)
			keys = g_list_append (keys, key);
		else
			list = g_list_next (list);
	}
	
	while (keys != NULL) {
		skey = add_key (keys->data, sctx);
		g_signal_emit (G_OBJECT (sctx), context_signals[ADD], 0, skey);
		keys = g_list_next (keys);
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
	g_return_val_if_fail (sctx != NULL && SEAHORSE_IS_CONTEXT (sctx), FALSE);
	g_return_val_if_fail (skey != NULL && SEAHORSE_IS_KEY (skey), FALSE);
	
	return (g_list_find (sctx->priv->key_pairs, skey) != NULL);
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
	
	gpgme_set_armor (sctx->ctx, ascii_armor);
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
seahorse_context_get_last_signer (SeahorseContext *sctx)
{
	GpgmeKey key;
	
	key = gpgme_signers_enum (sctx->ctx, 0);
	return seahorse_context_get_key (sctx, key);
}
