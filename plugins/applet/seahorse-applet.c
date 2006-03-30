/* 
 * seahorse-applet.c
 * 
 * Copyright (C) 2005 Adam Schreiber <sadam@clemson.edu>
 * Copyright (C) 1999 Dave Camp
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <config.h>
#include <gnome.h>
#include <panel-applet.h>

#include "seahorse-applet.h"
#include "seahorse-gtkstock.h"
#include "seahorse-context.h"
#include "seahorse-pgp-key.h"
#include "seahorse-op.h"
#include "seahorse-libdialogs.h"
#include "seahorse-widget.h"
#include "seahorse-util.h"
#include "seahorse-gconf.h"
#include "seahorse-secure-memory.h"
#include "seahorse-check-button-control.h"

#define APPLET_SCHEMAS                SEAHORSE_SCHEMAS "/applet"
#define SHOW_CLIPBOARD_STATE_KEY      APPLET_SCHEMAS "/show_clipboard_state"
#define DISPLAY_CLIPBOARD_ENC_KEY     APPLET_SCHEMAS "/display_encrypted_clipboard"
#define DISPLAY_CLIPBOARD_DEC_KEY     APPLET_SCHEMAS "/display_decrypted_clipboard"

/* -----------------------------------------------------------------------------
 * ICONS AND STATE 
 */

#define ICON_CLIPBOARD_UNKNOWN      "seahorse-applet-unknown"
#define ICON_CLIPBOARD_TEXT         "seahorse-applet-text"
#define ICON_CLIPBOARD_ENCRYPTED    "seahorse-applet-encrypted"
#define ICON_CLIPBOARD_SIGNED       "seahorse-applet-signed"
#define ICON_CLIPBOARD_KEY          "seahorse-applet-key"
#define ICON_CLIPBOARD_DEFAULT      ICON_CLIPBOARD_ENCRYPTED

static const gchar *clipboard_icons[] = {
    ICON_CLIPBOARD_UNKNOWN,
    ICON_CLIPBOARD_TEXT,
    ICON_CLIPBOARD_ENCRYPTED,
    ICON_CLIPBOARD_SIGNED,
    ICON_CLIPBOARD_KEY,
    NULL
};

typedef enum {
    SEAHORSE_TEXT_TYPE_NONE,
    SEAHORSE_TEXT_TYPE_PLAIN,
    SEAHORSE_TEXT_TYPE_KEY,
    SEAHORSE_TEXT_TYPE_MESSAGE,
    SEAHORSE_TEXT_TYPE_SIGNED
} SeahorseTextType;

typedef struct _SeahorsePGPHeader {
    const gchar *header;
    const gchar *footer;
    SeahorseTextType type;
} SeahorsePGPHeader;    

static const SeahorsePGPHeader seahorse_pgp_headers[] = {
    { 
        "-----BEGIN PGP MESSAGE-----", 
        "-----END PGP MESSAGE-----", 
        SEAHORSE_TEXT_TYPE_MESSAGE 
    }, 
    {
        "-----BEGIN PGP SIGNED MESSAGE-----",
        "-----END PGP SIGNATURE-----",
        SEAHORSE_TEXT_TYPE_SIGNED
    }, 
    {
        "-----BEGIN PGP PUBLIC KEY BLOCK-----",
        "-----END PGP PUBLIC KEY BLOCK-----",
        SEAHORSE_TEXT_TYPE_KEY
    }, 
    {
        "-----BEGIN PGP PRIVATE KEY BLOCK-----",
        "-----END PGP PRIVATE KEY BLOCK-----",
        SEAHORSE_TEXT_TYPE_KEY
    }
};

/* -----------------------------------------------------------------------------
 * OBJECT DECLARATION
 */

typedef struct _SeahorseAppletPrivate {
    GtkWidget           *image;
    GtkTooltips         *tooltips;
    SeahorseContext     *context;
    GtkClipboard        *board;
    GtkWidget           *menu;
    SeahorseTextType    clipboard_contents;
} SeahorseAppletPrivate;

#define SEAHORSE_APPLET_GET_PRIVATE(obj)  (G_TYPE_INSTANCE_GET_PRIVATE ((obj), SEAHORSE_TYPE_APPLET, SeahorseAppletPrivate))

