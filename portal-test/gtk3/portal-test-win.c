/*
 * Copyright (C) 2018, Matthias Clasen
 * Copyright (C) 2024 GNOME Foundation, Inc.
 *
 * This file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, version 3.0 of the
 * License.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-3.0-only
 *
 * Authors:
 *    Matthias Clasen <mclasen@redhat.com>
 *    Hubert Figuière <hub@figuiere.net>
 */

#include "config.h"

#include <gtk/gtk.h>
#include <gio/gdesktopappinfo.h>
#include <gio/gunixinputstream.h>
#include <gio/gunixoutputstream.h>
#include <gio/gunixfdlist.h>

#include <gst/gst.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

#include "libportal/settings.h"
#include "libportal/portal-gtk3.h"

#include "portal-test-app.h"
#include "portal-test-win.h"

#ifdef GDK_WINDOWING_X11
#include <gdk/gdkx.h>
#endif

#ifdef GDK_WINDOWING_WAYLAND
#include <gdk/gdkwayland.h>
#endif

struct _PortalTestWin
{
  GtkApplicationWindow parent;
  GtkWidget *sandbox_status;
  GtkWidget *network_status;
  GtkWidget *monitor_name;
  GtkWidget *resolver_name;
  GtkWidget *proxies;
  GtkWidget *encoding;
  GtkWidget *ack_image;

  XdpPortal *portal;
  XdpSession *session;

  XdpInputCaptureSession *input_capture_session;

  GNetworkMonitor *monitor;
  GProxyResolver *resolver;

  GtkWidget *image;
  GtkWidget *interactive;

  GtkWidget *inhibit_idle;
  GtkWidget *inhibit_logout;
  GtkWidget *inhibit_suspend;
  GtkWidget *inhibit_switch;
  int inhibit_cookie;
  GtkApplicationInhibitFlags inhibit_flags;

  GtkWidget *username;
  GtkWidget *realname;
  GtkWidget *avatar;
  GtkWidget *save_how;
  GtkWidget *open_local_ask;
  GtkWidget *open_local_dir;
  GtkWidget *file_chooser_button;

  XdpSettings *settings;
  GtkWidget *settings_value;
  GtkWidget *settings_count;

  GtkWidget *screencast_label;
  GtkWidget *screencast_toggle;

  GtkWidget *remotedesktop_label;
  GtkWidget *remotedesktop_toggle;

  GtkWidget *inputcapture_label;
  GtkWidget *inputcapture_toggle;

  GFileMonitor *update_monitor;
  GtkWidget *update_dialog;
  GtkWidget *update_dialog2;
  GtkWidget *update_progressbar;
  GtkWidget *update_label;
  GtkWidget *ok2;

  GtkWidget *clipboard_label;
  GtkWidget *clipboard_copy;
  GtkWidget *clipboard_paste;
  GtkWidget *clipboard_entry;

  gulong selection_owner_changed;
  gulong selection_transfer_handler_id;
};

struct _PortalTestWinClass
{
  GtkApplicationWindowClass parent_class;
};

G_DEFINE_TYPE (PortalTestWin, portal_test_win, GTK_TYPE_APPLICATION_WINDOW);

#define TEST_FILE_PATH APPDATADIR ".Gtk3/test.txt"

static void update_clipboard (PortalTestWin *win);

static char *
file_path_in_source_dir (const char *file)
{
  if (g_getenv ("MESON_SOURCE_ROOT") == NULL)
    g_warning ("MESON_SOURCE_ROOT was not set, run this application using `ninja portal-test-gtk3`");

  return g_build_filename (g_getenv ("MESON_SOURCE_ROOT"), "portal-test", "gtk3", file, NULL);
}

static void
update_network_status (PortalTestWin *win)
{
  g_autoptr(GString) s = g_string_new ("");
  GEnumClass *class;
  GEnumValue *value;

  if (g_network_monitor_get_network_available (win->monitor))
    g_string_append (s, "available");
  if (g_network_monitor_get_network_metered (win->monitor))
    {
      if (s->len > 0)
        g_string_append (s, ", ");
      g_string_append (s, "metered");
    }
  class = g_type_class_ref (G_TYPE_NETWORK_CONNECTIVITY);
  value = g_enum_get_value (class, g_network_monitor_get_connectivity (win->monitor));
  if (s->len > 0)
    g_string_append (s, ", ");
  g_string_append_printf (s, "connectivity=%s", value->value_nick);
  g_type_class_unref (class);

  gtk_label_set_label (GTK_LABEL (win->network_status), s->str);
}

static gboolean
show_restart_dialog (gpointer data)
{
  PortalTestWin *win = data;

  if (gtk_widget_get_visible (win->update_dialog2))
    return G_SOURCE_CONTINUE;

  gtk_window_present (GTK_WINDOW (win->update_dialog));
  return G_SOURCE_REMOVE;
}

static void
update_monitor_changed (GFileMonitor      *monitor,
                        GFile             *file,
                        GFile             *other,
                        GFileMonitorEvent  event,
                        PortalTestWin     *win)
{
  if (event == G_FILE_MONITOR_EVENT_CREATED)
    g_timeout_add_seconds (5, show_restart_dialog, win);
}

static void
update_dialog_response (GtkDialog     *dialog,
                        int            response,
                        PortalTestWin *win)
{
  if (response == GTK_RESPONSE_OK)
    portal_test_app_restart (PORTAL_TEST_APP (gtk_window_get_application (GTK_WINDOW (win))));

  gtk_widget_hide (win->update_dialog);
}

static void
update_available (XdpPortal *portal,
                  const char *running,
                  const char *local,
                  const char *remote,
                  PortalTestWin *win)
{
  g_message ("Update  available");

  gtk_label_set_label (GTK_LABEL (win->update_label), "Update available");
  gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (win->update_progressbar), 0.0);

  gtk_window_present (GTK_WINDOW (win->update_dialog2));
}

static void
update_progress (XdpPortal *portal,
                 guint      n_ops,
                 guint      op,
                 guint      progress,
                 int        status,
                 const char *error,
                 const char *message,
                 PortalTestWin *win)
{
  g_message ("Progress!");

  gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (win->update_progressbar), progress / 100.0);

  if (status == XDP_UPDATE_STATUS_FAILED)
    {
      gtk_label_set_label (GTK_LABEL (win->update_label), "Something went wrong");
      gtk_widget_hide (win->update_progressbar);
    }
  else if (status == XDP_UPDATE_STATUS_DONE)
    {
      gtk_label_set_label (GTK_LABEL (win->update_label), "Installed");
    }

  if (status != XDP_UPDATE_STATUS_RUNNING)
    g_signal_handlers_disconnect_by_func (win->portal, update_progress, win);
}

static void
update_called (GObject *source,
               GAsyncResult *result,
               gpointer data)
{
  PortalTestWin *win = data;
  g_autoptr(GError) error = NULL;

  if (!xdp_portal_update_install_finish (win->portal, result, &error))
    {
      g_warning ("Installation failed:  %s", error->message);
      gtk_widget_hide (win->update_dialog2);
    }

  gtk_label_set_label (GTK_LABEL (win->update_label), "Installing…");
  gtk_widget_set_sensitive (win->ok2, FALSE);
  g_signal_connect (win->portal, "update-progress", G_CALLBACK (update_progress), win);
}

