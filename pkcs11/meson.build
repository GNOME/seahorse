pkcs11_sources = files(
  'certificate-der-export-operation.vala',
  'pkcs11-certificate.vala',
  'pkcs11-certificate-widget.vala',
  'pkcs11-cert-key-pair.vala',
  'pkcs11-cert-key-pair-panel.vala',
  'pkcs11-delete-operation.vala',
  'pkcs11-generate.vala',
  'pkcs11-module.vala',
  'pkcs11-private-key.vala',
  'pkcs11-private-key-widget.vala',
  'pkcs11-request-certificate-dialog.vala',
  'pkcs11-token.vala',
  'pkcs11-token-filter.vala',
  'pkcs11-token-panel.vala',

  'seahorse-pkcs11-backend.c',
)

pkcs11_deps = [
  glib_deps,
  common_dep,
  pkcs11_dep,
]

pkcs11_vala_args = [
  '--gresources', resources_xml,
]

pkcs11_c_args = [
  vala_workaround_cflags,
]

pkcs11_lib = static_library('seahorse-pkcs11',
  pkcs11_sources,
  dependencies: pkcs11_deps,
  vala_args: pkcs11_vala_args,
  c_args: pkcs11_c_args,
)

pkcs11_dep = declare_dependency(
  link_with: pkcs11_lib,
  include_directories: include_directories('.'),
)
