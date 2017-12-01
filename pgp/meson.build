pgp_sources = [
  'seahorse-combo-keys.c',
  'seahorse-discovery.c',
  'seahorse-gpgme.c',
  'seahorse-gpgme-add-subkey.c',
  'seahorse-gpgme-add-uid.c',
  'seahorse-gpgme-data.c',
  'seahorse-gpgme-expires.c',
  'seahorse-gpgme-exporter.c',
  'seahorse-gpgme-generate.c',
  'gpgme-key.vala',
  'seahorse-gpgme-key-deleter.c',
  'seahorse-gpgme-key-op.c',
  'seahorse-gpgme-keyring.c',
  'gpgme-photo.vala',
  'seahorse-gpgme-photos.c',
  'seahorse-gpgme-revoke.c',
  'seahorse-gpgme-secret-deleter.c',
  'seahorse-gpgme-sign.c',
  'gpgme-subkey.vala',
  'gpgme-uid.vala',
  'seahorse-gpg-op.c',
  'seahorse-gpg-options.c',
  'seahorse-pgp.c',
  'seahorse-pgp-actions.c',
  'seahorse-pgp-backend.c',
  'pgp-key.vala',
  'seahorse-pgp-key-properties.c',
  'seahorse-pgp-keysets.c',
  'pgp-photo.vala',
  'pgp-signature.vala',
  'pgp-subkey.vala',
  'pgp-uid.vala',
  'seahorse-signer.c',
  'seahorse-transfer.c',
  'seahorse-unknown.c',
  'seahorse-unknown-source.c',
]

# Also use our custom VAPI's
gpg_vapis = [
  valac.find_library('gpgme', dirs: join_paths(meson.source_root(), 'vapi')),
  valac.find_library('gpg-error', dirs: join_paths(meson.source_root(), 'vapi')),
]

pgp_dependencies = [
  glib_deps,
  gcr,
  gpgme,
  gtk,
  config,
  common_dep,
  gpg_vapis,
]

pgp_c_flags = [
]

if with_ldap
  pgp_sources += 'seahorse-ldap-source.c'
  pgp_dependencies += [
    libldap,
    liblber,
  ]
  pgp_c_flags += '-DLDAP_DEPRECATED'
endif

if with_hkp
  pgp_sources += 'seahorse-hkp-source.c'
  pgp_dependencies += libsoup
endif

if with_keyservers
  pgp_sources += [
    'seahorse-server-source.c',
    'seahorse-keyserver-search.c',
    'seahorse-keyserver-sync.c',
    'seahorse-keyserver-results.c',
  ]
endif

if with_key_sharing
  pgp_dependencies += [
    avahi_client,
    avahi_gobject,
  ]
endif

pgp_lib = static_library('seahorse-pgp',
  pgp_sources,
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
)
