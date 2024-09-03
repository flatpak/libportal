/*
 * Copyright (C) 2022, Red Hat, Inc.
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
#include <stdlib.h>
#include <errno.h>

#include "inputcapture.h"
#include "inputcapture-private.h"
#include "portal-private.h"
#include "session-private.h"

/**
 * XdpInputCaptureSession
 *
 * A representation of a long-lived input capture portal interaction.
 *
 * The [class@InputCaptureSession] object is used to represent portal
 * interactions with the input capture desktop portal that extend over
 * multiple portal calls. Usually a caller creates an input capture session,
 * requests the available zones and sets up pointer barriers on those zones
 * before enabling the session.
 *
 * To find available zones, call [method@InputCaptureSession.get_zones].
 * These [class@InputCaptureZone] object represent the accessible desktop area
 * for input capturing. [class@InputCapturePointerBarrier] objects can be set
 * up on these zones to trigger input capture.
 *
 * The [class@InputCaptureSession] wraps a [class@Session] object.
 */

enum {
  SIGNAL_CLOSED,
  SIGNAL_ACTIVATED,
  SIGNAL_DEACTIVATED,
  SIGNAL_ZONES_CHANGED,
  SIGNAL_DISABLED,
  SIGNAL_LAST_SIGNAL
};

static guint signals[SIGNAL_LAST_SIGNAL];

struct _XdpInputCaptureSession
{
  GObject parent_instance;
  XdpSession *parent_session; /* strong ref */

  GList *zones;

  guint signal_ids[SIGNAL_LAST_SIGNAL];
  guint zone_serial;
  guint zone_set;
};

G_DEFINE_TYPE (XdpInputCaptureSession, xdp_input_capture_session, G_TYPE_OBJECT)

static gboolean
_xdp_input_capture_session_is_valid (XdpInputCaptureSession *session)
{
  return XDP_IS_INPUT_CAPTURE_SESSION (session) && session->parent_session != NULL;
}

static void
parent_session_destroy (gpointer data, GObject *old_session)
{
  XdpInputCaptureSession *session = XDP_INPUT_CAPTURE_SESSION (data);

  g_critical ("XdpSession destroyed before XdpInputCaptureSesssion, you lost count of your session refs");

  session->parent_session = NULL;
}

static void
xdp_input_capture_session_finalize (GObject *object)
{
  XdpInputCaptureSession *session = XDP_INPUT_CAPTURE_SESSION (object);
  XdpSession *parent_session = session->parent_session;

  if (parent_session == NULL)
    {
      g_critical ("XdpSession destroyed before XdpInputCaptureSesssion, you lost count of your session refs");
    }
  else
    {
      for (guint i = 0; i < SIGNAL_LAST_SIGNAL; i++)
        {
          guint signal_id = session->signal_ids[i];
          if (signal_id > 0)
            g_dbus_connection_signal_unsubscribe (parent_session->portal->bus, signal_id);
        }

      g_object_weak_unref (G_OBJECT (parent_session), parent_session_destroy, session);
      session->parent_session->input_capture_session = NULL;
      g_clear_pointer (&session->parent_session, g_object_unref);
    }

  g_list_free_full (g_steal_pointer (&session->zones), g_object_unref);

  G_OBJECT_CLASS (xdp_input_capture_session_parent_class)->finalize (object);
}

static void
xdp_input_capture_session_class_init (XdpInputCaptureSessionClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = xdp_input_capture_session_finalize;

  /**
   * XdpInputCaptureSession::zones-changed:
   * @session: the [class@InputCaptureSession]
   * @options: a GVariant with the signal options
   *
   * Emitted when an InputCapture session's zones have changed. When this
   * signal is emitted, all current zones will have their
   * [property@InputCaptureZone:is-valid] property set to %FALSE and all
   * internal references to those zones have been released. This signal is
   * sent after libportal has fetched the updated zones, a caller should call
   * xdp_input_capture_session_get_zones() to retrieve the new zones.
   */
  signals[SIGNAL_ZONES_CHANGED] =
    g_signal_new ("zones-changed",
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_CLEANUP | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS,
                  0,
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 1,
                  G_TYPE_VARIANT);
  /**
   * XdpInputCaptureSession::activated:
   * @session: the [class@InputCaptureSession]
   * @activation_id: the unique activation_id to identify this input capture
   * @options: a GVariant with the signal options
   *
   * Emitted when an InputCapture session activates and sends events. When this
   * signal is emitted, events will appear on the transport layer.
   */
  signals[SIGNAL_ACTIVATED] =
    g_signal_new ("activated",
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_CLEANUP | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS,
                  0,
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 2,
                  G_TYPE_UINT,
                  G_TYPE_VARIANT);
  /**
   * XdpInputCaptureSession::deactivated:
   * @session: the [class@InputCaptureSession]
   * @activation_id: the unique activation_id to identify this input capture
   * @options: a GVariant with the signal options
   *
   * Emitted when an InputCapture session deactivates and no longer sends
   * events.
   */
  signals[SIGNAL_DEACTIVATED] =
    g_signal_new ("deactivated",
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_CLEANUP | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS,
                  0,
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 2,
                  G_TYPE_UINT,
                  G_TYPE_VARIANT);

  /**
   * XdpInputCaptureSession::disabled:
   * @session: the [class@InputCaptureSession]
   * @options: a GVariant with the signal options
   *
   * Emitted when an InputCapture session is disabled. This signal
   * is emitted when capturing was disabled by the server.
   */
  signals[SIGNAL_DISABLED] =
    g_signal_new ("disabled",
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_CLEANUP | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS,
                  0,
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 1,
                  G_TYPE_VARIANT);
}

