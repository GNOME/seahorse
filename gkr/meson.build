gkr_sources = [
  'gkr-backend.vala',
  'gkr-dialogs.vala',
  'gkr-module.vala',
  'gkr-item.vala',
  'gkr-item-add.vala',
  'gkr-item-properties.vala',
  'gkr-keyring.vala',
  'gkr-keyring-add.vala',
  'gkr-keyring-properties.vala',
]

gkr_dependencies = [
  glib_deps,
  gtk,
  gcr,
  gcr_ui,
  libsecret,
  config,
  common_dep,
]

gkr_lib = static_library('seahorse-gkr',
  gkr_sources,
  dependencies: gkr_dependencies,
)
