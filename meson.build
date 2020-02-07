project('libportal','c',
        version: '0.3',
        meson_version: '>= 0.46.0')

cc = meson.get_compiler('c')

gnome = import('gnome')
pkgconfig = import('pkgconfig')

conf = configuration_data()
conf.set_quoted('G_LOG_DOMAIN', 'libportal')
conf.set_quoted('PACKAGE_NAME', 'libportal')
conf.set_quoted('PKGDATADIR', join_paths(get_option('prefix'), get_option('datadir'), 'libportal'))
conf.set_quoted('APPDATADIR', join_paths(get_option('prefix'), get_option('datadir'), 'org.gnome.PortalTest'))

visibility_arg = []
if cc.has_argument('-fvisibility=hidden')
  conf.set('XDP_PUBLIC', '__attribute__((visibility("default"))) extern')
  visibility_args = ['-fvisibility=hidden']
endif

configure_file(output : 'config.h', configuration : conf)

top_inc = include_directories('.')
libportal_inc = include_directories('libportal')

subdir('libportal')
subdir('doc')
if get_option('build-portal-test')
  subdir('portal-test')
endif
