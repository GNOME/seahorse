
#ifndef __SEAHORSE_COMPONENT_H__
#define __SEAHORSE_COMPONENT_H__

#include <bonobo/bonobo-object.h>

#define SEAHORSE_TYPE_COMPONENT			(seahorse_component_get_type ())
#define SEAHORSE_COMPONENT(obj)			(GTK_CHECK_CAST ((obj), SEAHORSE_TYPE_COMPONENT, SeahorseComponent))
#define SEAHORSE_COMPONENT_CLASS(klass)		(GTK_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_COMPONENT, SeahorseComponentClass))
#define SEAHORSE_IS_COMPONENT(obj)		(GTK_CHECK_TYPE ((obj), SEAHORSE_TYPE_COMPONENT))
#define SEAHORSE_IS_COMPONENT_CLASS(klass)	(GTK_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_COMPONENT))

typedef struct {
	BonoboObject parent;
} SeahorseComponent;

typedef struct {
	BonoboObjectClass parent;

	POA_Bonobo_Listener__epv epv;
} SeahorseComponentClass;

GType	seahorse_component_get_type	(void);

#endif /* __SEAHORSE_COMPONENT_H__ */
