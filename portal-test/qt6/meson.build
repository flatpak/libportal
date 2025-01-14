
add_languages('cpp', required : true)

src = [
  'main.cpp',
  'portal-test-qt.h',
  'portal-test-qt.cpp',
]

prep = qt6.preprocess(
  moc_headers : 'portal-test-qt.h',
  moc_extra_arguments: ['-DMAKES_MY_MOC_HEADER_COMPILE'],
  ui_files : 'portal-test-qt.ui',
  dependencies: libportal_qt6_dep,
)

install_data('org.freedesktop.PortalTest.Qt6.desktop', install_dir: 'share/applications')

executable('portal-test-qt6',
  [src, prep],
  include_directories: [top_inc, libportal_inc],
  dependencies: [libportal_qt6_dep],
  cpp_args : '-std=c++17',
  install : true,
)