G_DEFINE_TYPE (SeahorseApplet, seahorse_applet, PANEL_TYPE_APPLET);

/* -----------------------------------------------------------------------------
 * INTERNAL HELPERS
 */

static void
init_context (SeahorseApplet *sapplet)
{
    SeahorseAppletPrivate *priv;
    SeahorseOperation *op;
    
    priv = SEAHORSE_APPLET_GET_PRIVATE (sapplet);
    if (!priv->context) {
        
        priv->context = seahorse_context_new (TRUE, SKEY_PGP);
        op = seahorse_context_load_local_keys (priv->context);
    
        /* Let operation take care of itself */
        g_object_unref (op);
    }
}

SeahorseTextType    
detect_text_type (const gchar *text, gint len, const gchar **start, const gchar **end)
{
    const SeahorsePGPHeader *header;
    const gchar *pos = NULL;
    const gchar *t;
    int i;
    
    if (len == -1)
        len = strlen (text);
    
    /* Find the first of the headers */
    for (i = 0; i < (sizeof (seahorse_pgp_headers) / sizeof (seahorse_pgp_headers[0])); i++) {
        t = g_strstr_len (text, len, seahorse_pgp_headers[i].header);
        if (t != NULL) {
            if (pos == NULL || (t < pos)) {
                header = &(seahorse_pgp_headers[i]);
                pos = t;
            }
        }
    }
    
    if (pos != NULL) {
        
        if (start)
            *start = pos;
        
        /* Find the end of that block */
        t = g_strstr_len (pos, len - (pos - text), header->footer);
        if (t != NULL && end)
            *end = t + strlen(header->footer);
        else if (end)
            *end = NULL;
            
        return header->type;
    }
    
    return SEAHORSE_TEXT_TYPE_PLAIN;
}

static void 
update_icon (SeahorseApplet *sapplet)
{
    SeahorseAppletPrivate *priv = SEAHORSE_APPLET_GET_PRIVATE (sapplet);
    const char *stock = NULL;
    
    if (seahorse_gconf_get_boolean (SHOW_CLIPBOARD_STATE_KEY)) {
        switch (priv->clipboard_contents) {
        case SEAHORSE_TEXT_TYPE_NONE:
            stock = ICON_CLIPBOARD_UNKNOWN;
            break;
        case SEAHORSE_TEXT_TYPE_PLAIN:
            stock = ICON_CLIPBOARD_TEXT;
            break;
        case SEAHORSE_TEXT_TYPE_KEY:
            stock = ICON_CLIPBOARD_KEY;
            break;
        case SEAHORSE_TEXT_TYPE_MESSAGE:
            stock = ICON_CLIPBOARD_ENCRYPTED;
            break;
        case SEAHORSE_TEXT_TYPE_SIGNED:
            stock = ICON_CLIPBOARD_SIGNED;
            break;
        default:
            g_assert_not_reached ();
            return;
        };
        
    } else {
        stock = ICON_CLIPBOARD_DEFAULT;
    }
    
    gtk_image_set_from_stock (GTK_IMAGE (priv->image), stock, GTK_ICON_SIZE_LARGE_TOOLBAR);
}

static void
detect_received(GtkClipboard *board, const gchar *text, SeahorseApplet *sapplet)
{
    SeahorseAppletPrivate *priv = SEAHORSE_APPLET_GET_PRIVATE (sapplet);
    
    if (!text || !*text)
        priv->clipboard_contents = SEAHORSE_TEXT_TYPE_NONE;
    else
        priv->clipboard_contents = detect_text_type (text, -1, NULL, NULL);
    
    update_icon (sapplet);
}

static void 
handle_clipboard_owner_change(GtkClipboard *clipboard, GdkEvent *event, 
                              SeahorseApplet *sapplet)
{
    SeahorseAppletPrivate *priv = SEAHORSE_APPLET_GET_PRIVATE (sapplet);
    priv->board = clipboard; 
    
    if (seahorse_gconf_get_boolean (SHOW_CLIPBOARD_STATE_KEY)) 
        gtk_clipboard_request_text (clipboard,
             (GtkClipboardTextReceivedFunc)detect_received, sapplet);
    else
        update_icon (sapplet);
}

