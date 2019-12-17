project('libportal','c',
        version: '0.2',
        meson_version: '>= 0.46.0')

cc = meson.get_compiler('c')

gnome = import('gnome')

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

pkg_conf = configuration_data()
pkg_conf.set('VERSION', meson.project_version())
pkg_conf.set('prefix', get_option('prefix'))
pkg_conf.set('libdir', '${prefix}/@0@'.format(get_option('libdir')))
pkg_conf.set('includedir', '${prefix}/' + get_option('includedir'))

configure_file(input: 'libportal.pc.in',
               output: 'libportal.pc',
               configuration: pkg_conf,
               install_dir: join_paths(get_option('libdir'), 'pkgconfig'))

subdir('libportal')
subdir('doc')
if get_option('build-portal-test')
  subdir('portal-test')
endif