static void
update_dialog2_response (GtkDialog     *dialog,
                        int            response,
                        PortalTestWin *win)
{
  if (response == GTK_RESPONSE_OK)
    {
      XdpParent *parent = xdp_parent_new_gtk (GTK_WINDOW (win));
      xdp_portal_update_install (win->portal, parent, 0, NULL, update_called, win);
      xdp_parent_free (parent);
    }
  else
    {
      g_signal_handlers_disconnect_by_func (win->portal, update_available, win);
      gtk_widget_hide (win->update_dialog2);
    }
}

static void
reload_settings (PortalTestWin *win, GtkButton* button)
{
  g_autoptr(GError) error = NULL;
  const char *ns[] = { "org.freedesktop.appearance", NULL };
  guint32 value = xdp_settings_read_uint (win->settings, ns[0], "contrast", NULL, &error);
  g_autoptr(GVariant) value2 = NULL;

  if (!error)
    {
      guint32 uvalue2;
      g_autofree char *label = g_strdup_printf ("got %u", value);
      gtk_label_set_text (GTK_LABEL (win->settings_value), label);

      xdp_settings_read (win->settings, ns[0], "contrast", NULL, &error, "u", &uvalue2);
      if (!error)
	g_assert (uvalue2 == value);
    }


  value2 = xdp_settings_read_all_values (win->settings, ns, NULL, NULL);
  if (value2)
    {
      gsize count = g_variant_n_children (value2);
      g_autofree char *count_label = g_strdup_printf ("%lu", count);

      gtk_label_set_text (GTK_LABEL (win->settings_count), count_label);
    }
}

static void
settings_changed (XdpSettings* settings, const gchar *namespace, const gchar *key, GVariant *value, gpointer data)
{
  PortalTestWin *win = data;
  g_autofree char *label = g_strdup_printf ("chg %s %s", namespace, key);

  gtk_label_set_text (GTK_LABEL (win->settings_value), label);
}

static void
portal_test_win_init (PortalTestWin *win)
{
  const char *status;
  g_auto(GStrv) proxies = NULL;
  g_autofree char *proxy = NULL;
  g_autofree char *path = NULL;
  g_autofree char *filename = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GFile) file = NULL;
  g_autoptr(GFile) dst = NULL;
  g_autoptr(GFile) src = NULL;

  win->portal = xdp_portal_new ();

  gtk_widget_init_template (GTK_WIDGET (win));

  path = g_build_filename (g_get_user_runtime_dir (), "flatpak-info", NULL);
  if (g_file_test (path, G_FILE_TEST_EXISTS))
    status = "confined";
  else
    status = "unconfined";
  gtk_label_set_label (GTK_LABEL (win->sandbox_status), status);

  win->monitor = g_network_monitor_get_default ();
  gtk_label_set_label (GTK_LABEL (win->monitor_name), G_OBJECT_TYPE_NAME (win->monitor));
  g_signal_connect_swapped (win->monitor, "notify", G_CALLBACK (update_network_status), win);
  update_network_status (win);

  win->resolver = g_proxy_resolver_get_default ();
  gtk_label_set_label (GTK_LABEL (win->resolver_name), G_OBJECT_TYPE_NAME (win->resolver));

  proxies = g_proxy_resolver_lookup (win->resolver, "http://www.flatpak.org", NULL, &error);
  if (proxies == NULL)
    {
      g_print ("Failed to lookup proxies: %s\n", error->message);
      proxy = g_strdup ("");
    }
  else
    proxy = g_strjoinv (", ", proxies);
  gtk_label_set_label (GTK_LABEL (win->proxies), proxy);

  file = g_file_new_for_path ("/app/.updated");
  win->update_monitor = g_file_monitor_file (file, 0, NULL, NULL);
  g_signal_connect (win->update_monitor, "changed", G_CALLBACK (update_monitor_changed), win);

  g_signal_connect (win->update_dialog, "response", G_CALLBACK (update_dialog_response), win);

  filename = g_build_filename (g_get_user_data_dir (), "test.txt", NULL);
  dst = g_file_new_for_path (filename);
  src = g_file_new_for_path (TEST_FILE_PATH);
  g_file_copy (src, dst, 0, NULL, NULL, NULL, NULL);

  xdp_portal_update_monitor_start (win->portal, 0, NULL, NULL, NULL);
  g_signal_connect (win->portal, "update-available", G_CALLBACK (update_available), win);
  g_signal_connect (win->update_dialog2, "response", G_CALLBACK (update_dialog2_response), win);

  win->settings = xdp_portal_get_settings (win->portal);
  g_signal_connect (win->settings, "changed", G_CALLBACK (settings_changed), win);
}

static void
opened_uri (GObject *object,
            GAsyncResult *result,
            gpointer data)
{
  XdpPortal *portal = XDP_PORTAL (object);
  PortalTestWin *win = data;
  g_autoptr(GError) error = NULL;
  gboolean open_dir;
  gboolean res;

  open_dir = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (win->open_local_dir));

  if (open_dir)
    res = xdp_portal_open_directory_finish (portal, result, &error);
  else
   res = xdp_portal_open_uri_finish (portal, result, &error);

  if (!res)
    g_warning ("%s", error->message);
}

static void
open_local (GtkWidget *button, PortalTestWin *win)
{
  XdpParent *parent;
  g_autofree char *filename = NULL;
  g_autoptr(GFile) file = NULL;
  g_autofree char *uri = NULL;
  gboolean open_dir;
  XdpOpenUriFlags flags = XDP_OPEN_URI_FLAG_NONE;

  filename = g_build_filename (g_get_user_data_dir (), "test.txt", NULL);
  file = g_file_new_for_path (filename);
  uri = g_file_get_uri (file);

  g_message ("Opening '%s'", uri);

  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (win->open_local_ask)))
    flags |= XDP_OPEN_URI_FLAG_ASK;

  open_dir = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (win->open_local_dir));

  parent = xdp_parent_new_gtk (GTK_WINDOW (win));

  if (open_dir)
    xdp_portal_open_directory (win->portal, parent, uri, flags, NULL, opened_uri, win);
  else
    xdp_portal_open_uri (win->portal, parent, uri, flags, NULL, opened_uri, win);
  xdp_parent_free (parent);
}

static gboolean
write_file_atomically (const char *path, GError **error)
{
  return g_file_set_contents (path, "test", -1, error);
}

static gboolean
write_file_gio (const char *path, GError **error)
{
  g_autoptr(GFile) file = NULL;
  g_autoptr(GOutputStream) stream = NULL;

  file = g_file_new_for_path (path);
  stream = G_OUTPUT_STREAM (g_file_create (file, 0, NULL, error));
  if (stream == NULL)
    return FALSE;

  if (g_output_stream_write (stream, "test", strlen ("test"), NULL, error) < 0)
    return FALSE;

  if (!g_output_stream_close (stream, NULL, error))
    return FALSE;

  return TRUE;
}

