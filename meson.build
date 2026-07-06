project(
  'waywallpaper',
  'c',
  version: '0.1.0',
  default_options: [
    'c_std=c99',
    'warning_level=2',
  ],
)

add_project_arguments(
  '-D_POSIX_C_SOURCE=200809L',
  language: 'c',
)

wayland_client = dependency('wayland-client')
wayland_protocols = dependency('wayland-protocols').get_variable('pkgdatadir')
wayland_scanner = find_program(dependency('wayland-scanner').get_variable('wayland_scanner'))
pixman = dependency('pixman-1')
# TODO: make lib{png,jpeg} optional
# libpng = dependency('libpng')
# libjpeg = dependency('libjpeg')

protocols_src = []
foreach protocol : [
  'wlr-layer-shell-unstable-v1.xml',
  wayland_protocols + '/stable/xdg-shell/xdg-shell.xml',
]
  protocols_src += custom_target(
    protocol.underscorify() + '-client-header',
    output: '@BASENAME@.h',
    input: protocol,
    command: [wayland_scanner, 'client-header', '@INPUT@', '@OUTPUT@'],
  )
  protocols_src += custom_target(
    protocol.underscorify() + '-private-code',
    output: '@BASENAME@.c',
    input: protocol,
    command: [wayland_scanner, 'private-code', '@INPUT@', '@OUTPUT@'],
  )
endforeach

executable(
  'waywallpaper',
  [
    'waywallpaper.c',
    'shm.c',
    protocols_src,
  ],
  dependencies: [
    wayland_client,
    pixman,
    # libpng,
    # libjpeg,
  ],
)
