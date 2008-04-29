
#include "config.h"

#include "seahorse-gkr.h"

#include "seahorse-gkeyring-source.h"

const SeaRegisterType SEAHORSE_GKR_REGISTRY[] = {
	seahorse_gkeyring_source_get_type,
	NULL
};
