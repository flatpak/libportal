project('libportal','c',
        version: '0.5',
        meson_version: '>= 0.49.0')

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
introspection = get_option('introspection')
vapi = get_option('vapi')

top_inc = include_directories('.')
libportal_inc = include_directories('libportal')

subdir('libportal')

if get_option('portal-tests')
  subdir('portal-test')
endif

if get_option('tests')
  subdir('tests')
endif

if get_option('docs')
  subdir('doc')
endif
