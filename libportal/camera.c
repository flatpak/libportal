/*
 * Copyright (C) 2019, Matthias Clasen
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
 */

#include "config.h"

#include <gio/gunixfdlist.h>
#include "camera.h"
#include "session-private.h"
#include "portal-private.h"

/**
 * xdp_portal_is_camera_present:
 * @portal: a [class@Portal]
 *
 * Returns whether any camera are present.
 *
 * Returns: `TRUE` if the system has cameras
 */
gboolean
xdp_portal_is_camera_present (XdpPortal *portal)
{
  g_autoptr(GError) error = NULL;
  g_autoptr(GVariant) ret = NULL;
  g_autoptr(GVariant) prop = NULL;

  g_return_val_if_fail (XDP_IS_PORTAL (portal), FALSE);

  ret = g_dbus_connection_call_sync (portal->bus,
                                     PORTAL_BUS_NAME,
                                     PORTAL_OBJECT_PATH,
                                     "org.freedesktop.DBus.Properties",
                                     "Get",
                                     g_variant_new ("(ss)", "org.freedesktop.portal.Camera", "IsCameraPresent"),
                                     G_VARIANT_TYPE ("(v)"),
                                     G_DBUS_CALL_FLAGS_NONE,
                                     -1,
                                     NULL,
                                     &error);
  if (!ret)
    {
      g_warning ("Failed to get IsCameraPresent property: %s", error->message);
      return FALSE;
    }

  g_variant_get (ret, "(v)", &prop);

  return g_variant_get_boolean (prop);
}

typedef struct {
  XdpPortal *portal;
  guint signal_id;
  GCancellable *cancellable;
  GTask *task;
  char *request_path;
  guint cancelled_id;
} AccessCameraCall;

static void
access_camera_call_free (AccessCameraCall *call)
{
  if (call->signal_id)
    g_dbus_connection_signal_unsubscribe (call->portal->bus, call->signal_id);

  if (call->cancelled_id)
    g_signal_handler_disconnect (call->cancellable, call->cancelled_id);

  g_free (call->request_path);

  if (call->cancellable)
    g_object_unref (call->cancellable);
  g_object_unref (call->portal);
  g_object_unref (call->task);

  g_free (call);
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
  AccessCameraCall *call = data;
  guint32 response;
  g_autoptr(GVariant) ret = NULL;

  if (call->cancelled_id)
    {
      g_signal_handler_disconnect (call->cancellable, call->cancelled_id);
      call->cancelled_id = 0;
    }

  g_variant_get (parameters, "(u@a{sv})", &response, &ret);

  if (response == 0)
    g_task_return_boolean (call->task, TRUE);
  else if (response == 1)
    g_task_return_new_error (call->task, G_IO_ERROR, G_IO_ERROR_CANCELLED, "Camera access canceled");
  else
    g_task_return_new_error (call->task, G_IO_ERROR, G_IO_ERROR_FAILED, "Camera access failed");

  access_camera_call_free (call);
}

static void
cancelled_cb (GCancellable *cancellable,
              gpointer data)
{
  AccessCameraCall *call = data;

  g_debug ("Calling Close");
  g_dbus_connection_call (call->portal->bus,
                          PORTAL_BUS_NAME,
                          call->request_path,
                          REQUEST_INTERFACE,
                          "Close",
                          NULL,
                          NULL,
                          G_DBUS_CALL_FLAGS_NONE,
                          -1,
                          NULL, NULL, NULL);

  g_task_return_new_error (call->task, G_IO_ERROR, G_IO_ERROR_CANCELLED, "AccessCamera call canceled by caller");

  access_camera_call_free (call);
}

static void
call_returned (GObject *object,
               GAsyncResult *result,
               gpointer data)
{
  AccessCameraCall *call = data;
  GError *error = NULL;
  g_autoptr(GVariant) ret = NULL;

  ret = g_dbus_connection_call_finish (G_DBUS_CONNECTION (object), result, &error);
  if (error)
    {
      if (call->cancelled_id)
        {
          g_signal_handler_disconnect (g_task_get_cancellable (call->task), call->cancelled_id);
          call->cancelled_id = 0;
        }

      g_task_return_error (call->task, error);
      access_camera_call_free (call);
    }
}