static gboolean
write_file_posix (const char *path, GError **error)
{
  int fd;

  fd = creat (path, 0600);
  if (fd < 0)
    {
      g_set_error_literal (error, G_IO_ERROR, g_io_error_from_errno (errno), "creat failed");
      return FALSE;
    }

  if (write (fd, "test", strlen ("test")) < 0)
    {
      g_set_error (error, G_IO_ERROR, g_io_error_from_errno (errno), "write failed");
      return FALSE;
    }

  if (close (fd) < 0)
    {
      g_set_error (error, G_IO_ERROR, g_io_error_from_errno (errno), "close failed");
      return FALSE;
    }

  return TRUE;
}

static void
save_dialog (GtkWidget *button, PortalTestWin *win)
{
  gint res;
  GtkFileChooserNative *dialog;
  GtkFileChooser *chooser;
  GtkWindow *parent;
  const char *options[] = {
    "current",
    "iso8859-15",
    "utf-16",
    NULL
  };
  const char *labels[] = {
    "Current Locale (UTF-8)",
    "Western (ISO-8859-15)",
    "Unicode (UTF-16)",
    NULL,
  };
  const char *encoding;
  const char *label;
  g_autofree char *text = NULL;
  const char *canonicalize;
  int i;

  parent = GTK_WINDOW (gtk_widget_get_toplevel (button));
  dialog = gtk_file_chooser_native_new ("File Chooser Portal",
                                        parent,
                                        GTK_FILE_CHOOSER_ACTION_SAVE,
                                        "_Save",
                                        "_Cancel");
  chooser = GTK_FILE_CHOOSER (dialog);
#if GTK_CHECK_VERSION(3, 21, 4)
  gtk_file_chooser_add_choice (chooser,
                               "encoding", "Character Encoding:",
                               options, labels);
  gtk_file_chooser_set_choice (chooser, "encoding", "current");

  gtk_file_chooser_add_choice (chooser, "canonicalize", "Canonicalize", NULL, NULL);
  gtk_file_chooser_set_choice (chooser, "canonicalize", "true");
#endif

  res = gtk_native_dialog_run (GTK_NATIVE_DIALOG (dialog));
  g_message ("Saving file / Response: %d", res);
  if (res == GTK_RESPONSE_ACCEPT)
    {
      const char *method;
      char *filename;
      gboolean res = TRUE;
      g_autoptr(GError) error = NULL;

      method = gtk_combo_box_get_active_id (GTK_COMBO_BOX (win->save_how));

      filename = gtk_file_chooser_get_filename (chooser);
      g_message ("Saving file: '%s'", filename);

      if (strcmp (method, "atomically") == 0)
        res = write_file_atomically (filename, &error);
      else if (strcmp (method, "posix") == 0)
        res = write_file_posix (filename, &error);
      else if (strcmp (method, "gio") == 0)
        res = write_file_gio (filename, &error);
      else
        g_message ("Not writing");

      if (!res)
        g_message ("Failed: %s", error->message);
      g_free (filename);
    }

#if GTK_CHECK_VERSION(3, 21, 4)
  encoding = gtk_file_chooser_get_choice (chooser, "encoding");
  canonicalize = gtk_file_chooser_get_choice (chooser, "canonicalize");
#endif

  label = "";
  for (i = 0; options[i]; i++)
    {
      if (g_strcmp0 (encoding, options[i]) == 0)
        label = labels[i];
    }

  text = g_strdup_printf ("%s%s", label, g_str_equal (canonicalize, "true") ? " (canon)" : "");
  gtk_label_set_label (GTK_LABEL (win->encoding), text);

  g_object_unref (dialog);
}

static void
opened_directory (GObject *object,
                  GAsyncResult *result,
                  gpointer data)
{
  XdpPortal *portal = XDP_PORTAL (object);
  g_autoptr(GError) error = NULL;
  gboolean res;

  res = xdp_portal_open_directory_finish (portal, result, &error);

  if (!res)
    g_warning ("%s", error->message);
}

static void
open_directory (GtkWidget *button, PortalTestWin *win)
{
  GtkFileChooser *file_chooser;
  g_autofree gchar *uri = NULL;
  XdpParent *parent;

  file_chooser = GTK_FILE_CHOOSER (win->file_chooser_button);
  uri = gtk_file_chooser_get_uri (file_chooser);
  if (!uri)
    return;

  g_message ("Opening '%s'", uri);

  parent = xdp_parent_new_gtk (GTK_WINDOW (win));
  xdp_portal_open_directory (win->portal,
                             parent,
                             uri,
                             XDP_OPEN_URI_FLAG_NONE,
                             NULL,
                             opened_directory,
                             win);
  xdp_parent_free (parent);
}

static void
taken (GObject *source,
       GAsyncResult *result,
       gpointer data)
{
  PortalTestWin *win = data;
  g_autofree char *uri = NULL;
  g_autoptr(GError) error = NULL;

  uri = xdp_portal_take_screenshot_finish (win->portal, result, &error);
  if (uri != NULL)
    {
      g_autofree char *path = NULL;
      g_autoptr(GdkPixbuf) pixbuf = NULL;
      g_autoptr(GError) error = NULL;

      path = g_filename_from_uri (uri, NULL, NULL);
      pixbuf = gdk_pixbuf_new_from_file_at_scale (path, 60, 40, TRUE, &error);
      if (pixbuf == NULL)
        g_warning ("Failed to load screenshot from %s: %s", uri, error->message);
      else
        gtk_image_set_from_pixbuf (GTK_IMAGE (win->image), pixbuf);
    }
  else
    g_warning ("Failed to load screenshot: %s", error->message);
}

static void
take_screenshot (GtkButton *button,
                 PortalTestWin *win)
{
  XdpParent *parent;
  XdpScreenshotFlags flags = XDP_SCREENSHOT_FLAG_NONE;

  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (win->interactive)))
    flags = XDP_SCREENSHOT_FLAG_INTERACTIVE;

  parent = xdp_parent_new_gtk (GTK_WINDOW (win));
  xdp_portal_take_screenshot (win->portal, parent, flags, NULL, taken, win);
  xdp_parent_free (parent);
}

static void
on_selection_owner_changed (XdpSession *session,
                           GStrv *mime_types,
                           gboolean session_is_owner,
                           PortalTestWin *win)
{
  update_clipboard (win);
}

static void
on_selection_transfer (XdpSession *session,
                       const char *mime_type,
                       unsigned int serial,
                       PortalTestWin *win)
{
  int fd;
  g_autoptr(GOutputStream) output_stream = NULL;
  g_autoptr(GError) error = NULL;
  const char *text;

  fd = xdp_session_selection_write (session, serial);
  if (fd < 0)
    return;

  text = gtk_entry_get_text (GTK_ENTRY (win->clipboard_entry));

  output_stream = g_unix_output_stream_new (g_steal_fd (&fd), TRUE);
  if (!g_output_stream_write_all (output_stream,
                                  text,
                                  strlen (text),
                                  NULL, NULL, &error))
    g_warning ("Failed to write clipboard content: %s", error->message);

  g_clear_object (&output_stream);
  xdp_session_selection_write_done (session, serial, !error);
}

