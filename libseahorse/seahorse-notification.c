/*
 * Seahorse
 *
 * Copyright (C) 2006 Nate Nielsen
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

#include <gtk/gtk.h>
#include <glib.h>

#include "seahorse-libdialogs.h"
#include "seahorse-util.h"

#ifdef HAVE_LIBNOTIFY
#include <libnotify/notify.h>
#endif

/* -----------------------------------------------------------------------------
 * SEAHORSE NOTIFICATION 
 */
 
#define SEAHORSE_TYPE_NOTIFICATION            (seahorse_notification_get_type ())
#define SEAHORSE_NOTIFICATION(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_TYPE_NOTIFICATION, SeahorseNotification))
#define SEAHORSE_NOTIFICATION_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_NOTIFICATION, SeahorseNotificationClass))
#define SEAHORSE_IS_NOTIFICATION(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_TYPE_NOTIFICATION))
#define SEAHORSE_IS_NOTIFICATION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_NOTIFICATION))
#define SEAHORSE_NOTIFICATION_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_TYPE_NOTIFICATION, SeahorseNotificationClass))

typedef struct _SeahorseNotification {
    GObject parent;

    gchar *heading;
    gchar *message;
    const gchar *icon;

    GList *keys;
    GObject *widget;
    
} SeahorseNotification;

typedef struct _SeahorseNotificationClass {
    GObjectClass parent_class;
} SeahorseNotificationClass;

G_DEFINE_TYPE (SeahorseNotification, seahorse_notification, G_TYPE_OBJECT);

/* -----------------------------------------------------------------------------
 * INTERNAL HELPERS 
 */

/* Forward Declaration */
static void key_changed (SeahorseKey *skey, SeahorseKeyChange change, SeahorseNotification *snotif);

static void
insert_key_field (GString *res, const gchar *key, const gchar *field)
{
    SeahorseKey *skey;
    GValue value;
    GValue svalue;
    gchar *str;
    guint uid;
    
    skey = seahorse_context_key_from_dbus (SCTX_APP (), key, &uid);
    if (!skey) {
        g_warning ("key '%s' in key text does not exist", key);
        return;
    }
    
    /* A default field */
    if (!field)
        field = "display-name";
    
    memset (&value, 0, sizeof (value));
    memset (&svalue, 0, sizeof (value));
    
    if (seahorse_key_lookup_property (skey, uid, field, &value)) {
        g_value_init (&svalue, G_TYPE_STRING);
        if (g_value_transform (&value, &svalue)) {
            str = g_markup_escape_text (g_value_get_string (&svalue), -1);
            g_string_append (res, str);
            g_free (str);
        }
        g_value_unset (&svalue);
        g_value_unset (&value);
    }
}

static void
format_start_element (GMarkupParseContext *ctx, const gchar *element_name,
                      const gchar **attribute_names, const gchar **attribute_values,
                      gpointer user_data, GError **error)
{
    GString *res = (GString*)user_data;
    gchar *t;
    
    if (strcmp (element_name, "outer") == 0) 
        return;

    if (strcmp (element_name, "key") == 0) {
        
        const gchar *key = NULL;
        const gchar *field = NULL;
        
        for (; *attribute_names && *attribute_values; attribute_names++, attribute_values++) {
            if (strcmp (*attribute_names, "id") == 0) 
                key = *attribute_values;
            else if (strcmp (*attribute_names, "field") == 0)
                field = *attribute_values;
        }
        
        if (!key)
            g_warning ("key text <key> element requires the following attributes\n"
                       "     <key id=\"xxxxx\" field=\"xxxxx\"/>");
        else 
            insert_key_field (res, key, field);
        
        return;
    }

    /* Just pass through any other elements */
    g_string_append_printf (res, "<%s", element_name);
    for (; *attribute_names && *attribute_values; attribute_names++, attribute_values++) {
        t = g_markup_printf_escaped ("%s", *attribute_values);
        g_string_append_printf (res, " %s=\"%s\"", *attribute_names, t);
        g_free (t);
    }
    g_string_append (res, ">");
}

static void 
format_end_element (GMarkupParseContext *ctx, const gchar *element_name, 
                    gpointer user_data, GError **error)
{
    GString *res = (GString*)user_data;

    if (strcmp (element_name, "outer") == 0 || 
        strcmp (element_name, "key") == 0)
        return;
    
    /* Just pass through any other elements */;
    g_string_append_printf (res, "</%s>", element_name);
}

