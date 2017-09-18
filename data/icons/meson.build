icons_install_dir = join_paths(datadir, 'icons')

foreach size: [ 16, 22, 24, 32, 48 ]
  icon_size_str = '@0@x@0@'.format(size)
  icon_subfolder = join_paths('hicolor', icon_size_str, 'apps')

  foreach app: [ 'seahorse', 'seahorse-preferences' ]
    install_data(
      join_paths(icon_subfolder, app + '.png'),
      install_dir: join_paths(icons_install_dir, icon_subfolder),
    )
  endforeach
endforeach

# special case for symbolic and 256
icon_subfolder = join_paths('hicolor', '256x256', 'apps')
install_data(
  join_paths(icon_subfolder, 'seahorse.png'),
  install_dir: join_paths(icons_install_dir, icon_subfolder),
)
icon_subfolder = join_paths('hicolor', 'symbolic', 'apps')
install_data(
  join_paths(icon_subfolder, 'seahorse-symbolic.svg'),
  install_dir: join_paths(icons_install_dir, icon_subfolder),
)

# private icons
private_icons_install_dir = join_paths(pkgdatadir, 'icons')

foreach size: [ 22, 48 ]
  icon_size_str = '@0@x@0@'.format(size)

  # ..../apps
  icon_subfolder = join_paths('hicolor', icon_size_str, 'apps')
  foreach private_icon: [ 'key', 'key-personal', 'key-ssh', 'person' ]
    install_data(
      join_paths(icon_subfolder, 'seahorse-@0@.png'.format(private_icon)),
      install_dir: join_paths(private_icons_install_dir, icon_subfolder),
    )
  endforeach

  # ..../status
  icon_subfolder = join_paths('hicolor', icon_size_str, 'status')

  foreach private_icon: [ 'sign', 'sign-bad', 'sign-ok' ]
    install_data(
      join_paths(icon_subfolder, 'seahorse-@0@.png'.format(private_icon)),
      install_dir: join_paths(private_icons_install_dir, icon_subfolder),
    )
  endforeach
endforeach

# Only has a 48x48 version somehow
icon_subfolder = join_paths('hicolor', '48x48', 'status')
install_data(
  join_paths(icon_subfolder, 'seahorse-sign-unknown.png'),
  install_dir: join_paths(private_icons_install_dir, icon_subfolder),
)
