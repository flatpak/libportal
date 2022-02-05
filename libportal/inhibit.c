/*
 * Copyright (C) 2018, Matthias Clasen
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

#include "inhibit.h"
#include "portal-private.h"

typedef struct {
  XdpPortal *portal;
  XdpParent *parent;
  char *parent_handle;
  XdpInhibitFlags inhibit;
  guint signal_id;
  guint cancelled_id;
  char *request_path;
  char *reason;
  GTask *task;
  int id;
} InhibitCall;

static void
inhibit_call_free (InhibitCall *call)
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

  g_object_unref (call->portal);
  g_object_unref (call->task);

  g_free (call->reason);
  g_free (call->request_path);

  g_free (call);
}

static void do_inhibit (InhibitCall *call);

static void
inhibit_parent_exported (XdpParent *parent,
                         const char *handle,
                         gpointer data)
{
  InhibitCall *call = data;
  call->parent_handle = g_strdup (handle);
  do_inhibit (call);
}

static void
call_returned (GObject *object,
               GAsyncResult *result,
               gpointer data)
{
  InhibitCall *call = data;
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

      g_hash_table_remove (call->portal->inhibit_handles, GINT_TO_POINTER (call->id));
      g_task_return_error (call->task, error);
      inhibit_call_free (call);
    }
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
  InhibitCall *call = data;
  guint32 response;
  g_autoptr(GVariant) ret = NULL;

  if (call->cancelled_id)
    {
      g_signal_handler_disconnect (g_task_get_cancellable (call->task), call->cancelled_id);
      call->cancelled_id = 0;
    }

  g_variant_get (parameters, "(u@a{sv})", &response, &ret);

  if (response == 0)
    g_task_return_int (call->task, call->id);
  else if (response == 1)
    {
      g_hash_table_remove (call->portal->inhibit_handles, GINT_TO_POINTER (call->id));
      g_task_return_new_error (call->task, G_IO_ERROR, G_IO_ERROR_CANCELLED, "Account call canceled user");
    }
  else
    {
      g_hash_table_remove (call->portal->inhibit_handles, GINT_TO_POINTER (call->id));
      g_task_return_new_error (call->task, G_IO_ERROR, G_IO_ERROR_FAILED, "Account call failed");
    }

  inhibit_call_free (call);
}

static void
inhibit_cancelled_cb (GCancellable *cancellable,
                      gpointer data)
{
  InhibitCall *call = data;

  g_debug ("inhibit cancelled, calling Close");
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

  g_hash_table_remove (call->portal->inhibit_handles, GINT_TO_POINTER (call->id));
  g_task_return_new_error (call->task, G_IO_ERROR, G_IO_ERROR_CANCELLED, "Inhibit call canceled by caller");

  inhibit_call_free (call);
}

static void
do_inhibit (InhibitCall *call)
{
  GVariantBuilder options;
  g_autofree char *token = NULL;
  GCancellable *cancellable;

  if (call->parent_handle == NULL)
    {
      call->parent->parent_export (call->parent, inhibit_parent_exported, call);
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
                                                        response_received,
                                                        call,
                                                        NULL);

  g_hash_table_insert (call->portal->inhibit_handles, GINT_TO_POINTER (call->id), g_strdup (call->request_path));

  cancellable = g_task_get_cancellable (call->task);
  if (cancellable)
    call->cancelled_id = g_signal_connect (cancellable, "cancelled", G_CALLBACK (inhibit_cancelled_cb), call);

  g_variant_builder_init (&options, G_VARIANT_TYPE_VARDICT);
  g_variant_builder_add (&options, "{sv}", "handle_token", g_variant_new_string (token));
  g_variant_builder_add (&options, "{sv}", "reason", g_variant_new_string (call->reason));

  g_dbus_connection_call (call->portal->bus,
                          PORTAL_BUS_NAME,
                          PORTAL_OBJECT_PATH,
                          "org.freedesktop.portal.Inhibit",
                          "Inhibit",
                          g_variant_new ("(sua{sv})", call->parent_handle, call->inhibit, &options),
                          NULL,
                          G_DBUS_CALL_FLAGS_NONE,
                          -1,
                          NULL,
                          call_returned,
                          call);
}

/**
 * xdp_portal_session_inhibit:
 * @portal: a [class@Portal]
 * @parent: (nullable): parent window information
 * @reason: (nullable): user-visible reason for the inhibition
 * @flags: information about what to inhibit
 * @cancellable: (nullable): optional [class@Gio.Cancellable]
 * @callback: (scope async): a callback to call when the request is done
 * @data: (closure): data to pass to @callback
 *
 * Inhibits various session status changes.
 *
 * To obtain an ID that can be used to undo the inhibition, use
 * [method@Portal.session_inhibit_finish] in the callback.
 *
 * To remove an active inhibitor, call [method@Portal.session_uninhibit]
 * with the same ID.
 */
