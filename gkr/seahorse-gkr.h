#ifndef SEAHORSEGKR_H_
#define SEAHORSEGKR_H_

#include "common/sea-registry.h"

#define SEAHORSE_GKR_STR                     "gnome-keyring"
#define SEAHORSE_GKR                         (g_quark_from_static_string (SEAHORSE_GKR_STR))

extern const SeaRegisterType SEAHORSE_GKR_REGISTRY[];

#endif /*SEAHORSEGKR_H_*/
