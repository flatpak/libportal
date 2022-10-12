/*
 * Copyright (C) 2018, Matthias Clasen
 * Copyright (C) 2019, Patrick Griffis
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

#include "session-private.h"
#include "background.h"
#include "portal-private.h"

typedef struct {
  XdpPortal *portal;
  GTask *task;
  char *status_message;
} SetStatusCall;

static void
set_status_call_free (SetStatusCall *call)
{
  g_clear_pointer (&call->status_message, g_free);
  g_clear_object (&call->portal);
  g_clear_object (&call->task);
  g_free (call);
}

static void
set_status_returned (GObject      *object,
                     GAsyncResult *result,
                     gpointer      data)
{
  SetStatusCall *call = data;
  GError *error = NULL;
  g_autoptr(GVariant) ret = NULL;

  ret = g_dbus_connection_call_finish (G_DBUS_CONNECTION (object), result, &error);
  if (error)
    g_task_return_error (call->task, error);
  else
    g_task_return_boolean (call->task, TRUE);

  set_status_call_free (call);
}

static void
set_status (SetStatusCall *call)
{
  GVariantBuilder options;

  g_variant_builder_init (&options, G_VARIANT_TYPE_VARDICT);

  if (call->status_message)
    g_variant_builder_add (&options, "{sv}", "message", g_variant_new_string (call->status_message));

  g_dbus_connection_call (call->portal->bus,
                          PORTAL_BUS_NAME,
                          PORTAL_OBJECT_PATH,
                          "org.freedesktop.portal.Background",
                          "SetStatus",
                          g_variant_new ("(a{sv})", &options),
                          NULL,
                          G_DBUS_CALL_FLAGS_NONE,
                          -1,
                          g_task_get_cancellable (call->task),
                          set_status_returned,
                          call);
}

static void
get_background_version_returned (GObject      *object,
                                 GAsyncResult *result,
                                 gpointer      data)
{
  g_autoptr(GVariant) version_variant = NULL;
  g_autoptr(GVariant) ret = NULL;
  SetStatusCall *call = data;
  GError *error = NULL;

  ret = g_dbus_connection_call_finish (G_DBUS_CONNECTION (object), result, &error);
  if (error)
    {
      g_task_return_error (call->task, error);
      set_status_call_free (call);
      return;
    }

  g_variant_get_child (ret, 0, "v", &version_variant);
  call->portal->background_interface_version = g_variant_get_uint32 (version_variant);

  if (call->portal->background_interface_version < 2)
    {
      g_task_return_new_error (call->task, G_DBUS_ERROR, G_DBUS_ERROR_FAILED,
                               "Background portal does not implement version 2 of the interface");
      set_status_call_free (call);
      return;
    }

  set_status (call);
}

static void
get_background_interface_version (SetStatusCall *call)
{
  g_dbus_connection_call (call->portal->bus,
                          PORTAL_BUS_NAME,
                          PORTAL_OBJECT_PATH,
                          "org.freedesktop.DBus.Properties",
                          "Get",
                          g_variant_new ("(ss)", "org.freedesktop.portal.Background", "version"),
                          NULL,
                          G_DBUS_CALL_FLAGS_NONE,
                          -1,
                          g_task_get_cancellable (call->task),
                          get_background_version_returned,
                          call);
}

typedef struct {
  XdpPortal *portal;
  XdpParent *parent;
  char *parent_handle;
  guint signal_id;
  GTask *task;
  char *request_path;
  guint cancelled_id;

  gboolean autostart;
  gboolean dbus_activatable;
  GPtrArray *commandline;
  char *reason;
} BackgroundCall;

static void
background_call_free (BackgroundCall *call)
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

  g_clear_pointer (&call->commandline, g_ptr_array_unref);
  g_free (call->reason);

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
  BackgroundCall *call = data;
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
      gboolean permission_granted = FALSE;
      g_variant_lookup (ret, call->autostart ? "autostart" : "background", "b", &permission_granted);
      g_task_return_boolean (call->task, permission_granted);
    }
  else if (response == 1)
    g_task_return_new_error (call->task, G_IO_ERROR, G_IO_ERROR_CANCELLED, "Background request canceled");
  else
    g_task_return_new_error (call->task, G_IO_ERROR, G_IO_ERROR_FAILED, "Background request failed");

  background_call_free (call);
}

static void request_background (BackgroundCall *call);

static void
parent_exported (XdpParent *parent,
                 const char *handle,
                 gpointer data)
{
  BackgroundCall *call = data;
  call->parent_handle = g_strdup (handle);
  request_background (call);
}

static void
cancelled_cb (GCancellable *cancellable,
              gpointer data)
{
  BackgroundCall *call = data;

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

  background_call_free (call);
}

static void
call_returned (GObject *object,
               GAsyncResult *result,
               gpointer data)
{
  BackgroundCall *call = data;
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
      background_call_free (call);
    }
}

static void
request_background (BackgroundCall *call)
{
  GVariantBuilder options;
  g_autofree char *token = NULL;
  GCancellable *cancellable;

  if (call->parent_handle == NULL)
    {
      call->parent->parent_export (call->parent, parent_exported, call);
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

  cancellable = g_task_get_cancellable (call->task);
  if (cancellable)
    call->cancelled_id = g_signal_connect (cancellable, "cancelled", G_CALLBACK (cancelled_cb), call);

  g_variant_builder_init (&options, G_VARIANT_TYPE_VARDICT);
  g_variant_builder_add (&options, "{sv}", "handle_token", g_variant_new_string (token));

  g_variant_builder_add (&options, "{sv}", "autostart", g_variant_new_boolean (call->autostart));
  g_variant_builder_add (&options, "{sv}", "dbus-activatable", g_variant_new_boolean (call->dbus_activatable));
  if (call->reason)
    g_variant_builder_add (&options, "{sv}", "reason", g_variant_new_string (call->reason));
  if (call->commandline)
    g_variant_builder_add (&options, "{sv}", "commandline", g_variant_new_strv ((const char* const*)call->commandline->pdata, call->commandline->len));

  g_debug ("calling background");
  g_dbus_connection_call (call->portal->bus,
                          PORTAL_BUS_NAME,
                          PORTAL_OBJECT_PATH,
                          "org.freedesktop.portal.Background",
                          "RequestBackground",
                          g_variant_new ("(sa{sv})", call->parent_handle, &options),
                          NULL,
                          G_DBUS_CALL_FLAGS_NONE,
                          -1,
                          cancellable,
                          call_returned,
                          call);
}

/**
 * xdp_portal_request_background:
 * @portal: a [class@Portal]
 * @parent: (nullable): parent window information
 * @commandline: (element-type utf8) (transfer container): command line to autostart
 * @reason: (nullable): reason to present to user for request
 * @flags: options for this call
 * @cancellable: (nullable): optional [class@Gio.Cancellable]
 * @callback: (scope async): a callback to call when the request is done
 * @user_data: (closure): data to pass to @callback
 *
 * Requests background permissions.
 *
 * When the request is done, @callback will be called. You can then
 * call [method@Portal.request_background_finish] to get the results.
 */
