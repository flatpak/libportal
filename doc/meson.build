private_headers = [
	'portal-private.h',
	'session-private.h',
	'utils-private.h'
]

gnome.gtkdoc('libportal',
             main_xml: 'libportal.xml',
             namespace: 'xdp',
             mode: 'none',
             scan_args: [
                '--ignore-headers=' + ' '.join(private_headers),
             ],
             src_dir: [ 'libportal' ],
             dependencies: gio_dep,
             install: true)
