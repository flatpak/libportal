project('libportal','c',
        version: '1',
        meson_version: '>= 0.46.0')

gnome = import('gnome')

conf_data = configuration_data()
conf_data.set_quoted('G_LOG_DOMAIN', 'PortalTest')
conf_data.set_quoted('PACKAGE_NAME', 'portal-test')
conf_data.set_quoted('PKGDATADIR', join_paths(get_option('prefix'), get_option('datadir'), 'portal-test'))

configure_file(output : 'config.h', configuration : conf_data)

top_inc = include_directories('.')

subdir('libportal')
subdir('portal-test')
