/* seahorse-applet.c
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

#define SEAHORSE_APPLET_GET_PRIVATE(obj)  (G_TYPE_INSTANCE_GET_PRIVATE ((obj), SEAHORSE_TYPE_APPLET, SeahorseAppletPrivate))

typedef enum {
    SEAHORSE_TEXT_TYPE_NONE,
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


typedef struct _SeahorseAppletPrivate SeahorseAppletPrivate;

struct _SeahorseAppletPrivate
{
    /* applet UI stuff */
    GtkIconTheme *icon_theme;
    GdkPixbuf    *icon;
    GtkWidget    *image;
    GtkTooltips  *tooltips;
    SeahorseContext *context;
    GtkClipboard *board;
    GtkWidget *menu;
};

static void seahorse_applet_class_init (SeahorseAppletClass *class);
static void seahorse_applet_init       (SeahorseApplet      *applet);
static void seahorse_applet_finalize   (GObject          *object);

static void seahorse_applet_change_size (PanelApplet *applet, guint size);

static void seahorse_applet_change_background (PanelApplet *app,
					    PanelAppletBackgroundType type,
					    GdkColor  *colour,
					    GdkPixmap *pixmap);

static void set_atk_name_description (GtkWidget *widget, 
                                       const gchar *name, 
                                       const gchar *description);

static void handle_clipboard_owner_change(GtkClipboard *clipboard,
                                            GdkEvent *event,
                                            SeahorseAppletPrivate *priv);
                                            
static gboolean handle_button_press (GtkWidget *widget, 
                                      GdkEventButton *event);

static void encrypt_cb(GtkMenuItem *menuitem,
                        SeahorseApplet *applet);
                        
static void sign_cb(GtkMenuItem *menuitem,
                     SeahorseApplet *applet);
                     
static void dvi_cb(GtkMenuItem *menuitem,
                    SeahorseApplet *applet);
                    
static void enable_disable_keybindings(gboolean enable);

static void applet_gconf_callback(GConfClient *client, guint id, GConfEntry *entry, SeahorseApplet *applet);


static gpointer parent_class;

GType
seahorse_applet_get_type (void)
{
  static GType type = 0;

  if (!type)
    {
      static const GTypeInfo info =
        {
	  sizeof (SeahorseAppletClass),
	  NULL,		/* base_init */
	  NULL,		/* base_finalize */
	  (GClassInitFunc) seahorse_applet_class_init,
	  NULL,		/* class_finalize */
	  NULL,		/* class_data */
	  sizeof (SeahorseApplet),
	  0,		/* n_preallocs */
	  (GInstanceInitFunc) seahorse_applet_init,
	};

      type = g_type_register_static (PANEL_TYPE_APPLET, "SeahorseApplet",
				     &info, 0);
    }

  return type;
}

SeahorseApplet*
seahorse_applet_new (void)
{
        return g_object_new (SEAHORSE_TYPE_APPLET, NULL);
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
    
    return SEAHORSE_TEXT_TYPE_NONE;
}

static void
seahorse_applet_class_init (SeahorseAppletClass *klass)
{
  GObjectClass     *object_class;
  PanelAppletClass *applet_class;
  GtkWidgetClass *widget_class;

  object_class = G_OBJECT_CLASS (klass);
  applet_class = PANEL_APPLET_CLASS (klass);
  widget_class = GTK_WIDGET_CLASS(klass);
  
  parent_class = g_type_class_peek_parent (klass);

  object_class->finalize = seahorse_applet_finalize;
  applet_class->change_size = seahorse_applet_change_size;
  applet_class->change_background = seahorse_applet_change_background;
  widget_class->button_press_event = handle_button_press;

  g_type_class_add_private (object_class, sizeof (SeahorseAppletPrivate));
}

static void
seahorse_applet_init (SeahorseApplet *applet)
{
    SeahorseAppletPrivate *priv;
    SeahorseContext *sctx;
    SeahorseOperation *op;
    GdkAtom atom;
    GtkClipboard *board;
           
    priv = SEAHORSE_APPLET_GET_PRIVATE (applet);
    
    sctx = seahorse_context_new (TRUE, SKEY_PGP);
    op = seahorse_context_load_local_keys (sctx);
    priv->context = sctx;
    
    /* Let operation take care of itself */
    g_object_unref (op);

    priv->icon = NULL;
    priv->icon_theme = gtk_icon_theme_get_default ();
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
    g_signal_connect (board, "owner-change",
                      G_CALLBACK (handle_clipboard_owner_change), priv);
    
    atom = gdk_atom_intern ("PRIMARY", FALSE);
    board = gtk_clipboard_get (atom);
    g_signal_connect (board, "owner-change",
                      G_CALLBACK (handle_clipboard_owner_change), priv);                
}

