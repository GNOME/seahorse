# Libeggdatetime
libeggdatetime_dependencies = [
  gtk,
  config,
]

libeggdatetime_cflags = [
  '-DGTK_DISABLE_DEPRECATED',
  '-DGDK_DISABLE_DEPRECATED',
  '-DG_DISABLE_DEPRECATED',
]

libeggdatetime_lib = static_library('libeggdatetime',
  'egg-datetime.c',
  c_args: libeggdatetime_cflags,
  dependencies: libeggdatetime_dependencies,
)


# Libtreemultidnd
libtreemultidnd_dependencies = [
  gtk,
  config,
]

libtreemultidnd_cflags = [
  '-DGTK_DISABLE_DEPRECATED',
  '-DGDK_DISABLE_DEPRECATED',
  '-DG_DISABLE_DEPRECATED',
]

libtreemultidnd_lib = static_library('libtreemultidnd',
  'eggtreemultidnd.c',
  c_args: libtreemultidnd_cflags,
  dependencies: libtreemultidnd_dependencies,
)

libtreemultidnd_dep = declare_dependency(
  link_with: libtreemultidnd_lib,
  include_directories: include_directories('..'),
)
