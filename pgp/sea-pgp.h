#ifndef SEAHORSEPGP_H_
#define SEAHORSEPGP_H_

#include "common/sea-registry.h"

#define SEA_PGP_STR                     "openpgp"
#define SEA_PGP                         (g_quark_from_static_string (SEA_PGP_STR))

extern const SeaRegisterType SEA_PGP_REGISTRY[];

#endif /*SEAHORSEPGP_H_*/
