dbus_sources = gnome.gdbus_codegen('dbusgenerated',
                                   sources: '../data/org.freedesktop.portal.Screenshot.xml',
                                   interface_prefix: 'org.freedesktop.portal',
                                   namespace: '_xdp')

src = [
	'portal.c',
	'screenshot.c',
	'utils.c',
	dbus_sources
]

gio_dep = dependency('gio-2.0')
gio_unix_dep = dependency('gio-unix-2.0')
gtk_dep = dependency('gtk+-3.0')

libportal = library('portal',
                    src,
                    version: '0.0.1',
                    soversion: 0,
                    include_directories: top_inc,
                    install: true,
                    dependencies: [gio_dep, gio_unix_dep, gtk_dep])
