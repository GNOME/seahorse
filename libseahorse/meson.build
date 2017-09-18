marshaller = gnome.genmarshal('seahorse-marshal',
  sources: 'seahorse-marshal.list',
  prefix: 'seahorse_marshal',
)

search_provider_src = gnome.gdbus_codegen('seahorse-shell-search-provider-generated',
  'org.gnome.ShellSearchProvider2.xml',
  interface_prefix : 'org.gnome.',
  namespace : 'Seahorse',
)


libseahorse_sources = [
  'seahorse-application.c',
  'seahorse-bind.c',
  'seahorse-collection.c',
  'seahorse-interaction.c',
  'seahorse-key-manager-store.c',
  'seahorse-object.c',
  'seahorse-object-list.c',
  'seahorse-object-model.c',
  'seahorse-object-widget.c',
  'seahorse-passphrase.c',
  'seahorse-predicate.c',
  'seahorse-prefs.c',
  'seahorse-progress.c',
  'seahorse-search-provider.c',
  'seahorse-servers.c',
  'seahorse-util.c',
  'seahorse-validity.c',
  'seahorse-widget.c',
  marshaller,
  resources_src,
  search_provider_src,
]

if with_keyservers
  libseahorse_sources += 'seahorse-keyserver-control.c'
endif

libseahorse_deps = [
  glib_deps,
  gcr,
  libsecret,
  libtreemultidnd_dep,
  common_dep,
]

libseahorse_lib = static_library('libseahorse',
  libseahorse_sources,
  dependencies: libseahorse_deps,
)

libseahorse_dep = declare_dependency(
  link_with: libseahorse_lib,
)
