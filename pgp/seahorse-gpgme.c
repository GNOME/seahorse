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
 * License along with this program; if not, see
 * <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "operation"

#include "seahorse-gpgme.h"

#include "libseahorse/seahorse-util.h"

#include <glib/gi18n.h>

/**
 * SECTION:seahorse-gpgme
 * @short_description: gpgme specific error and data conversion functions
 * @include:seahorse-gpgme.h
 *
 **/

/**
 * seahorse_gpgme_error_domain:
 *
 *
 * Returns: A Quark with the content "seahorse-gpgme-error"
 */
GQuark
seahorse_gpgme_error_domain (void)
{
	static GQuark q = 0;
	if (q == 0)
		q = g_quark_from_static_string ("seahorse-gpgme-error");
	return q;
}

gboolean
seahorse_gpgme_propagate_error (gpgme_error_t gerr, GError** error)
{
	gpgme_err_code_t code;

	/* Make sure this is actually an error */
	code = gpgme_err_code (gerr);
	if (code == 0)
		return FALSE;

	/* Special case some error messages */
	switch (code) {
	case GPG_ERR_DECRYPT_FAILED:
		g_set_error_literal (error, SEAHORSE_GPGME_ERROR, code,
		             _("Decryption failed. You probably do not have the decryption key."));
		break;
	case GPG_ERR_CANCELED:
		g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_CANCELLED,
		                     _("The operation was cancelled"));
		break;
	default:
		g_set_error_literal (error, SEAHORSE_GPGME_ERROR, code,
		                     gpgme_strerror (gerr));
		break;
	}

	return TRUE;
}

/**
 * seahorse_gpgme_handle_error:
 * @err: the gpgme error to handle (display)
 * @desc: a printf formated string
 * @...: varargs to fill into this string
 *
 * Creates the heading of an error out of desc and the varargs. Displays the error
 *
 * The content of the error box is supported by gpgme (#gpgme_strerror)
 *
 * Some errors are not displayed.
 *
 */
void
seahorse_gpgme_handle_error (gpgme_error_t err, const char *desc, ...)
{
    va_list ap;
    g_autofree char *t = NULL;

	switch (gpgme_err_code(err)) {
	case GPG_ERR_CANCELED:
	case GPG_ERR_NO_ERROR:
	case GPG_ERR_ECANCELED:
		return;
    default:
		break;
	}

    va_start (ap, desc);

    if (desc)
        t = g_strdup_vprintf (desc, ap);

    va_end (ap);

	seahorse_util_show_error (NULL, t, gpgme_strerror (err));
}

/**
 * ref_return_key:
 * @key: the gpgme key
 *
 * Acquires an additional gpgme reference for the key
 *
 * Returns: the key
 */
static gpgme_key_t
ref_return_key (gpgme_key_t key)
{
	gpgme_key_ref (key);
	return key;
}

/**
 * seahorse_gpgme_boxed_key_type:
 *
 * Creates a new boxed type "gpgme_key_t"
 *
 * Returns: the new boxed type
 */
GType
seahorse_gpgme_boxed_key_type (void)
{
	static GType type = 0;
	if (!type)
		type = g_boxed_type_register_static ("gpgme_key_t",
		                                     (GBoxedCopyFunc)ref_return_key,
		                                     (GBoxedFreeFunc)gpgme_key_unref);
	return type;
}

/**
 * seahorse_gpgme_convert_validity:
 * @validity: the gpgme validity of a key
 *
 * converts the gpgme validity to the seahorse validity
 *
 * Returns: The seahorse validity
 */
SeahorseValidity
seahorse_gpgme_convert_validity (gpgme_validity_t validity)
{
	switch (validity) {
	case GPGME_VALIDITY_NEVER:
		return SEAHORSE_VALIDITY_NEVER;
	case GPGME_VALIDITY_MARGINAL:
		return SEAHORSE_VALIDITY_MARGINAL;
	case GPGME_VALIDITY_FULL:
		return SEAHORSE_VALIDITY_FULL;
	case GPGME_VALIDITY_ULTIMATE:
		return SEAHORSE_VALIDITY_ULTIMATE;
	case GPGME_VALIDITY_UNDEFINED:
	case GPGME_VALIDITY_UNKNOWN:
	default:
		return SEAHORSE_VALIDITY_UNKNOWN;
	}
}

