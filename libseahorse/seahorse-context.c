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
#include <eel/eel.h>

#include "seahorse-context.h"
#include "seahorse-marshal.h"
#include "seahorse-libdialogs.h"
#include "seahorse-preferences.h"

#define MAX_THREADS 5

struct _SeahorseContextPrivate
{
	GList *key_pairs;
	//GMutex *pair_mutex;
	
	GList *single_keys;
	//GMutex *single_mutex;
};

enum {
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
/* gpgme functions */
static void	gpgme_progress			(gpointer		data,
						 const gchar		*what,
						 gint			type,
						 gint			current,
						 gint			total);
static void	set_gpgme_signer		(SeahorseContext	*sctx,
						 const gchar		*id);
/* gconf notification */
static void	gconf_notification		(GConfClient		*gclient,
						 guint			id,
						 GConfEntry		*entry,
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
	
	klass->add = NULL;
	klass->progress = NULL;
	
	context_signals[ADD] = g_signal_new ("add", G_OBJECT_CLASS_TYPE (gobject_class),
		G_SIGNAL_RUN_LAST, G_STRUCT_OFFSET (SeahorseContextClass, add),
		NULL, NULL, g_cclosure_marshal_VOID__OBJECT, G_TYPE_NONE, 1, SEAHORSE_TYPE_KEY);
	context_signals[PROGRESS] = g_signal_new ("progress", G_OBJECT_CLASS_TYPE (gobject_class),
		G_SIGNAL_RUN_LAST, G_STRUCT_OFFSET (SeahorseContextClass, progress),
		NULL, NULL, seahorse_marshal_VOID__STRING_DOUBLE, G_TYPE_NONE, 2, G_TYPE_STRING, G_TYPE_DOUBLE);
}

static GpgmeError
init_gpgme (GpgmeCtx *ctx)
{
	GpgmeProtocol proto = GPGME_PROTOCOL_OpenPGP;
	GpgmeError err;
	
	err = gpgme_engine_check_version (proto);
	g_return_val_if_fail (err == GPGME_No_Error, err);
	
	err = gpgme_new (ctx);
	g_return_val_if_fail (err == GPGME_No_Error, err);
	
	err = gpgme_set_protocol (*ctx, proto);
	g_return_val_if_fail (err == GPGME_No_Error, err);
	
	gpgme_set_keylist_mode (*ctx, GPGME_KEYLIST_MODE_LOCAL);
	return err;
}

/* init context, private vars, set prefs, connect signals */
static void
seahorse_context_init (SeahorseContext *sctx)
{
	g_return_if_fail (init_gpgme (&sctx->ctx) == GPGME_No_Error);
	/* init private vars */
	sctx->priv = g_new0 (SeahorseContextPrivate, 1);
	sctx->priv->key_pairs = NULL;
	//sctx->priv->pair_mutex = g_mutex_new ();
	sctx->priv->single_keys = NULL;
	//sctx->priv->single_mutex = g_mutex_new ();
	/* init signer */
	set_gpgme_signer (sctx, eel_gconf_get_string (DEFAULT_KEY));
	/* set prefs */
	gpgme_set_armor (sctx->ctx, eel_gconf_get_boolean (ASCII_ARMOR));
	gpgme_set_textmode (sctx->ctx, eel_gconf_get_boolean (TEXT_MODE));
	/* do callbacks */
	gpgme_set_passphrase_cb (sctx->ctx, (GpgmePassphraseCb)seahorse_passphrase_get, sctx);
	gpgme_set_progress_cb (sctx->ctx, gpgme_progress, sctx);
	eel_gconf_notification_add (PREFERENCES, (GConfClientNotifyFunc)gconf_notification, sctx);
	eel_gconf_monitor_add (PREFERENCES);
}

/* destroy all keys, free private vars */
static void
seahorse_context_finalize (GObject *gobject)
{
	SeahorseContext *sctx;
	SeahorseKey *skey;
	GList *list = NULL;
	
	sctx = SEAHORSE_CONTEXT (gobject);
	list = g_list_concat (g_list_copy (sctx->priv->key_pairs), sctx->priv->single_keys);
	/* destroy all keys */
	while (list != NULL && (skey = list->data) != NULL) {
		seahorse_key_destroy (skey);
		list = g_list_next (list);
	}
	
	g_list_free (sctx->priv->key_pairs);
	//g_mutex_free (sctx->priv->pair_mutex);
	g_list_free (sctx->priv->single_keys);
	//g_mutex_free (sctx->priv->single_mutex);
	g_free (sctx->priv);
	gpgme_release (sctx->ctx);
	
	G_OBJECT_CLASS (parent_class)->finalize (gobject);
}

/* removes key from list, disconnects signals, unrefs */
static void
seahorse_context_key_destroyed (GtkObject *object, SeahorseContext *sctx)
{
	SeahorseKey *skey;
	
	skey = SEAHORSE_KEY (object);
	/* remove from list */
	if (SEAHORSE_IS_KEY_PAIR (skey))
		sctx->priv->key_pairs = g_list_remove (sctx->priv->key_pairs, skey);
	else
		sctx->priv->single_keys = g_list_remove (sctx->priv->single_keys, skey);
	/* disconnect signals */
	g_signal_handlers_disconnect_by_func (GTK_OBJECT (skey),
		seahorse_context_key_destroyed, sctx);
	g_signal_handlers_disconnect_by_func (skey,
		seahorse_context_key_changed, sctx);
	
	g_object_unref (skey);
}

/* refresh key's GpgmeKey(s) */
static void
seahorse_context_key_changed (SeahorseKey *skey, SeahorseKeyChange change, SeahorseContext *sctx)
{
	GpgmeKey key;
	SeahorseKeyPair *skpair;
	
	/* get key */
	g_return_if_fail (gpgme_op_keylist_start (sctx->ctx,
		seahorse_key_get_id (skey->key), FALSE) == GPGME_No_Error);
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
			seahorse_key_get_id (skey->key), TRUE) == GPGME_No_Error);
		g_return_if_fail (gpgme_op_keylist_next (sctx->ctx, &key) == GPGME_No_Error);
		gpgme_op_keylist_end (sctx->ctx);
		/* set key */
		gpgme_key_unref (skpair->secret);
		skpair->secret = key;
	}
}

