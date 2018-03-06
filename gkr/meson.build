gkr_sources = [
  'gkr-backend.vala',
  'gkr-dialogs.vala',
  'gkr-item-add.vala',
  'gkr-item-properties.vala',
  'gkr-item.vala',
  'gkr-keyring-add.vala',
  'gkr-keyring-properties.vala',
  'gkr-keyring.vala',
  'gkr-module.vala',
  'gkr-password-entry.vala',
]

gkr_dependencies = [
  glib_deps,
  gtk,
  gcr,
  gcr_ui,
  libsecret,
  common_dep,
]

gkr_lib = static_library('seahorse-gkr',
  gkr_sources,
  dependencies: gkr_dependencies,
)

gkr_dep = declare_dependency(
  link_with: gkr_lib,
  include_directories: include_directories('.'),
)
