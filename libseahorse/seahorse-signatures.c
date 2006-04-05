/*
 * Seahorse
 *
 * Copyright (C) 2002 Jacob Perkins
 * Copyright (C) 2004 Nate Nielsen
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
#include <gnome.h>

#include "seahorse-libdialogs.h"
#include "seahorse-widget.h"
#include "seahorse-util.h"
#include "seahorse-pgp-key.h"

/* Note that we can't use GTK stock here, as we hand these icons off 
   to other processes in the case of notifications */
#define ICON_PREFIX     DATA_DIR "/pixmaps/seahorse/48x48/"

void
seahorse_signatures_notify (const gchar* data, gpgme_verify_result_t status)
{
    SeahorseKey *key;
    gchar *t, *userid = NULL;
    gboolean ok = FALSE;
    const gchar *icon = NULL;
    const gchar *fingerprint;
    gchar *summary;
    gchar *body;
    guint l;    
    
    fingerprint = status->signatures->fpr;
    l = strlen (fingerprint);
    if (l >= 16)
        fingerprint += l - 16;
    
    /* TODO: Eventually we need to extend this to lookup keys remotely */
    key = seahorse_context_find_key (SCTX_APP (), SKEY_PGP, SKEY_LOC_LOCAL, fingerprint);

    if (key != NULL) {
        t = seahorse_key_get_simple_name (key);
        userid = g_strdup_printf ("%s (id: %s)", t, seahorse_key_get_short_keyid (key));
        g_free (t);
    } else {
        userid = g_strdup (fingerprint);
        icon = ICON_PREFIX "seahorse-unknown.png";
    }
    
    switch (gpgme_err_code (status->signatures->status))  {
    case GPG_ERR_KEY_EXPIRED:
        body = _("Signed by <i>%s <b>Expired</b></i> on %s.");
        summary = _("%s: Invalid Signature");
        icon = ICON_PREFIX "seahorse-sign-bad.png";
        ok = TRUE;
        break;
    case GPG_ERR_SIG_EXPIRED:
        body = _("Signed by <i>%s</i> on %s <b>Expired</b>.");
        summary = _("%s: Expired Signature");
        icon = ICON_PREFIX "seahorse-sign-bad.png";
        ok = TRUE;
        break;        
    case GPG_ERR_CERT_REVOKED:
        body = _("Signed by <i>%s <b>Revoked</b></i> on %s.");
        summary = _("%s: Invalid Signature");
        icon = ICON_PREFIX "seahorse-sign-bad.png";
        ok = TRUE;
        break;
    case GPG_ERR_NO_ERROR:
        body = _("Signed by <i>%s</i> on %s.");
        summary = _("%s: Good Signature");
        if (icon == NULL)
            icon = ICON_PREFIX "seahorse-sign-ok.png";
        ok = TRUE;
        break;
    case GPG_ERR_NO_PUBKEY:
        body = _("Signing key not in keyring.");
        summary = _("%s: Unknown Signature");
        icon = ICON_PREFIX "seahorse-sign-unknown.png";
        break;
    case GPG_ERR_BAD_SIGNATURE:
        body = _("Bad or forged signature. The signed data was modified.");
        summary = _("%s: Bad Signature");
        icon = ICON_PREFIX "seahorse-sign-bad.png";
        break;
    case GPG_ERR_NO_DATA:
        return;
    default:
        if (!GPG_IS_OK (status->signatures->status)) 
            seahorse_util_handle_gpgme (status->signatures->status, 
                                        _("Couldn't verify signature."));
        return;
    };

    data = seahorse_util_uri_get_last (data);

    if (ok) {
        gchar *date = seahorse_util_get_date_string (status->signatures->timestamp);
        body = g_markup_printf_escaped (body, userid, date);
        g_free (date);
    } else {
        body = g_strdup (body);
    }
    
    summary = g_strdup_printf (summary, data); 
    seahorse_notification_display (summary, body, !ok, icon);

    g_free (summary);
    g_free (body);
    g_free (userid);
}    

