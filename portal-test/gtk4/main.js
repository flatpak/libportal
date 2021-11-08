
pkg.initGettext();
pkg.initFormat();
pkg.require({
    'GdkPixbuf': '2.0',
    'Gio': '2.0',
    'Gst': '1.0',
    'Gtk': '4.0',
    'Xdp': '1.0',
    'XdpGtk4': '1.0',
});

const { Gdk, Gio, GLib, Gst, Gtk } = imports.gi;

const { Application } = imports.application;

function stopRunningInstance() {
    const bus = Gio.DBus.session;

    const mainloop = GLib.MainLoop.new(null, false);

    const watchId = bus.watch_name(
        'org.gnome.PortalTest.Gtk4',
        0,
        (bus, name, owner) => {
            log(`Name ${name} owned by ${owner}`);
        },
        (bus, name) => {
            log(`Name ${name} unowned`);
            mainloop.quit();
        });

    try {
        bus.call_sync(
            'org.gnome.PortalTest.Gtk4',
            '/org/gnome/PortalTest/Gtk4',
            'org.gtk.Actions',
            'Activate', GLib.Variant.new_tuple([
                GLib.Variant.new_string('quit'),
                new GLib.Variant('av', []),
                new GLib.Variant('a{sv}', {}),
            ]),
            null,
            Gio.DBusCallFlags.NONE,
            -1, null);
    } catch(e) {
        logError(e);
    }

    mainloop.run();

    bus.unwatch_name(watchId);
}

function main(argv) {
    Gst.init(null);

    if (argv.indexOf('--replace') !== -1)
        stopRunningInstance();

    const application = new Application({
        application_id: 'org.gnome.PortalTest.Gtk4',
        flags: Gio.ApplicationFlags.FLAGS_NONE,
    });

    return application.run(argv);
}