static void
seahorse_applet_finalize (GObject *object)
{
    SeahorseAppletPrivate *priv = SEAHORSE_APPLET_GET_PRIVATE (object);

    if (priv)
    {
        g_object_unref (priv->icon);

        seahorse_context_destroy (SCTX_APP ());
        priv->context = NULL;
        
        if(priv->menu){
            gtk_widget_destroy(priv->menu);
            priv->menu = NULL;
        }
        
        if(priv->tooltips) {
            g_object_unref (G_OBJECT (priv->tooltips));
            priv->tooltips = NULL;
        }
        
        if(priv->icon){
            g_object_unref(G_OBJECT (priv->tooltips));
            priv->tooltips = NULL;
        }
    }

    if (G_OBJECT_CLASS (parent_class)->finalize)
        (* G_OBJECT_CLASS (parent_class)->finalize) (object);
}

static void
seahorse_applet_change_size (PanelApplet *applet,
			  guint        size)
{
    SeahorseAppletPrivate *priv = SEAHORSE_APPLET_GET_PRIVATE (applet);

    if (priv->icon)
        g_object_unref (priv->icon);

    /* this might be too much overload, maybe should we get just one icon size and scale? */
    priv->icon = gtk_icon_theme_load_icon (priv->icon_theme,
    				 "seahorse", size, 0, NULL);
        
    gtk_image_set_from_pixbuf (GTK_IMAGE (priv->image), priv->icon);
}

static void
seahorse_applet_change_background (PanelApplet *app,
				PanelAppletBackgroundType type,
				GdkColor  *colour,
				GdkPixmap *pixmap)
{
  SeahorseApplet *applet = SEAHORSE_APPLET (app);
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
      gtk_widget_modify_bg (GTK_WIDGET (applet),
			    GTK_STATE_NORMAL, colour);
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
handle_clipboard_owner_change(GtkClipboard *clipboard,
                              GdkEvent *event,
                              SeahorseAppletPrivate *priv)
{
    priv->board = clipboard;
}

static void
seahorse_popup_position_menu (GtkMenu *menu, int *x, int *y,
                             gboolean *push_in, gpointer  gdata)
{
        GtkWidget      *widget;
        GtkRequisition  requisition;
        gint            menu_xpos;
        gint            menu_ypos;

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
handle_button_press (GtkWidget *widget, 
                     GdkEventButton *event)
{
    GtkWidget *item;    
    SeahorseApplet *applet = SEAHORSE_APPLET(widget);
    SeahorseAppletPrivate *priv = SEAHORSE_APPLET_GET_PRIVATE (applet);
    
    if (event->button == 1){
        
        if(priv->menu){
            gtk_widget_destroy(priv->menu);
            priv->menu = NULL;
        }
        
        /* Build Menu */
        priv->menu = gtk_menu_new();
        
        item = gtk_image_menu_item_new_with_mnemonic(_("_Encrypt Clipboard"));
        g_signal_connect(item, "activate", G_CALLBACK (encrypt_cb), applet);
        gtk_menu_shell_append  ((GtkMenuShell *)(priv->menu),(item));
        
        item = gtk_image_menu_item_new_with_mnemonic(_("_Sign Clipboard"));
        g_signal_connect(item, "activate", G_CALLBACK (sign_cb), applet);
        gtk_menu_shell_append  ((GtkMenuShell *)(priv->menu),(item));
        
        item = gtk_image_menu_item_new_with_mnemonic(_("_Decrypt/Verify/Import Clipboard"));
        g_signal_connect(item, "activate", G_CALLBACK (dvi_cb), applet);
        gtk_menu_shell_append  ((GtkMenuShell *)(priv->menu),(item));
        
        gtk_widget_show_all(priv->menu);
        
        gtk_menu_popup (GTK_MENU(priv->menu), NULL, NULL, seahorse_popup_position_menu, (gpointer) applet, 
        	  event->button, event->time);
        return TRUE;
	} else if (GTK_WIDGET_CLASS (parent_class)->button_press_event)
        return (* GTK_WIDGET_CLASS (parent_class)->button_press_event) (widget, event);
    else
        return FALSE;
}

static void
about_cb (BonoboUIComponent *uic,
	  SeahorseApplet        *seahorse_applet,
	  const gchar       *verbname)
{
        static const gchar *authors [] = {
		"Adam Schreiber <sadam@clemson.edu>",
		NULL
	};

	const gchar *documenters[] = {
        "Adam Schreiber <sadam@clemson.edu>",
        NULL
	};

	gtk_show_about_dialog (NULL,
		"name",		_("seahorse-applet"),
		"version",	VERSION,
		"comments",	_("Use PGP/GPG to encrypt/decrypt/sign/verify/import the clipboard."
				  "panel."),
		"copyright",	"\xC2\xA9 2005 Adam Schreiber",
		"authors",	authors,
		"documenters",	documenters,
		"translator-credits",	_("translator-credits"),
		"logo-icon-name",	"seahorse",
		NULL);
}

static void
display_text (gchar *title, gchar *text, gboolean editable)
{
    GtkWidget *dialog, *scrolled_window, *text_view;
    GtkTextBuffer *buffer;
    GdkPixbuf *pixbuf;

    dialog = gtk_dialog_new_with_buttons (title,
                                          NULL,
                                          0,
                                          GTK_STOCK_OK,
                                          GTK_RESPONSE_NONE,
                                          NULL);

    pixbuf = gtk_widget_render_icon (dialog, 
                                SEAHORSE_STOCK_SEAHORSE, 
                                (GtkIconSize)-1, 
                                NULL); 
    gtk_window_set_icon (GTK_WINDOW (dialog), gdk_pixbuf_copy(pixbuf));
    g_object_unref(pixbuf);
    gtk_window_set_default_size(GTK_WINDOW(dialog), 600, 480);
    g_signal_connect_swapped (dialog,
                              "response", 
                              G_CALLBACK (gtk_widget_destroy),
                              dialog);         
                                                            
    scrolled_window = gtk_scrolled_window_new (NULL, NULL);
    gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), scrolled_window);
                                                            
    text_view = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(text_view), editable);
    gtk_text_view_set_left_margin (GTK_TEXT_VIEW(text_view), 8);
    gtk_text_view_set_right_margin (GTK_TEXT_VIEW(text_view), 8);
    gtk_container_add(GTK_CONTAINER(scrolled_window), text_view);
    
    buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view));
    gtk_text_buffer_set_text(buffer,
                             text,
                             strlen(text));
    
    gtk_widget_show_all(dialog);
}