static void
xdp_input_capture_session_init (XdpInputCaptureSession *session)
{
  session->parent_session = NULL;
  session->zones = NULL;
  session->zone_set = 0;
  for (guint i = 0; i < SIGNAL_LAST_SIGNAL; i++)
    session->signal_ids[i] = 0;
}

/* A request-based method call */
typedef struct {
  XdpPortal *portal;
  char *session_path; /* object path for session */
  GTask *task;
  guint signal_id; /* Request::Response signal */
  char *request_path; /* object path for request */
  guint cancelled_id; /* signal id for cancelled gobject signal */

  /* CreateSession only */
  XdpParent *parent;
  char *parent_handle;
  XdpInputCapability capabilities;

  /* GetZones only */
  XdpInputCaptureSession *session;

  /* SetPointerBarrier only */
  GList *barriers;

} Call;

static void create_session (Call *call);
static void get_zones (Call *call);

static void
call_free (Call *call)
{
  /* CreateSesssion */
  if (call->parent)
    {
      call->parent->parent_unexport (call->parent);
      xdp_parent_free (call->parent);
    }
  g_free (call->parent_handle);

  /* Generic */
  if (call->signal_id)
    g_dbus_connection_signal_unsubscribe (call->portal->bus, call->signal_id);

  if (call->cancelled_id)
    g_signal_handler_disconnect (g_task_get_cancellable (call->task), call->cancelled_id);

  g_free (call->request_path);

  g_clear_object (&call->portal);
  g_clear_object (&call->task);
  g_clear_object (&call->session);

  g_free (call->session_path);

  g_free (call);
}

static void
call_returned (GObject *object,
               GAsyncResult *result,
               gpointer data)
{
  Call *call = data;
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
      call_free (call);
    }
}

static gboolean
handle_matches_session (XdpInputCaptureSession *session, const char *id)
{
  const char *sid = session->parent_session->id;

  return g_str_equal (sid, id);
}

static void
set_zones (XdpInputCaptureSession *session, GVariant *zones, guint zone_set)
{
  GList *list = NULL;
  gsize nzones = g_variant_n_children (zones);

  for (gsize i = 0; i < nzones; i++)
    {
        guint width, height;
        gint x, y;
        XdpInputCaptureZone *z;

        g_variant_get_child (zones, i, "(uuii)", &width, &height, &x, &y);

        z = g_object_new (XDP_TYPE_INPUT_CAPTURE_ZONE,
                          "width", width,
                          "height", height,
                          "x", x,
                          "y", y,
                          "zone-set", zone_set,
                          "is-valid", TRUE,
                          NULL);
        list = g_list_append (list, z);
    }

  g_list_free_full (g_steal_pointer (&session->zones), (GDestroyNotify)_xdp_input_capture_zone_invalidate_and_free);
  session->zones = list;
  session->zone_set = zone_set;
}


static void
prep_call (Call *call, GDBusSignalCallback callback, GVariantBuilder *options, void *userdata)
{
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
                                                        callback,
                                                        call,
                                                        userdata);

  g_variant_builder_init (options, G_VARIANT_TYPE_VARDICT);
  g_variant_builder_add (options, "{sv}", "handle_token", g_variant_new_string (token));
}

