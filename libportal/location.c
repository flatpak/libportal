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

#include "location.h"
#include "portal-private.h"


typedef struct {
  XdpPortal *portal;
  XdpParent *parent;
  char *parent_handle;
  char *id;
  guint signal_id;
  GTask *task;
  char *request_path;
  guint cancelled_id;
  int distance;
  int time;
  XdpLocationAccuracy accuracy;
} CreateCall;

static void
create_call_free (CreateCall *call)
{
  if (call->parent)
    {
      call->parent->parent_unexport (call->parent);
      xdp_parent_free (call->parent);
    }
  g_free (call->parent_handle);

  if (call->signal_id)
    g_dbus_connection_signal_unsubscribe (call->portal->bus, call->signal_id);

  if (call->cancelled_id)
    g_signal_handler_disconnect (g_task_get_cancellable (call->task), call->cancelled_id);

  g_free (call->request_path);

  g_object_unref (call->portal);
  g_object_unref (call->task);

  g_free (call->id);

  g_free (call);
}

static void
session_started (GDBusConnection *bus,
                 const char *sender_name,
                 const char *object_path,
                 const char *interface_name,
                 const char *signal_name,
                 GVariant *parameters,
                 gpointer data)
{
  CreateCall *call = data;
  guint32 response;
  g_autoptr(GVariant) ret = NULL;

  if (call->cancelled_id)
    {
      g_signal_handler_disconnect (g_task_get_cancellable (call->task), call->cancelled_id);
      call->cancelled_id = 0;
    }

  g_variant_get (parameters, "(u@a{sv})", &response, &ret);

  if (response == 0)
    g_task_return_boolean (call->task, TRUE);
  else
    {
      if (call->portal->location_updated_signal != 0)
        {
          g_dbus_connection_signal_unsubscribe (call->portal->bus, call->portal->location_updated_signal);
          call->portal->location_updated_signal = 0;
        }
      g_clear_pointer (&call->portal->location_monitor_handle, g_free);

      if (response == 1)
        g_task_return_new_error (call->task, G_IO_ERROR, G_IO_ERROR_CANCELLED, "StartLocation canceled");
      else if (response == 2)
        g_task_return_new_error (call->task, G_IO_ERROR, G_IO_ERROR_FAILED, "StartLocation failed");
    }

  create_call_free (call);
}

static void
location_updated (GDBusConnection *bus,
                  const char *sender_name,
                  const char *object_path,
                  const char *interface_name,
                  const char *signal_name,
                  GVariant *parameters,
                  gpointer data)
{
  XdpPortal *portal = data;
  g_autoptr(GVariant) variant = NULL;
  const char *handle = NULL;
  double latitude, longitude, altitude;
  double accuracy, speed, heading;
  const char *description = NULL;
  gint64 timestamp_s, timestamp_ms;

  g_variant_get (parameters, "(o@a{sv})", &handle, &variant);
  g_variant_lookup (variant, "Latitude", "d", &latitude);
  g_variant_lookup (variant, "Longitude", "d", &longitude);
  g_variant_lookup (variant, "Accuracy", "d", &accuracy);
  g_variant_lookup (variant, "Altitude", "d", &altitude);
  g_variant_lookup (variant, "Speed", "d", &speed);
  g_variant_lookup (variant, "Heading", "d", &heading);
  g_variant_lookup (variant, "Description", "&s", &description);
  g_variant_lookup (variant, "Timestamp", "(tt)", &timestamp_s, &timestamp_ms);

  g_signal_emit_by_name (portal, "location-updated",
                         latitude, longitude, altitude,
                         accuracy, speed, heading,
                         description, timestamp_s, timestamp_ms);
}

static void
ensure_location_updated_connected (XdpPortal *portal)
{
  if (portal->location_updated_signal == 0)
    portal->location_updated_signal =
        g_dbus_connection_signal_subscribe (portal->bus,
                                            PORTAL_BUS_NAME,
                                            "org.freedesktop.portal.Location",
                                            "LocationUpdated",
                                            PORTAL_OBJECT_PATH,
                                            NULL,
                                            G_DBUS_SIGNAL_FLAGS_NO_MATCH_RULE,
                                            location_updated,
                                            portal,
                                            NULL);
}

