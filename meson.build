project('libportal','c',
        version: '1',
        meson_version: '>= 0.46.0')

cc = meson.get_compiler('c')

gnome = import('gnome')

conf = configuration_data()
conf.set_quoted('G_LOG_DOMAIN', 'PortalTest')
conf.set_quoted('PACKAGE_NAME', 'portal-test')
conf.set_quoted('PKGDATADIR', join_paths(get_option('prefix'), get_option('datadir'), 'portal-test'))

visibility_arg = []
if cc.has_argument('-fvisibility=hidden')
  conf.set('XDP_PUBLIC', '__attribute__((visibility("default"))) extern')
  visibility_args = ['-fvisibility=hidden']
endif

configure_file(output : 'config.h', configuration : conf)

top_inc = include_directories('.')

subdir('libportal')
subdir('doc')
subdir('portal-test')
