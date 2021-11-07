generated_files = []

headers = files(
  'portal.h',
  'portal-gtk3.h',
  'portal-gtk4.h',
  'portal-qt5.h',
  'portal-helpers.h',
  'account.h',
  'background.h',
  'camera.h',
  'email.h',
  'filechooser.h',
  'inhibit.h',
  'location.h',
  'notification.h',
  'openuri.h',
  'print.h',
  'remote.h',
  'screenshot.h',
  'spawn.h',
  'trash.h',
  'updates.h',
  'wallpaper.h',
)

portal_enums = gnome.mkenums('portal-enums',
  sources: headers,
  c_template: 'portal-enums.c.template',
  h_template: 'portal-enums.h.template',
  install_dir: join_paths (get_option('includedir'), 'libportal'),
  install_header: true,
)
generated_files += portal_enums

src = [
  'account.c',
  'background.c',
  'camera.c',
  'email.c',
  'filechooser.c',
  'inhibit.c',
  'location.c',
  'notification.c',
  'openuri.c',
  'portal.c',
  'print.c',
  'remote.c',
  'screenshot.c',
  'session.c',
  'spawn.c',
  'trash.c',
  'updates.c',
  'wallpaper.c',
  'utils.c',
  generated_files,
]

gio_dep = dependency('gio-2.0', version: '>= 2.58')
gio_unix_dep = dependency('gio-unix-2.0')

install_headers(headers, subdir: 'libportal')

libportal = library('portal',
  src,
  version: '1.0.0',
  include_directories: [top_inc, libportal_inc],
  c_args: visibility_args,
  install: true,
  dependencies: [gio_dep, gio_unix_dep],
)

pkgconfig.generate(libportal,
  description: 'Portal API wrappers',
  name: 'libportal',
  requires: [ gio_dep ],
)

libportal_dep = declare_dependency(
  sources: generated_files,
  include_directories: top_inc,
  dependencies: [gio_dep, gio_unix_dep],
  link_with: libportal,
)
