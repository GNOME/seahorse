libseahorse_marshaller = gnome.genmarshal('seahorse-marshal',
  sources: 'seahorse-marshal.list',
  prefix: 'seahorse_marshal',
)


libseahorse_sources = files(
  'seahorse-bind.c',
  'seahorse-object-model.c',
  'seahorse-progress.c',
  'seahorse-util.c',
  'seahorse-widget.c',
)

libseahorse_deps = [
  glib_deps,
  gcr,
  libsecret,
  libtreemultidnd_dep,
  common_dep,
]

libseahorse_lib = static_library('libseahorse',
  [ libseahorse_sources, libseahorse_marshaller ],
  dependencies: libseahorse_deps,
  include_directories: include_directories('..'),
)

libseahorse_dep = declare_dependency(
  link_with: libseahorse_lib,
)
