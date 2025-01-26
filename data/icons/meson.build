icons_install_dir = get_option('datadir') / 'icons'

# special case for symbolic
icon_subfolder = 'hicolor' / 'symbolic' / 'apps'
install_data(icon_subfolder / 'org.gnome.Seahorse-symbolic.svg',
  rename: '@0@-symbolic.svg'.format(application_id),
  install_dir: icons_install_dir / icon_subfolder,
)
icon_subfolder = 'hicolor' / 'scalable' / 'apps'
install_data(icon_subfolder / '@0@.svg'.format(application_id),
  install_dir: icons_install_dir / icon_subfolder,
)

# private icons
private_icons_install_dir = get_option('datadir') / meson.project_name() / 'icons'

foreach size: [ 22, 48 ]
  icon_size_str = '@0@x@0@'.format(size)

  # ..../apps
  icon_subfolder = 'hicolor' / icon_size_str / 'apps'
  foreach private_icon: [ 'key', 'key-personal', 'key-ssh' ]
    install_data(
      icon_subfolder / 'seahorse-@0@.png'.format(private_icon),
      install_dir: private_icons_install_dir / icon_subfolder,
    )
  endforeach

  # ..../status
  icon_subfolder = 'hicolor' / icon_size_str / 'status'

  foreach private_icon: [ 'sign', 'sign-bad', 'sign-ok' ]
    install_data(
      icon_subfolder / 'seahorse-@0@.png'.format(private_icon),
      install_dir: private_icons_install_dir / icon_subfolder,
    )
  endforeach
endforeach

# Only has a 48x48 version somehow
icon_subfolder = 'hicolor' / '48x48' / 'status'
install_data(
  icon_subfolder / 'seahorse-sign-unknown.png',
  install_dir: private_icons_install_dir / icon_subfolder,
)