/* Calc fraction, call ::show_progress() */
static void
gpgme_progress (gpointer data, const gchar *what, gint type, gint current, gint total)
{
	gdouble fract;
	/* do fract */
	if (total == 0)
		fract = 0;
	else if (current == 100 || total == current)
		fract = -1;
	else
		fract = (gdouble)current / (gdouble)total;
	
	seahorse_context_show_progress (SEAHORSE_CONTEXT (data), what, fract);
}

/* sets key with id as signer */
static void
set_gpgme_signer (SeahorseContext *sctx, const gchar *id)
{
	GpgmeKey key;
	/* clear signers, get key, add key to signers */
	gpgme_signers_clear (sctx->ctx);
	g_return_if_fail (gpgme_op_keylist_start (sctx->ctx, id, TRUE) == GPGME_No_Error);
	if (gpgme_op_keylist_next (sctx->ctx, &key) == GPGME_No_Error)
		gpgme_signers_add (sctx->ctx, key);
	gpgme_op_keylist_end (sctx->ctx);
	gpgme_key_unref (key);
}

/* change context if prefs changed */
static void
gconf_notification (GConfClient *gclient, guint id, GConfEntry *entry, SeahorseContext *sctx)
{
	const gchar *key = gconf_entry_get_key (entry);
	GConfValue *value = gconf_entry_get_value (entry);
	
	if (g_str_equal (key, ASCII_ARMOR))
		gpgme_set_armor (sctx->ctx, gconf_value_get_bool (value));
	else if (g_str_equal (key, TEXT_MODE))
		gpgme_set_textmode (sctx->ctx, gconf_value_get_bool (value));
	else if (g_str_equal (key, DEFAULT_KEY))
		set_gpgme_signer (sctx, gconf_value_get_string (value));
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

/* refs key and connects signals */
static void
add_key (SeahorseContext *sctx, SeahorseKey *skey)
{
	/* do refs */
	g_object_ref (skey);
	gtk_object_sink (GTK_OBJECT (skey));
	/* do signals */
	g_signal_connect_after (GTK_OBJECT (skey), "destroy",
		G_CALLBACK (seahorse_context_key_destroyed), sctx);
	g_signal_connect (skey, "changed",
		G_CALLBACK (seahorse_context_key_changed), sctx);
	/* notify observers */
	g_signal_emit (G_OBJECT (sctx), context_signals[ADD], 0, skey);
}

static void
process_key_pair (GpgmeKey secret, SeahorseContext *sctx)
{
	GpgmeCtx ctx;
	GpgmeKey key;
	SeahorseKey *skey;
	
	/* init context & listing */
	g_return_if_fail (init_gpgme (&ctx) == GPGME_No_Error);
	/* if can do listing */
	if (gpgme_op_keylist_start (ctx, seahorse_key_get_id (secret), FALSE) == GPGME_No_Error &&
	    gpgme_op_keylist_next (ctx, &key) == GPGME_No_Error) {
		/* do new key, release gpgme keys */
		//g_mutex_lock (sctx->priv->pair_mutex);
		skey = seahorse_key_pair_new (key, secret);
		gpgme_key_unref (key);
		gpgme_key_unref (secret);
		/* add key to list */
		sctx->priv->key_pairs = g_list_append (sctx->priv->key_pairs, skey);
		add_key (sctx, skey);
		//g_mutex_unlock (sctx->priv->pair_mutex);
		/* end listing */
	}
	
	gpgme_op_keylist_end (ctx);
	gpgme_release (ctx);
}

static void
process_single_key (GpgmeKey key, SeahorseContext *sctx)
{
	GpgmeCtx ctx;
	GpgmeKey secret;
	SeahorseKey *skey;
	
	/* init new context & listing */
	g_return_if_fail (init_gpgme (&ctx) == GPGME_No_Error);
	/* if can do listing */
	if (gpgme_op_keylist_start (ctx, seahorse_key_get_id (key), TRUE) == GPGME_No_Error) {
		/* if got a secret, then already have so release */
		if (gpgme_op_keylist_next (ctx, &secret) == GPGME_No_Error) {
			gpgme_key_unref (secret);
			gpgme_key_unref (key);
		}
		/* otherwise don't have, add */
		else {
			/* do new key, release gpgme key */
			//g_mutex_lock (sctx->priv->single_mutex);
			skey = seahorse_key_new (key);
			gpgme_key_unref (key);
			/* add key to list */
			sctx->priv->single_keys = g_list_append (sctx->priv->single_keys, skey);
			add_key (sctx, skey);
			//g_mutex_unlock (sctx->priv->single_mutex);
		}
	}
	
	gpgme_op_keylist_end (ctx);
	gpgme_release (ctx);
}

static guint
do_keys (SeahorseContext *sctx, GFunc process_func, guint initial_count)
{
	//GThreadPool *pool;
	guint count = initial_count + 1;
	gint progress_update;
	GpgmeKey key;
	
	//pool = g_thread_pool_new (process_func, sctx, MAX_THREADS, TRUE, NULL);
	progress_update = eel_gconf_get_integer (PROGRESS_UPDATE);
	
	/* get remaining keys */
	while (gpgme_op_keylist_next (sctx->ctx, &key) == GPGME_No_Error) {
		if (progress_update > 0 && !(count % progress_update))
			seahorse_context_show_progress (sctx, g_strdup_printf (
				_("Loading key %d"), count), 0);
		/* process key */
		//g_thread_pool_push (pool, key, NULL);
		process_func (key, sctx);
		count++;
	}
	
	gpgme_op_keylist_end (sctx->ctx);
	/* wait for key processing to finish */
	//g_thread_pool_free (pool, FALSE, TRUE);
	//g_thread_pool_stop_unused_threads ();
	
	return count;
}

/* loads key pairs */
static void
do_key_pairs (SeahorseContext *sctx)
{
	static gboolean did_pairs = FALSE;
	guint total = 0;
	
	if (did_pairs)
		return;

	g_return_if_fail (gpgme_op_keylist_start (sctx->ctx, NULL, TRUE) == GPGME_No_Error);
	total = do_keys (sctx, (GFunc)process_key_pair, 0);
	
	did_pairs = TRUE;
	seahorse_context_show_progress (sctx, g_strdup_printf (
		_("Loaded %d key pairs"), total), -1);
}

/* loads single keys, doing pairs first */
static void
do_key_list (SeahorseContext *sctx)
{
	static gboolean did_keys = FALSE;
	guint total = 0;
	
	if (did_keys)
		return;
	
	if (sctx->priv->key_pairs == NULL)
		do_key_pairs (sctx);
	
	g_return_if_fail (gpgme_op_keylist_start (sctx->ctx, NULL, FALSE) == GPGME_No_Error);
	total = do_keys (sctx, (GFunc)process_single_key, g_list_length (sctx->priv->key_pairs));
	
	did_keys = TRUE;
	seahorse_context_show_progress (sctx, g_strdup_printf (
		_("Loaded %d keys"), total), -1);
	
}

/**
 * seahorse_context_get_keys:
 * @sctx: #SeahorseContext
 *
 * Gets the complete key ring.
 *
 * Returns: A #GList of #SeahorseKey
 **/
GList*
seahorse_context_get_keys (SeahorseContext *sctx)
{
	static gboolean init_list = FALSE;
	
	if (!init_list) {
		do_key_list (sctx);
		init_list = TRUE;
	}
	
	/* copy key_pairs since will append single_keys to list & key_pairs is small */
	return g_list_concat (g_list_copy (sctx->priv->key_pairs), sctx->priv->single_keys);
}

/**
 * seahorse_context_get_key_pairs:
 * @sctx: #SeahorseContext
 *
 * Gets the secret key ring
 *
 * Returns: A #GList of #SeahorseKeyPair
 **/
GList*
seahorse_context_get_key_pairs (SeahorseContext *sctx)
{
	static gboolean init_list = FALSE;
	
	if (!init_list) {
		do_key_pairs (sctx);
		init_list = TRUE;
	}
	
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
	const gchar *id1, *id2 = "";
	
	g_return_val_if_fail (sctx != NULL && SEAHORSE_IS_CONTEXT (sctx), NULL);
	g_return_val_if_fail (key != NULL, NULL);
	
	list = seahorse_context_get_keys (sctx);
	id1 = seahorse_key_get_id (key);
	
	/* Search by keyid */
	while (list != NULL && (skey = list->data) != NULL) {
		id2 = seahorse_key_get_id (skey->key);
		
		if (g_str_equal (id1, id2))
			break;
		
		list = g_list_next (list);
	}
	
	gpgme_key_unref (key);
	
	if (!g_str_equal (id1, id2))
		return NULL;
	else
		return skey;
}

/**
 * seahorse_context_show_progress:
 * @sctx: #SeahorseContext
 * @op: Operation message to display
 * @fract: Fract should have one of 3 values: 0 < fract <= 1 will show
 * the progress as a fraction of the total progress; fract == -1 signifies
 * that the operation is complete; any other value will pulse the progress.
 *
 * Emits the progress signal for @sctx.
 **/
void
seahorse_context_show_progress (SeahorseContext *sctx, const gchar *op, gdouble fract)
{
	g_return_if_fail (sctx != NULL && SEAHORSE_IS_CONTEXT (sctx));
	
	g_signal_emit (G_OBJECT (sctx), context_signals[PROGRESS], 0, op, fract);
}

/**
 * seahorse_context_get_last_signer:
 * @sctx: Current #SeahorseContext
 *
 * Gets the signing #SeahorseKey for @sctx
 *
 * Returns: The default signing #SeahorseKey
 **/
SeahorseKeyPair*
seahorse_context_get_default_key (SeahorseContext *sctx)
{
	GpgmeKey key;
	
	key = gpgme_signers_enum (sctx->ctx, 0);
	return SEAHORSE_KEY_PAIR (seahorse_context_get_key (sctx, key));
}

/* common func for adding new keys */
static guint
add_new_keys (SeahorseContext *sctx, gboolean secret, gint max, gint total, GFunc process_func)
{
	guint count;
	GpgmeError err;
	GpgmeKey key;
	
	/* start listing */
	g_return_if_fail (gpgme_op_keylist_start (sctx->ctx, NULL, secret) == GPGME_No_Error);
	/* iterate through list until done or have done current keys */
	for (count = 0; count < max; count++) {
		if (gpgme_op_keylist_next (sctx->ctx, &key) != GPGME_No_Error)
			break;
		else
			gpgme_key_unref (key);
	}
	
	/* if did all current keys, then try add any remaining */
	if (count == max)
		return do_keys (sctx, process_func, total);
	/* otherwise end listing, return current total */
	else {
		gpgme_op_keylist_end (sctx->ctx);
		return total;
	}
}

/**
 * seahorse_context_keys_added:
 * @sctx: #SeahorseContext
 *
 * Tries to add any new keys.
 **/
void
seahorse_context_keys_added (SeahorseContext *sctx, guint num_keys)
{
	gint num_pairs, num_single;
	guint total;
	
	g_return_if_fail (num_keys > 0);
	
	/* get current lengths */
	num_pairs = g_list_length (sctx->priv->key_pairs);
	num_single = g_list_length (sctx->priv->single_keys);
	
	/* get new total after adding new secret keys */
	total = add_new_keys (sctx, TRUE, num_pairs, num_pairs + num_single, (GFunc)process_key_pair);
	/* add new public keys */
	total = add_new_keys (sctx, FALSE, num_single, total, (GFunc)process_single_key);
	
	/* show status */
	seahorse_context_show_progress (sctx, g_strdup_printf (
		_("Loaded %d new keys"), num_keys), -1);
}
