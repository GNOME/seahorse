project('seahorse', ['vala', 'c'],
  version: '47.0.1',
  meson_version: '>= 1.2.0',
  license: 'GPL-2.0-or-later',
)

gnome = import('gnome')
i18n = import('i18n')

# Add our custom VAPI dir
add_project_arguments(
  ['--vapidir', meson.project_source_root() / 'vapi'],
  language: 'vala',
)

# Some variables
cc = meson.get_compiler('c')
valac = meson.get_compiler('vala')
buildtype = get_option('buildtype')

seahorse_prefix = get_option('prefix')
po_dir = meson.current_source_dir() / 'po'
libexecbindir = seahorse_prefix / get_option('libexecdir') / meson.project_name()

# Application ID
if get_option('profile') == 'default'
  application_id = 'org.gnome.Seahorse'
else
  application_id = 'org.gnome.Seahorse.Devel'
endif

# Dependencies
min_glib_version = '2.82'
min_gcr_version = '4.3.90'
accepted_gpg_versions = [ '2.4.0', '2.5.0' ]
gpg_check = find_program('build-aux' / 'gpg_check.py')

glib_deps = [
  dependency('glib-2.0',    version: '>=' + min_glib_version),
  dependency('gobject-2.0', version: '>=' + min_glib_version),
  dependency('gio-2.0',     version: '>=' + min_glib_version),
  dependency('gio-unix-2.0',version: '>=' + min_glib_version),
  dependency('gmodule-2.0', version: '>=' + min_glib_version),
]
gtk4_dep = dependency('gtk4', version: '>= 4.16')
libadwaita_dep = dependency('libadwaita-1', version: '>= 1.5')
gcr4_dep = dependency('gcr-4', version: '>=' + min_gcr_version)
libsecret = dependency('libsecret-1', version: '>= 0.21')
libpwquality = dependency('pwquality')
posix = valac.find_library('posix')
ssh_bin = find_program('ssh')
ssh_keygen = find_program('ssh-keygen')
pkcs11_dep = valac.find_library('pkcs11',
  dirs: meson.project_source_root() / 'vapi',
)
libqrencode_dep = dependency('libqrencode', version: '>= 4.1.1')
gpg_bin = find_program('gpg2', 'gpg', required: get_option('pgp-support'))
gpgme_dep = dependency('gpgme', version: '>= 1.14.0', required: get_option('pgp-support'))

if get_option('pgp-support')
  gpg_version_check = run_command(
    [ gpg_check, 'version', gpg_bin.full_path(), accepted_gpg_versions ],
    check: false,
  )
  # GnuPG now outputs e.g. 2.3.4-unknown so we need to drop this extra cruft
  gpg_version_raw = gpg_version_check.stdout()
  gpg_version_raw_split = gpg_version_raw.split('-')
  gpg_version = gpg_version_raw_split[0]
  message('GnuPG Version: @0@'.format(gpg_version))
  if get_option('check-compatible-gpg') and gpg_version_check.returncode() != 0
    error('Incompatible version of GnuPG. Accepted versions are: @0@'.format(accepted_gpg_versions))
  endif

  gpg_check_pubkey_algos = run_command(
    [ gpg_check, 'pubkey-algos', gpg_bin.full_path() ],
    check: true,
  ).stdout().strip()
  message(f'GnuPG supported pubkey algos: @gpg_check_pubkey_algos@')
  gpg_pubkey_algos = gpg_check_pubkey_algos.split(',')
endif

if get_option('ldap-support')
  libldap = cc.find_library('ldap')
  liblber = cc.find_library('lber')

  add_project_arguments('-D', 'WITH_LDAP', language: 'vala')
endif

if get_option('hkp-support')
  add_project_arguments('-D', 'WITH_HKP', language: 'vala')
endif
libsoup = dependency('libsoup-3.0', required: get_option('hkp-support'))
avahi_client = dependency('avahi-client', required: get_option('key-sharing'))
avahi_glib = dependency('avahi-glib', version: '>= 0.6', required: get_option('key-sharing'))

