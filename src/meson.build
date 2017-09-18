seahorse_sources = [
  'seahorse-generate-select.c',
  'seahorse-import-dialog.c',
  'seahorse-key-manager.c',
  'seahorse-main.c',
  'seahorse-sidebar.c',
  resources_src,
]

seahorse_dependencies = [
  glib_deps,
  gtk,
  gcr,
  config,
  common_dep,
  libseahorse_dep,
]

seahorse_linkedlibs = [
  libeggdatetime_lib,
  libtreemultidnd_lib,
  gkr_lib,
  ssh_lib,
]

if with_pgp
  seahorse_linkedlibs += pgp_lib
endif
if with_pkcs11
  seahorse_linkedlibs += pkcs11_lib
endif


seahorse_exe = executable('seahorse',
  seahorse_sources,
  dependencies: seahorse_dependencies,
  link_with: seahorse_linkedlibs,
  install: true,
)

# Manpage
install_man('seahorse.1')