static void 
format_text (GMarkupParseContext *ctx, const gchar *text, gsize text_len,
             gpointer user_data, GError **error)
{
    GString *res = (GString*)user_data;
    g_string_append_len (res, text, text_len);
}

static gchar*
format_key_text (const gchar *text)
{
    GError *err = NULL;
    GMarkupParseContext *ctx;
    GMarkupParser parser;
    gboolean ret;
    GString *res;
    gchar *outer;
    
    if(!text)
        return g_strdup ("");
    
    memset (&parser, 0, sizeof (parser));
    parser.start_element = format_start_element;
    parser.end_element = format_end_element;
    parser.text = format_text;
    parser.passthrough = format_text;
    
    res = g_string_new (NULL);
    
    /* We need an outer tag in order to parse */
    outer = g_strdup_printf("<outer>%s</outer>", text);
    ctx = g_markup_parse_context_new (&parser, 0, res, NULL);
    ret = g_markup_parse_context_parse (ctx, outer, strlen (outer), &err);
    g_markup_parse_context_free (ctx);
    g_free (outer);

    if (ret)
        return g_string_free (res, FALSE);
    
    g_string_free (res, TRUE);
    return NULL;
}

#ifdef HAVE_LIBNOTIFY

static void 
libnotify_closed (NotifyNotification *notif, SeahorseNotification *snotif)
{
    // Make this object go away
    g_object_unref (snotif);
}

static void
setup_libnotify_notification (SeahorseNotification *snotif, gboolean urgent, 
                              GtkWidget *attachto)
{
    NotifyNotification *notif;
    GError *err = NULL;
    gchar *heading, *message;
    gboolean res;
    
    if (!notify_is_initted ())
        notify_init ("seahorse");
    
    heading = format_key_text (snotif->heading);
    message = format_key_text (snotif->message);
    
    notif = notify_notification_new (heading, message, snotif->icon, attachto);
    
    g_free (heading);
    g_free (message);
    
    g_return_if_fail (notif != NULL);
    notify_notification_set_urgency (notif, urgent ?
                                              NOTIFY_URGENCY_CRITICAL : 
                                              NOTIFY_URGENCY_NORMAL);
    
    res = notify_notification_show (notif, &err);
    if (!res) {
        g_warning ("couldn't show notification through libnotify: %s", err->message);
        g_clear_error (&err);
        g_object_unref (notif);
        return;
    } 
    
    snotif->widget = G_OBJECT (notif);
    g_signal_connect (notif, "closed", G_CALLBACK (libnotify_closed), snotif);
}

static void 
update_libnotify_notification (SeahorseNotification *snotif)
{
    gchar *heading, *message;
    NotifyNotification *notif = NOTIFY_NOTIFICATION (snotif->widget);
    g_return_if_fail (notif != NULL);
    
    heading = format_key_text (snotif->heading);
    message = format_key_text (snotif->message);
    
    if (!notify_notification_update (notif, heading, message, snotif->icon))
        g_warning ("couldn't update notification through libnotify");
    
    g_free (heading);
    g_free (message);
}

#endif /* HAVE_LIBNOTIFY */ 

static void
fallback_closed (GtkWidget *widget, SeahorseNotification *snotif)
{
    /* Make this object go away */
    g_object_unref (snotif);
}

