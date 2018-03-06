marshaller = gnome.genmarshal('seahorse-marshal',
  sources: 'seahorse-marshal.list',
  prefix: 'seahorse_marshal',
)


libseahorse_sources = [
  'seahorse-bind.c',
  'seahorse-interaction.c',
  'seahorse-object-list.c',
  'seahorse-object-model.c',
  'seahorse-object-widget.c',
  'seahorse-progress.c',
  'seahorse-util.c',
  'seahorse-widget.c',
  marshaller,
  resources_src,
]

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
  include_directories: include_directories('..'),
)

libseahorse_dep = declare_dependency(
  link_with: libseahorse_lib,
)
