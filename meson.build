project('seahorse', ['vala', 'c'],
  version: '3.20.0',
  meson_version: '>= 0.42',
  license: 'GPL2+',
)

gnome = import('gnome')
i18n = import('i18n')

# Options
with_pgp = get_option('pgp-support')
check_compatible_gpg = get_option('check-compatible-gpg')
with_pkcs11 = get_option('pkcs11-support')
with_hkp = get_option('hkp-support')
with_ldap = get_option('ldap-support')
with_keyservers = get_option('keyservers-support')
with_key_sharing = get_option('key-sharing')

# Some variables
cc = meson.get_compiler('c')
valac = meson.get_compiler('vala')
buildtype = get_option('buildtype')

seahorse_prefix = get_option('prefix')
po_dir = join_paths(meson.source_root(), 'po')
datadir = join_paths(seahorse_prefix, get_option('datadir'))
bindir = join_paths(seahorse_prefix, get_option('bindir'))
pkgdatadir = join_paths(datadir, meson.project_name())
libexecbindir = join_paths(seahorse_prefix, get_option('libexecdir'), meson.project_name())
localedir = join_paths(seahorse_prefix, get_option('localedir'))

# Dependencies
min_glib_version = '2.44'
min_gcr_version = '3.11.91'
min_gpgme_version = '1.7.0'
accepted_gpg_versions = [ '2.1.13', '2.2.0' ]
gpg_check_version = find_program(join_paths('meson', 'gpg_check_version.py'))

glib_deps = [
  dependency('glib-2.0',    version: '>=' + min_glib_version),
  dependency('gobject-2.0', version: '>=' + min_glib_version),
  dependency('gio-2.0',     version: '>=' + min_glib_version),
  dependency('gmodule-2.0', version: '>=' + min_glib_version),
]
gtk = dependency('gtk+-3.0', version: '>= 3.4.0')
gcr = dependency('gcr-3',       version: '>=' + min_gcr_version)
gcr_ui = dependency('gcr-ui-3', version: '>=' + min_gcr_version)
libsecret = dependency('libsecret-1', version: '>= 0.16')
posix = valac.find_library('posix')
ssh_bin = find_program('ssh')
ssh_keygen = find_program('ssh-keygen')
gpg_bin = find_program('gpg2', 'gpg', required: with_pgp)
gpgme = cc.find_library('gpgme', required: with_pgp)
gpgme_config = find_program('gpgme-config', required: with_pgp)

if check_compatible_gpg
  gpg_version_check = run_command([gpg_check_version, gpg_bin.path(), 'false', accepted_gpg_versions ])
  gpg_version = gpg_version_check.stdout()
  message('GnuPG Version: @0@'.format(gpg_version))
  if gpg_version_check.returncode() != 0
    error('Incompatible version of GnuPG. Accepted versions are: @0@'.format(accepted_gpg_versions))
  endif

  gpgme_version_check = run_command([gpg_check_version, gpgme_config.path(), 'true', min_gpgme_version ])
  gpgme_version = gpgme_version_check.stdout()
  message('GPGME version: @0@'.format(gpgme_version))
  if gpgme_version_check.returncode() != 0
    error('Incompatible version of GPGME. Minimal version required is @0@'.format(min_gpgme_version))
  endif
endif

pkcs11_dep = valac.find_library('pkcs11', required: with_pkcs11)
if with_pkcs11 and not pkcs11_dep.found()
  error('Required library "pkcs11" not found (needed for PKCS#11 support)')
endif

if with_ldap
  libldap = cc.find_library('ldap')
  liblber = cc.find_library('lber')
  if not libldap.found()
    error('Required library "ldap" not found (needed for LDAP support)')
  elif not liblber.found()
    error('Required library "lber" not found (needed for LDAP support)')
  endif
endif

libsoup = dependency('libsoup-2.4', version: '>= 2.33.92', required: with_hkp)
avahi_client = dependency('avahi-client', required: with_key_sharing)
avahi_gobject = dependency('avahi-gobject', version: '>= 0.6', required: with_key_sharing)


# Project-wide flags
add_project_arguments([
    '--target-glib=@0@'.format(min_glib_version),
  ],
  language: 'vala',
)
add_project_arguments([
    '-DGCR_API_SUBJECT_TO_CHANGE',
    '-DGCK_API_SUBJECT_TO_CHANGE',
    '-include', 'config.h',
  ],
  language: 'c',
)

# Use more warnings
warning_flags = [
  '-Wmissing-declarations',
  '-Wmissing-prototypes',
  '-Wnested-externs',
  '-Wpointer-arith',
  '-Wcast-align',
  '-Wsign-compare',
  '-Wno-deprecated-declarations',
  '-Wno-strict-aliasing',
  '-Wno-sign-compare',
]
supported_warning_flags = []
foreach flag : warning_flags
  if cc.has_argument(flag)
    supported_warning_flags += flag
  endif
endforeach
add_project_arguments(supported_warning_flags,
  language: 'c',
)


# configuration
conf = configuration_data()

conf.set_quoted('VERSION', meson.project_version())
conf.set_quoted('PACKAGE', meson.project_name())
conf.set_quoted('PACKAGE_NAME', meson.project_name())
conf.set_quoted('PACKAGE_VERSION', meson.project_version())
conf.set_quoted('PACKAGE_STRING', '@0@ @1@'.format(meson.project_name(), meson.project_version()))
conf.set_quoted('PKGDATADIR', pkgdatadir)
conf.set_quoted('EXECDIR', libexecbindir)
conf.set_quoted('LOCALEDIR', localedir)
conf.set_quoted('GETTEXT_PACKAGE', meson.project_name())
conf.set_quoted('G_LOG_DOMAIN', meson.project_name())
conf.set10('WITH_DEBUG', buildtype.contains('debug'))
conf.set10('_DEBUG', buildtype.contains('debug'))
conf.set('HAVE_STRSEP', cc.has_function('strsep'))
conf.set('WITH_PGP', with_pgp)
conf.set('WITH_PKCS11', with_pkcs11)
conf.set('WITH_LDAP', with_ldap)
conf.set('WITH_HKP', with_hkp)
conf.set('WITH_SOUP', with_hkp)
conf.set('WITH_KEYSERVER', with_keyservers)
conf.set('WITH_SHARING', with_key_sharing)
conf.set_quoted('SSH_KEYGEN_PATH', ssh_keygen.path())
conf.set_quoted('SSH_PATH', ssh_bin.path())
if with_pgp
  conf.set_quoted('GNUPG', gpg_bin.path())
  gpg_version_split = gpg_version.split('.')
  conf.set('GPG_MAJOR', gpg_version_split[0])
  conf.set('GPG_MINOR', gpg_version_split[1])
  conf.set('GPG_MICRO', gpg_version_split[2])
endif

config_file = configure_file(
  output: 'config.h',
  configuration: conf,
)

config = declare_dependency(
  sources: config_file,
  dependencies: [
    valac.find_library('config', dirs: join_paths(meson.source_root(), 'common')),
  ],
  include_directories: include_directories('.'),
)

# Post-install scripts
meson.add_install_script(join_paths('meson', 'post_install.py'))

# subdirs
subdir('po')
subdir('data')
subdir('help')
subdir('common')
subdir('libegg')
subdir('gkr')
subdir('ssh')
if with_pgp
  subdir('pgp')
endif
if with_pkcs11
  subdir('pkcs11')
endif
subdir('libseahorse')
subdir('src')
