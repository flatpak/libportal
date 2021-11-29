Title: libportal Reference Manual
Slug: reference-manual

# Portals

libportal is a library that provides GIO-style async API wrappers
for the D-Bus portal APIs in xdg-desktop-portal.

Where possible, applications should prefer to use existing library
APIs that have portal support. For example, a GTK+ application should
use [class@Gtk.FileChooserNative] instead of [method@Portal.open_file].

Some portal interfaces are not covered in libportal, since
they already have perfectly adequate APIs in GLib (which libportal depends on anyway).

- NetworkMonitor: Use [iface@Gio.NetworkMonitor]
- ProxyResolver: Use [iface@Gio.ProxyResolver]
- Settings: Use a plain [class@Gio.DBusProxy]
- GameMode: Not currently covered
- Secrets: Not currently covered
