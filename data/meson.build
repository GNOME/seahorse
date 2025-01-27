subdir('icons')

# GSettings
gsettings_files = [
  'org.gnome.seahorse.gschema.xml',
  'org.gnome.seahorse.manager.gschema.xml',
  'org.gnome.seahorse.window.gschema.xml',
]
install_data(gsettings_files,
  install_dir: get_option('datadir') / 'glib-2.0' / 'schemas',
)

# Resources
resources_xml = meson.project_source_root() / 'data' / 'seahorse.gresource.xml'
resources_src = gnome.compile_resources('seahorse-resources',
  resources_xml,
)

# The metainfo file
metainfo_file = i18n.merge_file(
  input: configure_file(
    input: 'org.gnome.Seahorse.metainfo.xml.in.in',
    output: '@BASENAME@',
    configuration: {
      'appid': application_id,
    },
  ),
  output: '@0@.metainfo.xml'.format(application_id),
  type: 'xml',
  po_dir: po_dir,
  install: true,
  install_dir: get_option('datadir') / 'metainfo',
)

appstreamcli = find_program('appstreamcli', required: false)
if appstreamcli.found()
  test('validate-metainfo', appstreamcli,
    suite: 'data',
    args: [
      'validate', '--no-net', '--explain', metainfo_file
    ],
    depends: metainfo_file,
  )
endif

# The desktop file
desktop_file = i18n.merge_file(
  input: configure_file(
    input: 'org.gnome.Seahorse.desktop.in.in',
    output: '@BASENAME@',
    configuration: {
      'bindir': seahorse_prefix / get_option('bindir'),
      'icon': application_id,
    },
  ),
  output: '@0@.desktop'.format(application_id),
  type: 'desktop',
  po_dir: po_dir,
  install: true,
  install_dir: get_option('datadir') / 'applications',
)

desktop_file_validate = find_program ('desktop-file-validate', required: false)
if desktop_file_validate.found()
  test('validate-desktop',
    desktop_file_validate,
    suite: 'data',
    args: [
      desktop_file.full_path()
    ],
    depends: [
      desktop_file,
    ],
  )
endif

# The service file
configure_file(
  input: 'org.gnome.Seahorse.service.in',
  output: '@0@.service'.format(application_id),
  configuration: {
    'bindir': seahorse_prefix / get_option('bindir'),
    'application_id': application_id,
  },
  install_dir: get_option('datadir') / 'dbus-1' / 'services',
)

# The search provider
install_data(
  'seahorse-search-provider.ini',
  install_dir: get_option('datadir') / 'gnome-shell' / 'search-providers',
)

# manpage
if get_option('manpage')
  xsltproc = find_program('xsltproc')
  custom_target('manfile-seahorse',
    input: 'seahorse.xml',
    output: 'seahorse.1',
    install: true,
    install_dir: get_option('mandir') / 'man1',
    command: [
      xsltproc,
      '--nonet',
      '--stringparam', 'man.output.quietly', '1',
      '--stringparam', 'funcsynopsis.style', 'ansi',
      '--stringparam', 'man.th.extra1.suppress', '1',
      '--stringparam', 'man.authors.section.enabled', '0',
      '--stringparam', 'man.copyright.section.enabled', '0',
      '-o', '@OUTPUT@',
      'http://docbook.sourceforge.net/release/xsl/current/manpages/docbook.xsl',
      '@INPUT@'
    ]
  )
endif