static void
update_fallback_notification (SeahorseNotification *snotif)
{
    GList *children, *l;
    GtkWidget *label, *secondary_label;
    gchar *heading, *message, *t;
    GtkContainer *vbox;
    
    vbox = GTK_CONTAINER (snotif->widget);
    g_return_if_fail (vbox != NULL);
    
    children = gtk_container_get_children (vbox);
    for (l = children; l; l = g_list_next (l))
        gtk_container_remove (vbox, GTK_WIDGET (l->data));
    g_list_free (children);
    
    heading = format_key_text (snotif->heading);
    message = format_key_text (snotif->message);

    /* Format the heading */
    t = g_markup_printf_escaped ("<b><big>%s</big></b>", heading);
    label = gtk_label_new (NULL);
    gtk_label_set_markup (GTK_LABEL (label), t);
    gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
    gtk_label_set_selectable (GTK_LABEL (label), TRUE);
    gtk_misc_set_alignment (GTK_MISC  (label), 0.0, 0.0);
    g_free (t);
    
    /* The subtext */
    secondary_label = gtk_label_new (NULL);
    gtk_label_set_markup (GTK_LABEL (secondary_label), message);
    gtk_widget_set_size_request (secondary_label, 300, -1);
    gtk_label_set_line_wrap (GTK_LABEL (secondary_label), TRUE);
    gtk_label_set_selectable (GTK_LABEL (secondary_label), TRUE);
    gtk_misc_set_alignment (GTK_MISC  (secondary_label), 0.0, 0.0);
    
    g_free (heading);
    g_free (message);
    
    gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX (vbox), secondary_label, TRUE, TRUE, 0);
}

static void
setup_fallback_notification (SeahorseNotification *snotif, gboolean urgent,
                             GtkWidget *attachto)
{
    SeahorseWidget *swidget;
    GtkWidget *messages;
    GtkWidget *hbox, *vbox, *image;
    
    swidget = seahorse_widget_find ("notify");
    if (!swidget) {
        swidget = seahorse_widget_new ("notify");
        g_return_if_fail (swidget != NULL);
    }
    
    messages = seahorse_widget_get_widget (swidget, "message-area");
    g_return_if_fail (messages != NULL);
    
    /* Load up the image */
    if (snotif->icon)
        image = gtk_image_new_from_file (snotif->icon);
    else
        image = gtk_image_new_from_stock (urgent ? 
                                            GTK_STOCK_DIALOG_WARNING : 
                                            GTK_STOCK_DIALOG_INFO, 
                                          GTK_ICON_SIZE_DIALOG);
    gtk_misc_set_alignment (GTK_MISC (image), 0.5, 0.0);

    /* Layout */
    hbox = gtk_hbox_new (FALSE, 12);
    vbox = gtk_vbox_new (FALSE, 6);
    gtk_box_pack_start (GTK_BOX (hbox), image, FALSE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX (hbox), vbox, TRUE, TRUE, 0);
    gtk_box_pack_start (GTK_BOX (messages), hbox, FALSE, FALSE, 0);
    gtk_box_reorder_child (GTK_BOX (messages), hbox, 0);
    
    snotif->widget = G_OBJECT (vbox);
    g_signal_connect (vbox, "destroy", G_CALLBACK (fallback_closed), snotif);
    update_fallback_notification (snotif);
    
    gtk_widget_show_all (messages);
}

static void 
key_changed (SeahorseKey *skey, SeahorseKeyChange change, SeahorseNotification *snotif)
{
    if (!snotif->widget)
        return;
    
#ifdef HAVE_LIBNOTIFY
    if (NOTIFY_IS_NOTIFICATION (snotif->widget))
        update_libnotify_notification (snotif);
    else
#endif 
        update_fallback_notification (snotif);
}

static void
keys_start_element (GMarkupParseContext *ctx, const gchar *element_name,
                    const gchar **attribute_names, const gchar **attribute_values,
                    gpointer user_data, GError **error)
{
    SeahorseNotification* snotif = SEAHORSE_NOTIFICATION (user_data);
    SeahorseKey *skey;

    if (strcmp (element_name, "key") == 0) {
        
        const gchar *key = NULL;
        const gchar *field = NULL;
        
        for (; *attribute_names && *attribute_values; attribute_names++, attribute_values++) {
            if (strcmp (*attribute_names, "id") == 0) 
                key = *attribute_values;
            else if (strcmp (*attribute_names, "field") == 0)
                field = *attribute_values;
        }
        
        if (!key)
            g_warning ("key text <key> element requires the following attributes\n"
                       "     <key id=\"xxxxx\" field=\"xxxxx\"/>");
        
        skey = seahorse_context_key_from_dbus (SCTX_APP (), key, NULL);
        if (skey) {
            snotif->keys = g_list_append (snotif->keys, skey);
            g_signal_connect (skey, "changed", G_CALLBACK (key_changed), snotif);
        }
    }
    
}

/* -----------------------------------------------------------------------------
 * OBJECT 
 */

static void
seahorse_notification_init (SeahorseNotification *snotif)
{

}