static void
about_cb (BonoboUIComponent *uic, SeahorseApplet *sapplet, const gchar *verbname)
{
    GdkPixbuf *pixbuf;
    
    pixbuf = gtk_widget_render_icon (GTK_WIDGET(sapplet), 
                                     ICON_CLIPBOARD_DEFAULT, 
                                     (GtkIconSize)-1, 
                                     NULL);
                                     
    static const gchar *authors [] = {
        "Adam Schreiber <sadam@clemson.edu>",
        "Nate Nielsen <nielsen@memberwebs.com>",
        NULL    
    };

    static const gchar *documenters[] = {
        "Adam Schreiber <sadam@clemson.edu>",
        NULL
    };
    
    static const gchar *artists[] = {
        "Nate Nielsen <nielsen@memberwebs.com>",
        NULL    
    };

    gtk_show_about_dialog (NULL, 
                "name", _("seahorse-applet"),
                "version", VERSION,
                "comments", _("Use PGP/GPG to encrypt/decrypt/sign/verify/import the clipboard."),
                "copyright", "\xC2\xA9 2005, 2006 Adam Schreiber",
                "authors", authors,
                "documenters", documenters,
                "artists", artists,
                "translator-credits", _("translator-credits"),
                "logo", pixbuf,
                "website", "http://seahorse.sf.net",
                "website-label", _("Seahorse Project Homepage"),
                NULL);
    
    g_object_unref(pixbuf);
}

static void
display_text (gchar *title, gchar *text, gboolean editable)
{
    GtkWidget *dialog, *scrolled_window, *text_view;
    GtkTextBuffer *buffer;
    GdkPixbuf *pixbuf;

    dialog = gtk_dialog_new_with_buttons (title, NULL, 0,
                                          GTK_STOCK_OK, GTK_RESPONSE_NONE, NULL);

    pixbuf = gtk_widget_render_icon (dialog,  SEAHORSE_STOCK_SEAHORSE, 
                                     (GtkIconSize)-1,  NULL); 
    gtk_window_set_icon (GTK_WINDOW (dialog), gdk_pixbuf_copy(pixbuf));
    g_object_unref (pixbuf);
    
    gtk_window_set_default_size (GTK_WINDOW (dialog), 600, 450);
    g_signal_connect_swapped (dialog, "response", 
                              G_CALLBACK (gtk_widget_destroy), dialog);

    scrolled_window = gtk_scrolled_window_new (NULL, NULL);
    gtk_container_add (GTK_CONTAINER (GTK_DIALOG (dialog)->vbox), scrolled_window);

    text_view = gtk_text_view_new ();
    gtk_text_view_set_editable (GTK_TEXT_VIEW (text_view), editable);
    gtk_text_view_set_left_margin (GTK_TEXT_VIEW (text_view), 8);
    gtk_text_view_set_right_margin (GTK_TEXT_VIEW (text_view), 8);
    gtk_container_add (GTK_CONTAINER (scrolled_window), text_view);
    
    buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (text_view));
    gtk_text_buffer_set_text (buffer, text, strlen (text));

    pixbuf = gtk_widget_render_icon (dialog, 
                                     ICON_CLIPBOARD_DEFAULT, 
                                     (GtkIconSize)-1, 
                                     NULL);
                                     
    gtk_window_set_icon (GTK_WINDOW (dialog), pixbuf);
    
    g_object_unref(pixbuf);
    
    gtk_widget_show_all (dialog);
}

static void
encrypt_received (GtkClipboard *board, const gchar *text, SeahorseApplet *sapplet)
{
    SeahorsePGPKey *signer = NULL;
    gpgme_error_t err = GPG_OK;
    gchar *enctext = NULL;
    GList *keys;
    
    /* No text on clipboard */
    if (!text)
        return;
 
    keys = seahorse_recipients_get (&signer);
    
    /* User may have cancelled */
    if (g_list_length (keys) == 0)
        return;
    
    if (signer == NULL)
        enctext = seahorse_op_encrypt_text (keys, text, &err);
    else
        enctext = seahorse_op_encrypt_sign_text (keys, signer, text, &err);

    g_list_free (keys);

    if (!GPG_IS_OK (err)) {
        g_assert (!enctext);
        seahorse_util_handle_gpgme (err, _("Couldn't encrypt text"));
        return;
    }

    /* And finish up */
    gtk_clipboard_set_text (board, enctext, strlen (enctext));
    detect_received (board, enctext, sapplet);
    
    if (seahorse_gconf_get_boolean (DISPLAY_CLIPBOARD_ENC_KEY) == TRUE)
        display_text (_("Encrypted Text"), enctext, FALSE);
    
    g_free (enctext);
}