static void
update_clipboard (PortalTestWin *win)
{
  if (win->session &&
      xdp_session_is_clipboard_enabled (win->session))
    {
      const char **mime_types;

      mime_types = xdp_session_get_selection_mime_types (win->session);
      if (!xdp_session_is_selection_owned_by_session (win->session) &&
          mime_types &&
          g_strv_contains (mime_types, "text/plain"))
        gtk_widget_set_sensitive (win->clipboard_paste, TRUE);
      else
        gtk_widget_set_sensitive (win->clipboard_paste, FALSE);

      gtk_widget_set_sensitive (win->clipboard_copy, TRUE);

      if (!win->selection_owner_changed)
        {
          win->selection_owner_changed =
            g_signal_connect (win->session, "selection-owner-changed",
                              G_CALLBACK (on_selection_owner_changed), win);
          win->selection_transfer_handler_id =
            g_signal_connect (win->session, "selection-transfer",
                              G_CALLBACK (on_selection_transfer), win);
        }
    }
  else
    {
      gtk_widget_set_sensitive (win->clipboard_copy, FALSE);
      gtk_widget_set_sensitive (win->clipboard_paste, FALSE);
      if (win->session)
        {
          g_clear_signal_handler (&win->selection_owner_changed,
                                  win->session);
          g_clear_signal_handler (&win->selection_transfer_handler_id,
                                  win->session);
        }
      else
        {
          win->selection_transfer_handler_id = 0;
          win->selection_owner_changed = 0;
        }
    }
}

static void
inputcapture_session_started (GObject *source,
                              GAsyncResult *result,
                              gpointer data)
{
  XdpInputCaptureSession *ic = XDP_INPUT_CAPTURE_SESSION (source);
  PortalTestWin *win = data;
  g_autoptr(GError) error = NULL;
  GList *zones;
  g_autoptr (GString) s = NULL;

  if (!xdp_input_capture_session_start_finish (ic, result, &error))
    {
      g_warning ("Failed to start inputcapture session: %s", error->message);
      return;
    }

  zones = xdp_input_capture_session_get_zones (ic);
  s = g_string_new ("");
  for (GList *elem = g_list_first (zones); elem; elem = g_list_next (elem))
    {
      XdpInputCaptureZone *zone = elem->data;
      guint w, h;
      gint x, y;

      g_object_get (zone,
		    "width", &w,
		    "height", &h,
		    "x", &x,
		    "y", &y,
		    NULL);

      g_string_append_printf (s, "%ux%u@%d,%d ", w, h, x, y);
    }
  gtk_label_set_label (GTK_LABEL (win->inputcapture_label), s->str);

  update_clipboard (win);
}

static void
inputcapture_session_created2 (GObject *source,
                               GAsyncResult *result,
                               gpointer data)
{
  XdpPortal *portal = XDP_PORTAL (source);
  PortalTestWin *win = data;
  g_autoptr(GError) error = NULL;
  g_autoptr (GString) s = NULL;
  XdpInputCaptureSession *ic;

  ic = xdp_portal_create_input_capture_session2_finish (portal, result, &error);
  if (ic == NULL)
    {
      g_warning ("Failed to create inputcapture session: %s", error->message);
      return;
    }
  win->session = xdp_input_capture_session_get_session (ic);
  win->input_capture_session = ic;

  xdp_session_request_clipboard (win->session);

  xdp_input_capture_session_start (ic,
                                   NULL,
                                   XDP_INPUT_CAPABILITY_POINTER | XDP_INPUT_CAPABILITY_KEYBOARD,
                                   NULL,
                                   inputcapture_session_started,
                                   win);
}

static void
inputcapture_session_created (GObject *source,
                              GAsyncResult *result,
                              gpointer data)
{
  XdpPortal *portal = XDP_PORTAL (source);
  PortalTestWin *win = data;
  g_autoptr(GError) error = NULL;
  GList *zones;
  g_autoptr (GString) s = NULL;
  XdpInputCaptureSession *ic;

  ic = xdp_portal_create_input_capture_session_finish (portal, result, &error);
  if (ic == NULL)
    {
      g_warning ("Failed to create inputcapture session: %s", error->message);
      return;
    }
  win->session = xdp_input_capture_session_get_session (ic);
  win->input_capture_session = ic;

  zones = xdp_input_capture_session_get_zones (ic);
  s = g_string_new ("");
  for (GList *elem = g_list_first (zones); elem; elem = g_list_next (elem))
    {
      XdpInputCaptureZone *zone = elem->data;
      guint w, h;
      gint x, y;

      g_object_get (zone,
		    "width", &w,
		    "height", &h,
		    "x", &x,
		    "y", &y,
		    NULL);

      g_string_append_printf (s, "%ux%u@%d,%d ", w, h, x, y);
    }
  gtk_label_set_label (GTK_LABEL (win->inputcapture_label), s->str);
}

static void
start_input_capture (PortalTestWin *win)
{
  g_clear_object (&win->session);

  update_clipboard (win);

  if (xdp_portal_get_input_capture_version_sync (win->portal, NULL, NULL) > 1)
    {
      g_print ("Using new input capture session construction API\n");
      xdp_portal_create_input_capture_session2 (win->portal,
                                                NULL,
                                                inputcapture_session_created2,
                                                win);
    }
  else
    {
      g_print ("Using legacy input capture session construction API\n");
      xdp_portal_create_input_capture_session (win->portal,
                                               NULL,
                                               XDP_INPUT_CAPABILITY_POINTER | XDP_INPUT_CAPABILITY_KEYBOARD,
                                               NULL,
                                               inputcapture_session_created,
                                               win);
    }
}

static void
stop_input_capture (PortalTestWin *win)
{
  if (win->session != NULL)
    {
      xdp_session_close (win->session);

      if (xdp_session_get_session_type (win->session) == XDP_SESSION_INPUT_CAPTURE)
        {
          g_clear_object (&win->input_capture_session);
          win->session = NULL;
        }
      else
        {
          g_clear_object (&win->session);
        }

      update_clipboard (win);

      gtk_label_set_label (GTK_LABEL (win->inputcapture_label), "");
    }
}

static void
capture_input (GtkButton *button,
               PortalTestWin *win)
{
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button)))
    start_input_capture (win);
  else
    stop_input_capture (win);
}

static void
capture_input_release (GtkButton *button,
                       PortalTestWin *win)
{
  /* FIXME */
}

static void
capture_input_toggle_enable (GtkButton *button,
                             PortalTestWin *win)
{
  if (!win->input_capture_session)
    return;

  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button)))
    xdp_input_capture_session_enable (win->input_capture_session);
  else
    xdp_input_capture_session_disable (win->input_capture_session);
}

