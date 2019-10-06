private_headers = [
	'portal-private.h',
	'session-private.h',
	'utils-private.h'
]

version_conf = configuration_data()
version_conf.set('VERSION', meson.project_version())
configure_file(input: 'version.xml.in',
               output: 'version.xml',
               configuration: version_conf)

gnome.gtkdoc('libportal',
             main_xml: 'libportal.xml',
             namespace: 'xdp',
             mode: 'none',
             scan_args: [
                '--ignore-headers=' + ' '.join(private_headers),
             ],
             src_dir: [ 'libportal' ],
             dependencies: libportal_dep,
             gobject_typesfile: 'libportal.types',
             install: true)
