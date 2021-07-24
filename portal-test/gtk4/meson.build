
gtk4_dep = dependency('gtk4')
gst_dep = dependency('gstreamer-audio-1.0')

resources = gnome.compile_resources(
  'resources',
  'portal-test-gtk4.gresource.xml',
  c_name: '_portal_test_gtk4')

src = [
  'main.c',
  'portal-test-app.h',
  'portal-test-app.c',
  'portal-test-win.h',
  'portal-test-win.c',
  resources,
]

executable(
  'portal-test-gtk4',
  src,
  include_directories: [top_inc, libportal_inc],
  dependencies: [gtk4_dep, gst_dep, libportal_dep],
  install : true,
)

install_data('test.txt', install_dir: 'share/org.gnome.PortalTest')
install_data('org.gnome.PortalTest.Gtk4.desktop', install_dir: 'share/applications')
install_data('org.gnome.PortalTest.Gtk4.service', install_dir: 'share/dbus-1/services')

executable('portal-linking-test-gtk4',
           [ 'portal-linking-test.c' ],
           include_directories: [libportal_inc],
           dependencies: [gtk4_dep, libportal_dep])
