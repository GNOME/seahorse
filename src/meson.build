search_provider_src = gnome.gdbus_codegen('seahorse-shell-search-provider-generated',
  'org.gnome.ShellSearchProvider2.xml',
  interface_prefix : 'org.gnome.',
  namespace : 'Seahorse',
)

seahorse_sources = [
  'application.vala',
  'import-dialog.vala',
  'key-manager.vala',
  'key-manager-filter.vala',
  'key-manager-item-row.vala',
  'main.vala',
  'prefs-dialog.vala',
  'search-provider.vala',
  'sidebar.vala',

  resources_src,
  search_provider_src,
]

seahorse_dependencies = [
  pkcs11_dep,
  glib_deps,
  gtk4_dep,
  libadwaita_dep,
  libsecret,
  common_dep,
  libseahorse_dep,
  gkr_dep,
  ssh_dep,
]

seahorse_c_flags = [
  '-DHANDY_USE_UNSTABLE_API',
  vala_workaround_cflags,
]

seahorse_vala_flags = [
]

if get_option('keyservers-support')
  seahorse_vala_flags += [ '-D', 'WITH_KEYSERVER' ]
endif
if get_option('pgp-support')
  seahorse_dependencies += pgp_dep
  seahorse_vala_flags += [ '-D', 'WITH_PGP' ]
endif
if get_option('pkcs11-support')
  seahorse_dependencies += pkcs11_dep
  seahorse_vala_flags += [ '-D', 'WITH_PKCS11' ]
endif


seahorse_exe = executable('seahorse',
  seahorse_sources,
  dependencies: seahorse_dependencies,
  vala_args: seahorse_vala_flags,
  c_args: seahorse_c_flags,
  install: true,
)
