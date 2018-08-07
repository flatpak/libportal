dbus_sources = gnome.gdbus_codegen('dbusgenerated',
                                   sources: [
                                     '../data/org.freedesktop.portal.Screenshot.xml',
                                     '../data/org.freedesktop.portal.Notification.xml',
                                     '../data/org.freedesktop.portal.Email.xml',
                                     '../data/org.freedesktop.portal.Account.xml',
                                     '../data/org.freedesktop.portal.Inhibit.xml'
                                   ],
                                   interface_prefix: 'org.freedesktop.portal',
                                   namespace: '_xdp')

src = [
	'portal.c',
	'utils.c',
	'screenshot.c',
	'notification.c',
	'email.c',
	'account.c',
	'inhibit.c',
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