static void
encrypt_cb (GtkMenuItem *menuitem, SeahorseApplet *sapplet)
{
    SeahorseAppletPrivate *priv = SEAHORSE_APPLET_GET_PRIVATE (sapplet);
    
    init_context (sapplet);

    gtk_clipboard_request_text (priv->board,
            (GtkClipboardTextReceivedFunc)encrypt_received, sapplet);
}

static void
sign_received (GtkClipboard *board, const gchar *text, SeahorseApplet *sapplet)
{
    SeahorsePGPKey *signer = NULL;
    gpgme_error_t err = GPG_OK;
    gchar *enctext = NULL;
    
    /* No text on clipboard */
    if (!text)
        return;

    signer = seahorse_signer_get ();
    if (signer == NULL)
        return;

    /* Perform the signing */
    enctext = seahorse_op_sign_text (signer, text, &err);

    if (!GPG_IS_OK (err)) {
        g_assert (!enctext);
        seahorse_util_handle_gpgme (err, _("Couldn't sign text"));
        return;
    }

    /* And finish up */
    gtk_clipboard_set_text (board, enctext, strlen (enctext));
    detect_received (board, enctext, sapplet);

    if (seahorse_gconf_get_boolean (DISPLAY_CLIPBOARD_ENC_KEY) == TRUE)
        display_text (_("Signed Text"), enctext, FALSE);

    g_free (enctext);
}

static void
sign_cb (GtkMenuItem *menuitem, SeahorseApplet *sapplet)
{
    SeahorseAppletPrivate *priv = SEAHORSE_APPLET_GET_PRIVATE (sapplet);

    init_context (sapplet);

    gtk_clipboard_request_text (priv->board,
            (GtkClipboardTextReceivedFunc)sign_received, sapplet);
}

/* When we try to 'decrypt' a key, this gets called */
static guint
import_keys (const gchar *text)
{
    SeahorseKeySource *sksrc;
    GError *err = NULL;
    gint keys;

    sksrc = seahorse_context_find_key_source (SCTX_APP (), SKEY_PGP, SKEY_LOC_LOCAL);
    g_assert (sksrc && SEAHORSE_IS_PGP_SOURCE (sksrc));
    
    keys = seahorse_op_import_text (SEAHORSE_PGP_SOURCE (sksrc), text, &err);
    
    if (keys < 0) {
        seahorse_util_handle_error (err, _("Couldn't import keys"));
        return 0;
    } else if (keys == 0) {    
        return 0;
    } else {
        return keys;
    }
}

/* Decrypt an encrypted message */
static gchar*
decrypt_text (const gchar *text, gpgme_verify_result_t *status)
{
    SeahorseKeySource *sksrc;
    gpgme_error_t err;
    gchar *rawtext = NULL;
    
    sksrc = seahorse_context_find_key_source (SCTX_APP (), SKEY_PGP, SKEY_LOC_LOCAL);
    g_return_val_if_fail (sksrc && SEAHORSE_IS_PGP_SOURCE (sksrc), 0);

    rawtext = seahorse_op_decrypt_verify_text (SEAHORSE_PGP_SOURCE (sksrc), 
                                               text, status, &err);

    if (!GPG_IS_OK (err)) {
        seahorse_util_handle_gpgme (err, _("Couldn't decrypt text"));
        return NULL;
    }

    return rawtext;
}

/* Verify a signed message */
static gchar*
verify_text (const gchar *text, gpgme_verify_result_t *status)
{
    SeahorseKeySource *sksrc;
    gpgme_error_t err;
    gchar *rawtext = NULL;

    sksrc = seahorse_context_find_key_source (SCTX_APP (), SKEY_PGP, SKEY_LOC_LOCAL);
    g_return_val_if_fail (sksrc && SEAHORSE_IS_PGP_SOURCE (sksrc), 0);    

    rawtext = seahorse_op_verify_text (SEAHORSE_PGP_SOURCE (sksrc), text, status, &err);

    if (!GPG_IS_OK (err)) {
        seahorse_util_handle_gpgme (err, _("Couldn't decrypt text"));
        return NULL;
    }

    return rawtext;
}

