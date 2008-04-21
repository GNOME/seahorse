
#include "config.h"

#include "sea-pgp.h"

#include "seahorse-pgp-source.h"

#ifdef WITH_LDAP
#include "seahorse-ldap-source.h"
#endif
#ifdef WITH_HKP
#include "seahorse-hkp-source.h"
#endif
	
const SeaRegisterType SEA_PGP_REGISTRY[] = {
	seahorse_pgp_source_get_type,
#ifdef WITH_LDAP
	seahorse_ldap_source_get_type,
#endif
#ifdef WITH_HKP
	seahorse_hkp_source_get_type,
#endif
	NULL
};
