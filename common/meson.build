common_sources = [
  'actions.vala',
  'add-keyserver-dialog.vala',
  'backend.vala',
  'catalog.vala',
  'collection.vala',
  'deletable.vala',
  'delete-dialog.vala',
  'deleter.vala',
  'exportable.vala',
  'exporter.vala',
  'icons.vala',
  'lockable.vala',
  'object.vala',
  'passphrase-prompt.vala',
  'place.vala',
  'predicate.vala',
  'prefs.vala',
  'registry.vala',
  'servers.vala',
  'types.vala',
  'util.vala',
  'validity.vala',
  'viewable.vala',
]

common_deps = [
  glib_deps,
  gtk,
  gcr,
  gcr_ui,
  config,
]

common_vala_args = [
]

if with_keyservers
  common_sources += 'keyserver-control.vala'
  common_vala_args += [ '-D', 'WITH_KEYSERVER' ]
endif

common_lib = static_library('common',
  common_sources,
  dependencies: common_deps,
  vala_args: common_vala_args,
  vala_header: 'seahorse-common.h',
)

common_dep = declare_dependency(
  link_with: common_lib,
  include_directories: include_directories('.'),
)
