project('libportal','c',
        version: '0.4',
        meson_version: '>= 0.48.0')

cc = meson.get_compiler('c')

gnome = import('gnome')
pkgconfig = import('pkgconfig')

qt = import('qt5')

conf = configuration_data()
conf.set_quoted('G_LOG_DOMAIN', 'libportal')
conf.set_quoted('PACKAGE_NAME', 'libportal')
conf.set_quoted('PKGDATADIR', join_paths(get_option('prefix'), get_option('datadir'), 'libportal'))
conf.set_quoted('APPDATADIR', join_paths(get_option('prefix'), get_option('datadir'), 'org.gnome.PortalTest'))

if cc.has_argument('-fvisibility=hidden')
  conf.set('XDP_PUBLIC', '__attribute__((visibility("default"))) extern')
endif

configure_file(output : 'config.h', configuration : conf)

backends = get_option('backends')

top_inc = include_directories('.')
libportal_inc = include_directories('libportal')

subdir('libportal')

if get_option('qt5')
  libportalqt_inc = include_directories('libportal-qt')
  subdir('libportal-qt')
endif

if get_option('portal-tests')
  subdir('portal-test')
endif

if get_option('tests')
  subdir('tests')
endif

if get_option('gtk_doc')
  subdir('doc')
endif