static void
encrypt_received(GtkClipboard *board, const gchar *text, gpointer data)
{
    SeahorsePGPKey *signer = NULL;
    gpgme_error_t err = GPG_OK;
    gchar *enctext = NULL;
    GList *keys;
 
    keys = seahorse_recipients_get (&signer);
    
    /* User may have cancelled */
    if (g_list_length(keys) == 0)
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
    
    if(seahorse_gconf_get_boolean(DISPLAY_ENCRYPT_ENABLED) == TRUE){
        display_text(_("Encryption Applet - Encrypted Text"), enctext, FALSE);
    }
    
    g_free (enctext);
}

static void
encrypt_cb (GtkMenuItem *menuitem,
                        SeahorseApplet *applet)
{
    SeahorseAppletPrivate *priv = SEAHORSE_APPLET_GET_PRIVATE (applet);

    gtk_clipboard_request_text (priv->board,
         (GtkClipboardTextReceivedFunc)encrypt_received, NULL);    
}

static void
sign_received(GtkClipboard *board, const gchar *text, gpointer data)
{
    SeahorsePGPKey *signer = NULL;
    gpgme_error_t err = GPG_OK;
    gchar *enctext = NULL;

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

    if(seahorse_gconf_get_boolean(DISPLAY_SIGN_ENABLED) == TRUE){
        display_text(_("Encryption Applet - Signed Text"), enctext, FALSE);
    }

    g_free (enctext);
}

static void
sign_cb (GtkMenuItem *menuitem,
         SeahorseApplet *applet)
{
    SeahorseAppletPrivate *priv = SEAHORSE_APPLET_GET_PRIVATE (applet);

    gtk_clipboard_request_text (priv->board,
         (GtkClipboardTextReceivedFunc)sign_received, NULL);    
}


/* When we try to 'decrypt' a key, this gets called */
static guint
import_keys (const gchar * text)
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
decrypt_text (const gchar * text, gpgme_verify_result_t *status)
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
verify_text (const gchar * text, gpgme_verify_result_t *status)
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
dvi_received(GtkClipboard *board, const gchar *text, gpointer data)
{
    SeahorseTextType type;      /* Type of the current block */
    gchar *rawtext = NULL;      /* Replacement text */
    guint keys = 0;             /* Number of keys imported */
    gpgme_verify_result_t status;   /* Signature status of last operation */
    //gchar *last;                /* Pointer to end of last block */
    const gchar *start;         /* Pointer to start of the block */
    const gchar *end;           /* Pointer to end of the block */
    
    /* Try to figure out what it contains */
    type = detect_text_type (text, -1, &start, &end);
    
    if (type == SEAHORSE_TEXT_TYPE_NONE) {
        return;
    }
    
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
        
        if(seahorse_gconf_get_boolean(DISPLAY_DECRYPT_ENABLED) == TRUE){
            display_text(_("Encryption Applet - Decrypted Text"), rawtext, TRUE);
        }
        
        g_free (rawtext);
        rawtext = NULL;

        if(status && status->signatures) {  
            seahorse_signatures_notify ("Text", status);
        }    
    }
    
    if (keys > 0)
        seahorse_import_notify(keys);
}

