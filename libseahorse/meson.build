libseahorse_sources = files(
  'seahorse-progress.c',
  'seahorse-util.c',
)

libseahorse_deps = [
  glib_deps,
  gcr4_dep,
  libsecret,
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