typedef struct _WatchData {
	GSource *gsource;
	gboolean registered;
	GPollFD poll_fd;

	/* GPGME watch info */
	gpgme_io_cb_t fnc;
	void *fnc_data;

} WatchData;

typedef gboolean (*SeahorseGpgmeCallback)   (gpgme_error_t status,
                                             gpointer user_data);

typedef struct {
	GSource source;
	gpgme_ctx_t gctx;
	struct gpgme_io_cbs io_cbs;
	gboolean busy;
	GList *watches;
	GCancellable *cancellable;
	int cancelled_sig;
	gboolean finished;
	gpgme_error_t status;
} SeahorseGpgmeGSource;

static gboolean
seahorse_gpgme_gsource_prepare (GSource *gsource,
                                int *timeout)
{
	SeahorseGpgmeGSource *gpgme_gsource = (SeahorseGpgmeGSource *)gsource;

	if (gpgme_gsource->finished)
		return TRUE;

	/* No other way, but to poll */
	*timeout = -1;
	return FALSE;
}

static gboolean
seahorse_gpgme_gsource_check (GSource *gsource)
{
	SeahorseGpgmeGSource *gpgme_gsource = (SeahorseGpgmeGSource *)gsource;
	WatchData *watch;
	GList *l;

	for (l = gpgme_gsource->watches; l != NULL; l = g_list_next (l)) {
		watch = l->data;
		if (watch->registered && watch->poll_fd.revents)
			return TRUE;
	}

	if (gpgme_gsource->finished)
		return TRUE;

	return FALSE;
}

static gboolean
seahorse_gpgme_gsource_dispatch (GSource *gsource,
                                GSourceFunc callback,
                                gpointer user_data)
{
	SeahorseGpgmeGSource *gpgme_gsource = (SeahorseGpgmeGSource *)gsource;
	WatchData *watch;
	GList *l, *watches;

	watches = g_list_copy (gpgme_gsource->watches);
	for (l = watches; l != NULL; l = g_list_next (l)) {
		watch = l->data;
		if (watch->registered && watch->poll_fd.revents) {
			g_debug ("GPGME OP: io for GPGME on %d", watch->poll_fd.fd);
			g_assert (watch->fnc);
			watch->poll_fd.revents = 0;
			(watch->fnc) (watch->fnc_data, watch->poll_fd.fd);
		}
	}
	g_list_free (watches);

	if (gpgme_gsource->finished)
		return ((SeahorseGpgmeCallback)callback) (gpgme_gsource->status,
		                                          user_data);

	return TRUE;
}

static void
seahorse_gpgme_gsource_finalize (GSource *gsource)
{
	SeahorseGpgmeGSource *gpgme_gsource = (SeahorseGpgmeGSource *)gsource;
	g_cancellable_disconnect (gpgme_gsource->cancellable,
	                          gpgme_gsource->cancelled_sig);
	g_clear_object (&gpgme_gsource->cancellable);
}

static GSourceFuncs seahorse_gpgme_gsource_funcs = {
	seahorse_gpgme_gsource_prepare,
	seahorse_gpgme_gsource_check,
	seahorse_gpgme_gsource_dispatch,
	seahorse_gpgme_gsource_finalize,
};

static void
register_watch (WatchData *watch)
{
	if (watch->registered)
		return;

	g_debug ("GPGME OP: registering watch %d", watch->poll_fd.fd);

	watch->registered = TRUE;
	g_source_add_poll (watch->gsource, &watch->poll_fd);
}

static void
unregister_watch (WatchData *watch)
{
	if (!watch->registered)
		return;

	g_debug ("GPGME OP: unregistering watch %d", watch->poll_fd.fd);

	watch->registered = FALSE;
	g_source_remove_poll (watch->gsource, &watch->poll_fd);
}