static void
dvi_cb (GtkMenuItem *menuitem,
        SeahorseApplet *applet)
{
    SeahorseAppletPrivate *priv = SEAHORSE_APPLET_GET_PRIVATE (applet);

    gtk_clipboard_request_text (priv->board,
         (GtkClipboardTextReceivedFunc)dvi_received, NULL);    
}

void
properties_cb (BonoboUIComponent *uic,
	 SeahorseApplet        *seahorse_applet,
	 const char        *verbname)
{
    GError *err = NULL;
    g_spawn_command_line_async ("seahorse-preferences --applet", &err);
    
    if (err != NULL) {
        g_warning ("couldn't execute seahorse-preferences: %s", err->message);
        g_error_free (err);
    }
}

static void
help_cb (BonoboUIComponent *uic,
	     SeahorseApplet        *seahorse_applet,
	     const char        *verbname)
{
	GError *error = NULL;
	GtkWidget *dialog;
	
    fprintf(stderr, "Before Help Display PACKAGE=%s\n", PACKAGE);
	gnome_help_display ("seahorse-applet", NULL, &error);
    fprintf(stderr, "After Help Display error %s\n", error?error->message:"NULL");
    
	if (error) {
		dialog = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE, 
							    _("There was an error displaying help: %s"), error->message);
		fprintf(stderr, "After create dialog\n");
		g_signal_connect (G_OBJECT (dialog), "response", G_CALLBACK (gtk_widget_destroy), dialog);
		fprintf(stderr, "After signal connect\n");
		gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
		fprintf(stderr, "After set resizable\n");
		gtk_window_set_screen (GTK_WINDOW (dialog), gtk_widget_get_screen (GTK_WIDGET (seahorse_applet)));
		fprintf(stderr, "Before Show\n");
		gtk_widget_show (dialog);
		fprintf(stderr, "After Show\n");
		g_error_free (error);
		error = NULL;
	}
}


static const BonoboUIVerb seahorse_applet_menu_verbs [] = {
        BONOBO_UI_UNSAFE_VERB ("Props", properties_cb),
        BONOBO_UI_UNSAFE_VERB ("Help", help_cb),
        BONOBO_UI_UNSAFE_VERB ("About", about_cb),

        BONOBO_UI_VERB_END
};

static void
set_atk_name_description (GtkWidget *widget, const gchar *name,
    const gchar *description)
{
	AtkObject *aobj;
   
	aobj = gtk_widget_get_accessible (widget);
	/* Check if gail is loaded */
	if (GTK_IS_ACCESSIBLE (aobj) == FALSE)
		return;

	atk_object_set_name (aobj, name);
	atk_object_set_description (aobj, description);
}

static gboolean
seahorse_applet_fill (PanelApplet *applet)
{
	SeahorseApplet *seahorse_applet;    

    /* Insert Icons into Stock */ 
    seahorse_gtk_stock_init();
        	
    g_return_val_if_fail (PANEL_IS_APPLET (applet), FALSE);

    gtk_widget_show_all (GTK_WIDGET (applet));
	
	panel_applet_setup_menu_from_file (applet,
                                       UIDIR,
            				           "GNOME_SeahorseApplet.xml",
                                       NULL,
               				           seahorse_applet_menu_verbs,
            				           seahorse_applet);

	if (panel_applet_get_locked_down (applet)) {
		BonoboUIComponent *popup_component;

		popup_component = panel_applet_get_popup_component (applet);

		bonobo_ui_component_set_prop (popup_component,
					      "/commands/Props",
					      "hidden", "1",
					      NULL);
	}

	return TRUE;
}

static gboolean
seahorse_applet_factory (PanelApplet *applet,
		      const gchar *iid,
		      gpointer     data)
{
	gboolean retval = FALSE;

	if (!strcmp (iid, "OAFIID:GNOME_SeahorseApplet"))
		retval = seahorse_applet_fill (applet); 
   
	if (retval == FALSE) {
		exit (-1);
	}

	return retval;
}

PANEL_APPLET_BONOBO_FACTORY ("OAFIID:GNOME_SeahorseApplet_Factory",
			     SEAHORSE_TYPE_APPLET,
			     "seahorse-applet",
			     "0",
			     seahorse_applet_factory,
			     NULL)
