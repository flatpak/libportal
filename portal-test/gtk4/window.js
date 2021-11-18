const { GdkPixbuf, Gio, GLib, GObject, Gst,
        Gtk, Pango, PangoCairo, Xdp, XdpGtk4 } = imports.gi;

var PortalTestWindow = GObject.registerClass({
    GTypeName: 'PortalTestWin',
    Template: 'resource:///org/gnome/PortalTest/Gtk4/window.ui',
    InternalChildren: [
        'sandboxStatus',
        'networkStatus',
        'monitorName',
        'proxies',
        'resolverName',
        'image',
        'interactive',
        'encoding',
        'ackImage',
        'inhibitIdle',
        'inhibitLogout',
        'inhibitSuspend',
        'inhibitSwitch',
        'username',
        'realname',
        'avatar',
        'saveHow',
        'screencastLabel',
        'screencastToggle',
        'updateDialog',
        'updateDialog2',
        'updateProgressbar',
        'updateLabel',
        'ok2',
        'openLocalDir',
        'openLocalAsk',
    ],
}, class PortalTestWindow extends Gtk.ApplicationWindow {
    _init(application) {
        super._init({ application });

        this._portal = new Xdp.Portal();

        this._restoreToken = null;

        // Sandbox status
        const path = GLib.build_filenamev([GLib.get_user_runtime_dir(), 'flatpak-info']);
        this._sandboxStatus.label =
            GLib.file_test(path, GLib.FileTest.EXISTS) ? 'confined' : 'unconfined';

        // Network monitor
        this._monitor = Gio.NetworkMonitor.get_default();
        this._monitorName.label = this._monitor.constructor.$gtype.name;
        this._monitor.connect('notify', () => this._updateNetworkStatus());
        this._updateNetworkStatus();

        // Proxy resolver
        this._resolver = Gio.ProxyResolver.get_default();
        this._resolverName.label = this._resolver.constructor.$gtype.name;

        try {
            const proxies = this._resolver.lookup('http://www.flatpak.org', null);
            this._proxies.label = proxies.join(', ');
        } catch(e) {
            logError(e);
        }

        // Updates
        const file = Gio.File.new_for_path('/app/.updated');
        this._updateMonitor = file.monitor_file(Gio.FileMonitorFlags.NONE, null);
        this._updateMonitor.connect('changed', (monitor, file, other, event) => {
            if (event !== Gio.FileMonitorEvent.CREATED)
                return;

            GLib.timeout_add_seconds(GLib.PRIORITY_DEFAULT, 5, () => {
                if (this._updateDialog2.visible)
                    return GLib.SOURCE_CONTINUE;
                this._updateDialog2.present();
                return GLib.SOURCE_REMOVE;
            });
        });

        this._portal.update_monitor_start(Xdp.UpdateMonitorFlags.NONE, null, null);
        this._portal.connect('update-available', (portal, running, local, remote) => {
            log('Update available');
            this._updateLabel.label = 'Update available';
            this._updateProgressbar.fraction = 0.0;
            this._updateDialog2.present();
        });
    }

    _updateNetworkStatus() {
        const networkStatus = [];

        if (this._monitor.networkAvailable)
            networkStatus.push('available');

        if (this._monitor.networkMetered)
            networkStatus.push('metered');

        switch (this._monitor.connectivity) {
        case Gio.NetworkConnectivity.LOCAL:
            networkStatus.push('local');
            break;
        case Gio.NetworkConnectivity.LIMITED:
            networkStatus.push('limited');
            break;
        case Gio.NetworkConnectivity.PORTAL:
            networkStatus.push('portal');
            break;
        case Gio.NetworkConnectivity.FULL:
            networkStatus.push('full');
            break;
        }
        this._networkStatus.label = networkStatus.join(', ');
    }

    _updateDialogResponse(dialog, response) {
        if (response === Gtk.ResponseType.OK)
            this.get_application().restart();

        dialog.hide();
    }

    _updateDialog2Response(dialog, response) {
        if (response === Gtk.ResponseType.OK) {
            const parent = XdpGtk4.parent_new_gtk(this);
            this._portal.update_install(
                parent,
                Xdp.UpdateInstallFlags.NONE,
                (portal, result) => {
                    try {
                        this._portal.update_install_finish(result);
                    } catch(e) {
                        logError(e);
                        this._updateDialog2.hide();
                    }

                    this._updateLabel.label = 'Installing…';
                    this._ok2.sensitive = false;
                    const updateProgressId = this._portal.connect(
                        'update-progress',
                        (portal, nOps, op, progress, status, error, message) => {
                            log('Progress!');
                            this._updateProgressbar.fraction = progress / 100.0;
                            if (status === Xdp.UpdateStatus.FAILED) {
                                this._updateLabel.label = 'Something went wrong';
                                this._updateProgressbar.hide();
                            } else if (status === Xdp.UpdateStatus.DONE) {
                                this._updateLabel.label = 'Installed';
                            }

                            if (status !== Xdp.UpdateStatus.RUNNING)
                                this._portal.disconnect(updateProgressId);
                        });
                });
        }

        dialog.hide();
    }

    _composeEmail() {
        const parent = XdpGtk4.parent_new_gtk(this);

        this._portal.compose_email(
            parent,
            ['recipes-list@gnome.org'],
            ['mclasen@redhat.com', 'dead@email.com'],
            null,
            'Test subject',
            'Test body',
            null,
            Xdp.EmailFlags.NONE,
            null,
            (source, result) => {
                try {
                    this._portal.compose_email_finish(result);
                    log('Email sent');
                } catch (e) {
                    logError(e);
                }
            }
        );
    }

    _getUserInformation() {
        const parent = XdpGtk4.parent_new_gtk(this);

        this._portal.get_user_information(
            parent,
            'Allows portal-test to test the Account portal.',
            Xdp.UserInformationFlags.NONE,
            null,
            (portal, result) => {
                let info = null;

                try {
                    info = this._portal.get_user_information_finish(result);
                } catch(e) {
                    log(e);
                    return;
                }

                if (!info) {
                    log('Account cancelled');
                    return;
                }

                info = info.deepUnpack();
                this._username.label = info['id'].deepUnpack();
                this._realname.label = info['name'].deepUnpack();

                const uri = info['uri'] ? info['uri'].deepUnpack() : null;
                log(uri);
                if (uri && uri !== '') {
                    try {
                        const [path] = GLib.filename_from_uri(uri);
                        const pixbuf =
                            GdkPixbuf.Pixbuf.new_from_file_at_scale(path, 60, 40, true);
                        this._avatar.set_from_pixbuf(pixbuf);
                    } catch(e) {
                        logError(e);
                    }
                }
            }
        );
    }

    _inhibitChanged() {
        let inhibitFlags = 0;

        if (this._inhibitLogout.active)
            inhibitFlags |= Xdp.InhibitFlags.LOGOUT;
        if (this._inhibitSwitch.active)
            inhibitFlags |= Xdp.InhibitFlags.USER_SWITCH;
        if (this._inhibitSuspend.active)
            inhibitFlags |= Xdp.InhibitFlags.SUSPEND;
        if (this._inhibitIdle.active)
            inhibitFlags |= Xdp.InhibitFlags.IDLE;

        if (this._inhibitFlags === inhibitFlags)
            return;

        if (this._inhibitCookie) {
            this._portal.session_uninhibit(this._inhibitCookie);
            delete this._inhibitCookie;
        }

        if (inhibitFlags !== 0) {
            this._inhibitFlags = inhibitFlags;

            const parent = XdpGtk4.parent_new_gtk(this);
            this._portal.session_inhibit(
                parent,
                'Portal Testing',
                inhibitFlags,
                null,
                (portal, result) => {
                    try {
                        this._inhibitCookie = this._portal.session_inhibit_finish(result);
                    } catch(e) {
                        logError(e);
                    }
                });
        } else {
            delete this._inhibitFlags;
        }

    }

    _notifyMe() {
        this._ackImage.hide();

        const notification = new GLib.Variant('a{sv}', {
            'title': new GLib.Variant('s', 'Notify me'),
            'body': new GLib.Variant('s', 'Really important information would ordinarily appear here'),
            'buttons': new GLib.Variant('aa{sv}', [{
                'label': new GLib.Variant('s', 'Yup'),
                'action': new GLib.Variant('s', 'app.ack'),
            }]),
        });

        this._portal.add_notification(
            'notification',
            notification,
            Xdp.NotificationFlags.NONE,
            null, (portal, result) => {
                try {
                    this._portal.add_notification_finish(result);
                } catch(e) {
                    logError(e);
                }
            });
    }

    _openLocal() {
        let flags = Xdp.OpenUriFlags.NONE;

        if (this._openLocalAsk.active)
            flags |= Xdp.OpenUriFlags.ASK;

        const filename = GLib.build_filenamev([GLib.get_user_data_dir(), 'test.txt']);
        const file = Gio.File.new_for_path(filename);
        const uri = file.get_uri();

        log(`Opening ${uri}`);

        const openDir = this._openLocalDir.active;
        const parent = XdpGtk4.parent_new_gtk(this);

        const callback = (portal, result) => {
            try {
                if (openDir)
                    this._portal.open_directory_finish(result);
                else
                    this._portal.open_uri_finish(result);
            } catch(e) {
                logError(e);
            }

        };
        if (openDir)
            this._portal.open_directory(parent, uri, flags, null, callback);
        else
            this._portal.open_uri(parent, uri, flags, null, callback);
    }

    _playClicked() {
        const pipeline = new Gst.Pipeline({ name: 'note' });

        const source = Gst.ElementFactory.make('audiotestsrc', 'source');
        const sink = Gst.ElementFactory.make('autoaudiosink', 'output');

        source.freq = 440.0;

        pipeline.add(source);
        pipeline.add(sink);

        source.link(sink);

        const bus = pipeline.get_bus();
        bus.add_watch(GLib.PRIORITY_DEFAULT, (bus, message) => {
            log(`message ${message.type}`);

            if (message.type === Gst.MessageType.STATE_CHANGED) {
                const [change, state, pending] = pipeline.get_state(Gst.MSECOND);

                if (change === Gst.StateChangeReturn.SUCCESS)
                    log(`Pipeline state now ${state}`);
                else if (change === Gst.StateChangeReturn.ASYNC)
                    log(`Pipeline state now ${state}, pending ${pending}`);
                else
                    log(`Pipeline state change failed, now ${state}`);

                if (state === Gst.State.PLAYING) {
                    GLib.timeout_add(GLib.PRIORITY_DEFAULT, 500, () => {
                        pipeline.set_state(Gst.State.NULL);
                        return GLib.SOURCE_REMOVE;
                    })
                }

            } else if (message.type === Gst.MessageType.ERROR) {
                pipeline.set_state(Gst.State.NULL);
                const [error, string] = Gst.Message.parse_error(message);
                log(error.message);
                return GLib.SOURCE_REMOVE;
            }

            return GLib.SOURCE_CONTINUE;
        });

        log('Set pipeline state to playing');
        pipeline.set_state(Gst.State.PLAYING);
    }

    _printCb() {
        const printData = {
            text: 'Test',
            font: 'Sans 12',
            layout: null,
            pageBreaks: [],
        };

        const print = new Gtk.PrintOperation();
        print.connect('begin-print', (operation, context) => {
            const width = context.get_width();
            const height = context.get_height();

            printData.layout = context.create_pango_layout();

            const desc = Pango.FontDescription.from_string(printData.font);
            printData.layout.set_font_description(desc);
            printData.layout.set_width(width * Pango.SCALE);
            printData.layout.set_text(printData.text, -1);

            const nLines = printData.layout.get_line_count();

            let pageHeight = 0;
            for (let line = 0; line < nLines; line++) {
                const layoutLine = printData.layout.get_line(line);
                const [inkRect, logicalRect] = layoutLine.get_extents();
                const lineHeight = logicalRect.height / 1024.0;

                if (pageHeight + lineHeight > 0) {
                    printData.pageBreaks.push(line);
                    pageHeight = 0;
                }

                pageHeight += lineHeight;
            }

            printData.pageBreaks.reverse();
            print.set_n_pages(printData.pageBreaks.length + 1);
        });
        print.connect('draw-page', (operation, context, pageNumber) => {
            let start = 0;
            if (pageNumber > 0)
                start = printData.pageBreaks[pageNumber - 1];

            let end;
            if (printData.pageBreaks[pageNumber])
                end = printData.layout.get_line_count();
            else
                end = printData.pageBreaks[pageNumber];

            const cr = context.get_cairo_context();
            cr.setSourceRGB(0, 0, 0);

            const iter = printData.layout.get_iter();
            let i = 0;

            do {
                if (i >= start) {
                    const baseline = iter.get_baseline();
                    const [, logicalRect] = iter.get_line_extents();

                    let startPos = 0;
                    if (i === start)
                        startPos = logicalRect.y / 1024.0;

                    cr.moveTo(logicalRect.x / 1024.0, baseline / 1024.0 - startPos);

                    const line = iter.get_line();
                    PangoCairo.show_layout_line(cr, line);
                }

                i++;
            } while (i < end && iter.next_line());
        });
        print.connect('done', () => {
        });

        print.run(Gtk.PrintOperationAction.PRINT_DIALOG, this);
    }

    _requestBackground() {
        const parent = XdpGtk4.parent_new_gtk(this);
        this._portal.request_background(
            parent,
            'Test reason',
            ['/bin/true'],
            Xdp.BackgroundFlags.AUTOSTART,
            null,
            (portal, result) => {
                try {
                    this._portal.request_background_finish(result);
                } catch(e) {
                    logError(e);
                }
            });
    }

    _saveDialog() {
        const dialog = Gtk.FileChooserNative.new(
            'File Chooser Portal',
            this.get_native(),
            Gtk.FileChooserAction.SAVE,
            'Save', 'Cancel');

        const encodingOptions = [
            'current',
            'iso8859-15',
            'utf-16',
        ];
        const encodingLabels = [
            'Current Locale (UTF-8)',
            'Western (ISO-8859-15)',
            'Unicode (UTF-16)',
        ];

        dialog.add_choice('encoding', 'Character Encoding', encodingOptions,
            encodingLabels);
        dialog.set_choice('encoding', 'current');

        dialog.add_choice('canonicalize', 'Canonicalize', null, null);
        dialog.set_choice('canonicalize', 'true');

        dialog.connect('response', (dialog, response) => {
            if (response === Gtk.ResponseType.ACCEPT) {
                const path = dialog.get_file().get_path();
                const method = this._saveHow.activeId;
                let result = false;

                log(method);
                if (method === 'atomically') {
                    try {
                        result = GLib.file_set_contents(path, 'test');
                    } catch(e) {
                        logError(e);
                    }
                } else if (method === 'gio') {
                    const file = Gio.File.new_for_path(path);

                    try {
                        const stream = file.create(Gio.FileCreateFlags.NONE, null);
                        stream.write('test', null);
                        stream.close(null);
                        result = true;
                    } catch(e) {
                        logError(e);
                    }
                } else {
                    log('Not writing');
                }

                if (!result)
                    log('Failed to write');
            }

            const encoding = dialog.get_choice('encoding');
            const canonicalize = dialog.get_choice('canonicalize') === 'true';
            this._encoding.label =
                `${encodingLabels[encodingOptions.indexOf(encoding)]}${canonicalize ? '(canon) ' : ''}`;
        });
        dialog.show();
    }

    _screencastToggled() {
        if (this._screencastToggle.active) {
            delete this._session;

            // Create screencast session
            this._portal.create_screencast_session(
                Xdp.OutputType.WINDOW | Xdp.OutputType.MONITOR,
                Xdp.ScreencastFlags.NONE,
                Xdp.CursorMode.HIDDEN,
                Xdp.PersistMode.TRANSIENT,
                this._restoreToken,
                null,
                (portal, result) => {
                    try {
                        this._session = this._portal.create_screencast_session_finish(result);
                    } catch (e) {
                        logError(e);
                        return;
                    }

                    // Start screencast
                    const parent = XdpGtk4.parent_new_gtk(this);
                    this._session.start(parent, null, (session, startResult) => {
                        try {
                            this._session.start_finish(startResult);
                        } catch (e) {
                            logError(e);

                            delete this._session;
                            this._screencastToggle.active = false;
                            this._screencastLabel.label = '';
                            return;
                        }

                        let label = '';
                        const streams =this._session.get_streams().deepUnpack();
                        for (const [streamId, streamData] of streams) {
                            let x = 0, y = 0;
                            if (streamData['position'])
                                [x, y] = streamData['position'].deepUnpack();

                            let w = 0, h = 0;
                            if (streamData['size'])
                                [w, h] = streamData['size'].deepUnpack();

                            if (label !== '')
                                label += '\n';

                            label += `Stream ${streamId}: ${w}x${h} @ ${x},${y}`;
                        }

                        this._restoreToken = session.get_restore_token();
                        if (this._restoreToken)
                            label += `\nRestore token: ${this._restoreToken}`;

                        this._screencastLabel.label = label;
                    });
                });
        } else if (this._session) {
            this._session.close();
            delete this._session;
            this._screencastLabel.label = '';
        }
    }

    _setWallpaper() {
        const parent = XdpGtk4.parent_new_gtk(this);
        this._portal.set_wallpaper(
            parent,
            'file:///usr/share/backgrounds/gnome/adwaita-morning.png',
            Xdp.WallpaperFlags.BACKGROUND | Xdp.WallpaperFlags.PREVIEW,
            null,
            (portal, result) => {
                try {
                    this._portal.set_wallpaper_finish(result);
                } catch(e) {
                    logError(e);
                }
            });
    }

    _takeScreenshot() {
        let flags = Xdp.ScreenshotFlags.NONE;

        if (this._interactive.active)
            flags |= Xdp.ScreenshotFlags.INTERACTIVE;

        const parent = XdpGtk4.parent_new_gtk(this);
        this._portal.take_screenshot(
            parent,
            flags,
            null,
            (portal, result) => {
                let path = null;

                try {
                    const uri = this._portal.take_screenshot_finish(result);
                    const [path] = GLib.filename_from_uri(uri);
                    const pixbuf =
                        GdkPixbuf.Pixbuf.new_from_file_at_scale(path, 60, 40, true);
                    this._image.set_from_pixbuf(pixbuf);
                } catch(e) {
                    logError(e);
                    return;
                }

            });
    }
});
