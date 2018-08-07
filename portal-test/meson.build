
gtk_dep = dependency('gtk+-3.0')
gst_dep = dependency('gstreamer-audio-1.0')

resources = gnome.compile_resources('resources',
                                    'portal-test.gresource.xml',
                                    c_name: '_portal_test')
screencast = gnome.gdbus_codegen('screencast-portal',
                            'org.freedesktop.portal.ScreenCast.xml',
                            interface_prefix: 'org.freedesktop.portal.',
                            namespace: 'Xdp')

src = ['main.c',
       'portal-test-app.h',
       'portal-test-app.c',
       'portal-test-win.h',
       'portal-test-win.c',
       resources,
       screencast]

executable('portal-test',
           src,
           link_with: libportal,
           include_directories: top_inc,
           dependencies: [gtk_dep, gst_dep],
           install : true)

install_data('org.gnome.PortalTest.desktop', install_dir: 'share/applications')
install_data('org.gnome.PortalTest.service', install_dir: 'share/dbus-1/services')
