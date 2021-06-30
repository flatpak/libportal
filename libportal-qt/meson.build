
libportal_version = meson.project_version()

add_languages('cpp', required : true)

qt5_dep = dependency('qt5', modules: ['Core', 'Gui'])

src = [
    'account.cpp',
    'background.cpp',
    'camera.cpp',
    'email.cpp',
    'filechooser.cpp',
    'inhibit.cpp',
    'location.cpp',
    'openuri.cpp',
    'parent.cpp',
    'portal.cpp',
    'response.cpp',
    portal_enums
]

pub_headers = [
    'account.h',
    'background.h',
    'camera.h',
    'email.h',
    'filechooser.h',
    'inhibit.h',
    'libportalqt_export.h',
    'location.h',
    'openuri.h',
    'parent.h',
    'portal.h',
    'response.h'
]

priv_headers = [
    'parent_p.h',
    'portal_p.h'
]

moc = qt.preprocess(moc_headers : pub_headers + priv_headers,
                    moc_extra_arguments: ['-DMAKES_MY_MOC_HEADER_COMPILE'])


libportalqt = library ('LibPortalQt',
                       [src, pub_headers, priv_headers, moc],
                       soversion: 0,
                       version: libportal_version,
                       dependencies: [qt5_dep, gio_dep, gio_unix_dep],
                       link_with: [libportal],
                       include_directories: [top_inc, libportal_inc],
                       install: true)

libportalqt_dep = declare_dependency(sources: [src, pub_headers, priv_headers, moc],
                                     include_directories: [top_inc, libportal_inc, libportalqt_inc],
                                     dependencies: [qt5_dep, gio_dep, gio_unix_dep],
                                     link_with: libportal)

install_headers(pub_headers, subdir: 'LibPortalQt')

pkgconfig.generate(libportalqt,
  description: 'Qt portal API wrappers',
  name: 'libportal-qt',
  requires: [ qt5_dep ],
)

#
# CMake support
#
cmake_data = configuration_data()
cmake_data.set('LIBDIR_FULL', join_paths(get_option('prefix'), get_option('libdir')))
cmake_data.set('PREFIX', get_option('prefix'))
cmake_data.set('VERSION', libportal_version)

configure_file (input: 'cmake/LibPortalQtConfig.cmake.in',
                output: 'LibPortalQtConfig.cmake',
                configuration: cmake_data
)
configure_file (input: 'cmake/LibPortalQtConfigVersion.cmake.in',
                output: 'LibPortalQtConfigVersion.cmake',
                configuration: cmake_data
)

install_data (
    join_paths(meson.current_build_dir(), 'LibPortalQtConfig.cmake'),
    join_paths(meson.current_build_dir(), 'LibPortalQtConfigVersion.cmake'),
    install_dir: join_paths(get_option('libdir'), 'cmake', 'LibPortalQt')
)