static void
cancelled_cb (GCancellable *cancellable,
              gpointer data)
{
  CreateCall *call = data;

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

  g_task_return_new_error (call->task, G_IO_ERROR, G_IO_ERROR_CANCELLED, "Location call canceled by caller");

  create_call_free (call);
}

static void
call_returned (GObject *object,
               GAsyncResult *result,
               gpointer data)
{
  CreateCall *call = data;
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
      create_call_free (call);
    }
}

static void
session_created (GObject *object,
                 GAsyncResult *result,
                 gpointer data)
{
  CreateCall *call = data;
  GVariantBuilder options;
  g_autofree char *token = NULL;
  g_autoptr(GVariant) ret = NULL;
  GError *error = NULL;
  GCancellable *cancellable;

  ret = g_dbus_connection_call_finish (G_DBUS_CONNECTION (object), result, &error);

  if (error)
    {
      g_task_return_error (call->task, error);

      create_call_free (call);
      return;
    }

  token = g_strdup_printf ("portal%d", g_random_int_range (0, G_MAXINT));
  call->request_path = g_strconcat (REQUEST_PATH_PREFIX, call->portal->sender, "/", token, NULL);
  call->signal_id = g_dbus_connection_signal_subscribe (call->portal->bus,
                                                        PORTAL_BUS_NAME,
                                                        REQUEST_INTERFACE,
                                                        "Response",
                                                        call->request_path,
                                                        NULL,
                                                        G_DBUS_SIGNAL_FLAGS_NO_MATCH_RULE,
                                                        session_started,
                                                        call,
                                                        NULL);

  g_variant_get (ret, "(o)", &call->portal->location_monitor_handle);
  ensure_location_updated_connected (call->portal);

  cancellable = g_task_get_cancellable (call->task);

  if (cancellable)
    call->cancelled_id = g_signal_connect (cancellable, "cancelled", G_CALLBACK (cancelled_cb), call);

  g_variant_builder_init (&options, G_VARIANT_TYPE_VARDICT);
  g_variant_builder_add (&options, "{sv}", "handle_token", g_variant_new_string (token));
  g_dbus_connection_call (call->portal->bus,
                          PORTAL_BUS_NAME,
                          PORTAL_OBJECT_PATH,
                          "org.freedesktop.portal.Location",
                          "Start",
                          g_variant_new ("(osa{sv})",
                                         call->id,
                                         call->parent_handle,
                                         &options),
                          NULL,
                          G_DBUS_CALL_FLAGS_NONE,
                          -1,
                          NULL,
                          call_returned,
                          call);
}

static void create_session (CreateCall *call);

static void
parent_exported (XdpParent *parent,
                 const char *handle,
                 gpointer data)
{
  CreateCall *call = data;
  call->parent_handle = g_strdup (handle);
  create_session (call);
}

static void
create_session (CreateCall *call)
{
  GVariantBuilder options;
  g_autofree char *session_token = NULL;
  GCancellable *cancellable;

  if (call->portal->location_monitor_handle)
    {
      g_task_return_boolean (call->task, TRUE);
      create_call_free (call);
      return;
    }

  if (call->parent_handle == NULL)
    {
      call->parent->parent_export (call->parent, parent_exported, call);
      return;
    }

  session_token = g_strdup_printf ("portal%d", g_random_int_range (0, G_MAXINT));
  call->id = g_strconcat (SESSION_PATH_PREFIX, call->portal->sender, "/", session_token, NULL);

  cancellable = g_task_get_cancellable (call->task);

  g_variant_builder_init (&options, G_VARIANT_TYPE_VARDICT);
  g_variant_builder_add (&options, "{sv}", "session_handle_token", g_variant_new_string (session_token));
  g_variant_builder_add (&options, "{sv}", "distance-threshold", g_variant_new_uint32 (call->distance));
  g_variant_builder_add (&options, "{sv}", "time-threshold", g_variant_new_uint32 (call->time));
  g_variant_builder_add (&options, "{sv}", "accuracy", g_variant_new_uint32 (call->accuracy));
  g_dbus_connection_call (call->portal->bus,
                          PORTAL_BUS_NAME,
                          PORTAL_OBJECT_PATH,
                          "org.freedesktop.portal.Location",
                          "CreateSession",
                          g_variant_new ("(a{sv})", &options),
                          NULL,
                          G_DBUS_CALL_FLAGS_NONE,
                          -1,
                          cancellable,
                          session_created,
                          call);
}