static void
dvi_received (GtkClipboard *board, const gchar *text, SeahorseApplet *sapplet)
{
    SeahorseTextType type;      /* Type of the current block */
    gchar *rawtext = NULL;      /* Replacement text */
    guint keys = 0;             /* Number of keys imported */
    gpgme_verify_result_t status;   /* Signature status of last operation */
    const gchar *start;         /* Pointer to start of the block */
    const gchar *end;           /* Pointer to end of the block */
    
    /* No text on clipboard */
    if (text == NULL)
        return;
    
    /* Try to figure out what it contains */
    type = detect_text_type (text, -1, &start, &end);
    
    if (type == SEAHORSE_TEXT_TYPE_NONE)
        return;
    
    status = NULL;
    
    switch (type) {

    /* A key, import it */
    case SEAHORSE_TEXT_TYPE_KEY:
        keys = import_keys (text);
        break;

    /* A message decrypt it */
    case SEAHORSE_TEXT_TYPE_MESSAGE:
        rawtext = decrypt_text (text, &status);
        break;

    /* A message verify it */
    case SEAHORSE_TEXT_TYPE_SIGNED:
        rawtext = verify_text (text, &status);
        break;

    default:
        g_assert_not_reached ();
        break;
    };
    
    /* We got replacement text */
    if (rawtext) {

        gtk_clipboard_set_text (board, rawtext, strlen (rawtext));
        detect_received (board, rawtext, sapplet);
        
        if (seahorse_gconf_get_boolean (DISPLAY_CLIPBOARD_DEC_KEY) == TRUE)
            display_text (_("Decrypted Text"), rawtext, TRUE);
        
        g_free (rawtext);
        rawtext = NULL;

        if(status && status->signatures)
            seahorse_signatures_notify ("Text", status);
    }
    
    if (keys > 0)
        seahorse_import_notify (keys);
}

static void
dvi_cb (GtkMenuItem *menuitem, SeahorseApplet *sapplet)
{
    SeahorseAppletPrivate *priv = SEAHORSE_APPLET_GET_PRIVATE (sapplet);

    init_context (sapplet);

    gtk_clipboard_request_text (priv->board,
            (GtkClipboardTextReceivedFunc)dvi_received, sapplet);
}

void
properties_cb (BonoboUIComponent *uic, SeahorseApplet *sapplet, const char *verbname)
{
    SeahorseWidget *swidget;
    GtkWidget *widget;
    GdkPixbuf *pixbuf;
    
    /* SeahorseWidget needs a SeahorseContext initialized */
    init_context (sapplet);
    
    swidget = seahorse_widget_new ("applet-preferences");
    
    widget = glade_xml_get_widget (swidget->xml, swidget->name);

    pixbuf = gtk_widget_render_icon (widget, 
                                     ICON_CLIPBOARD_DEFAULT, 
                                     (GtkIconSize)-1, 
                                     NULL);
                                     
    gtk_window_set_icon (GTK_WINDOW (widget), pixbuf);
    
    g_object_unref(pixbuf);
    
    /* Preferences window is already open */
    if (!swidget)
        return;
    
    widget = glade_xml_get_widget (swidget->xml, "show-clipboard-state");
    if (widget && GTK_IS_CHECK_BUTTON (widget))
        seahorse_check_button_gconf_attach (GTK_CHECK_BUTTON (widget), SHOW_CLIPBOARD_STATE_KEY);
    
    widget = glade_xml_get_widget (swidget->xml, "display-encrypted-clipboard");
    if (widget && GTK_IS_CHECK_BUTTON (widget))
        seahorse_check_button_gconf_attach (GTK_CHECK_BUTTON (widget), DISPLAY_CLIPBOARD_ENC_KEY);
    
    widget = glade_xml_get_widget (swidget->xml, "display-decrypted-clipboard");
    if (widget && GTK_IS_CHECK_BUTTON (widget))
        seahorse_check_button_gconf_attach (GTK_CHECK_BUTTON (widget), DISPLAY_CLIPBOARD_DEC_KEY);
    
    seahorse_widget_show (swidget);
}