static void
zones_changed_emit_signal (GObject *source_object,
                           GAsyncResult *res,
                           gpointer data)
{
  XdpInputCaptureSession *session = XDP_INPUT_CAPTURE_SESSION (data);
  GVariantBuilder options;

  g_variant_builder_init (&options, G_VARIANT_TYPE_VARDICT);
  g_variant_builder_add (&options, "{sv}", "zone_set", g_variant_new_uint32 (session->zone_set - 1));

  g_signal_emit (session, signals[SIGNAL_ZONES_CHANGED], 0, g_variant_new ("a{sv}", &options));
}

static void
zones_changed (GDBusConnection *bus,
               const char      *sender_name,
               const char      *object_path,
               const char      *interface_name,
               const char      *signal_name,
               GVariant        *parameters,
               gpointer         data)
{
  XdpInputCaptureSession *session = XDP_INPUT_CAPTURE_SESSION (data);
  XdpPortal *portal = session->parent_session->portal;
  g_autoptr(GVariant) options = NULL;
  const char *handle = NULL;
  Call *call;

  g_variant_get(parameters, "(o@a{sv})", &handle, &options);

  if (!handle_matches_session (session, handle))
    return;

  /* Zones have changed, but let's fetch the new zones before we notify the
   * caller so they're already available by the time they get notified */
  call = g_new0 (Call, 1);
  call->portal = g_object_ref (portal);
  call->task = g_task_new (portal, NULL, zones_changed_emit_signal, session);
  call->session = g_object_ref (session);

  get_zones (call);
}

static void
activated (GDBusConnection *bus,
           const char      *sender_name,
           const char      *object_path,
           const char      *interface_name,
           const char      *signal_name,
           GVariant        *parameters,
           gpointer         data)
{
  XdpInputCaptureSession *session = XDP_INPUT_CAPTURE_SESSION (data);
  g_autoptr(GVariant) options = NULL;
  guint32 activation_id = 0;
  const char *handle = NULL;

  g_variant_get (parameters, "(o@a{sv})", &handle, &options);

  /* FIXME: we should remove the activation_id from options, but ... meh? */
  if (!g_variant_lookup (options, "activation_id", "u", &activation_id))
    g_warning ("Portal bug: activation_id missing from Activated signal");

  if (!handle_matches_session (session, handle))
    return;

  g_signal_emit (session, signals[SIGNAL_ACTIVATED], 0, activation_id, options);
}

static void
deactivated (GDBusConnection *bus,
             const char        *sender_name,
             const char        *object_path,
             const char        *interface_name,
             const char        *signal_name,
             GVariant          *parameters,
             gpointer           data)
{
  XdpInputCaptureSession *session = XDP_INPUT_CAPTURE_SESSION (data);
  g_autoptr(GVariant) options = NULL;
  guint32 activation_id = 0;
  const char *handle = NULL;

  g_variant_get(parameters, "(o@a{sv})", &handle, &options);

  /* FIXME: we should remove the activation_id from options, but ... meh? */
  if (!g_variant_lookup (options, "activation_id", "u", &activation_id))
    g_warning ("Portal bug: activation_id missing from Deactivated signal");

  if (!handle_matches_session (session, handle))
    return;

  g_signal_emit (session, signals[SIGNAL_DEACTIVATED], 0, activation_id, options);
}

static void
disabled (GDBusConnection *bus,
          const char      *sender_name,
          const char      *object_path,
          const char      *interface_name,
          const char      *signal_name,
          GVariant        *parameters,
          gpointer         data)
{
  XdpInputCaptureSession *session = XDP_INPUT_CAPTURE_SESSION (data);
  g_autoptr(GVariant) options = NULL;
  const char *handle = NULL;

  g_variant_get(parameters, "(o@a{sv})", &handle, &options);

  if (!handle_matches_session (session, handle))
      return;

  g_signal_emit (session, signals[SIGNAL_DISABLED], 0, options);
}

static XdpInputCaptureSession *
_xdp_input_capture_session_new (XdpPortal *portal, const char *session_path)
{
  g_autoptr(XdpSession) parent_session = _xdp_session_new (portal, session_path, XDP_SESSION_INPUT_CAPTURE);
  g_autoptr(XdpInputCaptureSession) session = g_object_new (XDP_TYPE_INPUT_CAPTURE_SESSION, NULL);

  parent_session->input_capture_session = session; /* weak ref */
  g_object_weak_ref (G_OBJECT (parent_session), parent_session_destroy, session);
  session->parent_session = g_object_ref(parent_session); /* strong ref */

  return g_object_ref(session);
}