/**
 * xdp_portal_location_monitor_start:
 * @portal: a [class@Portal]
 * @parent: (nullable): a [struct@Parent], or `NULL`
 * @distance_threshold: distance threshold, in meters
 * @time_threshold: time threshold, in seconds
 * @accuracy: desired accuracy
 * @flags: options for this call
 * @cancellable: (nullable): optional [class@Gio.Cancellable]
 * @callback: (scope async): a callback to call when the request is done
 * @data: (closure): data to pass to @callback
 *
 * Makes `XdpPortal` start monitoring location changes.
 *
 * When the location changes, the [signal@Portal::location-updated].
 * signal is emitted.
 *
 * Use [method@Portal.location_monitor_stop] to stop monitoring.
 *
 * Note that [class@Portal] only maintains a single location monitor
 * at a time. If you want to change the @distance_threshold,
 * @time_threshold or @accuracy of the current monitor, you
 * first have to call [method@Portal.location_monitor_stop] to
 * stop monitoring.
 */
void
xdp_portal_location_monitor_start (XdpPortal *portal,
                                   XdpParent *parent,
                                   guint distance_threshold,
                                   guint time_threshold,
                                   XdpLocationAccuracy accuracy,
                                   XdpLocationMonitorFlags flags,
                                   GCancellable *cancellable,
                                   GAsyncReadyCallback callback,
                                   gpointer data)
{
  CreateCall *call;

  g_return_if_fail (XDP_IS_PORTAL (portal));
  g_return_if_fail (flags == XDP_LOCATION_MONITOR_FLAG_NONE);

  call = g_new0 (CreateCall, 1);
  call->portal = g_object_ref (portal);
  if (parent)
    call->parent = xdp_parent_copy (parent);
  else
    call->parent_handle = g_strdup ("");
  call->distance = distance_threshold;
  call->time = time_threshold;
  call->accuracy = accuracy;
  call->task = g_task_new (portal, cancellable, callback, data);
  g_task_set_source_tag (call->task, xdp_portal_location_monitor_start);

  create_session (call);
}

/**
 * xdp_portal_location_monitor_start_finish:
 * @portal: a [class@Portal]
 * @result: a [iface@Gio.AsyncResult]
 * @error: return location for an error
 *
 * Finishes a location-monitor request.
 *
 * Returns result in the form of boolean.
 *
 * Returns: `TRUE` if the request succeeded
 */
gboolean
xdp_portal_location_monitor_start_finish (XdpPortal *portal,
                                          GAsyncResult *result,
                                          GError **error)
{
  g_return_val_if_fail (XDP_IS_PORTAL (portal), FALSE);
  g_return_val_if_fail (g_task_is_valid (result, portal), FALSE);
  g_return_val_if_fail (g_task_get_source_tag (G_TASK (result)) == xdp_portal_location_monitor_start, FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

/**
 * xdp_portal_location_monitor_stop:
 * @portal: a [class@Portal]
 *
 * Stops location monitoring that was started with
 * [method@Portal.location_monitor_start].
 */
void
xdp_portal_location_monitor_stop (XdpPortal *portal)
{
  g_return_if_fail (XDP_IS_PORTAL (portal));

  if (portal->location_monitor_handle != NULL)
    {
      g_dbus_connection_call (portal->bus,
                              PORTAL_BUS_NAME,
                              portal->location_monitor_handle,
                              SESSION_INTERFACE,
                              "Close",
                              NULL,
                              NULL, 0, -1, NULL, NULL, NULL);
      g_clear_pointer (&portal->location_monitor_handle, g_free);
    }

  if (portal->location_updated_signal)
    {
      g_dbus_connection_signal_unsubscribe (portal->bus, portal->location_updated_signal);
      portal->location_updated_signal = 0;
    }
}
