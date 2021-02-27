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
  files('eggtreemultidnd.c'),
  c_args: libtreemultidnd_cflags,
  dependencies: libtreemultidnd_dependencies,
)

libtreemultidnd_dep = declare_dependency(
  link_with: libtreemultidnd_lib,
  include_directories: include_directories('..'),
)
