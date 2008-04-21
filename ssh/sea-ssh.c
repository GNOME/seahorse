
#include "config.h"

#include "sea-ssh.h"

#include "seahorse-ssh-source.h"

const SeaRegisterType SEA_SSH_REGISTRY[] = {
	seahorse_ssh_source_get_type,
	NULL
};
