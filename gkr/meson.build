gkr_sources = files(
  'gkr-backend.vala',
  'gkr-dialogs.vala',
  'gkr-item-add.vala',
  'gkr-item-info.vala',
  'gkr-item-properties.vala',
  'gkr-item.vala',
  'gkr-keyring-add.vala',
  'gkr-keyring-permission.vala',
  'gkr-keyring-properties.vala',
  'gkr-keyring.vala',
  'gkr-module.vala',
  'gkr-password-entry.vala',
)

gkr_dependencies = [
  glib_deps,
  gtk,
  gcr,
  gcr_ui,
  libsecret,
  libpwquality,
  common_dep,
]

gkr_vala_args = [
  '--gresources', resources_xml,
  '--vapidir', meson.current_source_dir(),
  '--pkg=pwquality',
]

gkr_lib = static_library('seahorse-gkr',
  gkr_sources,
  vala_args: gkr_vala_args,
  dependencies: gkr_dependencies,
)

gkr_dep = declare_dependency(
  link_with: gkr_lib,
  include_directories: include_directories('.'),
)