static void
help_cb (BonoboUIComponent *uic, SeahorseApplet *sapplet, const char *verbname)
{
    GError *err = NULL;
    
    gnome_help_display ("seahorse-applet", NULL, &err);
    if (err)
        seahorse_util_handle_error(err, _("Could not display the help file"));
}

static void
seahorse_popup_position_menu (GtkMenu *menu, int *x, int *y, gboolean *push_in, 
                              gpointer gdata)
{
    GtkWidget *widget;
    GtkRequisition requisition;
    gint menu_xpos;
    gint menu_ypos;

    widget = GTK_WIDGET (gdata);

    gtk_widget_size_request (GTK_WIDGET (menu), &requisition);
    gdk_window_get_origin (widget->window, &menu_xpos, &menu_ypos);

    menu_xpos += widget->allocation.x;
    menu_ypos += widget->allocation.y;

    switch (panel_applet_get_orient (PANEL_APPLET (widget))) {
    case PANEL_APPLET_ORIENT_DOWN:
    case PANEL_APPLET_ORIENT_UP:
        if (menu_ypos > gdk_screen_get_height (gtk_widget_get_screen (widget)) / 2)
            menu_ypos -= requisition.height;
        else
            menu_ypos += widget->allocation.height;
        break;
    case PANEL_APPLET_ORIENT_RIGHT:
    case PANEL_APPLET_ORIENT_LEFT:
        if (menu_xpos > gdk_screen_get_width (gtk_widget_get_screen (widget)) / 2)
            menu_xpos -= requisition.width;
        else
            menu_xpos += widget->allocation.width;
        break;
    default:
        g_assert_not_reached ();
    }
           
    *x = menu_xpos;
    *y = menu_ypos;
    *push_in = TRUE;
}

static gboolean 
handle_button_press (GtkWidget *widget, GdkEventButton *event)
{
    GtkWidget *item;    
    SeahorseApplet *applet = SEAHORSE_APPLET (widget);
    SeahorseAppletPrivate *priv = SEAHORSE_APPLET_GET_PRIVATE (applet);
    
    if (event->button == 1) {
        
        if (priv->menu) {
            gtk_widget_destroy (priv->menu);
            priv->menu = NULL;
        }
        
        /* Build Menu */
        priv->menu = gtk_menu_new ();
        
        if (priv->clipboard_contents == SEAHORSE_TEXT_TYPE_PLAIN ||
            priv->clipboard_contents == SEAHORSE_TEXT_TYPE_NONE) {
            item = gtk_image_menu_item_new_with_mnemonic (_("_Encrypt Clipboard"));
            g_signal_connect (item, "activate", G_CALLBACK (encrypt_cb), applet);
            gtk_menu_shell_append (GTK_MENU_SHELL (priv->menu), item);
        }
        
        if (priv->clipboard_contents == SEAHORSE_TEXT_TYPE_PLAIN ||
            priv->clipboard_contents == SEAHORSE_TEXT_TYPE_NONE) {
            item = gtk_image_menu_item_new_with_mnemonic (_("_Sign Clipboard"));
            g_signal_connect (item, "activate", G_CALLBACK (sign_cb), applet);
            gtk_menu_shell_append (GTK_MENU_SHELL (priv->menu), item);
        }
        
        if (priv->clipboard_contents == SEAHORSE_TEXT_TYPE_MESSAGE ||
            priv->clipboard_contents == SEAHORSE_TEXT_TYPE_SIGNED) {
            item = gtk_image_menu_item_new_with_mnemonic (_("_Decrypt/Verify Clipboard"));
            g_signal_connect (item, "activate", G_CALLBACK (dvi_cb), applet);
            gtk_menu_shell_append (GTK_MENU_SHELL (priv->menu), item);
        }
        
        if (priv->clipboard_contents == SEAHORSE_TEXT_TYPE_KEY) {
            item = gtk_image_menu_item_new_with_mnemonic (_("_Import Keys from Clipboard"));
            g_signal_connect (item, "activate", G_CALLBACK (dvi_cb), applet);
            gtk_menu_shell_append (GTK_MENU_SHELL (priv->menu), item);
        }
        
        gtk_widget_show_all (priv->menu);
        
        gtk_menu_popup (GTK_MENU (priv->menu), NULL, NULL, seahorse_popup_position_menu, 
                        (gpointer)applet, event->button, event->time);
        return TRUE;
    }
    
    if (GTK_WIDGET_CLASS (seahorse_applet_parent_class)->button_press_event)
        return (* GTK_WIDGET_CLASS (seahorse_applet_parent_class)->button_press_event) (widget, event);
    
    return FALSE;
}