static void
seahorse_notification_dispose (GObject *gobject)
{
    SeahorseNotification *snotif = SEAHORSE_NOTIFICATION (gobject);
    GList *l;
    
    if (snotif->widget) {
#ifdef HAVE_LIBNOTIFY
        if (NOTIFY_IS_NOTIFICATION (snotif->widget)) {
            g_signal_handlers_disconnect_by_func (snotif->widget, libnotify_closed, snotif);
            g_object_unref (snotif->widget);
        } else
#endif 
            g_signal_handlers_disconnect_by_func (snotif->widget, fallback_closed, snotif);
    }

    snotif->widget = NULL;
    
    for (l = snotif->keys; l; l = g_list_next (l)) 
        g_signal_handlers_disconnect_by_func (l->data, key_changed, snotif);
    g_list_free (snotif->keys);
    snotif->keys = NULL;
    
    G_OBJECT_CLASS (seahorse_notification_parent_class)->dispose (gobject);
}

static void
seahorse_notification_finalize (GObject *gobject)
{
    SeahorseNotification *snotif = SEAHORSE_NOTIFICATION (gobject);
    
    g_assert (!snotif->widget);
    
    if (snotif->heading)
        g_free (snotif->heading);
    snotif->heading = NULL;
    
    if (snotif->message)
        g_free (snotif->message);
    snotif->message = NULL;
 
    G_OBJECT_CLASS (seahorse_notification_parent_class)->finalize (gobject);
}

static void
seahorse_notification_class_init (SeahorseNotificationClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    seahorse_notification_parent_class = g_type_class_peek_parent (klass);
    
    gobject_class->dispose = seahorse_notification_dispose;
    gobject_class->finalize = seahorse_notification_finalize;
}


static SeahorseNotification*
seahorse_notification_new (const gchar *summary, const gchar* body, 
                           gboolean urgent, const gchar *icon, GtkWidget *attachto)
{
    SeahorseNotification* snotif = g_object_new (SEAHORSE_TYPE_NOTIFICATION, NULL);
    GError *err = NULL;
    GMarkupParseContext *ctx;
    GMarkupParser parser;
    gboolean ret;
    gchar *osummary, *obody;
    
    snotif->heading = g_strdup (summary);
    snotif->message = g_strdup (body);
    snotif->icon = icon;
    
    memset (&parser, 0, sizeof (parser));
    parser.start_element = keys_start_element;
    parser.end_element = NULL;
    parser.text = NULL;
    parser.passthrough = NULL;
    
    /* We need an outer tag in order to parse */
    obody = g_strdup_printf("<outer>%s</outer>", body);
    osummary = g_strdup_printf("<outer>%s</outer>", summary);
    ctx = g_markup_parse_context_new (&parser, 0, snotif, NULL);
    if((ret = g_markup_parse_context_parse (ctx, osummary, strlen (osummary), &err)))
        ret = g_markup_parse_context_parse (ctx, obody, strlen (obody), &err);
    g_markup_parse_context_free (ctx);
    g_free (obody);
    g_free (osummary);
    
    if (!ret) {
        g_warning ("couldn't parse keys out of key text: %s", err->message); 
        g_clear_error (&err);
    }

#ifdef HAVE_LIBNOTIFY
    setup_libnotify_notification (snotif, urgent, attachto);
#endif 
    if (!snotif->widget)
        setup_fallback_notification (snotif, urgent, attachto);
    
    if (snotif->widget) {
        /* Reference kept until widget is closed */
        g_object_ref (snotif);
        return snotif;
    }
    
    g_object_unref (snotif);
    return NULL;
}

/* -----------------------------------------------------------------------------
 * PUBLIC STUFF
 */

gboolean
seahorse_notification_have ()
{
    return seahorse_widget_find ("notify") != NULL;
}

void 
seahorse_notification_display (const gchar *summary, const gchar *body,
                               gboolean urgent, const gchar *icon, GtkWidget *attachto)
{
    SeahorseNotification *snotif = seahorse_notification_new (summary, body, urgent, 
                                                              icon, attachto);
    
    // Let object handle and cleanup itself
    if (snotif)
        g_object_unref (snotif);
}

/* -----------------------------------------------------------------------------
 * SPECIFIC NOTIFICATIONS
 */