if get_option('key-sharing')
  message('NOTE: key-sharing over DNS-SD will possibly be removed in the future')
endif


# Project-wide flags
add_project_arguments([
    '--target-glib=@0@'.format(min_glib_version),
  ],
  language: 'vala',
)
add_project_arguments([
    '-DGCR_API_SUBJECT_TO_CHANGE',
    '-DGCK_API_SUBJECT_TO_CHANGE',
    '-DSECRET_WITH_UNSTABLE',
    '-include', 'config.h',
    # Don't care about unused parameters (which can happen a lot with callbacks)
    '-Wno-unused-parameter',
    '-Wno-missing-field-initializers',
  ],
  language: 'c',
)

# With the latest GCC, Vala is generating a lot of warning, with no solution in sight...
# We should only apply this to the pieces that are Vala-only thoughh, since we
# still want to have warnings for our C code.
vala_workaround_cflags = [ '-w' ]

# configuration
conf = configuration_data()

conf.set_quoted('VERSION', '@VCS_TAG@')
conf.set_quoted('PACKAGE', meson.project_name())
conf.set_quoted('PACKAGE_NAME', meson.project_name())
conf.set_quoted('PACKAGE_VERSION', '@VCS_TAG@')
conf.set_quoted('PACKAGE_STRING', '@0@ @1@'.format(meson.project_name(), meson.project_version()))
conf.set_quoted('PKGDATADIR', get_option('datadir') / meson.project_name())
conf.set_quoted('EXECDIR', libexecbindir)
conf.set_quoted('LOCALEDIR', seahorse_prefix / get_option('localedir'))
conf.set_quoted('APPLICATION_ID', application_id)
conf.set_quoted('PROFILE', get_option('profile'))
conf.set_quoted('GETTEXT_PACKAGE', meson.project_name())
conf.set_quoted('G_LOG_DOMAIN', meson.project_name())
conf.set('WITH_DEBUG', buildtype.contains('debug'))
conf.set10('_DEBUG', buildtype.contains('debug'))
conf.set('HAVE_STRSEP', cc.has_function('strsep'))
conf.set('WITH_PGP', get_option('pgp-support'))
conf.set('WITH_PKCS11', get_option('pkcs11-support'))
conf.set('WITH_LDAP', get_option('ldap-support'))
conf.set('WITH_HKP', get_option('hkp-support'))
conf.set('WITH_KEYSERVER', get_option('keyservers-support'))
conf.set('WITH_SHARING', get_option('key-sharing'))
conf.set_quoted('SSH_KEYGEN_PATH', ssh_keygen.full_path())
conf.set_quoted('SSH_PATH', ssh_bin.full_path())
if get_option('pgp-support')
  conf.set_quoted('GNUPG', gpg_bin.full_path())
  gpg_version_split = gpg_version.split('.')
  conf.set('GPG_MAJOR', gpg_version_split[0])
  conf.set('GPG_MINOR', gpg_version_split[1])
  conf.set('GPG_MICRO', gpg_version_split[2])
  foreach _algo : gpg_pubkey_algos
    conf.set(f'GPG_HAS_PUBKEY_ALGO_@_algo@', true)
  endforeach
endif

config_file = vcs_tag(
  input: configure_file(
    output: 'config.h.in',
    configuration: conf
  ),
  output: 'config.h'
)

config = declare_dependency(
  sources: config_file,
  dependencies: [
    valac.find_library('config', dirs: meson.current_source_dir() / 'common'),
  ],
  include_directories: include_directories('.'),
)

# Post-install scripts
gnome.post_install(
  gtk_update_icon_cache: true,
  glib_compile_schemas: true,
  update_desktop_database: true,
)

# subdirs
subdir('po')
subdir('data')
if get_option('help')
  subdir('help')
endif
subdir('common')
subdir('libseahorse')
subdir('gkr')
subdir('ssh')
if get_option('pgp-support')
  subdir('pgp')
endif
if get_option('pkcs11-support')
  subdir('pkcs11')
endif
subdir('src')
