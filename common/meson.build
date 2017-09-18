common_sources = [
  'actions.vala',
  'backend.vala',
  'catalog.vala',
  'deletable.vala',
  'delete-dialog.vala',
  'deleter.vala',
  'exportable.vala',
  'exporter.vala',
  'icons.vala',
  'lockable.vala',
  'place.vala',
  'registry.vala',
  'types.vala',
  'util.vala',
  'viewable.vala',
]

common_deps = [
  glib_deps,
  gtk,
  gcr,
  config,
]

common_lib = static_library('common',
  common_sources,
  dependencies: common_deps,
  vala_header: 'seahorse-common.h',
)

common_dep = declare_dependency(
  link_with: common_lib,
  include_directories: include_directories('.'),
)
