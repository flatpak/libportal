headers = files('portal.h', 'portal-gtk.h')

src = [
	'portal.c',
        'session.c',
	'utils.c',
	'screenshot.c',
	'notification.c',
	'email.c',
	'account.c',
	'inhibit.c',
	'openuri.c',
        'file.c',
        'print.c',
        'remote.c' ]

gio_dep = dependency('gio-2.0')
gio_unix_dep = dependency('gio-unix-2.0')

install_headers(headers, subdir: 'libportal')

libportal = library('portal',
                    src,
                    version: '0.0.1',
                    soversion: 0,
                    include_directories: top_inc,
                    c_args: visibility_args,
                    install: true,
                    dependencies: [gio_dep, gio_unix_dep])
