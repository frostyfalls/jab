# SPDX-License-Identifier: MIT

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
  '-DWW_VERSION="@0@"'.format(meson.project_version()),
  language: 'c',
)

wayland_client = dependency('wayland-client')
wayland_protocols = dependency('wayland-protocols').get_variable('pkgdatadir')
wayland_scanner = find_program(dependency('wayland-scanner').get_variable('wayland_scanner'))
pixman = dependency('pixman-1')
libpng = dependency('libpng', required: get_option('png'))
libjpeg = dependency('libjpeg', required: get_option('jpeg'))
libwebp = dependency('libwebp', required: get_option('webp'))

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

formats_src = []
if libpng.found()
  add_project_arguments('-DWW_HAVE_PNG', language: 'c')
  formats_src += ['png.c']
endif
if libjpeg.found()
  add_project_arguments('-DWW_HAVE_JPEG', language: 'c')
  formats_src += ['jpeg.c']
endif
if libjpeg.found()
  add_project_arguments('-DWW_HAVE_WEBP', language: 'c')
  formats_src += ['webp.c']
endif

executable(
  'waywallpaper',
  [
    'waywallpaper.c',
    'shm.c',
    protocols_src,
    formats_src,
  ],
  dependencies: [
    wayland_client,
    pixman,
    libpng,
    libjpeg,
  ],
)

summary(
  {
    'PNG support': libpng.found(),
    'JPEG support': libjpeg.found(),
    'WEBP support': libwebp.found(),
  },
  bool_yn: true,
)