void
xdp_portal_session_inhibit (XdpPortal            *portal,
                            XdpParent            *parent,
                            const char           *reason,
                            XdpInhibitFlags       flags,
                            GCancellable         *cancellable,
                            GAsyncReadyCallback   callback,
                            gpointer              data)
{
  InhibitCall *call = NULL;

  g_return_if_fail (XDP_IS_PORTAL (portal));
  g_return_if_fail ((flags & ~(XDP_INHIBIT_FLAG_LOGOUT |
                               XDP_INHIBIT_FLAG_USER_SWITCH |
                               XDP_INHIBIT_FLAG_SUSPEND |
                               XDP_INHIBIT_FLAG_IDLE)) == 0);

  if (portal->inhibit_handles == NULL)
    portal->inhibit_handles = g_hash_table_new_full (NULL, NULL, NULL, g_free);

  portal->next_inhibit_id++;
  if (portal->next_inhibit_id < 0)
    portal->next_inhibit_id = 1;

  call = g_new0 (InhibitCall, 1);
  call->portal = g_object_ref (portal);
  if (parent)
    call->parent = xdp_parent_copy (parent);
  else
    call->parent_handle = g_strdup ("");
  call->inhibit = flags;
  call->id = portal->next_inhibit_id;
  call->reason = g_strdup (reason);
  call->task = g_task_new (portal, cancellable, callback, data);
  g_task_set_source_tag (call->task, xdp_portal_session_inhibit);

  do_inhibit (call);
}

/**
 * xdp_portal_session_inhibit_finish:
 * @portal: a [class@Portal]
 * @result: a [iface@Gio.AsyncResult]
 * @error: return location for an error
 *
 * Finishes the inhbit request.
 *
 * Returns the ID of the inhibition as a positive integer. The ID can be passed
 * to [method@Portal.session_uninhibit] to undo the inhibition.
 *
 * Returns: the ID of the inhibition, or -1 if there was an error
 */
int
xdp_portal_session_inhibit_finish (XdpPortal *portal,
                                   GAsyncResult *result,
                                   GError **error)
{
  g_return_val_if_fail (XDP_IS_PORTAL (portal), -1);
  g_return_val_if_fail (g_task_is_valid (result, portal), -1);
  g_return_val_if_fail (g_task_get_source_tag (G_TASK (result)) == xdp_portal_session_inhibit, -1);

  return g_task_propagate_int (G_TASK (result), error);
}

/**
 * xdp_portal_session_uninhibit:
 * @portal: a [class@Portal]
 * @id: unique ID for an active inhibition
 *
 * Removes an inhibitor that was created by a call
 * to [method@Portal.session_inhibit].
 */
void
xdp_portal_session_uninhibit (XdpPortal *portal,
                              int        id)
{
  gpointer key;
  g_autofree char *value = NULL;

  g_return_if_fail (XDP_IS_PORTAL (portal));
  g_return_if_fail (id > 0);

  if (portal->inhibit_handles == NULL ||
      !g_hash_table_steal_extended (portal->inhibit_handles,
                                    GINT_TO_POINTER (id),
                                    (gpointer *)&key,
                                    (gpointer *)&value))
    {
      g_warning ("No inhibit handle found");
      return;
    }

  g_dbus_connection_call (portal->bus,
                          PORTAL_BUS_NAME,
                          value,
                          REQUEST_INTERFACE,
                          "Close",
                          g_variant_new ("()"),
                          G_VARIANT_TYPE_UNIT,
                          G_DBUS_CALL_FLAGS_NONE,
                          G_MAXINT,
                          NULL, NULL, NULL);
}

typedef struct {
  XdpPortal *portal;
  XdpParent *parent;
  char *parent_handle;
  GTask *task;
  char *request_path;
  guint signal_id;
  guint cancelled_id;
  char *id;
} CreateMonitorCall;

static void
create_monitor_call_free (CreateMonitorCall *call)
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
  g_free (call->id);

  g_object_unref (call->portal);
  g_object_unref (call->task);

  g_free (call);
}