static void
access_camera (AccessCameraCall *call)
{
  GVariantBuilder options;
  g_autofree char *token = NULL;

  token = g_strdup_printf ("portal%d", g_random_int_range (0, G_MAXINT));
  call->request_path = g_strconcat (REQUEST_PATH_PREFIX, call->portal->sender, "/", token, NULL);
  call->signal_id = g_dbus_connection_signal_subscribe (call->portal->bus,
                                                        PORTAL_BUS_NAME,
                                                        REQUEST_INTERFACE,
                                                        "Response",
                                                        call->request_path,
                                                        NULL,
                                                        G_DBUS_SIGNAL_FLAGS_NO_MATCH_RULE,
                                                        response_received,
                                                        call,
                                                        NULL);

  if (call->cancellable)
    call->cancelled_id = g_signal_connect (call->cancellable, "cancelled", G_CALLBACK (cancelled_cb), call);

  g_variant_builder_init (&options, G_VARIANT_TYPE_VARDICT);
  g_variant_builder_add (&options, "{sv}", "handle_token", g_variant_new_string (token));

  g_debug ("Calling AccessCamera");
  g_dbus_connection_call (call->portal->bus,
                          PORTAL_BUS_NAME,
                          PORTAL_OBJECT_PATH,
                          "org.freedesktop.portal.Camera",
                          "AccessCamera",
                          g_variant_new ("(a{sv})", &options),
                          NULL,
                          G_DBUS_CALL_FLAGS_NONE,
                          -1,
                          NULL,
                          call_returned,
                          call);
}

/**
 * xdp_portal_access_camera:
 * @portal: a [class@Portal]
 * @parent: (nullable): parent window information
 * @flags: options for this call
 * @cancellable: (nullable): optional [class@Gio.Cancellable]
 * @callback: (scope async): a callback to call when the request is done
 * @data: (closure): data to pass to @callback
 *
 * Request access to a camera.
 *
 * When the request is done, @callback will be called.
 * You can then call [method@Portal.access_camera_finish]
 * to get the results.
 */
void
xdp_portal_access_camera (XdpPortal           *portal,
                          XdpParent           *parent,
                          XdpCameraFlags       flags,
                          GCancellable        *cancellable,
                          GAsyncReadyCallback  callback,
                          gpointer             data)
{
  AccessCameraCall *call;

  g_return_if_fail (XDP_IS_PORTAL (portal));
  g_return_if_fail (flags == XDP_CAMERA_FLAG_NONE);

  call = g_new0 (AccessCameraCall, 1);
  call->portal = g_object_ref (portal);
  if (cancellable)
    call->cancellable = g_object_ref (cancellable);
  call->task = g_task_new (portal, NULL, callback, data);
  g_task_set_source_tag (call->task, xdp_portal_access_camera);

  access_camera (call);
}

/**
 * xdp_portal_access_camera_finish:
 * @portal: a [class@Portal]
 * @result: a [iface@Gio.AsyncResult]
 * @error: return location for an error
 *
 * Finishes a camera acess request.
 *
 * Returns the result as a boolean.
 *
 * If the access was granted, you can then call
 * [method@Portal.open_pipewire_remote_for_camera]
 * to obtain a pipewire remote.
 *
 * Returns: `TRUE` if access to a camera was granted
 */
gboolean
xdp_portal_access_camera_finish (XdpPortal     *portal,
                                 GAsyncResult  *result,
                                 GError       **error)
{
  g_return_val_if_fail (XDP_IS_PORTAL (portal), FALSE);
  g_return_val_if_fail (g_task_is_valid (result, portal), FALSE);
  g_return_val_if_fail (g_task_get_source_tag (G_TASK (result)) == xdp_portal_access_camera, FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

/**
 * xdp_portal_open_pipewire_remote_for_camera:
 * @portal: a [class@Portal]
 *
 * Opens a file descriptor to the pipewire remote where the camera
 * nodes are available.
 *
 * The file descriptor should be used to create a pw_core object, by using
 * pw_context_connect_fd(). Only the camera nodes will be available from this
 * pipewire node.
 *
 * Returns: the file descriptor
 */
int
xdp_portal_open_pipewire_remote_for_camera (XdpPortal *portal)
{
  GVariantBuilder options;
  g_autoptr(GError) error = NULL;
  g_autoptr(GVariant) ret = NULL;
  g_autoptr(GUnixFDList) fd_list = NULL;
  int fd_out;

  g_return_val_if_fail (XDP_IS_PORTAL (portal), -1);

  g_variant_builder_init (&options, G_VARIANT_TYPE_VARDICT);
  ret = g_dbus_connection_call_with_unix_fd_list_sync (portal->bus,
                                                       PORTAL_BUS_NAME,
                                                       PORTAL_OBJECT_PATH,
                                                       "org.freedesktop.portal.Camera",
                                                       "OpenPipeWireRemote",
                                                       g_variant_new ("(a{sv})", &options),
                                                       G_VARIANT_TYPE ("(h)"),
                                                       G_DBUS_CALL_FLAGS_NONE,
                                                       -1,
                                                       NULL,
                                                       &fd_list,
                                                       NULL,
                                                       &error);

  if (ret == NULL)
    {
      g_warning ("Failed to get pipewire fd: %s", error->message);
      return -1;
    }

  g_variant_get (ret, "(h)", &fd_out);

  return g_unix_fd_list_get (fd_list, fd_out, NULL);
}
