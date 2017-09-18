subdir('icons')

# GSettings
gsettings_files = [
  'org.gnome.seahorse.gschema.xml',
  'org.gnome.seahorse.manager.gschema.xml',
  'org.gnome.seahorse.window.gschema.xml',
]
install_data(gsettings_files,
  install_dir: join_paths(datadir, 'glib-2.0', 'schemas'),
)

# Resources
resources_src = gnome.compile_resources('seahorse-resources',
  'seahorse.gresource.xml',
  c_name: 'seahorse',
  extra_args: [
    '--manual-register',
  ]
)

# The appdata file
i18n.merge_file(
  input: 'seahorse.appdata.xml.in',
  output: 'seahorse.appdata.xml',
  type: 'xml',
  po_dir: po_dir,
  install: true,
  install_dir: join_paths(datadir, 'metainfo')
)

# The configuration for the desktop and service files
desktop_conf = configuration_data()
desktop_conf.set('bindir', bindir)
desktop_conf.set('VERSION', meson.project_version())

# The desktop file
configured_desktop_file = configure_file(
  input: 'seahorse.desktop.in.in',
  output: 'seahorse.desktop.in',
  configuration: desktop_conf,
)
i18n.merge_file(
  input: configured_desktop_file,
  output: 'seahorse.desktop',
  type: 'desktop',
  po_dir: po_dir,
  install: true,
  install_dir: join_paths(datadir, 'applications')
)

# The service file
configure_file(
  input: 'org.gnome.seahorse.Application.service.in',
  output: 'org.gnome.seahorse.Application.service',
  configuration: desktop_conf,
  install: true,
  install_dir: join_paths(datadir, 'dbus-1', 'services'),
)

# The search provider
install_data(
  'seahorse-search-provider.ini',
  install_dir: join_paths(datadir, 'gnome-shell', 'search-providers')
)
