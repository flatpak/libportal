
gtk_dep = dependency('gtk+-3.0')
gst_dep = dependency('gstreamer-audio-1.0')

resources = gnome.compile_resources('resources',
                                    'portal-test.gresource.xml',
                                    c_name: '_portal_test')

src = ['main.c',
       'portal-test-app.h',
       'portal-test-app.c',
       'portal-test-win.h',
       'portal-test-win.c',
       resources ]

executable('portal-test',
           src,
           include_directories: [top_inc, libportal_inc],
           dependencies: [gtk_dep, gst_dep, libportal_dep],
           install : true)

install_data('test.txt', install_dir: 'share/org.gnome.PortalTest.Gtk3')
install_data('org.gnome.PortalTest.Gtk3.desktop', install_dir: 'share/applications')
install_data('org.gnome.PortalTest.Gtk3.service', install_dir: 'share/dbus-1/services')

executable('portal-linking-test',
           [ 'portal-linking-test.c' ],
           include_directories: [libportal_inc],
           dependencies: [gtk_dep, libportal_dep])
