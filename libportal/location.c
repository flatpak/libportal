/*
 * Copyright (C) 2019, Matthias Clasen
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

#include "location-private.h"
#include "portal-private.h"
#include "utils-private.h"


/**
 * SECTION:location
 * @title: Location
 * @short_description: access to location information
 *
 * A location session makes location information available
 * via the #XdpLocation::updated signal.
 */

/**
 * SECTION:locationsession
 * @title: XdpLocation
 * @short_description: a representation of long-lived location portal interactions
 *
 * The XdpLocation object is used to represent an ongoing location portal interaction.
 *
 * All locations start in an initial state.
 * They can be made active by calling xdp_location_start(),
 * and ended by calling xdp_location_close().
 */
enum {
  CLOSED,
  UPDATED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE (XdpLocation, xdp_location, G_TYPE_OBJECT)

static void
xdp_location_finalize (GObject *object)
{
  XdpLocation *location = XDP_LOCATION (object);

  if (location->signal_id)
    g_dbus_connection_signal_unsubscribe (location->portal->bus, location->signal_id);

  if (location->updated_id)
    g_dbus_connection_signal_unsubscribe (location->portal->bus, location->updated_id);

  g_free (location->id);
  g_clear_object (&location->portal);

  G_OBJECT_CLASS (xdp_location_parent_class)->finalize (object);
}

static void
xdp_location_class_init (XdpLocationClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = xdp_location_finalize;

  /**
   * XdpLocation::closed:
   *
   * The ::closed signal is emitted when a location is closed externally.
   */
  signals[CLOSED] =
    g_signal_new ("closed",
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_CLEANUP | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS,
                  0,
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);

  /**
   * XdpLocation::updated:
   * @latitude: the latitude, in degrees
   * @longitude: the longitude, in degrees
   * @altitude: the altitude, in meters
   * @accuracy: the accuracy, in meters
   * @speed: the speed, in meters per second
   * @heading: the heading, in degrees
   * @timestamp_s: the timestamp seconds since the Unix epoch
   * @timestamp_ms: the microseconds fraction of the timestamp
   *
   * The ::updated signal is emitted when the location changes.
   */
  signals[UPDATED] =
    g_signal_new ("updated",
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_FIRST,
                  0,
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 8,
                  G_TYPE_INT,
                  G_TYPE_INT,
                  G_TYPE_INT,
                  G_TYPE_INT,
                  G_TYPE_INT,
                  G_TYPE_INT,
                  G_TYPE_INT64,
                  G_TYPE_INT64);
}

static void
xdp_location_init (XdpLocation *location)
{
}

static void
location_closed (GDBusConnection *bus,
                 const char *sender_name,
                 const char *object_path,
                 const char *interface_name,
                 const char *signal_name,
                 GVariant *parameters,
                 gpointer data)
{
  XdpLocation *location = data;

  if (location->updated_id)
    {
      g_dbus_connection_signal_unsubscribe (location->portal->bus, location->updated_id);
      location->updated_id = 0;
    }

  _xdp_location_set_session_state (location, XDP_SESSION_CLOSED);
}

XdpLocation *
_xdp_location_new (XdpPortal  *portal,
                   const char *id)
{
  XdpLocation *location;

  location = g_object_new (XDP_TYPE_SESSION, NULL);
  location->portal = g_object_ref (portal);
  location->id = g_strdup (id);
  location->state = XDP_SESSION_INITIAL;

  location->signal_id = g_dbus_connection_signal_subscribe (portal->bus,
                                                            PORTAL_BUS_NAME,
                                                            SESSION_INTERFACE,
                                                            "Closed",
                                                            id,
                                                            NULL,
                                                            G_DBUS_SIGNAL_FLAGS_NO_MATCH_RULE,
                                                            location_closed,
                                                            location,
                                                            NULL);
  return location;
}

/**
 * xdp_location_get_session_state:
 * @location: an #XdpLocation
 *
 * Obtains information about the state of the location that is
 * represented by @location.
 * 
 * Returns: the state of @location
 */
XdpSessionState
xdp_location_get_session_state (XdpLocation *location)
{
  g_return_val_if_fail (XDP_IS_LOCATION (location), XDP_SESSION_CLOSED);

  return location->state;
}

void
_xdp_location_set_session_state (XdpLocation *location,
                                 XdpSessionState state)
{
  location->state = state;

  if (state == XDP_SESSION_INITIAL && location->state != XDP_SESSION_INITIAL)
    {
      g_warning ("Can't move a location back to initial state");
      return;
    }
  if (location->state == XDP_SESSION_CLOSED && state != XDP_SESSION_CLOSED)
    {
      g_warning ("Can't move a location back from closed state");
      return;
    }

  if (state == XDP_SESSION_CLOSED)
    g_signal_emit (location, signals[CLOSED], 0);
}

typedef struct {
  XdpPortal *portal;
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
session_created (GDBusConnection *bus,
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

  g_variant_get (parameters, "(u@a{sv})", &response, &ret);

  if (response == 0)
    {
      XdpLocation *location;

      g_dbus_connection_signal_unsubscribe (call->portal->bus, call->signal_id);
      call->signal_id = 0;

      location = _xdp_location_new (call->portal, call->id);
      g_task_return_pointer (call->task, location, g_object_unref);
    }
  else if (response == 1)
    g_task_return_new_error (call->task, G_IO_ERROR, G_IO_ERROR_CANCELLED, "CreateSession canceled");
  else if (response == 2)
    g_task_return_new_error (call->task, G_IO_ERROR, G_IO_ERROR_FAILED, "CreateSession failed");

  if (response != 0)
    create_call_free (call);
}

static void
create_cancelled_cb (GCancellable *cancellable,
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
}

static void
create_session (CreateCall *call)
{
  GVariantBuilder options;
  g_autofree char *token = NULL;
  g_autofree char *session_token = NULL;
  GCancellable *cancellable;

  token = g_strdup_printf ("portal%d", g_random_int_range (0, G_MAXINT));
  call->request_path = g_strconcat (REQUEST_PATH_PREFIX, call->portal->sender, "/", token, NULL);
  call->signal_id = g_dbus_connection_signal_subscribe (call->portal->bus,
                                                        PORTAL_BUS_NAME,
                                                        REQUEST_INTERFACE,
                                                        "Response",
                                                        call->request_path,
                                                        NULL,
                                                        G_DBUS_SIGNAL_FLAGS_NO_MATCH_RULE,
                                                        session_created,
                                                        call,
                                                        NULL);

  session_token = g_strdup_printf ("portal%d", g_random_int_range (0, G_MAXINT));
  call->id = g_strconcat (SESSION_PATH_PREFIX, call->portal->sender, "/", session_token, NULL);

  cancellable = g_task_get_cancellable (call->task);

  if (cancellable)
    call->cancelled_id = g_signal_connect (cancellable, "cancelled", G_CALLBACK (create_cancelled_cb), call);

  g_variant_builder_init (&options, G_VARIANT_TYPE_VARDICT);
  g_variant_builder_add (&options, "{sv}", "handle_token", g_variant_new_string (token));
  g_variant_builder_add (&options, "{sv}", "session_handle_token", g_variant_new_string (session_token));
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
                          NULL,
                          NULL);
}

void
xdp_portal_create_location (XdpPortal           *portal,
                            guint                distance_threshold,
                            guint                time_threshold,
                            XdpLocationAccuracy  accuracy,
                            GCancellable        *cancellable,
                            GAsyncReadyCallback  callback,
                            gpointer             data)
{
  CreateCall *call;

  g_return_if_fail (XDP_IS_PORTAL (portal));

  call = g_new0 (CreateCall, 1);
  call->portal = g_object_ref (portal);
  call->distance = distance_threshold;
  call->time = time_threshold;
  call->accuracy = accuracy;
  call->task = g_task_new (portal, cancellable, callback, data);

  create_session (call);
}

XdpLocation *
xdp_portal_create_location_finish (XdpPortal *portal,
                                   GAsyncResult *result,
                                   GError **error)
{
  g_return_val_if_fail (XDP_IS_PORTAL (portal), NULL);
  g_return_val_if_fail (g_task_is_valid (result, portal), NULL);

  return g_object_ref (g_task_propagate_pointer (G_TASK (result), error));
}

typedef struct {
  XdpPortal *portal;
  XdpLocation *location;
  XdpParent *parent;
  char *parent_handle;
  guint signal_id;
  GTask *task;
  char *request_path;
  guint cancelled_id;
} StartCall;

static void
start_call_free (StartCall *call)
{
  if (call->parent)
    {
      call->parent->unexport (call->parent);
      _xdp_parent_free (call->parent);
    }
  g_free (call->parent_handle);

  if (call->signal_id)
    g_dbus_connection_signal_unsubscribe (call->portal->bus, call->signal_id);

  if (call->cancelled_id)
    g_signal_handler_disconnect (g_task_get_cancellable (call->task), call->cancelled_id);

  g_free (call->request_path);

  g_object_unref (call->location);
  g_object_unref (call->portal);
  g_object_unref (call->task);

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
  StartCall *call = data;
  guint32 response;
  g_autoptr(GVariant) ret = NULL;

  g_variant_get (parameters, "(u@a{sv})", &response, &ret);

  if (response == 0)
    g_task_return_boolean (call->task, TRUE);
  else if (response == 1)
    g_task_return_new_error (call->task, G_IO_ERROR, G_IO_ERROR_CANCELLED, "Location canceled");
  else if (response == 2)
    g_task_return_new_error (call->task, G_IO_ERROR, G_IO_ERROR_FAILED, "Location failed");

  _xdp_location_set_session_state (call->location, response == 0 ? XDP_SESSION_ACTIVE
                                                               : XDP_SESSION_CLOSED);

  start_call_free (call);
}

static void start_session (StartCall *call);

static void
parent_exported (XdpParent *parent,
                 const char *handle,
                 gpointer data)
{
  StartCall *call = data;
  call->parent_handle = g_strdup (handle);
  start_session (call);
}

static void
start_cancelled_cb (GCancellable *cancellable,
                    gpointer data)
{
  StartCall *call = data;

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
  XdpLocation *location = data;
  int latitude, longitude, altitude;
  int accuracy, speed, heading;
  gint64 timestamp_s, timestamp_ms;

  g_variant_get (parameters, "(ddddddtt)",
                 &latitude, &longitude, &altitude,
                 &accuracy, &speed, &heading,
                 &timestamp_s, &timestamp_ms);
  g_signal_emit (location, signals[UPDATED], 0,
                 latitude, longitude, altitude,
                 accuracy, speed, heading,
                 timestamp_s, timestamp_ms);
}

static void
start_session (StartCall *call)
{
  GVariantBuilder options;
  g_autofree char *token = NULL;
  GCancellable *cancellable;

  if (call->parent_handle == NULL)
    {
      call->parent->export (call->parent, parent_exported, call);
      return;
    }

  call->location->updated_id = g_dbus_connection_signal_subscribe (call->portal->bus,
                                                                   PORTAL_BUS_NAME,
                                                                   "org.freedesktop.portal.Location",
                                                                   "LocationUpdated",
                                                                   call->location->id,
                                                                   NULL,
                                                                   G_DBUS_SIGNAL_FLAGS_NO_MATCH_RULE,
                                                                   location_updated,
                                                                   call->location,
                                                                   NULL);

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

  cancellable = g_task_get_cancellable (call->task);
  if (cancellable)
    call->cancelled_id = g_signal_connect (cancellable, "cancelled", G_CALLBACK (start_cancelled_cb), call);

  g_variant_builder_init (&options, G_VARIANT_TYPE_VARDICT);
  g_variant_builder_add (&options, "{sv}", "handle_token", g_variant_new_string (token));
  g_dbus_connection_call (call->portal->bus,
                          PORTAL_BUS_NAME,
                          PORTAL_OBJECT_PATH,
                          "org.freedesktop.portal.Location",
                          "Start",
                          g_variant_new ("(osa{sv})",
                                         call->location->id,
                                         call->parent_handle,
                                         &options),
                          NULL,
                          G_DBUS_CALL_FLAGS_NONE,
                          -1,
                          cancellable,
                          NULL,
                          NULL);
}

void
xdp_location_start (XdpLocation         *location,
                    XdpParent           *parent,
                    GCancellable        *cancellable,
                    GAsyncReadyCallback  callback,
                    gpointer             data)
{
  StartCall *call = NULL;

  g_return_if_fail (XDP_IS_LOCATION (location));

  call = g_new0 (StartCall, 1);
  call->portal = g_object_ref (location->portal);
  call->location = g_object_ref (location);
  if (parent)
    call->parent = _xdp_parent_copy (parent);
  else
    call->parent_handle = g_strdup ("");
  call->task = g_task_new (location, cancellable, callback, data);

  start_session (call);
}

gboolean
xdp_location_start_finish (XdpLocation   *location,
                           GAsyncResult  *result,
                           GError       **error)
{
  g_return_val_if_fail (XDP_IS_LOCATION (location), FALSE);
  g_return_val_if_fail (g_task_is_valid (result, location), FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

void
xdp_location_close (XdpLocation *location)
{
  g_return_if_fail (XDP_IS_LOCATION (location));

  g_dbus_connection_call (location->portal->bus,
                          PORTAL_BUS_NAME,
                          location->id,
                          SESSION_INTERFACE,
                          "Close",
                          NULL,
                          NULL, 0, -1, NULL, NULL, NULL);

  if (location->updated_id)
    {
      g_dbus_connection_signal_unsubscribe (location->portal->bus, location->updated_id);
      location->updated_id = 0;
    }

  _xdp_location_set_session_state (location, XDP_SESSION_CLOSED);
  g_signal_emit_by_name (location, "closed");
}

