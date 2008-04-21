#ifndef SEASSH_H_
#define SEASSH_H_

#include "common/sea-registry.h"

#define SEA_SSH_STR                     "openssh"
#define SEA_SSH                         (g_quark_from_static_string (SEA_SSH_STR))

extern const SeaRegisterType SEA_SSH_REGISTRY[];

#endif /*SEASSH_H_*/