static void
get_zones_done (GDBusConnection *bus,
                const char *sender_name,
                const char *object_path,
                const char *interface_name,
                const char *signal_name,
                GVariant *parameters,
                gpointer data)
{
  Call *call = data;
  guint32 response;
  g_autoptr(GVariant) ret = NULL;

  g_variant_get (parameters, "(u@a{sv})", &response, &ret);

  if (response != 0 && call->cancelled_id)
    {
      g_signal_handler_disconnect (g_task_get_cancellable (call->task), call->cancelled_id);
      call->cancelled_id = 0;
    }

  if (response == 0)
    {
      GVariant *zones = NULL;
      guint32 zone_set;
      XdpInputCaptureSession *session = call->session;

      g_dbus_connection_signal_unsubscribe (call->portal->bus, call->signal_id);
      call->signal_id = 0;

      if (session == NULL)
        {
          session = _xdp_input_capture_session_new (call->portal, call->session_path);
          session->signal_ids[SIGNAL_ZONES_CHANGED] =
            g_dbus_connection_signal_subscribe (bus,
                                                PORTAL_BUS_NAME,
                                                "org.freedesktop.portal.InputCapture",
                                                "ZonesChanged",
                                                PORTAL_OBJECT_PATH,
                                                NULL,
                                                G_DBUS_SIGNAL_FLAGS_NO_MATCH_RULE,
                                                zones_changed,
                                                session,
                                                NULL);

          session->signal_ids[SIGNAL_ACTIVATED] =
            g_dbus_connection_signal_subscribe (bus,
                                                PORTAL_BUS_NAME,
                                                "org.freedesktop.portal.InputCapture",
                                                "Activated",
                                                PORTAL_OBJECT_PATH,
                                                NULL,
                                                G_DBUS_SIGNAL_FLAGS_NO_MATCH_RULE,
                                                activated,
                                                session,
                                                NULL);

          session->signal_ids[SIGNAL_DEACTIVATED] =
            g_dbus_connection_signal_subscribe (bus,
                                                PORTAL_BUS_NAME,
                                                "org.freedesktop.portal.InputCapture",
                                                "Deactivated",
                                                PORTAL_OBJECT_PATH,
                                                NULL,
                                                G_DBUS_SIGNAL_FLAGS_NO_MATCH_RULE,
                                                deactivated,
                                                session,
                                                NULL);

          session->signal_ids[SIGNAL_DISABLED] =
            g_dbus_connection_signal_subscribe (bus,
                                                PORTAL_BUS_NAME,
                                                "org.freedesktop.portal.InputCapture",
                                                "Disabled",
                                                PORTAL_OBJECT_PATH,
                                                NULL,
                                                G_DBUS_SIGNAL_FLAGS_NO_MATCH_RULE,
                                                disabled,
                                                session,
                                                NULL);
        }

      if (g_variant_lookup (ret, "zone_set", "u", &zone_set) &&
          g_variant_lookup (ret, "zones", "@a(uuii)", &zones))
        {
          set_zones (session, zones, zone_set);
          g_task_return_pointer (call->task, session, g_object_unref);
        }
      else
        {
          g_warning("Faulty portal implementation, missing GetZone's zone_set or zones");
          response = 2;
        }
    }

  if (response == 1)
    g_task_return_new_error (call->task, G_IO_ERROR, G_IO_ERROR_CANCELLED, "InputCapture GetZones() canceled");
  else if (response == 2)
    g_task_return_new_error (call->task, G_IO_ERROR, G_IO_ERROR_FAILED, "InputCapture GetZones() failed");

  if (response != 0)
    call_free (call);
}