static void
set_atk_name_description (GtkWidget *widget, const gchar *name, const gchar *description)
{
    AtkObject *aobj = gtk_widget_get_accessible (widget);
    
    /* Check if gail is loaded */
    if (GTK_IS_ACCESSIBLE (aobj) == FALSE)
        return;

    atk_object_set_name (aobj, name);
    atk_object_set_description (aobj, description);
}

static void
gconf_notify (GConfClient *client, guint id, GConfEntry *entry, gpointer *data)
{
    update_icon (SEAHORSE_APPLET (data));
}

/* -----------------------------------------------------------------------------
 * OBJECT 
 */

static void
seahorse_applet_init (SeahorseApplet *applet)
{
    SeahorseAppletPrivate *priv;
    GdkAtom atom;
    GtkClipboard *board;
    
    priv = SEAHORSE_APPLET_GET_PRIVATE (applet);
    
    /* 
     * We initialize the context on first operation to avoid 
     * problems with slowing down gnome-panel loading at 
     * login.
     */
    
    priv->clipboard_contents = SEAHORSE_TEXT_TYPE_NONE;
    
    priv->image = gtk_image_new ();
    priv->tooltips = gtk_tooltips_new ();

    gtk_container_add (GTK_CONTAINER (applet), priv->image);

    g_object_ref (priv->tooltips);
    gtk_object_sink (GTK_OBJECT (priv->tooltips));
    gtk_tooltips_set_tip (priv->tooltips, GTK_WIDGET (applet), _("Encryption Applet"), NULL);

    set_atk_name_description (GTK_WIDGET (applet), _("Encryption Applet"), 
                              _("Use PGP/GPG to encrypt/decrypt/sign/verify/import the clipboard."));

    /* Setup Clipboard Handling */
    atom = gdk_atom_intern ("CLIPBOARD", FALSE);
    board = gtk_clipboard_get (atom);
    handle_clipboard_owner_change (board, NULL, applet);
    g_signal_connect (board, "owner-change",
                      G_CALLBACK (handle_clipboard_owner_change), applet);
    
    atom = gdk_atom_intern ("PRIMARY", FALSE);
    board = gtk_clipboard_get (atom);
    g_signal_connect (board, "owner-change",
                      G_CALLBACK (handle_clipboard_owner_change), applet);
                      
}

static void
seahorse_applet_change_background (PanelApplet *applet, PanelAppletBackgroundType type,
                                   GdkColor *colour, GdkPixmap *pixmap)
{
    GtkRcStyle *rc_style;
    GtkStyle *style;

    /* reset style */
    gtk_widget_set_style (GTK_WIDGET (applet), NULL);
    rc_style = gtk_rc_style_new ();
    gtk_widget_modify_style (GTK_WIDGET (applet), rc_style);
    gtk_rc_style_unref (rc_style);

    switch (type){
    case PANEL_NO_BACKGROUND:
        break;
    case PANEL_COLOR_BACKGROUND:
        gtk_widget_modify_bg (GTK_WIDGET (applet), GTK_STATE_NORMAL, colour);
        break;
    case PANEL_PIXMAP_BACKGROUND:
        style = gtk_style_copy (GTK_WIDGET (applet)->style);

        if (style->bg_pixmap[GTK_STATE_NORMAL])
            g_object_unref (style->bg_pixmap[GTK_STATE_NORMAL]);

        style->bg_pixmap[GTK_STATE_NORMAL] = g_object_ref (pixmap);
        gtk_widget_set_style (GTK_WIDGET (applet), style);
        g_object_unref (style);
        break;
    }
}

