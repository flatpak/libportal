add_languages('cpp', required : true)

qt_dep = dependency('qt5', modules: ['Core', 'Test'])

src = [
  'test.cpp',
  'test.h',
]

prep = qt.preprocess(
  moc_headers : 'test.h',
  moc_extra_arguments: ['-DMAKES_MY_MOC_HEADER_COMPILE'],
  dependencies: qt_dep,
)

exe = executable('qt-test',
  [src, prep],
  include_directories: [top_inc, libportal_inc],
  dependencies: [qt_dep, libportal_qt5_dep],
  cpp_args : '-std=c++11',
)

test('Qt unit test', exe)