/* Register a callback. */
static gpg_error_t
on_gpgme_add_watch (void *user_data,
                    int fd,
                    int dir,
                    gpgme_io_cb_t fnc,
                    void *fnc_data,
                    void **tag)
{
	SeahorseGpgmeGSource *gpgme_gsource = user_data;
	WatchData *watch;

	g_debug ("PGPOP: request to register watch %d", fd);

	watch = g_new0 (WatchData, 1);
	watch->registered = FALSE;
	watch->poll_fd.fd = fd;
	if (dir)
		watch->poll_fd.events = (G_IO_IN | G_IO_HUP | G_IO_ERR);
	else
		watch->poll_fd.events = (G_IO_OUT | G_IO_ERR);
	watch->fnc = fnc;
	watch->fnc_data = fnc_data;
	watch->gsource = (GSource*)gpgme_gsource;

	/*
	 * If the context is busy, we already have a START event, and can
	 * register watches freely.
	 */
	if (gpgme_gsource->busy)
		register_watch (watch);

	gpgme_gsource->watches = g_list_append (gpgme_gsource->watches, watch);
	*tag = watch;

	return GPG_OK;
}

static void
on_gpgme_remove_watch (void *tag)
{
	WatchData *watch = (WatchData*)tag;
	SeahorseGpgmeGSource *gpgme_gsource = (SeahorseGpgmeGSource*)watch->gsource;

	gpgme_gsource->watches = g_list_remove (gpgme_gsource->watches, watch);
	unregister_watch (watch);
	g_free (watch);
}

static void
on_gpgme_event (void *user_data,
                gpgme_event_io_t type,
                void *type_data)
{
	SeahorseGpgmeGSource *gpgme_gsource = user_data;
	gpg_error_t *gerr;
	GList *l;

	switch (type) {

	/* Called when the GPGME context starts an operation */
	case GPGME_EVENT_START:
		gpgme_gsource->busy = TRUE;
		gpgme_gsource->finished = FALSE;
		g_debug ("PGPOP: start event");

		/* Since we weren't supposed to register these before, do it now */
		for (l = gpgme_gsource->watches; l != NULL; l= g_list_next (l))
			register_watch (l->data);
		break;

	/* Called when the GPGME context is finished with an op */
	case GPGME_EVENT_DONE:
		gpgme_gsource->busy = FALSE;
		gerr = (gpgme_error_t *)type_data;
		g_debug ("PGPOP: done event (err: %d)", *gerr);

		/* Make sure we have no extra watches left over */
		for (l = gpgme_gsource->watches; l != NULL; l = g_list_next (l))
			unregister_watch (l->data);

		/* And try to figure out a good response */
		gpgme_gsource->finished = TRUE;
		gpgme_gsource->status = *gerr;
		break;

	case GPGME_EVENT_NEXT_KEY:
#if GPGME_VERSION_NUMBER < 0x020000
	case GPGME_EVENT_NEXT_TRUSTITEM:
#endif
	default:
		/* Ignore unsupported event types */
		break;
	}
}

static void
on_gpgme_gsource_cancelled (GCancellable *cancellable,
                            gpointer user_data)
{
	SeahorseGpgmeGSource *gpgme_gsource = user_data;
	if (gpgme_gsource->busy)
		gpgme_cancel (gpgme_gsource->gctx);
}

GSource *
seahorse_gpgme_gsource_new (gpgme_ctx_t gctx,
                            GCancellable *cancellable)
{
	SeahorseGpgmeGSource *gpgme_gsource;
	GSource *gsource;

	gsource = g_source_new (&seahorse_gpgme_gsource_funcs,
	                        sizeof (SeahorseGpgmeGSource));

	gpgme_gsource = (SeahorseGpgmeGSource *)gsource;
	gpgme_gsource->gctx = gctx;
	gpgme_gsource->io_cbs.add = on_gpgme_add_watch;
	gpgme_gsource->io_cbs.add_priv = gsource;
	gpgme_gsource->io_cbs.remove = on_gpgme_remove_watch;
	gpgme_gsource->io_cbs.event = on_gpgme_event;
	gpgme_gsource->io_cbs.event_priv = gsource;
	gpgme_set_io_cbs (gctx, &gpgme_gsource->io_cbs);

	if (cancellable) {
		gpgme_gsource->cancellable = g_object_ref (cancellable);
		gpgme_gsource->cancelled_sig = g_cancellable_connect (cancellable,
		                                                      G_CALLBACK (on_gpgme_gsource_cancelled),
		                                                      gpgme_gsource, NULL);
	}

	return gsource;
}
