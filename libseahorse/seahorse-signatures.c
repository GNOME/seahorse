/*
 * Seahorse
 *
 * Copyright (C) 2002 Jacob Perkins
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

#include "seahorse-libdialogs.h"
#include "seahorse-widget.h"
#include "seahorse-util.h"

static gchar*
userid_for_fingerprint (SeahorseContext *sctx, const gchar *fingerprint)
{
    SeahorseKeySource *sksrc;
    SeahorseKey *key;
    gchar *userid = NULL;
    
    sksrc = seahorse_context_get_key_source (sctx);
    g_return_val_if_fail (sksrc != NULL, g_strdup (""));
    
    /* TODO: Eventually we need to extend this to lookup keys remotely */
    key = seahorse_key_source_get_key (sksrc, fingerprint);

    if (key != NULL) {
        userid = seahorse_key_get_display_name (key);

        /* Fix up the id, so it doesn't think it's markup */
        g_strdelimit (userid, "<", '(');
        g_strdelimit (userid, ">", ')');
        
    } else 
        userid = g_strdup (_("[Unknown Key]"));
            
    return userid;
}
    
static gchar* 
generate_sig_text (SeahorseContext *sctx, const gchar* path, 
                        gpgme_verify_result_t status)
{
    gboolean good = FALSE;
    const gchar *t;
    gchar *msg;
    gchar *date;
    gchar *userid;
     
    switch (gpgme_err_code (status->signatures->status))  {
    case GPG_ERR_KEY_EXPIRED:
        t = _("%s: Good signature from (<b>expired</b>) '%s' on %s");
        good = TRUE;
        break;
    case GPG_ERR_SIG_EXPIRED:
        t = _("%s: <b>Expired</b> signature from '%s' on %s");
        good = TRUE;
        break;        
    case GPG_ERR_CERT_REVOKED:
        t = _("%s: Good signature from (<b>revoked</b>) '%s' on %s");
        good = TRUE;
        break;
    case GPG_ERR_NO_ERROR:
        t = _("%s: Good signature from '%s' on %s");
        good = TRUE;
        break;
    case GPG_ERR_NO_PUBKEY:
        t = _("%s: Signing key not in key ring");
        break;
    case GPG_ERR_BAD_SIGNATURE:
        t = _("%s: <b>Bad</b> signature");
        break;
    case GPG_ERR_NO_DATA:
        t = _("%s: Not a signature");
        break;
    default:
        t = _("%s: Verification error");
        break;
    };

    path = seahorse_util_uri_get_last (path);

    if (good) {
        date = seahorse_util_get_date_string (status->signatures->timestamp);
        userid = userid_for_fingerprint (sctx, status->signatures->fpr);
        
        msg = g_strdup_printf (t, path, userid, date);
        
        g_free (date);
        g_free (userid);
    } else {
        msg = g_strdup_printf (t, path);
    }
    
    return msg;
}    

void
seahorse_signatures_add (SeahorseContext *sctx, SeahorseWidget *swidget,
                            const gchar* data, gpgme_verify_result_t status)
{
    GtkWidget *label;
    gchar* msg;
    gchar* text;
    
    label = glade_xml_get_widget (swidget->xml, "status");
    msg = generate_sig_text (sctx, data, status);
    
    text = g_strconcat (gtk_label_get_label (GTK_LABEL (label)), msg, "\n", NULL);
    gtk_label_set_markup (GTK_LABEL (label), text);
    
    g_free (text);
    g_free (msg);
}

SeahorseWidget* 
seahorse_signatures_new (SeahorseContext *sctx)
{
	SeahorseWidget *swidget;
    	
	swidget = seahorse_widget_new_allow_multiple ("signatures", sctx);
    return swidget;
}

void
seahorse_signatures_run (SeahorseContext *sctx, SeahorseWidget *swidget)
{
    GtkWidget *w; 
    w = glade_xml_get_widget (swidget->xml, "signatures");
    seahorse_widget_show (swidget);
    gtk_dialog_run (GTK_DIALOG (w));
}    