static void
session_started (GObject *source,
                 GAsyncResult *result,
                 gpointer data)
{
  XdpSession *session = (XdpSession*) source;
  PortalTestWin *win = data;
  g_autoptr(GError) error = NULL;
  guint id;
  g_autoptr(GVariantIter) iter = NULL;
  GVariant *props;
  g_autoptr (GString) s = NULL;
  XdpDeviceType device_types;

  if (!xdp_session_start_finish (session, result, &error))
    {
      g_warning ("Failed to start session: %s", error->message);
      g_clear_object (&win->session);

      update_clipboard (win);

      if (xdp_session_get_session_type (session) == XDP_SESSION_SCREENCAST)
        {
          gtk_label_set_label (GTK_LABEL (win->screencast_label), "");
          gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (win->screencast_toggle), FALSE);
        }
      else if (xdp_session_get_session_type (session) == XDP_SESSION_REMOTE_DESKTOP)
        {
          gtk_label_set_label (GTK_LABEL (win->remotedesktop_label), "");
          gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (win->remotedesktop_toggle), FALSE);
        }
      return;
    }

  s = g_string_new ("");

  update_clipboard (win);

  iter = g_variant_iter_new (xdp_session_get_streams (session));
  while (g_variant_iter_next (iter, "(u@a{sv})", &id, &props))
    {
      int x, y, w, h;
      g_variant_lookup (props, "position", "(ii)", &x, &y);
      g_variant_lookup (props, "size", "(ii)", &w, &h);
      if (s->len > 0)
        g_string_append (s, "\n");
      g_string_append_printf (s, "Stream %d: %dx%d @ %d,%d", id, w, h, x, y);
      g_variant_unref (props);
    }

  device_types = xdp_session_get_devices (session);
  if (device_types)
    {
      g_autoptr(GString) devices_s = NULL;

      if (s->len > 0)
        g_string_append (s, "\n");

      devices_s = g_string_new (NULL);
      g_string_append (s, "Devices: ");

      if (device_types & XDP_DEVICE_KEYBOARD)
        g_string_append (devices_s, "keyboard");

      if (device_types & XDP_DEVICE_POINTER)
        {
          if (devices_s->len > 0)
            g_string_append (devices_s, ", ");
          g_string_append (devices_s, "pointer");
        }

      if (device_types & XDP_DEVICE_TOUCHSCREEN)
        {
          if (devices_s->len > 0)
            g_string_append (devices_s, ", ");
          g_string_append (devices_s, "touchscreen");
        }

      g_string_append (s, devices_s->str);
    }

  if (s->len > 0)
    g_string_append (s, "\n");
  g_string_append_printf (s, "Clipboard: %s",
                          xdp_session_is_clipboard_enabled (session) ? "yes"
                                                                     : "no");

  if (xdp_session_get_session_type (session) == XDP_SESSION_SCREENCAST)
    gtk_label_set_label (GTK_LABEL (win->screencast_label), s->str);
  else if (xdp_session_get_session_type (session) == XDP_SESSION_REMOTE_DESKTOP)
    gtk_label_set_label (GTK_LABEL (win->remotedesktop_label), s->str);
}

static void
screencast_session_created (GObject *source,
                            GAsyncResult *result,
                            gpointer data)
{
  XdpPortal *portal = XDP_PORTAL (source);
  PortalTestWin *win = data;
  g_autoptr(GError) error = NULL;
  XdpParent *parent = NULL;

  win->session = xdp_portal_create_screencast_session_finish (portal, result, &error);
  if (win->session == NULL)
    {
      g_warning ("Failed to create screencast session: %s", error->message);
      return;
    }

  parent = xdp_parent_new_gtk (GTK_WINDOW (win));
  xdp_session_start (win->session, parent, NULL, session_started, win);
  xdp_parent_free (parent);
}

static void
start_screencast (PortalTestWin *win)
{
  g_clear_object (&win->session);

  update_clipboard (win);

  xdp_portal_create_screencast_session (win->portal,
                                        XDP_OUTPUT_MONITOR | XDP_OUTPUT_WINDOW,
                                        XDP_SCREENCAST_FLAG_NONE,
                                        XDP_CURSOR_MODE_HIDDEN,
                                        XDP_PERSIST_MODE_NONE,
                                        NULL,
                                        NULL,
                                        screencast_session_created,
                                        win);
}

static void
stop_screencast (PortalTestWin *win)
{
  if (win->session != NULL)
    {
      xdp_session_close (win->session);
      g_clear_object (&win->session);
      update_clipboard (win);
      gtk_label_set_label (GTK_LABEL (win->screencast_label), "");
    }
}

static void
screencast_toggled (GtkToggleButton *button,
                    PortalTestWin *win)
{
  if (gtk_toggle_button_get_active (button))
    start_screencast (win);
  else
    stop_screencast (win);
}

static void
remotedesktop_session_created (GObject *source,
                               GAsyncResult *result,
                               gpointer data)
{
  XdpPortal *portal = XDP_PORTAL (source);
  PortalTestWin *win = data;
  g_autoptr(GError) error = NULL;
  XdpParent *parent = NULL;

  win->session = xdp_portal_create_remote_desktop_session_finish (portal, result, &error);
  if (win->session == NULL)
    {
      g_warning ("Failed to create remotedesktop session: %s", error->message);
      return;
    }

  xdp_session_request_clipboard (win->session);

  parent = xdp_parent_new_gtk (GTK_WINDOW (win));
  xdp_session_start (win->session, parent, NULL, session_started, win);
  xdp_parent_free (parent);
}

static void
start_remotedesktop (PortalTestWin *win)
{
  g_clear_object (&win->session);

  update_clipboard (win);

  xdp_portal_create_remote_desktop_session (win->portal,
                                            (XDP_DEVICE_KEYBOARD |
                                             XDP_DEVICE_POINTER |
                                             XDP_DEVICE_TOUCHSCREEN),
                                            XDP_OUTPUT_MONITOR | XDP_OUTPUT_WINDOW,
                                            XDP_REMOTE_DESKTOP_FLAG_NONE,
                                            XDP_CURSOR_MODE_HIDDEN,
                                            NULL,
                                            remotedesktop_session_created,
                                            win);
}

static void
stop_remotedesktop (PortalTestWin *win)
{
  if (win->session != NULL)
    {
      xdp_session_close (win->session);
      g_clear_object (&win->session);
      update_clipboard (win);
      gtk_label_set_label (GTK_LABEL (win->remotedesktop_label), "");
    }
}

static void
remotedesktop_toggled (GtkToggleButton *button,
                       PortalTestWin *win)
{
  if (gtk_toggle_button_get_active (button))
    start_remotedesktop (win);
  else
    stop_remotedesktop (win);
}

static void
paste_clicked (GtkButton *button,
               PortalTestWin *win)
{
  int fd;
  g_autoptr(GInputStream) input_stream = NULL;
  g_autoptr(GError) error = NULL;
  char clipboard_content[1024] = {};

  fd = xdp_session_selection_read (win->session, "text/plain");
  if (fd < 0)
    return;

  input_stream = g_unix_input_stream_new (g_steal_fd (&fd), TRUE);
  if (!g_input_stream_read_all (input_stream,
                                clipboard_content,
                                sizeof (clipboard_content) - 1,
                                NULL, NULL, &error))
    {
      g_warning ("Failed to read clipboard content: %s", error->message);
      return;
    }

  gtk_label_set_label (GTK_LABEL (win->clipboard_label), clipboard_content);
}

static void
copy_clicked (GtkButton *button,
              PortalTestWin *win)
{
  const char *mime_types[] = {
    "text/plain",
    NULL,
  };

  xdp_session_set_selection (win->session, mime_types);
}

