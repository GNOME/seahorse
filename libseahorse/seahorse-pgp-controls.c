
#include <config.h>
#include <gdk/gdkx.h>
#include <libbonoboui.h>
#include <gnome.h>
#include <eel/eel.h>

#include "seahorse-context.h"

#define MENU_SCTX "menu_context"

enum {
	ASCII_ARMOR_PROP,
	TEXT_MODE_PROP,
	ENCRYPT_SELF_PROP,
	DEFAULT_KEY_PROP
};

typedef struct {
	GtkWidget	*ascii_armor;
	GtkWidget	*text_mode;
	GtkWidget	*encrypt_self;
	GtkContainer	*hbox;
	GtkWidget	*default_key;
	GtkContainer	*vbox;
} ControlTable;

static void
gconf_notification (GConfClient *gclient, guint id, GConfEntry *entry, ControlTable *controls)
{
	const gchar *key;
	GConfValue *value;
	gint index = -1;
	GList *keys = NULL;
	SeahorseContext *sctx;
	SeahorseKeyPair *skpair;
	
	key = gconf_entry_get_key (entry);
	value = gconf_entry_get_value (entry);
	
	if (g_str_equal (key, ARMOR_KEY) && controls->ascii_armor != NULL)
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (controls->ascii_armor),
			gconf_value_get_bool (value));
	else if (g_str_equal (key, TEXTMODE_KEY) && controls->text_mode != NULL)
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (controls->text_mode),
			gconf_value_get_bool (value));
	else if (g_str_equal (key, ENCRYPTSELF_KEY) && controls->encrypt_self != NULL)
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (controls->encrypt_self),
			gconf_value_get_bool (value));
	else if (g_str_equal (key, DEFAULT_KEY) && controls->default_key != NULL) {
		sctx = g_object_get_data (G_OBJECT (controls->default_key), MENU_SCTX);
		keys = seahorse_context_get_key_pairs (sctx);
		
		for (; keys != NULL; keys = g_list_next (keys)) {
			skpair = keys->data;
			index++;
			if (g_str_equal (gconf_value_get_string (value), seahorse_key_get_id (skpair->secret)))
				break;
		}
		
		if (index >= 0)
			gtk_option_menu_set_history (GTK_OPTION_MENU (controls->default_key), index);
	}
}

static void
control_toggled (GtkToggleButton *toggle_button, const gchar *gconf_key)
{
	eel_gconf_set_boolean (gconf_key, gtk_toggle_button_get_active (toggle_button));
}

static void
key_activate (GtkMenuItem *item, GtkOptionMenu *options)
{
	SeahorseContext *sctx;
	GList *keys = NULL;
	SeahorseKeyPair *skpair;
	
	sctx = g_object_get_data (G_OBJECT (options), MENU_SCTX);
	keys = seahorse_context_get_key_pairs (sctx);
	keys = g_list_nth (keys, gtk_option_menu_get_history (options));
	
	skpair = keys->data;
	eel_gconf_set_string (DEFAULT_KEY, seahorse_key_get_id (skpair->secret));
}

static void
get_property (BonoboPropertyBag *pb, BonoboArg *arg, guint id,
	      CORBA_Environment *ev, ControlTable *controls)
{
	switch (id) {
		case ASCII_ARMOR_PROP:
			if (GTK_WIDGET_VISIBLE (controls->ascii_armor))
				BONOBO_ARG_SET_BOOLEAN (arg, CORBA_TRUE);
			else
				BONOBO_ARG_SET_BOOLEAN (arg, CORBA_FALSE);
			break;
		case TEXT_MODE_PROP:
			if (GTK_WIDGET_VISIBLE (controls->text_mode))
				BONOBO_ARG_SET_BOOLEAN (arg, CORBA_TRUE);
			else
				BONOBO_ARG_SET_BOOLEAN (arg, CORBA_FALSE);
			break;
		case ENCRYPT_SELF_PROP:
			if (GTK_WIDGET_VISIBLE (controls->encrypt_self))
				BONOBO_ARG_SET_BOOLEAN (arg, CORBA_TRUE);
			else
				BONOBO_ARG_SET_BOOLEAN (arg, CORBA_FALSE);
			break;
		case DEFAULT_KEY_PROP:
			if (GTK_WIDGET_VISIBLE (controls->default_key))
				BONOBO_ARG_SET_BOOLEAN (arg, CORBA_TRUE);
			else
				BONOBO_ARG_SET_BOOLEAN (arg, CORBA_FALSE);
			break;
		default:
			break;
	}
}

