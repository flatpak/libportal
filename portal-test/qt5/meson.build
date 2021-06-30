
add_languages('cpp', required : true)

qt_dep = dependency('qt5', modules: ['Core', 'Gui', 'Widgets'])

src = [
  'main.cpp',
  'portal-test-qt.h',
  'portal-test-qt.cpp',
]

prep = qt.preprocess(
  moc_headers : 'portal-test-qt.h',
  moc_extra_arguments: ['-DMAKES_MY_MOC_HEADER_COMPILE'],
  ui_files : 'portal-test-qt.ui',
  dependencies: libportal_qt5_dep,
)

executable('portal-test-qt',
  [src, prep],
  include_directories: [top_inc, libportal_inc],
  dependencies: [libportal_qt5_dep, libportalqt_dep],
  link_with: [libportalqt],
  cpp_args : '-std=c++11',
  install : true,
)