/* Note that we can't use GTK stock here, as we hand these icons off 
   to other processes in the case of notifications */
#define ICON_PREFIX     DATA_DIR "/pixmaps/seahorse/48x48/"

void
seahorse_notify_import (guint keys)
{
    gchar *body = g_strdup_printf(ngettext("Imported %i key", "Imported %i keys", keys), keys);
    const gchar *title = ngettext("Key Imported", "Keys Imported", keys);
    const gchar *icon = ICON_PREFIX "seahorse-key.png";
    
    /* Always try and display in the daemon */
    if (seahorse_context_is_daemon (SCTX_APP ()))
        seahorse_notification_display (title, body, FALSE, icon, NULL);
    else
        cryptui_display_notification (title, body, icon, FALSE);

    g_free (body);
}

void
seahorse_notify_import_local (guint keys, GtkWidget *attachto)
{
    gchar *body = g_strdup_printf(ngettext("Imported %i key", "Imported %i keys", keys), keys);
    seahorse_notification_display (ngettext("Key Imported", "Keys Imported", keys), body, 
                                   FALSE, ICON_PREFIX "seahorse-key.png", attachto);
    g_free (body);
}

void
seahorse_notify_signatures (const gchar* data, gpgme_verify_result_t status)
{
    const gchar *icon = NULL;
    gchar *title, *body;
    gboolean sig = FALSE;
    GSList *rawids;
    GList *keys;
    SeahorseKey *skey;
    
    /* Discover the key in question */
    rawids = g_slist_append (NULL, status->signatures->fpr);
    keys = seahorse_context_discover_keys (SCTX_APP (), SKEY_PGP, rawids);
    g_slist_free (rawids);
    
    g_return_if_fail (keys != NULL);
    skey = SEAHORSE_KEY (keys->data);
    g_list_free (keys);

    /* Figure out what to display */
    switch (gpgme_err_code (status->signatures->status))  {
    case GPG_ERR_KEY_EXPIRED:
        body = _("Signed by <i><key id='%s'/> <b>expired</b></i> on %s.");
        title = _("Invalid Signature");
        icon = ICON_PREFIX "seahorse-sign-bad.png";
        sig = TRUE;
        break;
    case GPG_ERR_SIG_EXPIRED:
        body = _("Signed by <i><key id='%s'/></i> on %s <b>Expired</b>.");
        title = _("Expired Signature");
        icon = ICON_PREFIX "seahorse-sign-bad.png";
        sig = TRUE;
        break;
    case GPG_ERR_CERT_REVOKED:
        body = _("Signed by <i><key id='%s'/> <b>Revoked</b></i> on %s.");
        title = _("Revoked Signature");
        icon = ICON_PREFIX "seahorse-sign-bad.png";
        sig = TRUE;
        break;
    case GPG_ERR_NO_ERROR:
        body = _("Signed by <i><key id='%s'/></i> on %s.");
        title = _("Good Signature");
        icon = ICON_PREFIX "seahorse-sign-ok.png";
        sig = TRUE;
        break;
    case GPG_ERR_NO_PUBKEY:
        body = _("Signing key not in keyring.");
        title = _("Unknown Signature");
        icon = ICON_PREFIX "seahorse-sign-unknown.png";
        break;
    case GPG_ERR_BAD_SIGNATURE:
        body = _("Bad or forged signature. The signed data was modified.");
        title = _("Bad Signature");
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

    if (sig) {
        gchar *date = seahorse_util_get_display_date_string (status->signatures->timestamp);
        gchar *id = seahorse_context_keyid_to_dbus (SCTX_APP (), seahorse_key_get_keyid (skey), 0);
        body = g_markup_printf_escaped (body, id, date);
        g_free (date);
        g_free (id);
    } else {
        body = g_strdup (body);
    }
    
    if (data) {
        data = seahorse_util_uri_get_last (data);
        title = g_strdup_printf ("%s: %s", data, title); 
    } else {
        title = g_strdup (title);
    }

    /* Always try and display in the daemon */
    if (seahorse_context_is_daemon (SCTX_APP ()))
        seahorse_notification_display (title, body, !sig, icon, NULL);
    else
        cryptui_display_notification (title, body, icon, !sig);

    g_free (title);
    g_free (body);
}    
