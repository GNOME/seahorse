ssh_sources = files(
  'actions.vala',
  'algorithm.vala',
  'backend.vala',
  'deleter.vala',
  'errors.vala',
  'exporter.vala',
  'generate.vala',
  'key-data.vala',
  'key-length-chooser.vala',
  'key-properties.vala',
  'key.vala',
  'operation.vala',
  'source.vala',
  'ssh.vala',
  'upload.vala',
)

ssh_dependencies = [
  glib_deps,
  gcr,
  gcr_ui,
  posix,
  gtk,
  common_dep,
]

ssh_vala_args = [
  '--gresources', resources_xml,
]

ssh_lib = static_library('seahorse-ssh',
  ssh_sources,
  vala_args: ssh_vala_args,
  dependencies: ssh_dependencies,
)

ssh_dep = declare_dependency(
  link_with: ssh_lib,
  include_directories: include_directories('.'),
)

# ssh-askpass helper binary
ssh_askpass_sources = files(
  'seahorse-ssh-askpass.c',
)

ssh_askpass_dependencies = [
  gcr,
  gtk,
  config,
  common_dep,
]

ssh_askpass = executable('ssh-askpass',
  ssh_askpass_sources,
  dependencies: ssh_askpass_dependencies,
  install: true,
  install_dir: libexecbindir,
)
