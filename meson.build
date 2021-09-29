project('seahorse', ['vala', 'c'],
  version: '41.0',
  meson_version: '>= 0.51',
  license: 'GPL2+',
)

gnome = import('gnome')
i18n = import('i18n')

# Some variables
cc = meson.get_compiler('c')
valac = meson.get_compiler('vala')
buildtype = get_option('buildtype')

seahorse_prefix = get_option('prefix')
po_dir = meson.source_root() / 'po'
libexecbindir = seahorse_prefix / get_option('libexecdir') / meson.project_name()

# Application ID
if get_option('profile') == 'default'
  application_id = 'org.gnome.seahorse.Application'
else
  application_id = 'org.gnome.seahorse.ApplicationDevel'
endif

# Dependencies
min_glib_version = '2.66'
min_gcr_version = '3.38'
accepted_gpg_versions = [ '2.2.0', '2.3.0' ]
gpg_check_version = find_program('build-aux' / 'gpg_check_version.py')

glib_deps = [
  dependency('glib-2.0',    version: '>=' + min_glib_version),
  dependency('gobject-2.0', version: '>=' + min_glib_version),
  dependency('gio-2.0',     version: '>=' + min_glib_version),
  dependency('gio-unix-2.0',version: '>=' + min_glib_version),
  dependency('gmodule-2.0', version: '>=' + min_glib_version),
]
gtk = dependency('gtk+-3.0', version: '>= 3.24.0')
libhandy_dep = dependency('libhandy-1', version: '>= 1.1.0')
gcr = dependency('gcr-3',       version: '>=' + min_gcr_version)
gcr_ui = dependency('gcr-ui-3', version: '>=' + min_gcr_version)
libsecret = dependency('libsecret-1', version: '>= 0.16')
libpwquality = dependency('pwquality')
posix = valac.find_library('posix')
ssh_bin = find_program('ssh')
ssh_keygen = find_program('ssh-keygen')
gpg_bin = find_program('gpg2', 'gpg', required: get_option('pgp-support'))
gpgme_dep = dependency('gpgme', version: '>= 1.14.0', required: get_option('pgp-support'))

if get_option('pgp-support')
  gpg_version_check = run_command([gpg_check_version, gpg_bin.path(), accepted_gpg_versions ])
  gpg_version = gpg_version_check.stdout()
  message('GnuPG Version: @0@'.format(gpg_version))
  if get_option('check-compatible-gpg') and gpg_version_check.returncode() != 0
    error('Incompatible version of GnuPG. Accepted versions are: @0@'.format(accepted_gpg_versions))
  endif
endif

pkcs11_dep = valac.find_library('pkcs11', required: get_option('pkcs11-support'))
if get_option('pkcs11-support') and not pkcs11_dep.found()
  error('Required library "pkcs11" not found (needed for PKCS#11 support)')
endif

if get_option('ldap-support')
  libldap = cc.find_library('ldap')
  liblber = cc.find_library('lber')

  add_project_arguments('-D', 'WITH_LDAP', language: 'vala')
endif

if get_option('hkp-support')
  add_project_arguments('-D', 'WITH_HKP', language: 'vala')
endif
libsoup = dependency('libsoup-2.4', version: '>= 2.33.92', required: get_option('hkp-support'))
avahi_client = dependency('avahi-client', required: get_option('key-sharing'))
avahi_glib = dependency('avahi-glib', version: '>= 0.6', required: get_option('key-sharing'))


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
conf.set_quoted('SSH_KEYGEN_PATH', ssh_keygen.path())
conf.set_quoted('SSH_PATH', ssh_bin.path())
if get_option('pgp-support')
  conf.set_quoted('GNUPG', gpg_bin.path())
  gpg_version_split = gpg_version.split('.')
  conf.set('GPG_MAJOR', gpg_version_split[0])
  conf.set('GPG_MINOR', gpg_version_split[1])
  conf.set('GPG_MICRO', gpg_version_split[2])
endif

vcs_tag(
  input: configure_file(
    output: 'config.h.in',
    configuration: conf
  ),
  output: 'config.h'
)

config_file = configure_file(
  output: 'config.h',
  configuration: conf,
)

config = declare_dependency(
  sources: config_file,
  dependencies: [
    valac.find_library('config', dirs: meson.source_root() / 'common'),
  ],
  include_directories: include_directories('.'),
)

# Post-install scripts
meson.add_install_script('build-aux' / 'meson_post_install.py')

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