static void
session_state_changed (GDBusConnection *bus,
                       const char *sender_name,
                       const char *object_path,
                       const char *interface_name,
                       const char *signal_name,
                       GVariant *parameters,
                       gpointer data)
{
  XdpPortal *portal = data;
  const char *handle;
  g_autoptr(GVariant) state = NULL;
  gboolean screensaver_active = FALSE;
  XdpLoginSessionState session_state = XDP_LOGIN_SESSION_RUNNING;

  g_variant_get (parameters, "(&o@a{sv})", &handle, &state);
  if (g_strcmp0 (handle, portal->session_monitor_handle) != 0)
    {
      g_warning ("Session monitor handle mismatch");
      return;
    }

  g_variant_lookup (state, "screensaver-active", "b", &screensaver_active);
  g_variant_lookup (state, "session-state", "u", &session_state);

  g_signal_emit_by_name (portal, "session-state-changed",
                         screensaver_active,
                         session_state);
}

static void
ensure_session_monitor_connection (XdpPortal *portal)
{
  if (portal->state_changed_signal == 0)
    portal->state_changed_signal =
       g_dbus_connection_signal_subscribe (portal->bus,
                                           PORTAL_BUS_NAME,
                                           "org.freedesktop.portal.Inhibit",
                                           "StateChanged",
                                           PORTAL_OBJECT_PATH,
                                           NULL,
                                           G_DBUS_SIGNAL_FLAGS_NO_MATCH_RULE,
                                           session_state_changed,
                                           portal,
                                           NULL);
}

static void
create_response_received (GDBusConnection *bus,
                          const char *sender_name,
                          const char *object_path,
                          const char *interface_name,
                          const char *signal_name,
                          GVariant *parameters,
                          gpointer data)
{
  CreateMonitorCall *call = data;
  guint32 response;
  g_autoptr(GVariant) ret = NULL;

  if (call->cancelled_id)
    {
      g_signal_handler_disconnect (g_task_get_cancellable (call->task), call->cancelled_id);
      call->cancelled_id = 0;
    }

  g_variant_get (parameters, "(u@a{sv})", &response, &ret);

  if (response == 0)
    {
      call->portal->session_monitor_handle = g_strdup (call->id);
      ensure_session_monitor_connection (call->portal);
      g_task_return_boolean (call->task, TRUE);
    }
  else if (response == 1)
    g_task_return_new_error (call->task, G_IO_ERROR, G_IO_ERROR_CANCELLED, "CreateMonitor canceled");
  else
    g_task_return_new_error (call->task, G_IO_ERROR, G_IO_ERROR_FAILED, "CreateMonitor failed");

  create_monitor_call_free (call);
}

static void create_monitor (CreateMonitorCall *call);

static void
create_parent_exported (XdpParent *parent,
                        const char *handle,
                        gpointer data)
{
  CreateMonitorCall *call = data;
  call->parent_handle = g_strdup (handle);
  create_monitor (call);
}

static void
create_cancelled_cb (GCancellable *cancellable,
                     gpointer data)
{
  CreateMonitorCall *call = data;

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
create_returned (GObject *object,
                 GAsyncResult *result,
                 gpointer data)
{
  CreateMonitorCall *call = data;
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
      create_monitor_call_free (call);
    }
}

static void
create_monitor (CreateMonitorCall *call)
{
  GVariantBuilder options;
  g_autofree char *token = NULL;
  g_autofree char *session_token = NULL;
  GCancellable *cancellable;

  if (call->portal->session_monitor_handle)
    {
      g_task_return_boolean (call->task, TRUE);
      create_monitor_call_free (call);
      return;
    }

  if (call->parent_handle == NULL)
    {
      call->parent->parent_export (call->parent, create_parent_exported, call);
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
                                                        create_response_received,
                                                        call,
                                                        NULL);

  cancellable = g_task_get_cancellable (call->task);
  if (cancellable)
    call->cancelled_id = g_signal_connect (cancellable, "cancelled", G_CALLBACK (create_cancelled_cb), call);

  session_token = g_strdup_printf ("portal%d", g_random_int_range (0, G_MAXINT));
  call->id = g_strconcat (SESSION_PATH_PREFIX, call->portal->sender, "/", session_token, NULL);

  g_variant_builder_init (&options, G_VARIANT_TYPE_VARDICT);
  g_variant_builder_add (&options, "{sv}", "handle_token", g_variant_new_string (token));
  g_variant_builder_add (&options, "{sv}", "session_handle_token", g_variant_new_string (session_token));
  g_dbus_connection_call (call->portal->bus,
                          PORTAL_BUS_NAME,
                          PORTAL_OBJECT_PATH,
                          "org.freedesktop.portal.Inhibit",
                          "CreateMonitor",
                          g_variant_new ("(sa{sv})", call->parent_handle, &options),
                          NULL,
                          G_DBUS_CALL_FLAGS_NONE,
                          -1,
                          NULL,
                          create_returned,
                          call);

}

