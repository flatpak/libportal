/*
 * Copyright (C) 2018, Matthias Clasen
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "portal-private.h"
#include "utils.h"

G_DECLARE_FINAL_TYPE (ScreenshotCall, screenshot_call, SCREENSHOT, CALL, GObject)

struct _ScreenshotCall {
  GObject parent_instance;

  XdpPortal *portal;
  GtkWindow *parent;
  gchar *parent_handle;
  gboolean modal;
  gboolean interactive;
  GTask *task;
  guint response_signal;
};

struct _ScreenshotCallClass {
  GObjectClass parent_class;
};

G_DEFINE_TYPE (ScreenshotCall, screenshot_call, G_TYPE_OBJECT)

static void
screenshot_call_finalize (GObject *object)
{
  ScreenshotCall *call = SCREENSHOT_CALL (object);

  if (call->response_signal)
    {
      GDBusConnection *bus;

      bus = g_dbus_proxy_get_connection (G_DBUS_PROXY (call->portal->screenshot));
      g_dbus_connection_signal_unsubscribe (bus, call->response_signal);
    }
  g_object_unref (call->portal);
  if (call->parent)
    {
      _gtk_window_unexport_handle (call->parent);
      g_object_unref (call->parent);
    }
  if (call->parent_handle)
    g_free (call->parent_handle);
  g_object_unref (call->task);

  G_OBJECT_CLASS (screenshot_call_parent_class)->finalize (object);
}

static void
screenshot_call_class_init (ScreenshotCallClass *class)
{
  G_OBJECT_CLASS (class)->finalize = screenshot_call_finalize;
}

static void
screenshot_call_init (ScreenshotCall *call)
{
}

static ScreenshotCall *
screenshot_call_new (XdpPortal *portal,
                     GtkWindow *parent,
                     gboolean modal,
                     gboolean interactive,
                     GTask *task)
{
  ScreenshotCall *call = g_object_new (screenshot_call_get_type (), NULL);

  call->portal = g_object_ref (portal);
  call->parent = parent ? g_object_ref (parent) : NULL;
  call->parent_handle = NULL;
  call->modal = modal;
  call->interactive = interactive;
  call->task = g_object_ref (task);

  return call;
}

static void do_screenshot (ScreenshotCall *call);

static void
got_proxy (GObject *source,
           GAsyncResult *res,
           gpointer data)
{
  g_autoptr(ScreenshotCall) call = data;
  g_autoptr(GError) error = NULL;

  call->portal->screenshot = _xdp_screenshot_proxy_new_for_bus_finish (res, &error);
  if (call->portal->screenshot == NULL)
    {
      g_task_return_error (call->task, error);
      return;
    }

  do_screenshot (call);
}

static void
window_handle_exported (GtkWindow *window,
                        const char *window_handle,
                        gpointer data)
{
  g_autoptr(ScreenshotCall) call = data;

  call->parent_handle = g_strdup (window_handle);

  do_screenshot (call);
}

static void
response_received (GDBusConnection *bus,
                   const char *sender_name,
                   const char *object_path,
                   const char *interface_name,
                   const char *signal_name,
                   GVariant *parameters,
                   gpointer data)
{
  g_autoptr(ScreenshotCall) call = data;
  guint32 response;
  g_autoptr(GVariant) ret = NULL;

  g_dbus_connection_signal_unsubscribe (bus, call->response_signal);
  call->response_signal = 0;

  g_variant_get (parameters, "(u@a{sv})", &response, &ret);

  if (response == 0)
    {
      g_autoptr(GdkPixbuf) pixbuf = NULL;
      g_autoptr(GError) error = NULL;
      const char *uri;
      g_autofree char *path = NULL;

      g_variant_lookup (ret, "uri", "&s", &uri);
      path = g_filename_from_uri (uri, NULL, NULL);

      pixbuf = gdk_pixbuf_new_from_file (path, &error);

      if (pixbuf)
        g_task_return_pointer (call->task, g_object_ref (pixbuf), g_object_unref);
      else
        g_task_return_error (call->task, error);
    }
  else if (response == 1)
    g_task_return_new_error (call->task, G_IO_ERROR, G_IO_ERROR_CANCELLED, "Screenshot canceled");
  else
    g_task_return_new_error (call->task, G_IO_ERROR, G_IO_ERROR_FAILED, "Screenshot failed");
}

static void
do_screenshot (ScreenshotCall *call)
{
  GVariantBuilder options;
  GDBusConnection *bus;
  g_autofree char *token = NULL;
  g_autofree char *sender = NULL;
  g_autofree char *handle = NULL;
  int i;

  if (call->portal->screenshot == NULL)
    {
       _xdp_screenshot_proxy_new_for_bus (G_BUS_TYPE_SESSION,
                                          G_DBUS_PROXY_FLAGS_NONE,
                                          PORTAL_BUS_NAME,
                                          PORTAL_OBJECT_PATH,
                                          g_task_get_cancellable (call->task),
                                          got_proxy,
                                          g_object_ref (call));
       return;
    }

  if (call->parent_handle == NULL)
    {
      if (call->parent != NULL)
        {
          _gtk_window_export_handle (call->parent,
                                     window_handle_exported,
                                     g_object_ref (call));
          return;
        }

      call->parent_handle = g_strdup ("");
    }

  bus = g_dbus_proxy_get_connection (G_DBUS_PROXY (call->portal->screenshot));

  token = g_strdup_printf ("portal%d", g_random_int_range (0, G_MAXINT));
  sender = g_strdup (g_dbus_connection_get_unique_name (bus) + 1);
  for (i = 0; sender[i]; i++)
    if (sender[i] == '.')
      sender[i] = '_';

  handle = g_strdup_printf ("/org/freedesktop/portal/desktop/request/%s/%s", sender, token);
  call->response_signal = g_dbus_connection_signal_subscribe (bus,
                                                              PORTAL_BUS_NAME,
                                                              REQUEST_INTERFACE,
                                                              "Response",
                                                              handle,
                                                              NULL,
                                                              G_DBUS_SIGNAL_FLAGS_NO_MATCH_RULE,
                                                              response_received,
                                                              g_object_ref (call),
                                                              NULL);

  g_variant_builder_init (&options, G_VARIANT_TYPE_VARDICT);
  g_variant_builder_add (&options, "{sv}", "handle_token", g_variant_new_string (token));
  g_variant_builder_add (&options, "{sv}", "modal", g_variant_new_boolean (call->modal));
  g_variant_builder_add (&options, "{sv}", "interactive", g_variant_new_boolean (call->interactive));

  _xdp_screenshot_call_screenshot (call->portal->screenshot,
                                   call->parent_handle,
                                   g_variant_builder_end (&options),
                                   g_task_get_cancellable (call->task),
                                   NULL,
                                   NULL);
}

void
xdp_portal_take_screenshot (XdpPortal *portal,
                            GtkWindow *parent,
                            gboolean modal,
                            gboolean interactive,
                            GCancellable *cancellable,
                            GAsyncReadyCallback  callback,
                            gpointer callback_data)
{
  g_autoptr(ScreenshotCall) call = NULL;
  g_autoptr(GTask) task = NULL;

  task = g_task_new (portal, cancellable, callback, callback_data);
  call = screenshot_call_new (portal, parent, modal, interactive, task);

  do_screenshot (call);
}

GdkPixbuf *
xdp_portal_take_screenshot_finish (XdpPortal *portal,
                                   GAsyncResult *result,
                                   GError **error)
{
  return g_task_propagate_pointer (G_TASK (result), error);
}
