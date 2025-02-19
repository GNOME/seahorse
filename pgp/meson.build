pgp_enums = gnome.mkenums_simple('seahorse-pgp-enums',
  sources: [
    'seahorse-gpgme-key-gen-type.h',
    'seahorse-gpgme-key-op.h',
    'seahorse-pgp-key-algorithm.h',
    'seahorse-pgp-subkey-usage.h',
  ],
)

pgp_sources = files(
  'seahorse-discovery.c',
  'seahorse-gpgme.c',
  'seahorse-gpgme-add-subkey.c',
  'seahorse-gpgme-add-uid.c',
  'seahorse-gpgme-data.c',
  'seahorse-gpgme-expires-dialog.c',
  'seahorse-gpgme-generate-dialog.c',
  'seahorse-gpgme-key.c',
  'seahorse-gpgme-key-delete-operation.c',
  'seahorse-gpgme-key-export-operation.c',
  'seahorse-gpgme-key-gen-type.c',
  'seahorse-gpgme-key-op.c',
  'seahorse-gpgme-key-parms.c',
  'seahorse-gpgme-keyring.c',
  'seahorse-gpgme-keyring-panel.c',
  'seahorse-gpgme-photo.c',
  'seahorse-gpgme-photos.c',
  'seahorse-gpgme-revoke-dialog.c',
  'seahorse-gpgme-sign-dialog.c',
  'seahorse-gpgme-subkey-delete-operation.c',
  'seahorse-gpgme-subkey.c',
  'seahorse-gpgme-uid-delete-operation.c',
  'seahorse-gpgme-uid.c',
  'seahorse-gpg-op.c',
  'seahorse-pgp-actions.c',
  'seahorse-pgp-backend.c',
  'seahorse-pgp-key.c',
  'seahorse-pgp-key-algorithm.c',
  'seahorse-pgp-key-panel.c',
  'seahorse-pgp-keysets.c',
  'seahorse-pgp-photo.c',
  'seahorse-pgp-photos-widget.c',
  'seahorse-pgp-signature.c',
  'seahorse-pgp-subkey.c',
  'seahorse-pgp-subkey-list-box.c',
  'seahorse-pgp-subkey-usage.c',
  'seahorse-pgp-uid.c',
  'seahorse-pgp-uid-list-box.c',
  'seahorse-transfer.c',
  'seahorse-unknown.c',
  'seahorse-unknown-source.c',
)

pgp_dependencies = [
  config,
  glib_deps,
  gcr4_dep,
  gpgme_dep,
  common_dep,
  libseahorse_dep,
]

pgp_c_flags = [
]

if get_option('ldap-support')
  pgp_sources = [ pgp_sources, 'seahorse-ldap-source.c' ]
  pgp_dependencies += [
    libldap,
    liblber,
  ]
  pgp_c_flags += '-DLDAP_DEPRECATED'
endif

if get_option('hkp-support')
  pgp_sources += [ pgp_sources, 'seahorse-hkp-source.c' ]
  pgp_dependencies += libsoup
endif

if get_option('keyservers-support')
  pgp_sources = [
    pgp_sources,
    'seahorse-server-source.c',
    'seahorse-keyserver-search.c',
    'seahorse-keyserver-sync.c',
  ]
endif

if get_option('key-sharing')
  pgp_dependencies += [
    avahi_client,
    avahi_glib,
  ]
endif

pgp_lib = static_library('seahorse-pgp',
  pgp_sources, pgp_enums,
  dependencies: pgp_dependencies,
  c_args: pgp_c_flags,
)

pgp_dep = declare_dependency(
  link_with: pgp_lib,
  include_directories: include_directories('.'),
)

# xloadimage helper binary
xloadimage = executable('xloadimage',
  'seahorse-xloadimage.c',
  install: true,
  install_dir: libexecbindir,
  dependencies: config,
)


# Tests
test_names = [
  'gpgme-backend',
]

if get_option('hkp-support')
  test_names += 'hkp-source'
endif

if get_option('ldap-support')
  test_names += 'ldap-source'
endif

foreach _test : test_names
  test_bin = executable(_test,
    files('test-@0@.c'.format(_test)),
    dependencies: [
      pgp_dep,
      pgp_dependencies,
    ],
    include_directories: include_directories('..'),
  )

  test(_test, test_bin,
    suite: 'pgp',
  )
endforeach
