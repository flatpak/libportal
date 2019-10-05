headers = files('portal.h', 'portal-gtk.h')

portal_enums = gnome.mkenums('portal-enums',
                             sources: headers,
                             c_template: 'portal-enums.c.template',
                             h_template: 'portal-enums.h.template',
                             install_dir: 'libportal',
                             install_header: true)

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
        'trash.c',
        'print.c',
        'remote.c',
        'background.c',
        'camera.c',
        'location.c',
        'spawn.c',
        portal_enums
]

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

libportal_dep = declare_dependency(sources: src,
                                   include_directories: top_inc,
                                   dependencies: [gio_dep, gio_unix_dep],
                                   link_with: libportal)
