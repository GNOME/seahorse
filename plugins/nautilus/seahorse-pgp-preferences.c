
#include <gnome.h>
#include <config.h>

#include "seahorse-widget.h"
#include "seahorse-check-button-control.h"
#include "seahorse-default-key-control.h"

static void
destroyed (GtkObject *object, gpointer data)
{
	gtk_exit (0);
}

int
main (int argc, char **argv)
{
	SeahorseWidget *swidget;
	GtkWidget *table, *label, *widget, *control;
	SeahorseContext *sctx;

#ifdef ENABLE_NLS
        bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
        bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
        textdomain (GETTEXT_PACKAGE);          
#endif

        gnome_program_init (PACKAGE, VERSION, LIBGNOMEUI_MODULE, argc, argv,
                GNOME_PARAM_HUMAN_READABLE_NAME, _("PGP Preferences"),
                GNOME_PARAM_APP_DATADIR, DATA_DIR, NULL);
	
	sctx = seahorse_context_new ();
	swidget = seahorse_widget_new ("pgp-preferences", sctx);
	
	table = gtk_vbox_new (FALSE, 12);
	gtk_container_set_border_width (GTK_CONTAINER (table), 12);
	gtk_container_add (GTK_CONTAINER (glade_xml_get_widget (swidget->xml, "vbox")), table);
	
	widget = gtk_table_new (1, 3, FALSE);
	gtk_table_set_row_spacings (GTK_TABLE (widget), 12);
	gtk_table_set_col_spacings (GTK_TABLE (widget), 12);
	gtk_container_add (GTK_CONTAINER (table), widget);
	
	gtk_table_attach (GTK_TABLE (widget), seahorse_check_button_control_new (
		_("_Ascii Armor"), ARMOR_KEY), 0, 1, 0, 1, GTK_FILL, 0, 0, 0);
	gtk_table_attach (GTK_TABLE (widget), seahorse_check_button_control_new (
		_("_Text Mode"), TEXTMODE_KEY), 1, 2, 0, 1, GTK_FILL, 0, 0, 0);
	gtk_table_attach (GTK_TABLE (widget), seahorse_check_button_control_new (
		_("_Encrypt to Self"), ENCRYPTSELF_KEY), 2, 3, 0, 1, GTK_FILL, 0, 0, 0);
		
	widget = gtk_hbox_new (FALSE, 12);
	gtk_container_add (GTK_CONTAINER (table), widget);
	
	label = gtk_label_new_with_mnemonic (_("_Default Key:"));
	gtk_container_add (GTK_CONTAINER (widget), label);
	
	control = seahorse_default_key_control_new (sctx);
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), control);
	gtk_container_add (GTK_CONTAINER (widget), control);
	
	gtk_widget_show_all (table);
	g_signal_connect (GTK_OBJECT (table), "destroy", G_CALLBACK (destroyed), NULL);
	
	gtk_main();
	return 0;
}