static void
get_zones (Call *call)
{
  GVariantBuilder options;
  const char *session_id;

  /* May be called after CreateSession before we have an XdpInputCaptureSession, or by the
   * ZoneChanged signal when we do have a session */
  session_id = call->session ? call->session->parent_session->id : call->session_path;

  prep_call (call, get_zones_done, &options, NULL);
  g_dbus_connection_call (call->portal->bus,
                          PORTAL_BUS_NAME,
                          PORTAL_OBJECT_PATH,
                          "org.freedesktop.portal.InputCapture",
                          "GetZones",
                          g_variant_new ("(oa{sv})", session_id, &options),
                          NULL,
                          G_DBUS_CALL_FLAGS_NONE,
                          -1,
                          g_task_get_cancellable (call->task),
                          call_returned,
                          call);
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
  Call *call = data;
  guint32 response;
  g_autoptr(GVariant) ret = NULL;

  g_variant_get (parameters, "(u@a{sv})", &response, &ret);

  if (response != 0 && call->cancelled_id)
    {
      g_signal_handler_disconnect (g_task_get_cancellable (call->task), call->cancelled_id);
      call->cancelled_id = 0;
    }

  if (response == 0)
    {
      g_dbus_connection_signal_unsubscribe (call->portal->bus, call->signal_id);
      call->signal_id = 0;

      if (!g_variant_lookup (ret, "session_handle", "o", &call->session_path))
        {
          g_task_return_new_error (call->task, G_IO_ERROR, G_IO_ERROR_FAILED, "CreateSession failed to return a session handle");
          response = 2;
        }
      else
        get_zones (call);
    }
  else if (response == 1)
    g_task_return_new_error (call->task, G_IO_ERROR, G_IO_ERROR_CANCELLED, "CreateSession canceled");
  else if (response == 2)
    g_task_return_new_error (call->task, G_IO_ERROR, G_IO_ERROR_FAILED, "CreateSession failed");

  if (response != 0)
    call_free (call);
}

static void
call_cancelled_cb (GCancellable *cancellable,
                   gpointer data)
{
  Call *call = data;

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
parent_exported (XdpParent *parent,
                 const char *handle,
                 gpointer data)
{
  Call *call = data;
  call->parent_handle = g_strdup (handle);
  create_session (call);
}

static void
create_session (Call *call)
{
  GVariantBuilder options;
  g_autofree char *session_token = NULL;
  GCancellable *cancellable;

  if (call->parent_handle == NULL)
    {
      call->parent->parent_export (call->parent, parent_exported, call);
      return;
    }

  cancellable = g_task_get_cancellable (call->task);
  if (cancellable)
    call->cancelled_id = g_signal_connect (cancellable, "cancelled", G_CALLBACK (call_cancelled_cb), call);

  session_token = g_strdup_printf ("portal%d", g_random_int_range (0, G_MAXINT));

  prep_call (call, session_created, &options, NULL);
  g_variant_builder_add (&options, "{sv}", "session_handle_token", g_variant_new_string (session_token));
  g_variant_builder_add (&options, "{sv}", "capabilities", g_variant_new_uint32 (call->capabilities));

  g_dbus_connection_call (call->portal->bus,
                          PORTAL_BUS_NAME,
                          PORTAL_OBJECT_PATH,
                          "org.freedesktop.portal.InputCapture",
                          "CreateSession",
                          g_variant_new ("(sa{sv})", call->parent_handle, &options),
                          NULL,
                          G_DBUS_CALL_FLAGS_NONE,
                          -1,
                          cancellable,
                          call_returned,
                          call);
}

/**
 * xdp_portal_create_input_capture_session:
 * @portal: a [class@Portal]
 * @parent: (nullable): parent window information
 * @capabilities: which kinds of capabilities to request
 * @cancellable: (nullable): optional [class@Gio.Cancellable]
 * @callback: (scope async): a callback to call when the request is done
 * @data: (closure): data to pass to @callback
 *
 * Creates a session for input capture
 *
 * When the request is done, @callback will be called. You can then
 * call [method@Portal.create_input_capture_session_finish] to get the results.
 */
void
xdp_portal_create_input_capture_session (XdpPortal *portal,
                                         XdpParent *parent,
                                         XdpInputCapability capabilities,
                                         GCancellable *cancellable,
                                         GAsyncReadyCallback callback,
                                         gpointer data)
{
  Call *call;

  g_return_if_fail (XDP_IS_PORTAL (portal));

  call = g_new0 (Call, 1);
  call->portal = g_object_ref (portal);
  call->task = g_task_new (portal, cancellable, callback, data);

  if (parent)
    call->parent = xdp_parent_copy (parent);
  else
    call->parent_handle = g_strdup ("");

  call->capabilities = capabilities;

  create_session (call);
}

/**
 * xdp_portal_create_input_capture_session_finish:
 * @portal: a [class@Portal]
 * @result: a [iface@Gio.AsyncResult]
 * @error: return location for an error
 *
 * Finishes the InputCapture CreateSession request, and returns a
 * [class@InputCaptureSession]. To get to the [class@Session] within use
 * xdp_input_capture_session_get_session().
 *
 * Returns: (transfer full): a [class@InputCaptureSession]
 */
XdpInputCaptureSession *
xdp_portal_create_input_capture_session_finish (XdpPortal *portal,
                                                GAsyncResult *result,
                                                GError **error)
{
  XdpInputCaptureSession *session;

  g_return_val_if_fail (XDP_IS_PORTAL (portal), NULL);
  g_return_val_if_fail (g_task_is_valid (result, portal), NULL);

  session = g_task_propagate_pointer (G_TASK (result), error);

  if (session)
    return session;
  else
    return NULL;
}

/**
 * xdp_input_capture_session_get_session:
 * @session: a [class@XdpInputCaptureSession]
 *
 * Return the [class@XdpSession] for this InputCapture session.
 *
 * Returns: (transfer none): a [class@Session] object
 */
XdpSession *
xdp_input_capture_session_get_session (XdpInputCaptureSession *session)
{
  return session->parent_session;
}

/**
 * xdp_input_capture_session_get_zones:
 * @session: a [class@InputCaptureSession]
 *
 * Obtains the current set of [class@InputCaptureZone] objects.
 *
 * The returned object is valid until the zones are invalidated by the
 * [signal@InputCaptureSession::zones-changed] signal.
 *
 * Unless the session is active, this function returns `NULL`.
 *
 * Returns: (element-type XdpInputCaptureZone) (transfer none): the available
 * zones. The caller must keep a reference to the list or the elements if used
 * outside the immediate scope.
 */
GList *
xdp_input_capture_session_get_zones (XdpInputCaptureSession *session)
{
  g_return_val_if_fail (_xdp_input_capture_session_is_valid (session), NULL);

  return session->zones;
}

/**
 * xdp_input_capture_session_connect_to_eis:
 * @session: a [class@InputCaptureSession]
 * @error: return location for a #GError pointer
 *
 * Connect this session to an EIS implementation and return the fd.
 * This fd can be passed into ei_setup_backend_fd(). See the libei
 * documentation for details.
 *
 * This is a sync DBus invocation.
 *
 * Returns: a socket to the EIS implementation for this input capture
 * session or a negative errno on failure.
 */
int
xdp_input_capture_session_connect_to_eis (XdpInputCaptureSession  *session,
                                          GError                 **error)
{
  GVariantBuilder options;
  g_autoptr(GVariant) ret = NULL;
  g_autoptr(GUnixFDList) fd_list = NULL;
  int fd_out;
  XdpPortal *portal;
  XdpSession *parent_session = session->parent_session;

  if (!_xdp_input_capture_session_is_valid (session))
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT, "Session is not an InputCapture session");
      return -1;
    }

  g_variant_builder_init (&options, G_VARIANT_TYPE_VARDICT);

  portal = parent_session->portal;
  ret = g_dbus_connection_call_with_unix_fd_list_sync (portal->bus,
                                                       PORTAL_BUS_NAME,
                                                       PORTAL_OBJECT_PATH,
                                                       "org.freedesktop.portal.InputCapture",
                                                       "ConnectToEIS",
                                                       g_variant_new ("(oa{sv})",
                                                                      parent_session->id,
                                                                      &options),
                                                       NULL,
                                                       G_DBUS_CALL_FLAGS_NONE,
                                                       -1,
                                                       NULL,
                                                       &fd_list,
                                                       NULL,
                                                       error);

  if (!ret)
    return -1;

  g_variant_get (ret, "(h)", &fd_out);

  return g_unix_fd_list_get (fd_list, fd_out, NULL);
}

