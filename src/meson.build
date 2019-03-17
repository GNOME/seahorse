search_provider_src = gnome.gdbus_codegen('seahorse-shell-search-provider-generated',
  'org.gnome.ShellSearchProvider2.xml',
  interface_prefix : 'org.gnome.',
  namespace : 'Seahorse',
)

seahorse_sources = [
  'application.vala',
  'import-dialog.vala',
  'key-manager.vala',
  'main.vala',
  'search-provider.vala',
  'sidebar.vala',

  resources_src,
  search_provider_src,
]

seahorse_dependencies = [
  glib_deps,
  gtk,
  libsecret,
  common_dep,
  libseahorse_dep,
  gkr_dep,
  ssh_dep,
]

seahorse_linkedlibs = [
  libeggdatetime_lib,
  libtreemultidnd_lib,
]

seahorse_vala_flags = [
]

if with_pgp
  seahorse_dependencies += pgp_dep
  seahorse_vala_flags += [ '-D', 'WITH_PGP' ]
endif
if with_pkcs11
  seahorse_dependencies += pkcs11_dep
  seahorse_vala_flags += [ '-D', 'WITH_PKCS11' ]
endif


seahorse_exe = executable('seahorse',
  seahorse_sources,
  dependencies: seahorse_dependencies,
  vala_args: seahorse_vala_flags,
  link_with: seahorse_linkedlibs,
  install: true,
)