static void
account_response (GObject *source,
                  GAsyncResult *result,
                  gpointer data)
{
  PortalTestWin *win = data;
  g_autoptr(GVariant) ret = NULL;
  g_autoptr(GError) error = NULL;

  ret = xdp_portal_get_user_information_finish (win->portal, result, &error);

  if (ret)
    {
      const char *id;
      const char *name;
      const char *uri;
      g_autofree char *path = NULL;

      g_variant_lookup (ret, "id", "&s", &id);
      g_variant_lookup (ret, "name", "&s", &name);
      g_variant_lookup (ret, "image", "&s", &uri);

      gtk_label_set_label (GTK_LABEL (win->username), id);
      gtk_label_set_label (GTK_LABEL (win->realname), name);

      if (uri && uri[0])
        {
          g_autoptr(GdkPixbuf) pixbuf = NULL;

          path = g_filename_from_uri (uri, NULL, NULL);
          pixbuf = gdk_pixbuf_new_from_file_at_scale (path, 60, 40, TRUE, &error);
          if (error)
            g_warning ("Failed to load photo %s: %s", path, error->message);
          else
            gtk_image_set_from_pixbuf (GTK_IMAGE (win->avatar), pixbuf);
       }
    }
  else if (error)
    g_message ("Account error: %s", error->message);
  else
    g_message ("Account canceled");
}

static void
get_user_information (PortalTestWin *win)
{
  XdpParent *parent;

  parent = xdp_parent_new_gtk (GTK_WINDOW (win));
  xdp_portal_get_user_information (win->portal,
                                   parent,
                                   "Allows portal-test to test the Account portal.",
                                   XDP_USER_INFORMATION_FLAG_NONE,
                                   NULL,
                                   account_response,
                                   win);
  xdp_parent_free (parent);
}

static void
compose_email_called (GObject *source,
                      GAsyncResult *result,
                      gpointer data)
{
  PortalTestWin *win = data;
  g_autoptr(GError) error = NULL;

  if (!xdp_portal_compose_email_finish (win->portal, result, &error))
    {
      g_warning ("Email error: %s", error->message);
      return;
    }

  g_print ("Email sent\n");
}

static void
compose_email (PortalTestWin *win)
{
  XdpParent *parent;
  g_autofree char *filename = NULL;
  const char *addresses[2];
  const char *cc[3];
  const char *attachments[2];

  filename = g_build_filename (g_get_user_data_dir (), "test.txt", NULL);

  attachments[0] = filename;
  attachments[1] = NULL;

  addresses[0] = "recipes-list@gnome.org";
  addresses[1] = NULL;

  cc[0] = "mclasen@redhat.com";
  cc[1] = "dead@email.com";
  cc[2] = NULL;

  parent = xdp_parent_new_gtk (GTK_WINDOW (win));
  xdp_portal_compose_email (win->portal,
                            parent,
                            addresses, cc, NULL,
                            "Test subject",
                            "Test body",
                            attachments,
                            XDP_EMAIL_FLAG_NONE,
                            NULL,
                            compose_email_called,
                            win);
  xdp_parent_free (parent);
}

static void
request_background_called (GObject *source,
                           GAsyncResult *result,
                           gpointer data)
{
  PortalTestWin *win = data;
  g_autoptr(GError) error = NULL;
  gboolean ret = xdp_portal_request_background_finish (win->portal, result, &error);
  if (!ret)
    {
      g_warning ("Background request failed: %s", error ? error->message : "Unknown error");
      return;
    }

  g_message ("Background request successful!");
}

static void
request_background (PortalTestWin *win)
{
  XdpParent *parent;
  g_autoptr(GPtrArray) commandline = g_ptr_array_new_with_free_func (g_free);

  g_ptr_array_add (commandline, g_strdup ("/bin/true"));

  parent = xdp_parent_new_gtk (GTK_WINDOW (win));
  xdp_portal_request_background (win->portal,
                                 parent,
                                 "Test reason",
                                 commandline,
                                 XDP_BACKGROUND_FLAG_AUTOSTART,
                                 NULL,
                                 request_background_called,
                                 win);
  xdp_parent_free (parent);
}

static void
notify_me (PortalTestWin *win)
{
  GVariantBuilder button;
  GVariantBuilder buttons;
  GVariantBuilder notification;

  gtk_widget_hide (win->ack_image);

  g_variant_builder_init (&button, G_VARIANT_TYPE_VARDICT);
  g_variant_builder_add (&button, "{sv}", "label", g_variant_new_string ("Yup"));
  g_variant_builder_add (&button, "{sv}", "action", g_variant_new_string ("app.ack"));

  g_variant_builder_init (&buttons, G_VARIANT_TYPE ("aa{sv}"));
  g_variant_builder_add (&buttons, "a{sv}", &button);

  g_variant_builder_init (&notification, G_VARIANT_TYPE_VARDICT);
  g_variant_builder_add (&notification, "{sv}", "title", g_variant_new_string ("Notify me"));
  g_variant_builder_add (&notification, "{sv}", "body", g_variant_new_string ("Really important information would ordinarily appear here"));
  g_variant_builder_add (&notification, "{sv}", "buttons", g_variant_builder_end (&buttons));

  xdp_portal_add_notification (win->portal, "notification", g_variant_builder_end (&notification), XDP_NOTIFICATION_FLAG_NONE, NULL, NULL, NULL);
}

void
portal_test_win_ack (PortalTestWin *win)
{
  gtk_widget_show (win->ack_image);
}

static GList *active_prints = NULL;

typedef struct {
  char *text;
  PangoLayout *layout;
  GList *page_breaks;
  char *font;
} PrintData;

static void
status_changed_cb (GtkPrintOperation *op,
                   gpointer user_data)
{
  if (gtk_print_operation_is_finished (op))
    {
      active_prints = g_list_remove (active_prints, op);
      g_object_unref (op);
    }
}

static void
begin_print (GtkPrintOperation *operation,
             GtkPrintContext *context,
             PrintData *print_data)
{
  PangoFontDescription *desc;
  PangoLayoutLine *layout_line;
  double width, height;
  double page_height;
  GList *page_breaks;
  int num_lines;
  int line;

  width = gtk_print_context_get_width (context);
  height = gtk_print_context_get_height (context);

  print_data->layout = gtk_print_context_create_pango_layout (context);

  desc = pango_font_description_from_string (print_data->font);
  pango_layout_set_font_description (print_data->layout, desc);
  pango_font_description_free (desc);

  pango_layout_set_width (print_data->layout, width * PANGO_SCALE);
  pango_layout_set_text (print_data->layout, print_data->text, -1);

  num_lines = pango_layout_get_line_count (print_data->layout);

  page_breaks = NULL;
  page_height = 0;

  for (line = 0; line < num_lines; line++)
    {
      PangoRectangle ink_rect, logical_rect;
      double line_height;

      layout_line = pango_layout_get_line (print_data->layout, line);
      pango_layout_line_get_extents (layout_line, &ink_rect, &logical_rect);

      line_height = logical_rect.height / 1024.0;

      if (page_height + line_height > height)
        {
          page_breaks = g_list_prepend (page_breaks, GINT_TO_POINTER (line));
          page_height = 0;
        }

      page_height += line_height;
    }

  page_breaks = g_list_reverse (page_breaks);
  gtk_print_operation_set_n_pages (operation, g_list_length (page_breaks) + 1);

  print_data->page_breaks = page_breaks;
}