static void
free_barrier_list (GList *list)
{
  g_list_free_full (list, g_object_unref);
}

static void
set_pointer_barriers_done (GDBusConnection *bus,
                           const char *sender_name,
                           const char *object_path,
                           const char *interface_name,
                           const char *signal_name,
                           GVariant *parameters,
                           gpointer data)
{
  Call *call = data;
  guint32 response;
  g_autoptr(GVariant) ret = NULL;
  GVariant *failed = NULL;
  GList *failed_list = NULL;

  g_variant_get (parameters, "(u@a{sv})", &response, &ret);

  if (g_variant_lookup (ret, "failed_barriers", "@au", &failed))
    {
      const guint *failed_barriers = NULL;
      gsize n_elements;
      GList *it = call->barriers;

      failed_barriers = g_variant_get_fixed_array (failed, &n_elements, sizeof (guint32));

      while (it)
        {
          XdpInputCapturePointerBarrier *b = it->data;
          gboolean is_failed = FALSE;

          for (gsize i = 0; !is_failed && i < n_elements; i++)
            is_failed = _xdp_input_capture_pointer_barrier_get_id (b) == failed_barriers[i];

          _xdp_input_capture_pointer_barrier_set_is_active (b, !is_failed);

          if (is_failed)
            failed_list = g_list_append (failed_list, g_object_ref(b));

          it = it->next;
        }
    }

  /* all failed barriers have an extra ref in failed_list, so we can unref all barriers
     in our original list */
  free_barrier_list (call->barriers);
  call->barriers = NULL;
  g_task_return_pointer (call->task, failed_list,  (GDestroyNotify)free_barrier_list);
}