/**
 * xdp_portal_session_monitor_start:
 * @portal: a [class@Portal]
 * @parent: (nullable): a XdpParent, or `NULL`
 * @flags: options for this call
 * @cancellable: (nullable): optional [class@Gio.Cancellable]
 * @callback: (scope async): a callback to call when the request is done
 * @data: (closure): data to pass to @callback
 *
 * Makes [class@Portal] start monitoring the login session state.
 *
 * When the state changes, the [signal@Portal::session-state-changed]
 * signal is emitted.
 *
 * Use [method@Portal.session_monitor_stop] to stop monitoring.
 */
void
xdp_portal_session_monitor_start (XdpPortal *portal,
                                  XdpParent *parent,
                                  XdpSessionMonitorFlags flags,
                                  GCancellable *cancellable,
                                  GAsyncReadyCallback callback,
                                  gpointer data)

{
  CreateMonitorCall *call;

  g_return_if_fail (XDP_IS_PORTAL (portal));
  g_return_if_fail (flags == XDP_SESSION_MONITOR_FLAG_NONE);

  call = g_new0 (CreateMonitorCall, 1);
  call->portal = g_object_ref (portal);
  if (parent)
    call->parent = xdp_parent_copy (parent);
  else
    call->parent_handle = g_strdup ("");
  call->task = g_task_new (portal, cancellable, callback, data);
  g_task_set_source_tag (call->task, xdp_portal_session_monitor_start);

  create_monitor (call);
}

/**
 * xdp_portal_session_monitor_start_finish:
 * @portal: a [class@Portal]
 * @result: a [iface@Gio.AsyncResult]
 * @error: return location for an error
 *
 * Finishes a session-monitor request.
 *
 * Returns the result in the form of boolean.
 *
 * Returns: `TRUE` if the request succeeded
 */
gboolean
xdp_portal_session_monitor_start_finish (XdpPortal *portal,
                                         GAsyncResult *result,
                                         GError **error)
{
  g_return_val_if_fail (XDP_IS_PORTAL (portal), FALSE);
  g_return_val_if_fail (g_task_is_valid (result, portal), FALSE);
  g_return_val_if_fail (g_task_get_source_tag (G_TASK (result)) == xdp_portal_session_monitor_start, FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

/**
 * xdp_portal_session_monitor_stop:
 * @portal: a [class@Portal]
 *
 * Stops session state monitoring that was started with
 * [method@Portal.session_monitor_start].
 */
void
xdp_portal_session_monitor_stop (XdpPortal *portal)
{
  g_return_if_fail (XDP_IS_PORTAL (portal));

  if (portal->state_changed_signal)
    {
      g_dbus_connection_signal_unsubscribe (portal->bus, portal->state_changed_signal);
      portal->state_changed_signal = 0;
    }

  if (portal->session_monitor_handle)
    {
      g_dbus_connection_call (portal->bus,
                              PORTAL_BUS_NAME,
                              portal->session_monitor_handle,
                              SESSION_INTERFACE,
                              "Close",
                              NULL,
                              NULL, 0, -1, NULL, NULL, NULL);
      g_clear_pointer (&portal->session_monitor_handle, g_free);
    }
}

/**
 * xdp_portal_session_monitor_query_end_response:
 * @portal: a [class@Portal]
 *
 * This method should be called within one second of
 * receiving a [signal@Portal::session-state-changed] signal
 * with the 'Query End' state, to acknowledge that they
 * have handled the state change.
 *
 * Possible ways to handle the state change are either
 * to call [method@Portal.session_inhibit] to prevent the
 * session from ending, or to save your state and get
 * ready for the session to end.
 */
void
xdp_portal_session_monitor_query_end_response (XdpPortal *portal)
{
  g_return_if_fail (XDP_IS_PORTAL (portal));

  if (portal->session_monitor_handle != NULL)
    g_dbus_connection_call (portal->bus,
                            PORTAL_BUS_NAME,
                            PORTAL_OBJECT_PATH,
                            "org.freedesktop.portal.Inhibit",
                            "QueryEndResponse",
                            g_variant_new ("(o)", portal->session_monitor_handle),
                            NULL, 0, -1, NULL, NULL, NULL);
}
