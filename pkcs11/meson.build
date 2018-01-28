pkcs11_sources = [
  'certificate-der-exporter.vala',
  'pkcs11-certificate.vala',
  'pkcs11-deleter.vala',
  'pkcs11-key-deleter.vala',
  'pkcs11-module.vala',
  'pkcs11-private-key.vala',
  'pkcs11-properties.vala',
  'pkcs11-request.vala',
  'pkcs11-token.vala',

  'seahorse-pkcs11-backend.c',
  'seahorse-pkcs11-generate.c',
]

pkcs11_deps = [
  glib_deps,
  gcr_ui,
  pkcs11_dep,
  common_dep,
]

pkcs11_lib = static_library('seahorse-pkcs11',
  pkcs11_sources,
  dependencies: pkcs11_deps,
)
