
#include <eel/eel.h>

#include "seahorse-default-key-control.h"

static void
item_activated (GtkMenuItem *item, gchar *id)
{
	eel_gconf_set_string (DEFAULT_KEY, id);
}

static void
key_destroyed (GtkObject *object, GtkWidget *widget)
{
	gtk_widget_destroy (GTK_WIDGET (widget));
}

static void
key_added (SeahorseContext *sctx, SeahorseKey *skey, GtkWidget *menu)
{
	GtkWidget *item;
	
	if (!SEAHORSE_IS_KEY_PAIR (skey) || !seahorse_key_pair_can_sign (SEAHORSE_KEY_PAIR (skey)))
		return;
	
	item = gtk_menu_item_new_with_label (seahorse_key_get_userid (skey, 0));
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
	gtk_widget_show (item);
	
	g_signal_connect_after (GTK_MENU_ITEM (item), "activate", G_CALLBACK (item_activated),
		(gchar*)seahorse_key_get_id (SEAHORSE_KEY_PAIR (skey)->secret));
	g_signal_connect_after (GTK_OBJECT (skey), "destroy",
		G_CALLBACK (key_destroyed), item);
}

static void
menu_destroyed (GtkObject *object, SeahorseContext *sctx)
{
	g_signal_handlers_disconnect_by_func (sctx, key_added, GTK_WIDGET (object));
}

GtkWidget*
seahorse_default_key_control_new (SeahorseContext *sctx)
{
	GList *list = NULL;
	GtkWidget *option, *menu, *item;
	gint index = 0, history = 0;
	SeahorseKey *skey;
	const gchar *default_key, *id;
	
	menu = gtk_menu_new ();
	default_key = eel_gconf_get_string (DEFAULT_KEY);
	
	for (list = seahorse_context_get_key_pairs (sctx); list != NULL; list = g_list_next (list)) {
		skey = SEAHORSE_KEY (list->data);
		
		if (!SEAHORSE_IS_KEY_PAIR (skey) || !seahorse_key_pair_can_sign (SEAHORSE_KEY_PAIR (skey)))
			return;
		
		key_added (sctx, skey, menu);
		
		if (g_str_equal (default_key, seahorse_key_get_id (SEAHORSE_KEY_PAIR (skey)->secret)))
			history = index;
		
		index++;
	}
	
	g_signal_connect_after (sctx, "add", G_CALLBACK (key_added), menu);
	g_signal_connect_after (GTK_OBJECT (menu), "destroy",
		G_CALLBACK (menu_destroyed), sctx);
	
	option = gtk_option_menu_new ();
	gtk_option_menu_set_menu (GTK_OPTION_MENU (option), menu);
	gtk_widget_show (menu);
	
	gtk_option_menu_set_history (GTK_OPTION_MENU (option), history);
	
	return option;
}