static void
convert_barrier (gpointer data, gpointer user_data)
{
  XdpInputCapturePointerBarrier *barrier = data;
  GVariantBuilder *builder = user_data;
  GVariantBuilder dict;
  int id, x1, x2, y1, y2;

  g_object_get (barrier, "id", &id, "x1", &x1, "x2", &x2, "y1", &y1, "y2", &y2, NULL);

  g_variant_builder_init (&dict, G_VARIANT_TYPE_VARDICT);
  g_variant_builder_add (&dict, "{sv}", "barrier_id", g_variant_new_uint32 (id));
  g_variant_builder_add (&dict, "{sv}", "position",
                         g_variant_new("(iiii)", x1, y1, x2, y2));
  g_variant_builder_add (builder, "a{sv}", &dict);
}

static void
set_pointer_barriers (Call *call)
{
  GVariantBuilder options;
  GVariantBuilder barriers;
  g_autoptr(GVariantType) vtype = NULL;

  prep_call (call, set_pointer_barriers_done, &options, NULL);

  vtype = g_variant_type_new ("aa{sv}");

  g_variant_builder_init (&barriers, vtype);
  g_list_foreach (call->barriers, convert_barrier, &barriers);

  g_dbus_connection_call (call->portal->bus,
                          PORTAL_BUS_NAME,
                          PORTAL_OBJECT_PATH,
                          "org.freedesktop.portal.InputCapture",
                          "SetPointerBarriers",
                          g_variant_new ("(oa{sv}aa{sv}u)",
                                         call->session->parent_session->id,
                                         &options,
                                         &barriers,
                                         call->session->zone_set),
                          NULL,
                          G_DBUS_CALL_FLAGS_NONE,
                          -1,
                          g_task_get_cancellable (call->task),
                          call_returned,
                          call);
}

static void
gobject_ref_wrapper (gpointer data, gpointer user_data)
{
  g_object_ref (G_OBJECT (data));
}

/**
 * xdp_input_capture_session_set_pointer_barriers:
 * @session: a [class@InputCaptureSession]
 * @barriers: (element-type XdpInputCapturePointerBarrier) (transfer container): the pointer barriers to apply
 *
 * Sets the pointer barriers for this session. When the request is done,
 * @callback will be called. You can then call
 * [method@InputCaptureSession.set_pointer_barriers_finish] to
 * get the results. The result of this request is the list of pointer barriers
 * that failed to apply - barriers not present in the returned list are active.
 *
 * Once the pointer barrier is
 * applied (i.e. the reply to the DBus Request has been received), the
 * the [property@InputCapturePointerBarrier:is-active] property is changed on
 * that barrier. Failed barriers have the property set to a %FALSE value.
 */
void
xdp_input_capture_session_set_pointer_barriers (XdpInputCaptureSession         *session,
                                                GList                          *barriers,
                                                GCancellable                   *cancellable,
                                                GAsyncReadyCallback             callback,
                                                gpointer                        data)
{
  Call *call;
  XdpPortal *portal;

  g_return_if_fail (_xdp_input_capture_session_is_valid (session));
  g_return_if_fail (barriers != NULL);

  portal = session->parent_session->portal;

  /* The list is ours, but we ref each object so we can create the list for the
   * returned barriers during _finish*/
  g_list_foreach (barriers, gobject_ref_wrapper, NULL);

  call = g_new0 (Call, 1);
  call->portal = g_object_ref (portal);
  call->session = g_object_ref (session);
  call->task = g_task_new (session, cancellable, callback, data);
  call->barriers = barriers;

  set_pointer_barriers (call);
}

/**
 * xdp_input_capture_session_set_pointer_barriers_finish:
 * @session: a [class@InputCaptureSession]
 * @result: a [iface@Gio.AsyncResult]
 * @error: return location for an error
 *
 * Finishes the set-pointer-barriers request, and returns a GList with the pointer
 * barriers that failed to apply and should be cleaned up by the caller.
 *
 * Returns: (element-type XdpInputCapturePointerBarrier) (transfer full): a list of failed pointer barriers
 */

