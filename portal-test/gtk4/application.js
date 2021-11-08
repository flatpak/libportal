const { Gio, GLib, GObject, Gtk } = imports.gi;

const { PortalTestWindow } = imports.window;
const ByteArray = imports.byteArray;

var Application = GObject.registerClass({
    GTypeName: 'Gtk4PortalTestApplication',
}, class Application extends Gtk.Application {
    _init(params) {
        super._init(params);

        this.add_main_option('replace', 0, 0, 0, 'Replace the running instance', null);

        let action = new Gio.SimpleAction({ name: 'ack' });
        action.connect('activate', () => this._window.ack());
        this.add_action(action);

        action = new Gio.SimpleAction({ name: 'restart' });
        action.connect('activate', () => this.restart());
        this.add_action(action);

        action = new Gio.SimpleAction({ name: 'quit' });
        action.connect('activate', () => this.quit());
        this.add_action(action);
    }

    vfunc_activate() {
        super.vfunc_activate();

        this._window.present();
    }

    vfunc_startup() {
        super.vfunc_startup();

        if (!this._window)
            this._window = new PortalTestWindow(this);
    }

    restart() {
        const bus = this.get_dbus_connection();

        const s2ay = value => {
            let bytes;
            if (typeof value === 'string') {
                let byteArray = ByteArray.fromString(value);
                if (byteArray[byteArray.length - 1] !== 0)
                    byteArray = Uint8Array.of(...byteArray, 0);
                bytes = ByteArray.toGBytes(byteArray);
            } else {
                bytes = new GLib.Bytes(value);
            }
            return GLib.Variant.new_from_bytes(new GLib.VariantType('ay'),
                bytes, true);
        };

        bus.call(
            'org.freedesktop.portal.Flatpak',
            '/org/freedesktop/portal/Flatpak',
            'org.freedesktop.portal.Flatpak',
            'Spawn', GLib.Variant.new_tuple([
                s2ay('/'),
                GLib.Variant.new_array(new GLib.VariantType('ay'), [
                    s2ay('org.gnome.PortalTest.Gtk4'),
                    s2ay('--replace'),
                ]),
                GLib.Variant.new_array(new GLib.VariantType('{uh}'), []),
                GLib.Variant.new_array(new GLib.VariantType('{ss}'), []),
                GLib.Variant.new_uint32(2),
                GLib.Variant.new_array(new GLib.VariantType('{sv}'), []),
            ]),
            new GLib.VariantType('(u)'),
            0, -1, null,
            (connection, result) => {
                try {
                    const res = bus.call_finish(result);
                    log(`Restarted with pid ${res.deepUnpack()}`);
                } catch(e) {
                    logError(e);
                    return;
                }
            });
    }
});