static void
seahorse_applet_finalize (GObject *object)
{
    SeahorseAppletPrivate *priv = SEAHORSE_APPLET_GET_PRIVATE (object);

    if (priv) {
        if (priv->context)
            seahorse_context_destroy (priv->context);
        priv->context = NULL;
        
        if (priv->menu)
            gtk_widget_destroy (priv->menu);
        priv->menu = NULL;
        
        if (priv->tooltips)
            g_object_unref (G_OBJECT (priv->tooltips));
        priv->tooltips = NULL;
    }
    
    if (G_OBJECT_CLASS (seahorse_applet_parent_class)->finalize)
        (* G_OBJECT_CLASS (seahorse_applet_parent_class)->finalize) (object);
}

static void
seahorse_applet_class_init (SeahorseAppletClass *klass)
{
    GObjectClass *object_class;
    PanelAppletClass *applet_class;
    GtkWidgetClass *widget_class;

    object_class = G_OBJECT_CLASS (klass);
    applet_class = PANEL_APPLET_CLASS (klass);
    widget_class = GTK_WIDGET_CLASS(klass);

    object_class->finalize = seahorse_applet_finalize;
    applet_class->change_background = seahorse_applet_change_background;
    widget_class->button_press_event = handle_button_press;

    g_type_class_add_private (object_class, sizeof (SeahorseAppletPrivate));
}


/* -----------------------------------------------------------------------------
 * APPLET
 */

static const BonoboUIVerb seahorse_applet_menu_verbs [] = {
    BONOBO_UI_UNSAFE_VERB ("Props", properties_cb),
    BONOBO_UI_UNSAFE_VERB ("Help", help_cb),
    BONOBO_UI_UNSAFE_VERB ("About", about_cb),
    BONOBO_UI_VERB_END
};

static gboolean
seahorse_applet_fill (PanelApplet *applet)
{
    SeahorseApplet *sapplet = SEAHORSE_APPLET (applet);
    BonoboUIComponent *pcomp;

    /* Insert Icons into Stock */ 
    seahorse_gtkstock_init ();
    seahorse_gtkstock_add_icons (clipboard_icons);
    g_return_val_if_fail (PANEL_IS_APPLET (applet), FALSE);
    gtk_widget_show_all (GTK_WIDGET (applet));

    panel_applet_setup_menu_from_file (applet, UIDIR, "GNOME_SeahorseApplet.xml",
                                       NULL, seahorse_applet_menu_verbs, sapplet);
        
    pcomp = panel_applet_get_popup_component (applet);

    if (panel_applet_get_locked_down (applet))
        bonobo_ui_component_set_prop (pcomp, "/commands/Props", "hidden", "1", NULL);    

    update_icon (sapplet);
    seahorse_gconf_notify_lazy (APPLET_SCHEMAS, (GConfClientNotifyFunc)gconf_notify, 
                                sapplet, GTK_WIDGET (applet));
    
    return TRUE;
}

static gboolean
seahorse_applet_factory (PanelApplet *applet, const gchar *iid, gpointer data)
{
    gboolean retval = FALSE;

    if (!strcmp (iid, "OAFIID:GNOME_SeahorseApplet"))
        retval = seahorse_applet_fill (applet); 
   
    if (retval == FALSE)
        exit (-1);

    return retval;
}

/* 
 * We define our own main() since we have to do prior initialization.
 * This is copied from panel-applet.h
 */

int 
main (int argc, char *argv [])
{

    seahorse_secure_memory_init (65536);
    
    /* We need to drop privileges completely for security */
#if defined(HAVE_SETRESUID) && defined(HAVE_SETRESGID)

    /* Not in header files for all OSs, even where present */
    int setresuid(uid_t ruid, uid_t euid, uid_t suid);
    int setresgid(gid_t rgid, gid_t egid, gid_t sgid);
  
    if (setresuid (getuid (), getuid (), getuid ()) == -1 ||
        setresgid (getgid (), getgid (), getgid ()) == -1)
#else
    if (setuid (getuid ()) == -1 || setgid (getgid ()) == -1)
#endif
        g_error (_("couldn't drop privileges properly"));

    
    gnome_program_init ("seahorse-applet", VERSION, LIBGNOMEUI_MODULE, argc, argv,
                        GNOME_CLIENT_PARAM_SM_CONNECT, FALSE, GNOME_PARAM_NONE);
    
    return panel_applet_factory_main ("OAFIID:GNOME_SeahorseApplet_Factory", 
                                      SEAHORSE_TYPE_APPLET, seahorse_applet_factory, NULL);
}
