pkcs11_sources = files(
  'certificate-der-exporter.vala',
  'pkcs11-certificate.vala',
  'pkcs11-deleter.vala',
  'pkcs11-generate.vala',
  'pkcs11-key-deleter.vala',
  'pkcs11-module.vala',
  'pkcs11-private-key.vala',
  'pkcs11-properties.vala',
  'pkcs11-request.vala',
  'pkcs11-token.vala',

  'cryptoki.vapi',
  'seahorse-pkcs11-backend.c',
)

pkcs11_deps = [
  glib_deps,
  gcr_ui,
  pkcs11_dep,
  common_dep,
]

pkcs11_vala_args = [
  '--gresources', resources_xml,
]

pkcs11_lib = static_library('seahorse-pkcs11',
  pkcs11_sources,
  dependencies: pkcs11_deps,
  vala_args: pkcs11_vala_args,
)

pkcs11_dep = declare_dependency(
  link_with: pkcs11_lib,
  include_directories: include_directories('.'),
)
