
#ifndef __SEAHORSE_CHECK_BUTTON_CONTROL_H__
#define __SEAHORSE_CHECK_BUTTON_CONTROL_H__

#include <gtk/gtk.h>

#define SEAHORSE_TYPE_CHECK_BUTTON_CONTROL		(seahorse_check_button_control_get_type ())
#define SEAHORSE_CHECK_BUTTON_CONTROL(obj)		(GTK_CHECK_CAST ((obj), SEAHORSE_TYPE_CHECK_BUTTON_CONTROL, SeahorseCheckButtonControl))
#define SEAHORSE_CHECK_BUTTON_CONTROL_CLASS(klass)	(GTK_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_CHECK_BUTTON_CONTROL, SeahorseCheckButtonControlClass))
#define SEAHORSE_IS_CHECK_BUTTON_CONTROL(obj)		(GTK_CHECK_TYPE ((obj), SEAHORSE_TYPE_CHECK_BUTTON_CONTROL))
#define SEAHORSE_IS_CHECK_BUTTON_CONTROL_CLASS(klass)	(GTK_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_CHECK_BUTTON_CONTROL))
#define SEAHORSE_CHECK_BUTTON_CONTROL_GET_CLASS(obj)	(GTK_CHECK_GET_CLASS ((obj), SEAHORSE_TYPE_CHECK_BUTTON_CONTROL, SeahorseCheckButtonControlClass))

typedef struct _SeahorseCheckButtonControl SeahorseCheckButtonControl;
typedef struct _SeahorseCheckButtonControlClass SeahorseCheckButtonControlClass;

struct _SeahorseCheckButtonControl
{
	GtkCheckButton		parent;
	
	gchar			*gconf_key;
	guint			notify_id;
};

struct _SeahorseCheckButtonControlClass
{
	GtkCheckButtonClass	parent_class;
};

GtkWidget*	seahorse_check_button_control_new	(const gchar	*label,
							 const gchar	*gconf_key);

#endif /* __SEAHORSE_CHECK_BUTTON_CONTROL_H__ */