static void
draw_page (GtkPrintOperation *operation,
           GtkPrintContext *context,
           int page_nr,
           PrintData *print_data)
{
  cairo_t *cr;
  GList *pagebreak;
  int start, end, i;
  PangoLayoutIter *iter;
  double start_pos;

  if (page_nr == 0)
    start = 0;
  else
    {
      pagebreak = g_list_nth (print_data->page_breaks, page_nr - 1);
      start = GPOINTER_TO_INT (pagebreak->data);
    }

  pagebreak = g_list_nth (print_data->page_breaks, page_nr);
  if (pagebreak == NULL)
    end = pango_layout_get_line_count (print_data->layout);
  else
    end = GPOINTER_TO_INT (pagebreak->data);

  cr = gtk_print_context_get_cairo_context (context);

  cairo_set_source_rgb (cr, 0, 0, 0);

  i = 0;
  start_pos = 0;
  iter = pango_layout_get_iter (print_data->layout);
  do
    {
      PangoRectangle   logical_rect;
      PangoLayoutLine *line;
      int              baseline;

      if (i >= start)
        {
          line = pango_layout_iter_get_line (iter);

          pango_layout_iter_get_line_extents (iter, NULL, &logical_rect);
          baseline = pango_layout_iter_get_baseline (iter);

          if (i == start)
            start_pos = logical_rect.y / 1024.0;

          cairo_move_to (cr, logical_rect.x / 1024.0, baseline / 1024.0 - start_pos);
          pango_cairo_show_layout_line  (cr, line);
        }
      i++;
    }
  while (i < end &&
         pango_layout_iter_next_line (iter));

  pango_layout_iter_free (iter);
}

static void
end_print (GtkPrintOperation *op,
           GtkPrintContext *context,
           PrintData *print_data)
{
  g_list_free (print_data->page_breaks);
  print_data->page_breaks = NULL;
  g_object_unref (print_data->layout);
  print_data->layout = NULL;
}

static void
print_done (GtkPrintOperation *op,
            GtkPrintOperationResult res,
            PrintData *print_data)
{
  GError *error = NULL;

  if (res == GTK_PRINT_OPERATION_RESULT_ERROR)
    {

      GtkWidget *error_dialog;

      gtk_print_operation_get_error (op, &error);

      error_dialog = gtk_message_dialog_new (NULL,
                                             GTK_DIALOG_DESTROY_WITH_PARENT,
                                             GTK_MESSAGE_ERROR,
                                             GTK_BUTTONS_CLOSE,
                                             "Error printing file:\n%s",
                                             error ? error->message : "no details");
      g_signal_connect (error_dialog, "response", G_CALLBACK (gtk_widget_destroy), NULL);
      gtk_widget_show (error_dialog);
    }

  g_free (print_data->text);
  g_free (print_data->font);
  g_free (print_data);
  if (!gtk_print_operation_is_finished (op))
    {
      g_object_ref (op);
      active_prints = g_list_append (active_prints, op);

      /* This ref is unref:ed when we get the final state change */
      g_signal_connect (op, "status-changed",
                        G_CALLBACK (status_changed_cb), NULL);
    }
}

static char *
get_text (void)
{
  char *text;
  g_autofree char* test_file_path = file_path_in_source_dir ("portal-test-win.c");
  g_autoptr(GError) error = NULL;

  if (!g_file_get_contents (test_file_path, &text, NULL, &error))
    {
      g_warning ("Failed to load print text: %s", error->message);
      text = g_strdup (error->message);
    }

  return text;
}

static void
print_cb (GtkButton *button, PortalTestWin *win)
{
  GtkPrintOperation *print;
  PrintData *print_data;

  print_data = g_new0 (PrintData, 1);

  print_data->text = get_text ();
  print_data->font = g_strdup ("Sans 12");

  print = gtk_print_operation_new ();

  g_signal_connect (print, "begin-print", G_CALLBACK (begin_print), print_data);
  g_signal_connect (print, "end-print", G_CALLBACK (end_print), print_data);
  g_signal_connect (print, "draw-page", G_CALLBACK (draw_page), print_data);
  g_signal_connect (print, "done", G_CALLBACK (print_done), print_data);

  //gtk_print_operation_set_allow_async (print, TRUE);
  gtk_print_operation_run (print, GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG,
                           GTK_WINDOW (win), NULL);

  g_object_unref (print);
}

static void
inhibit_finished (GObject *source,
                  GAsyncResult *result,
                  gpointer data)
{
  g_autoptr(GError) error = NULL;
  PortalTestWin *win = data;

  win->inhibit_cookie = xdp_portal_session_inhibit_finish (XDP_PORTAL (source), result, &error);
}

static void
inhibit_changed (GtkToggleButton *button, PortalTestWin *win)
{
  GtkApplicationInhibitFlags flags = 0;

  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (win->inhibit_logout)))
    flags |= GTK_APPLICATION_INHIBIT_LOGOUT;
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (win->inhibit_switch)))
    flags |= GTK_APPLICATION_INHIBIT_SWITCH;
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (win->inhibit_suspend)))
    flags |= GTK_APPLICATION_INHIBIT_SUSPEND;
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (win->inhibit_idle)))
    flags |= GTK_APPLICATION_INHIBIT_IDLE;

  if (win->inhibit_flags == flags)
    return;

  if (win->inhibit_cookie != 0)
    {
      xdp_portal_session_uninhibit (win->portal, win->inhibit_cookie);
      win->inhibit_cookie = 0;
    }

  win->inhibit_flags = flags;

  if (win->inhibit_flags != 0)
    {
      XdpParent *parent;

      parent = xdp_parent_new_gtk (GTK_WINDOW (win));
      xdp_portal_session_inhibit (win->portal,
                                  parent,
                                  "Portal Testing",
                                  (XdpInhibitFlags)win->inhibit_flags,
                                  NULL,
                                  inhibit_finished,
                                  win);
      xdp_parent_free (parent);
    }
}

static gboolean
pipeline_stop (GstElement *pipeline)
{
  gst_element_set_state (pipeline, GST_STATE_NULL);
  g_object_unref (pipeline);

  return FALSE;
}

static gboolean
watch_func (GstBus     *bus,
            GstMessage *message,
            gpointer    data)
{
  GstElement *pipeline = data;

  g_print ("message %d\n", GST_MESSAGE_TYPE (message));

  if (GST_MESSAGE_TYPE (message) == GST_MESSAGE_ERROR)
    {
      g_autoptr(GError) err = NULL;

      gst_message_parse_error (message, &err, NULL);
      g_print ("ERROR: %s\n", err->message);

      gst_element_set_state (pipeline, GST_STATE_NULL);
      g_object_unref (pipeline);
      g_object_unref (bus);

      return FALSE;
    }
  else if (GST_MESSAGE_TYPE (message) == GST_MESSAGE_STATE_CHANGED)
    {
      GstStateChangeReturn change;
      GstState state;
      GstState pending;

      change = gst_element_get_state (pipeline, &state, &pending, GST_MSECOND);
      if (change == GST_STATE_CHANGE_SUCCESS)
        g_print ("Pipeline state now %d\n", state);
      else if (change == GST_STATE_CHANGE_ASYNC)
        g_print ("Pipeline state now %d, pending %d\n", state, pending);
      else
        g_print ("Pipeline state change failed, now %d\n", state);

      if (state == GST_STATE_PLAYING)
        {
          g_timeout_add (500, (GSourceFunc) pipeline_stop, pipeline);
          g_object_unref (bus);

          return FALSE;
        }
    }

  return TRUE;
}

