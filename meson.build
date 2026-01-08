project('jab', 'c', version: '0.1.0', default_options: ['c_std=c99'])
add_project_arguments(
  '-D_POSIX_C_SOURCE=200809L',
  '-DVERSION="@0@"'.format(meson.project_version()),
  language: 'c',
)

pixman = dependency('pixman-1')
tllist = dependency('tllist')
wayland_client = dependency('wayland-client')
wayland_protocols = dependency('wayland-protocols')

wayland_scanner = find_program('wayland-scanner')
wl_proto_dir = wayland_protocols.get_variable('pkgdatadir')

wl_scanner_code = generator(
  wayland_scanner,
  output: '@BASENAME@-protocol.c',
  arguments: ['private-code', '@INPUT@', '@OUTPUT@'],
)

wl_scanner_client = generator(
  wayland_scanner,
  output: '@BASENAME@-client-protocol.h',
  arguments: ['client-header', '@INPUT@', '@OUTPUT@'],
)

protos_src = []

foreach proto : [
  wl_proto_dir / 'stable/xdg-shell/xdg-shell.xml',
  'wlr-layer-shell-unstable-v1.xml',
]
  protos_src += wl_scanner_code.process(proto)
  protos_src += wl_scanner_client.process(proto)
endforeach

executable(
  'jab',
  [
    'buffer.c',
    'jab.c',
    protos_src,
  ],
  dependencies: [
    pixman,
    tllist,
    wayland_client,
    wayland_protocols,
  ],
  install: true,
)
