gkr_sources = files(
  'add-item-dialog.vala',
  'add-keyring-dialog.vala',
  'gkr-backend.vala',
  'gkr-dialogs.vala',
  'gkr-item-delete-operation.vala',
  'gkr-item-info.vala',
  'gkr-item-panel.vala',
  'gkr-item.vala',
  'gkr-keyring-delete-operation.vala',
  'gkr-keyring-panel.vala',
  'gkr-keyring-permission.vala',
  'gkr-keyring.vala',
  'gkr-module.vala',
)

gkr_dependencies = [
  glib_deps,
  gtk4_dep,
  gcr4_dep,
  libsecret,
  libpwquality,
  common_dep,
]

gkr_vala_args = [
  '--gresources', resources_xml,
]

gkr_c_args = [
  vala_workaround_cflags,
]

gkr_lib = static_library('seahorse-gkr',
  gkr_sources,
  vala_args: gkr_vala_args,
  c_args: gkr_c_args,
  dependencies: gkr_dependencies,
)

gkr_dep = declare_dependency(
  link_with: gkr_lib,
  include_directories: include_directories('.'),
)