GList *
xdp_input_capture_session_set_pointer_barriers_finish (XdpInputCaptureSession *session,
                                                       GAsyncResult           *result,
                                                       GError                **error)
{
  g_return_val_if_fail (_xdp_input_capture_session_is_valid (session), NULL);
  g_return_val_if_fail (g_task_is_valid (result, session), NULL);

  return g_task_propagate_pointer (G_TASK (result), error);
}

/**
 * xdp_input_capture_session_enable:
 * @session: a [class@InputCaptureSession]
 *
 * Enables this input capture session. In the future, this client may receive
 * input events.
 */
void
xdp_input_capture_session_enable (XdpInputCaptureSession *session)
{
  XdpPortal *portal;
  GVariantBuilder options;

  g_return_if_fail (_xdp_input_capture_session_is_valid (session));

  portal = session->parent_session->portal;

  g_variant_builder_init (&options, G_VARIANT_TYPE_VARDICT);

  g_dbus_connection_call  (portal->bus,
                           PORTAL_BUS_NAME,
                           PORTAL_OBJECT_PATH,
                           "org.freedesktop.portal.InputCapture",
                           "Enable",
                           g_variant_new ("(oa{sv})",
                                          session->parent_session->id,
                                          &options),
                           NULL,
                           G_DBUS_CALL_FLAGS_NONE,
                           1,
                           NULL,
                           NULL,
                           NULL);
}

/**
 * xdp_input_capture_session_disable:
 * @session: a [class@InputCaptureSession]
 *
 * Disables this input capture session.
 */
void
xdp_input_capture_session_disable (XdpInputCaptureSession *session)
{
  XdpPortal *portal;
  GVariantBuilder options;

  g_return_if_fail (_xdp_input_capture_session_is_valid (session));

  g_variant_builder_init (&options, G_VARIANT_TYPE_VARDICT);

  portal = session->parent_session->portal;
  g_dbus_connection_call (portal->bus,
                          PORTAL_BUS_NAME,
                          PORTAL_OBJECT_PATH,
                          "org.freedesktop.portal.InputCapture",
                          "Disable",
                          g_variant_new ("(oa{sv})",
                                         session->parent_session->id,
                                         &options),
                          NULL,
                          G_DBUS_CALL_FLAGS_NONE,
                          -1,
                          NULL,
                          NULL,
                          NULL);
}

static void
release_session (XdpInputCaptureSession   *session,
                 guint                     activation_id,
                 gboolean                  with_position,
                 gdouble                   x,
                 gdouble                   y)
{
  XdpPortal *portal;
  GVariantBuilder options;

  g_return_if_fail (_xdp_input_capture_session_is_valid (session));

  g_variant_builder_init (&options, G_VARIANT_TYPE_VARDICT);
  g_variant_builder_add (&options, "{sv}", "activation_id", g_variant_new_uint32 (activation_id));

  if (with_position)
    {
      g_variant_builder_add (&options,
                             "{sv}",
                             "cursor_position",
                             g_variant_new ("(dd)", x, y));
    }

  portal = session->parent_session->portal;
  g_dbus_connection_call (portal->bus,
                          PORTAL_BUS_NAME,
                          PORTAL_OBJECT_PATH,
                          "org.freedesktop.portal.InputCapture",
                          "Release",
                          g_variant_new ("(oa{sv})",
                                         session->parent_session->id,
                                        &options),
                          NULL,
                          G_DBUS_CALL_FLAGS_NONE,
                          -1,
                          NULL,
                          NULL,
                          NULL);
}

/**
 * xdp_input_capture_session_release:
 * @session: a [class@InputCaptureSession]
 *
 * Releases this input capture session without a suggested cursor position.
 */
void
xdp_input_capture_session_release (XdpInputCaptureSession *session,
                                   guint                   activation_id)
{
  g_return_if_fail (_xdp_input_capture_session_is_valid (session));

  release_session (session, activation_id, FALSE, 0, 0);
}

/**
 * xdp_input_capture_session_release_at:
 * @session: a [class@InputCaptureSession]
 * @cursor_x_position: the suggested cursor x position once capture has been released
 * @cursor_y_position: the suggested cursor y position once capture has been released
 *
 * Releases this input capture session with a suggested cursor position.
 * Note that the implementation is not required to honour this position.
 */
void
xdp_input_capture_session_release_at (XdpInputCaptureSession *session,
                                      guint                   activation_id,
                                      gdouble                 cursor_x_position,
                                      gdouble                 cursor_y_position)
{
  g_return_if_fail (_xdp_input_capture_session_is_valid (session));

  release_session (session, activation_id, TRUE, cursor_x_position, cursor_y_position);
}
