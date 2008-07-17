
#ifndef __SEAHORSE_TYPES_H__
#define __SEAHORSE_TYPES_H__

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS


#define SEAHORSE_TYPE_LOCATION (seahorse_location_get_type ())

#define SEAHORSE_TYPE_USAGE (seahorse_usage_get_type ())

/* 
 * These types should never change. These values are exported via DBUS. In the 
 * case of a key being in multiple locations, the highest location always 'wins'.
 */
typedef enum  {
	SEAHORSE_LOCATION_INVALID = 0,
	SEAHORSE_LOCATION_MISSING = 10,
	SEAHORSE_LOCATION_SEARCHING = 20,
	SEAHORSE_LOCATION_REMOTE = 50,
	SEAHORSE_LOCATION_LOCAL = 100
} SeahorseLocation;

/* Again, never change these values */
typedef enum  {
	SEAHORSE_USAGE_NONE = 0,
	SEAHORSE_USAGE_SYMMETRIC_KEY = 1,
	SEAHORSE_USAGE_PUBLIC_KEY = 2,
	SEAHORSE_USAGE_PRIVATE_KEY = 3,
	SEAHORSE_USAGE_CREDENTIALS = 4,
	SEAHORSE_USAGE_OTHER = 10
} SeahorseUsage;


GType seahorse_location_get_type (void);
GType seahorse_usage_get_type (void);


G_END_DECLS

#endif
