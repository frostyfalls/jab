# SPDX-License-Identifier: MIT

project(
  'waywallpaper',
  'c',
  version: '0.1.0',
  license: 'MIT',
  license_files: 'LICENSE',
  default_options: ['c_std=c99', 'warning_level=2'],
)

add_project_arguments(
  '-D_POSIX_C_SOURCE=200809L',
  '-DWW_VERSION="@0@"'.format(meson.project_version()),
  language: 'c',
)

wayland_client = dependency('wayland-client')
pixman = dependency('pixman-1')
libpng = dependency('libpng', required: get_option('png'))
libjpeg = dependency('libjpeg', required: get_option('jpeg'))
libwebp = dependency('libwebp', required: get_option('webp'))

wayland_protocols = dependency('wayland-protocols').get_variable('pkgdatadir')
wayland_scanner = find_program(dependency('wayland-scanner').get_variable('wayland_scanner'))

sources = ['src/waywallpaper.c', 'src/shm.c']
dependencies = [wayland_client, pixman]

foreach protocol : [
  'external/wlr-layer-shell-unstable-v1.xml',
  wayland_protocols / 'stable/xdg-shell/xdg-shell.xml',
]
  sources += custom_target(
    protocol.underscorify() + '-client-header',
    output: '@BASENAME@.h',
    input: protocol,
    command: [wayland_scanner, 'client-header', '@INPUT@', '@OUTPUT@'],
  )
  sources += custom_target(
    protocol.underscorify() + '-private-code',
    output: '@BASENAME@.c',
    input: protocol,
    command: [wayland_scanner, 'private-code', '@INPUT@', '@OUTPUT@'],
  )
endforeach

if libpng.found()
  add_project_arguments('-DWW_HAVE_PNG', language: 'c')
  sources += 'src/png.c'
  dependencies += libpng
endif
if libjpeg.found()
  add_project_arguments('-DWW_HAVE_JPEG', language: 'c')
  sources += 'src/jpeg.c'
  dependencies += libjpeg
endif
if libjpeg.found()
  add_project_arguments('-DWW_HAVE_WEBP', language: 'c')
  sources += 'src/webp.c'
  dependencies += libwebp
endif

include = include_directories('include')

executable(
  'waywallpaper',
  sources: sources,
  dependencies: dependencies,
  include_directories: include,
)

summary(
  {
    'PNG support': libpng.found(),
    'JPEG support': libjpeg.found(),
    'WEBP support': libwebp.found(),
  },
  bool_yn: true,
)
