Title: libportal Reference Manual
Slug: reference-manual

# Portals

libportal is a library that provides GIO-style async API wrappers
for the D-Bus portal APIs in xdg-desktop-portal.

Where possible, applications should prefer to use existing library
APIs that have portal support. For example, a GTK application should
use [class@Gtk.FileChooserNative] instead of [method@Portal.open_file].

Some portal interfaces are not covered in libportal, since
they already have perfectly adequate APIs in GLib (which libportal depends on anyway).

- NetworkMonitor: Use [iface@Gio.NetworkMonitor]
- ProxyResolver: Use [iface@Gio.ProxyResolver]
- Settings: Use a plain [class@Gio.DBusProxy]
- GameMode: Not currently covered
- Secrets: Not currently covered

## Accounts

The Accounts portal lets applications query basic information about
the user, such as user ID, name and avatar picture.

See [method@Portal.get_user_information].

The underlying D-Bus interface is [org.freedesktop.portal.Account](https://flatpak.github.io/xdg-desktop-portal/portal-docs.html#gdbus-org.freedesktop.portal.Account).

## Background

The Background portal lets applications request background permissions
such as enabling autostart.

See [method@Portal.request_background].

The underlying D-Bus interface is [org.freedesktop.portal.Background](https://flatpak.github.io/xdg-desktop-portal/portal-docs.html#gdbus-org.freedesktop.portal.Background).

## Camera

The Camera portal lets applications access camera devices and open [PipeWire][pipewire] remotes for them.

To test if the Camera portal is available, you can use [method@Portal.is_camera_present].

See [method@Portal.access_camera].

The underlying D-Bus interface is [org.freedesktop.portal.Camera](https://flatpak.github.io/xdg-desktop-portal/portal-docs.html#gdbus-org.freedesktop.portal.Camera).

## Email

The Email portal lets applications send email, by prompting
the user to compose a message. The email may already have
an address, subject, body or attachments.

See [method@Portal.compose_email].

The underlying D-Bus interface is [org.freedesktop.portal.Email](https://flatpak.github.io/xdg-desktop-portal/portal-docs.html#gdbus-org.freedesktop.portal.Email).

## File

The File portal lets applications ask the user for access to
files outside the sandbox, by presenting a file chooser dialog.

The selected files will be made accessible to the application
via the document portal, and the returned URI will point
into the document portal fuse filesystem in `/run/user/$UID/doc/`.

See [method@Portal.open_file], [method@Portal.save_file], and [method@Portal.save_files].

The underlying D-Bus interface is [org.freedesktop.portal.FileChooser](https://flatpak.github.io/xdg-desktop-portal/portal-docs.html#gdbus-org.freedesktop.portal.FileChooser).

## Session

The Session portal lets applications inhibit certain login session
state changes, and be informed about the impending end of the
session.

A typical use for this functionality is to prevent the session
from locking while a video is playing.

See [method@Portal.session_inhibit], and [method@Portal.session_monitor_start].

The underlying D-Bus interface is [org.freedesktop.portal.Inhibit](https://flatpak.github.io/xdg-desktop-portal/portal-docs.html#gdbus-org.freedesktop.portal.Inhibit).

## Location

The Access portal lets app access to location information.

Location monitoring makes location information available
via the [signal@Portal::location-updated] signal.

See [method@Portal.location_monitor_start].

The underlying D-Bus interface is [org.freedesktop.portal.Location](https://flatpak.github.io/xdg-desktop-portal/portal-docs.html#gdbus-org.freedesktop.portal.Location).

## Notification

The Notification portal lets applications send desktop notifications.

See [method@Portal.add_notification].

The underlying D-Bus interface is [org.freedesktop.portal.Notification](https://flatpak.github.io/xdg-desktop-portal/portal-docs.html#gdbus-org.freedesktop.portal.Notification).

## Open URI

The Open URI portal lets applications open URIs in external handlers.
A typical example is to open a pdf file in a document viewer.

See [method@Portal.open_uri], and [method@Portal.open_directory].

The underlying D-Bus interface is [org.freedesktop.portal.OpenURI](https://flatpak.github.io/xdg-desktop-portal/portal-docs.html#gdbus-org.freedesktop.portal.OpenURI).

## Print

The Print portal lets applications send documents to a printer.

See [method@Portal.prepare_print], and [method@Portal.print_file].

The underlying D-Bus interface is [org.freedesktop.portal.Print](https://flatpak.github.io/xdg-desktop-portal/portal-docs.html#gdbus-org.freedesktop.portal.Print).

## Screencast

The Screencast portal lets applications create screencast sessions.

A screencast session makes the content of a monitor or window
available as a [PipeWire][pipewire] stream.

See [method@Portal.create_screencast_session].

The underlying D-Bus interface is [org.freedesktop.portal.ScreenCast](https://flatpak.github.io/xdg-desktop-portal/portal-docs.html#gdbus-org.freedesktop.portal.ScreenCast).

## Remote Desktop

The Remote Desktop portal allows remote control of a session.

A remote desktop session allows to inject events into the input stream.

See [method@Portal.create_remote_desktop_session].

The underlying D-Bus interface is [org.freedesktop.portal.RemoteDesktop](https://flatpak.github.io/xdg-desktop-portal/portal-docs.html#gdbus-org.freedesktop.portal.RemoteDesktop).

## Screenshot

The Screenshot portal allows applications to take screenshots or pick colors.

See [method@Portal.take_screenshot], and [method@Portal.pick_color].

The underlying D-Bus interface is [org.freedesktop.portal.Screenshot](https://flatpak.github.io/xdg-desktop-portal/portal-docs.html#gdbus-org.freedesktop.portal.Screenshot).

## Spawn

The Spawn portal lets applications spawn a process in another
copy of their sandbox.

To monitor spawned processes, use the [signal@Portal::spawn-exited]
signal.

See [method@Portal.spawn].

The underlying D-Bus interface is [org.freedesktop.portal.Flatpak](https://flatpak.github.io/xdg-desktop-portal/portal-docs.html#gdbus-org.freedesktop.portal.Flatpak).

## Trash

The Trash portal lets applications send a file to the trash can.

See [method@Portal.trash_file].

The underlying D-Bus interface is [org.freedesktop.portal.Trash](https://flatpak.github.io/xdg-desktop-portal/portal-docs.html#gdbus-org.freedesktop.portal.Trash).

## Updates

The Updates portal lets applications be informed about available
software updates (for themselves) and install those updates.

See [method@Portal.update_install].

The underlying D-Bus interface is [org.freedesktop.portal.Flatpak](https://flatpak.github.io/xdg-desktop-portal/portal-docs.html#gdbus-org.freedesktop.portal.Flatpak).

## Wallpaper

The Wallpaper portal lets applications set desktop backgrounds.

See [method@Portal.set_wallpaper].

The underlying D-Bus interface is [org.freedesktop.portal.Wallpaper](https://flatpak.github.io/xdg-desktop-portal/portal-docs.html#gdbus-org.freedesktop.portal.Wallpaper).

[pipewire]: https://pipewire.org
