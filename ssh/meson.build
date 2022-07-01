ssh_sources = files(
  'actions.vala',
  'algorithm.vala',
  'backend.vala',
  'errors.vala',
  'generate-key-dialog.vala',
  'key-data.vala',
  'key-delete-operation.vala',
  'key-export-operation.vala',
  'key-length-chooser.vala',
  'key-panel.vala',
  'key.vala',
  'operation.vala',
  'source.vala',
  'ssh.vala',
  'upload-remote-dialog.vala',
)

ssh_dependencies = [
  glib_deps,
  gcr4_dep,
  posix,
  gtk4_dep,
  common_dep,
  libseahorse_dep,
]

ssh_vala_args = [
  '--gresources', resources_xml,
]

ssh_c_args = [
  vala_workaround_cflags,
]

ssh_lib = static_library('seahorse-ssh',
  ssh_sources,
  vala_args: ssh_vala_args,
  c_args: ssh_c_args,
  dependencies: ssh_dependencies,
)

ssh_dep = declare_dependency(
  link_with: ssh_lib,
  include_directories: include_directories('.'),
)

# ssh-askpass helper binary
ssh_askpass_sources = files(
  'seahorse-ssh-askpass.vala',
)

ssh_askpass_dependencies = [
  gcr4_dep,
  gtk4_dep,
  config,
  common_dep,
  posix,
]

ssh_askpass = executable('ssh-askpass',
  [ ssh_askpass_sources, resources_src, ],
  dependencies: ssh_askpass_dependencies,
  install: true,
  install_dir: libexecbindir,
)

# Tests
ssh_test_names = [
  'key-parse',
]

foreach _test : ssh_test_names
  test_bin = executable(_test,
    files('test-@0@.vala'.format(_test)),
    c_args: vala_workaround_cflags,
    dependencies: [
      ssh_dep,
      ssh_dependencies,
    ],
    include_directories: include_directories('..'),
  )

  test(_test, test_bin,
    suite: 'ssh',
  )
endforeach