static void
set_property (BonoboPropertyBag *pb, const BonoboArg *arg, guint id,
	      CORBA_Environment *ev, ControlTable *controls)
{
	switch (id) {
		case ASCII_ARMOR_PROP:
			if (BONOBO_ARG_GET_BOOLEAN (arg))
				gtk_widget_show (controls->ascii_armor);
			else
				gtk_widget_hide (controls->ascii_armor);
			break;
		case TEXT_MODE_PROP:
			if (BONOBO_ARG_GET_BOOLEAN (arg))
				gtk_widget_show (controls->text_mode);
			else
				gtk_widget_hide (controls->text_mode);
			break;
		case ENCRYPT_SELF_PROP:
			if (BONOBO_ARG_GET_BOOLEAN (arg))
				gtk_widget_show (controls->encrypt_self);
			else
				gtk_widget_hide (controls->encrypt_self);
			break;
		case DEFAULT_KEY_PROP:
			if (BONOBO_ARG_GET_BOOLEAN (arg))
				gtk_widget_show (controls->default_key);
			else
				gtk_widget_hide (controls->default_key);
			break;
		default:
			break;
	}
}

BonoboObject*
seahorse_pgp_controls_new (void)
{
	ControlTable *controls;
	BonoboControl *control;
	BonoboPropertyBag *pb;
	BonoboArg *arg;
	SeahorseContext *sctx;
	GList *keys;
	GtkWidget *menu, *item;
	const gchar *keyid;
	gint index = 0, history = 0;

	g_print ("creating controls\n");
	controls = g_new0 (ControlTable, 1);
	
	controls->hbox = GTK_CONTAINER (gtk_hbox_new (FALSE, 12));
	gtk_widget_show (GTK_WIDGET (controls->hbox));
	
	controls->vbox = GTK_CONTAINER (gtk_vbox_new (FALSE, 12));
	gtk_container_set_border_width (GTK_CONTAINER (controls->vbox), 12);
	gtk_widget_show (GTK_WIDGET (controls->vbox));
	gtk_container_add (controls->vbox, GTK_WIDGET (controls->hbox));
	
	controls->ascii_armor = gtk_check_button_new_with_mnemonic (_("_Ascii Armor"));
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (controls->ascii_armor),
		eel_gconf_get_boolean (ARMOR_KEY));
	g_signal_connect_after (GTK_TOGGLE_BUTTON (controls->ascii_armor), "toggled",
		G_CALLBACK (control_toggled), ARMOR_KEY);
	gtk_container_add (controls->hbox, controls->ascii_armor);
	
	controls->text_mode = gtk_check_button_new_with_mnemonic (_("_Text Mode"));
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (controls->text_mode),
		eel_gconf_get_boolean (TEXTMODE_KEY));
	g_signal_connect_after (GTK_TOGGLE_BUTTON (controls->text_mode),"toggled",
		G_CALLBACK (control_toggled), TEXTMODE_KEY);
	gtk_container_add (controls->hbox, controls->text_mode);
	
	controls->encrypt_self = gtk_check_button_new_with_mnemonic (_("_Encrypt to Self"));
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (controls->encrypt_self),
		eel_gconf_get_boolean (ENCRYPTSELF_KEY));
	g_signal_connect_after (GTK_TOGGLE_BUTTON (controls->encrypt_self),"toggled",
		G_CALLBACK (control_toggled), ENCRYPTSELF_KEY);
	gtk_container_add (controls->hbox, controls->encrypt_self);
	
	controls->default_key = gtk_option_menu_new ();
	gtk_container_add (controls->vbox, controls->default_key);
				
	sctx = seahorse_context_new ();
	g_object_set_data (G_OBJECT (controls->default_key), MENU_SCTX, sctx);
				
	menu = gtk_menu_new ();
	gtk_widget_show (menu);
	gtk_option_menu_set_menu (GTK_OPTION_MENU (controls->default_key), menu);
				
	keys = seahorse_context_get_key_pairs (sctx);
	keyid = eel_gconf_get_string (DEFAULT_KEY);
				
	for (; keys != NULL; keys = g_list_next (keys)) {
		SeahorseKeyPair *skpair = keys->data;
		item = gtk_menu_item_new_with_label (seahorse_key_get_userid (SEAHORSE_KEY (skpair), 0));
		gtk_widget_show (item);
		gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
		g_signal_connect_after (item, "activate", G_CALLBACK (key_activate),
			GTK_OPTION_MENU (controls->default_key));
					
		if (g_str_equal (keyid, seahorse_key_get_id (skpair->secret)))
			history = index;
		index++;
	}
				
	gtk_option_menu_set_history (GTK_OPTION_MENU (controls->default_key), history);

	control = bonobo_control_new (GTK_WIDGET (controls->vbox));
	
	pb = bonobo_property_bag_new ((BonoboPropertyGetFn)get_property,
		(BonoboPropertySetFn)set_property, controls);

	arg = bonobo_arg_new (BONOBO_ARG_BOOLEAN);
	BONOBO_ARG_SET_BOOLEAN (arg, CORBA_TRUE);
	bonobo_property_bag_add (pb, "ascii_armor", ASCII_ARMOR_PROP,
		BONOBO_ARG_BOOLEAN, arg, NULL, 0);
	
	arg = bonobo_arg_copy (arg);
	bonobo_property_bag_add (pb, "text_mode", TEXT_MODE_PROP,
		BONOBO_ARG_BOOLEAN, arg, NULL, 0);

	arg = bonobo_arg_copy (arg);
	bonobo_property_bag_add (pb, "encrypt_self", ENCRYPT_SELF_PROP,
		BONOBO_ARG_BOOLEAN, arg, NULL, 0);

	arg = bonobo_arg_copy (arg);
	bonobo_property_bag_add (pb, "default_key", DEFAULT_KEY_PROP,
		BONOBO_ARG_BOOLEAN, arg, NULL, 0);
	
	eel_gconf_notification_add (PGP_SCHEMAS, (GConfClientNotifyFunc) gconf_notification, controls);
	eel_gconf_monitor_add (PGP_SCHEMAS);

	bonobo_control_set_properties (control, BONOBO_OBJREF (pb), NULL);
	bonobo_object_unref (BONOBO_OBJECT (pb));
	
	bonobo_control_life_instrument (control);

	return BONOBO_OBJECT (control);
}

static BonoboObject*
control_factory (BonoboGenericFactory *this, const gchar *object_id, gpointer data)
{
	BonoboObject *object = NULL;
	
	g_return_val_if_fail (object_id != NULL, NULL);

	if (g_str_equal (object_id, "OAFIID:Seahorse_PGP_Controls"))
		object = seahorse_pgp_controls_new ();

	return object;
}

int
main (int argc, char *argv [])
{
	int retval;
	char *iid;

	if (!bonobo_ui_init ("seahorse-pgp-controls", VERSION, &argc, argv))
		g_error (_("Could not initialize Bonobo UI"));

	/***** DisplayString really should be replaced because it is X specific *****/
	iid = bonobo_activation_make_registration_id (
		"OAFIID:Seahorse_PGP_ControlsFactory", DisplayString (gdk_display));

	retval = bonobo_generic_factory_main (iid, control_factory, NULL);

	g_free (iid);

	return retval;
}
