generated_files = []
version = '1.0.0'
gir_version = '1.0'

headers = [
  'portal.h',
  'portal-helpers.h',
  'account.h',
  'background.h',
  'camera.h',
  'dynamic-launcher.h',
  'email.h',
  'filechooser.h',
  'inhibit.h',
  'location.h',
  'notification.h',
  'openuri.h',
  'parent.h',
  'print.h',
  'remote.h',
  'screenshot.h',
  'session.h',
  'spawn.h',
  'trash.h',
  'types.h',
  'updates.h',
  'wallpaper.h',
]

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
  'dynamic-launcher.c',
  'email.c',
  'filechooser.c',
  'inhibit.c',
  'location.c',
  'notification.c',
  'openuri.c',
  'parent.c',
  'portal.c',
  'print.c',
  'remote.c',
  'screenshot.c',
  'session.c',
  'spawn.c',
  'trash.c',
  'updates.c',
  'wallpaper.c',
  generated_files,
]

gio_dep = dependency('gio-2.0', version: '>= 2.58')
gio_unix_dep = dependency('gio-unix-2.0')

install_headers(headers, subdir: 'libportal')

libportal = library('portal',
  src,
  version: version,
  include_directories: [top_inc, libportal_inc],
  install: true,
  dependencies: [gio_dep, gio_unix_dep],
  gnu_symbol_visibility: 'hidden',
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

if introspection
  libportal_gir = gnome.generate_gir(libportal,
    sources: generated_files + headers + src,
    nsversion: gir_version,
    namespace: 'Xdp',
    symbol_prefix: 'xdp',
    identifier_prefix: 'Xdp',
    header: 'libportal/portal.h',
    link_with: libportal,
    includes: ['Gio-2.0'],
    install: true,
    export_packages: ['libportal'],
   )
  if vapi
    libportal_vapi = gnome.generate_vapi(
      'libportal',
      sources: [libportal_gir[0]],
      packages: [ 'gio-2.0' ],
      install: true,
    )
  endif
endif

########
# GTK3 #
########

if 'gtk3' in backends
  gtk3_dep = dependency('gtk+-3.0')
  gtk3_headers = ['portal-gtk3.h']
  gtk3_sources = ['portal-gtk3.c']

  install_headers(gtk3_headers, subdir: 'libportal-gtk3')

  libportal_gtk3 = library('portal-gtk3',
    gtk3_sources,
    version: version,
    include_directories: [top_inc, libportal_inc],
    install: true,
    dependencies: [libportal_dep, gtk3_dep],
    gnu_symbol_visibility: 'hidden',
  )

  pkgconfig.generate(libportal_gtk3,
    description: 'Portal API wrappers (GTK3)',
    name: 'libportal-gtk3',
    requires: [gtk3_dep, libportal],
  )

  libportal_gtk3_dep = declare_dependency(
    dependencies: [libportal_dep, gtk3_dep],
    link_with: libportal_gtk3,
  )

  if introspection
    libportal_gtk3_gir = gnome.generate_gir(libportal_gtk3,
      sources: gtk3_sources + gtk3_headers,
      nsversion: gir_version,
      namespace: 'XdpGtk3',
      symbol_prefix: 'xdp',
      identifier_prefix: 'Xdp',
      header: 'libportal-gtk3/portal-gtk3.h',
      link_with: [libportal_gtk3],
      includes: [libportal_gir[0], 'Gtk-3.0'],
      install: true,
      export_packages: ['libportal-gtk3'],
     )

    if vapi
      libportal_gtk3_vapi = gnome.generate_vapi('libportal-gtk3',
        sources: libportal_gtk3_gir[0],
        packages: ['gio-2.0', 'gtk+-3.0', 'libportal'],
        gir_dirs: [meson.current_build_dir()],
        vapi_dirs: [meson.current_build_dir()],
        install: true,
      )
    endif
  endif
endif

########
# GTK4 #
########

if 'gtk4' in backends
  gtk4_dep = dependency('gtk4')
  gtk4_headers = ['portal-gtk4.h']
  gtk4_sources = ['portal-gtk4.c']

  install_headers(gtk4_headers, subdir: 'libportal-gtk4')

  libportal_gtk4 = library('portal-gtk4',
    gtk4_sources,
    version: version,
    include_directories: [top_inc, libportal_inc],
    install: true,
    dependencies: [libportal_dep, gtk4_dep],
    gnu_symbol_visibility: 'hidden',
  )

  pkgconfig.generate(libportal_gtk4,
    description: 'Portal API wrappers (GTK4)',
    name: 'libportal-gtk4',
    requires: [gtk4_dep, libportal],
  )

  libportal_gtk4_dep = declare_dependency(
    dependencies: [libportal_dep, gtk4_dep],
    link_with: libportal_gtk4,
  )

  if introspection
    libportal_gtk4_gir = gnome.generate_gir(
      libportal_gtk4,
      sources: gtk4_sources + gtk4_headers,
      nsversion: gir_version,
      namespace: 'XdpGtk4',
      symbol_prefix: 'xdp',
      identifier_prefix: 'Xdp',
      header: 'libportal-gtk4/portal-gtk4.h',
      link_with: [libportal_gtk4],
      includes: [libportal_gir[0], 'Gtk-4.0'],
      install: true,
      export_packages: ['libportal-gtk4'],
     )

    if vapi
      libportal_gtk4_vapi = gnome.generate_vapi('libportal-gtk4',
        sources: libportal_gtk4_gir[0],
        packages: ['gio-2.0', 'gtk4', 'libportal'],
        gir_dirs: [meson.current_build_dir()],
        vapi_dirs: [meson.current_build_dir()],
        install: true,
      )
    endif
  endif
endif

########
# Qt 5 #
########

if 'qt5' in backends
  add_languages('cpp', required : true)

  qt5_dep = dependency('qt5', modules: ['Core', 'Gui', 'X11Extras', 'Widgets'])
  qt5_headers = ['portal-qt5.h']
  qt5_sources = ['portal-qt5.cpp']

  install_headers(qt5_headers, subdir: 'libportal-qt5')

  libportal_qt5 = library('portal-qt5',
    qt5_sources,
    version: version,
    include_directories: [top_inc, libportal_inc],
    cpp_args : '-std=c++11',
    install: true,
    dependencies: [libportal_dep, qt5_dep],
    gnu_symbol_visibility: 'hidden',
  )

  pkgconfig.generate(libportal_qt5,
    description: 'Portal API wrappers (Qt 5)',
    name: 'libportal-qt5',
    requires: [qt5_dep, libportal],
  )

  libportal_qt5_dep = declare_dependency(
    dependencies: [libportal_dep, qt5_dep],
    link_with: libportal_qt5,
  )
endif