static void
play_sound (gdouble frequency)
{
  GstElement *source, *sink;
  GstElement *pipeline;
  GstBus *bus;

  pipeline = gst_pipeline_new ("note");
  source = gst_element_factory_make ("audiotestsrc", "source");
  sink = gst_element_factory_make ("autoaudiosink", "output");

  g_object_set (source, "freq", frequency, NULL);

  gst_bin_add_many (GST_BIN (pipeline), source, sink, NULL);
  gst_element_link (source, sink);

  bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
  gst_bus_add_watch (bus, watch_func, pipeline);

  g_print ("Set pipeline state to playing\n");
  gst_element_set_state (pipeline, GST_STATE_PLAYING);
}

static void
play_clicked (GtkButton *button, PortalTestWin *win)
{
  play_sound (440.0);
}

static void
set_wallpaper_called (GObject *source,
                      GAsyncResult *result,
                      gpointer data)
{
  PortalTestWin *win = data;
  g_autoptr(GError) error = NULL;
  gboolean ret = xdp_portal_set_wallpaper_finish (win->portal, result, &error);
  if (!ret)
    {
      g_warning ("Wallpaper request failed: %s", error ? error->message : "Unknown error");
      return;
    }

  g_message ("Wallpaper request successful!");
}

static void
set_wallpaper (PortalTestWin *win)
{
  XdpParent *parent;
//  const char *uri = "https://gitlab.gnome.org/GNOME/gtk/raw/master/demos/gtk-demo/portland-rose.jpg";
  const char *uri = "file:///usr/share/backgrounds/gnome/adwaita-morning.jpg";

  parent = xdp_parent_new_gtk (GTK_WINDOW (win));
  xdp_portal_set_wallpaper (win->portal,
                            parent,
                            uri,
                            XDP_WALLPAPER_FLAG_BACKGROUND | XDP_WALLPAPER_FLAG_PREVIEW,
                            NULL,
                            set_wallpaper_called,
                            win);
  xdp_parent_free (parent);
}

static void
portal_test_win_class_init (PortalTestWinClass *class)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gtk/portal-test/portal-test-win.ui");

  gtk_widget_class_bind_template_callback (widget_class, save_dialog);
  gtk_widget_class_bind_template_callback (widget_class, open_directory);
  gtk_widget_class_bind_template_callback (widget_class, open_local);
  gtk_widget_class_bind_template_callback (widget_class, take_screenshot);
  gtk_widget_class_bind_template_callback (widget_class, capture_input);
  gtk_widget_class_bind_template_callback (widget_class, capture_input_release);
  gtk_widget_class_bind_template_callback (widget_class, capture_input_toggle_enable);
  gtk_widget_class_bind_template_callback (widget_class, screencast_toggled);
  gtk_widget_class_bind_template_callback (widget_class, remotedesktop_toggled);
  gtk_widget_class_bind_template_callback (widget_class, paste_clicked);
  gtk_widget_class_bind_template_callback (widget_class, copy_clicked);
  gtk_widget_class_bind_template_callback (widget_class, notify_me);
  gtk_widget_class_bind_template_callback (widget_class, print_cb);
  gtk_widget_class_bind_template_callback (widget_class, inhibit_changed);
  gtk_widget_class_bind_template_callback (widget_class, play_clicked);
  gtk_widget_class_bind_template_callback (widget_class, get_user_information);
  gtk_widget_class_bind_template_callback (widget_class, compose_email);
  gtk_widget_class_bind_template_callback (widget_class, reload_settings);
  gtk_widget_class_bind_template_callback (widget_class, request_background);
  gtk_widget_class_bind_template_callback (widget_class, set_wallpaper);
  gtk_widget_class_bind_template_child (widget_class, PortalTestWin, sandbox_status);
  gtk_widget_class_bind_template_child (widget_class, PortalTestWin, network_status);
  gtk_widget_class_bind_template_child (widget_class, PortalTestWin, monitor_name);
  gtk_widget_class_bind_template_child (widget_class, PortalTestWin, proxies);
  gtk_widget_class_bind_template_child (widget_class, PortalTestWin, resolver_name);
  gtk_widget_class_bind_template_child (widget_class, PortalTestWin, image);
  gtk_widget_class_bind_template_child (widget_class, PortalTestWin, interactive);
  gtk_widget_class_bind_template_child (widget_class, PortalTestWin, encoding);
  gtk_widget_class_bind_template_child (widget_class, PortalTestWin, ack_image);
  gtk_widget_class_bind_template_child (widget_class, PortalTestWin, inhibit_idle);
  gtk_widget_class_bind_template_child (widget_class, PortalTestWin, inhibit_logout);
  gtk_widget_class_bind_template_child (widget_class, PortalTestWin, inhibit_suspend);
  gtk_widget_class_bind_template_child (widget_class, PortalTestWin, inhibit_switch);
  gtk_widget_class_bind_template_child (widget_class, PortalTestWin, inputcapture_label);
  gtk_widget_class_bind_template_child (widget_class, PortalTestWin, inputcapture_toggle);
  gtk_widget_class_bind_template_child (widget_class, PortalTestWin, username);
  gtk_widget_class_bind_template_child (widget_class, PortalTestWin, realname);
  gtk_widget_class_bind_template_child (widget_class, PortalTestWin, avatar);
  gtk_widget_class_bind_template_child (widget_class, PortalTestWin, save_how);
  gtk_widget_class_bind_template_child (widget_class, PortalTestWin, settings_value);
  gtk_widget_class_bind_template_child (widget_class, PortalTestWin, settings_count);
  gtk_widget_class_bind_template_child (widget_class, PortalTestWin, screencast_label);
  gtk_widget_class_bind_template_child (widget_class, PortalTestWin, screencast_toggle);
  gtk_widget_class_bind_template_child (widget_class, PortalTestWin, remotedesktop_label);
  gtk_widget_class_bind_template_child (widget_class, PortalTestWin, remotedesktop_toggle);
  gtk_widget_class_bind_template_child (widget_class, PortalTestWin, clipboard_label);
  gtk_widget_class_bind_template_child (widget_class, PortalTestWin, clipboard_copy);
  gtk_widget_class_bind_template_child (widget_class, PortalTestWin, clipboard_paste);
  gtk_widget_class_bind_template_child (widget_class, PortalTestWin, clipboard_entry);
  gtk_widget_class_bind_template_child (widget_class, PortalTestWin, update_dialog);
  gtk_widget_class_bind_template_child (widget_class, PortalTestWin, update_dialog2);
  gtk_widget_class_bind_template_child (widget_class, PortalTestWin, update_progressbar);
  gtk_widget_class_bind_template_child (widget_class, PortalTestWin, update_label);
  gtk_widget_class_bind_template_child (widget_class, PortalTestWin, ok2);
  gtk_widget_class_bind_template_child (widget_class, PortalTestWin, open_local_dir);
  gtk_widget_class_bind_template_child (widget_class, PortalTestWin, open_local_ask);
  gtk_widget_class_bind_template_child (widget_class, PortalTestWin, file_chooser_button);
}

GtkApplicationWindow *
portal_test_win_new (PortalTestApp *app)
{
  return g_object_new (portal_test_win_get_type (), "application", app, NULL);
}