void
xdp_portal_request_background (XdpPortal *portal,
                               XdpParent *parent,
                               char *reason,
                               GPtrArray *commandline,
                               XdpBackgroundFlags flags,
                               GCancellable *cancellable,
                               GAsyncReadyCallback callback,
                               gpointer user_data)
{
  BackgroundCall *call;

  g_return_if_fail (XDP_IS_PORTAL (portal));
  g_return_if_fail ((flags & ~(XDP_BACKGROUND_FLAG_AUTOSTART |
                               XDP_BACKGROUND_FLAG_ACTIVATABLE)) == 0);

  call = g_new0 (BackgroundCall, 1);
  call->portal = g_object_ref (portal);
  if (parent)
    call->parent = xdp_parent_copy (parent);
  else
    call->parent_handle = g_strdup ("");

  call->autostart = (flags & XDP_BACKGROUND_FLAG_AUTOSTART) != 0;
  call->dbus_activatable = (flags & XDP_BACKGROUND_FLAG_ACTIVATABLE) != 0;
  call->reason = g_strdup (reason);
  if (commandline)
    call->commandline = g_ptr_array_ref (commandline);

  call->task = g_task_new (portal, cancellable, callback, user_data);

  request_background (call);
}

/**
 * xdp_portal_request_background_finish:
 * @portal: a [class@Portal]
 * @result: a [iface@Gio.AsyncResult]
 * @error: return location for an error
 *
 * Finishes the request.
 *
 * Returns `TRUE` if successful.
 *
 * Returns: `TRUE` if successful.
 */
gboolean
xdp_portal_request_background_finish (XdpPortal *portal,
                                      GAsyncResult *result,
                                      GError **error)
{
  g_return_val_if_fail (XDP_IS_PORTAL (portal), FALSE);
  g_return_val_if_fail (g_task_is_valid (result, portal), FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

/**
 * xdp_portal_set_background_status:
 * @portal: a [class@Portal]
 * @status_message: (nullable): status message when running in background
 * @cancellable: (nullable): optional [class@Gio.Cancellable]
 * @callback: (scope async): a callback to call when the request is done
 * @data: (closure): data to pass to @callback
 *
 * Sets the status information of the application, for when it's running
 * in background.
 */
void
xdp_portal_set_background_status (XdpPortal           *portal,
                                  const char          *status_message,
                                  GCancellable        *cancellable,
                                  GAsyncReadyCallback  callback,
                                  gpointer             data)
{
  SetStatusCall *call;

  g_return_if_fail (XDP_IS_PORTAL (portal));

  call = g_new0 (SetStatusCall, 1);
  call->portal = g_object_ref (portal);
  call->status_message = g_strdup (status_message);
  call->task = g_task_new (portal, cancellable, callback, data);
  g_task_set_source_tag (call->task, xdp_portal_set_background_status);

  if (portal->background_interface_version == 0)
    get_background_interface_version (call);
  else
    set_status (call);
}

/**
 * xdp_portal_set_background_status_finish:
 * @portal: a [class@Portal]
 * @result: a [iface@Gio.AsyncResult]
 * @error: return location for an error
 *
 * Finishes setting the background status of the application.
 *
 * Returns: %TRUE if successfully set status, %FALSE otherwise
 */
gboolean
xdp_portal_set_background_status_finish (XdpPortal     *portal,
                                         GAsyncResult  *result,
                                         GError       **error)
{
  g_return_val_if_fail (XDP_IS_PORTAL (portal), FALSE);
  g_return_val_if_fail (g_task_is_valid (result, portal), FALSE);
  g_return_val_if_fail (g_task_get_source_tag (G_TASK (result)) == xdp_portal_set_background_status, FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}
