
add_languages('cpp', required : true)

qt_dep = dependency('qt5', modules: ['Core', 'Test'])

src = ['qttest.cpp',
       'qttest.h']

prep = qt.preprocess(moc_headers : 'qttest.h',
                     moc_extra_arguments: ['-DMAKES_MY_MOC_HEADER_COMPILE'],
                     dependencies: qt_dep)

exe = executable('qt-test',
                 [src, prep],
                 include_directories: [top_inc, libportalqt_inc],
                 dependencies: [qt_dep, libportalqt_dep],
                 link_with: [libportalqt],
                 cpp_args : '-std=c++11')

test('Qt unit test', exe)

