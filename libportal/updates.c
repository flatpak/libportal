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

#include "updates.h"
#include "portal-private.h"

#define UPDATE_MONITOR_INTERFACE "org.freedesktop.portal.Flatpak.UpdateMonitor"
#define UPDATE_MONITOR_PATH_PREFIX "/org/freedesktop/portal/Flatpak/update_monitor/"

typedef struct {
  XdpPortal *portal;
  GTask *task;
  char *request_path;
  char *id;
} CreateMonitorCall;

static void
create_monitor_call_free (CreateMonitorCall *call)
{
  g_free (call->request_path);
  g_free (call->id);

  g_object_unref (call->portal);
  g_object_unref (call->task);

  g_free (call);
}

static void
update_available_received (GDBusConnection *bus,
                           const char *sender_name,
                           const char *object_path,
                           const char *interface_name,
                           const char *signal_name,
                           GVariant *parameters,
                           gpointer data)
{
  XdpPortal *portal = data;
  g_autoptr(GVariant) update_info = NULL;
  const char *running_commit;
  const char *local_commit;
  const char *remote_commit;

  g_variant_get (parameters, "(@a{sv})", &update_info);
  g_variant_lookup (update_info, "running-commit", "&s", &running_commit);
  g_variant_lookup (update_info, "local-commit", "&s", &local_commit);
  g_variant_lookup (update_info, "remote-commit", "&s", &remote_commit);

  g_signal_emit_by_name (portal, "update-available",
                         running_commit,
                         local_commit,
                         remote_commit);
}

static void
update_progress_received (GDBusConnection *bus,
                          const char *sender_name,
                          const char *object_path,
                          const char *interface_name,
                          const char *signal_name,
                          GVariant *parameters,
                          gpointer data)
{
  XdpPortal *portal = data;
  g_autoptr(GVariant) info = NULL;
  guint n_ops;
  guint op;
  guint progress;
  XdpUpdateStatus status;
  const char *error = NULL;
  const char *error_message = NULL;

  g_variant_get (parameters, "(@a{sv})", &info);
  g_variant_lookup (info, "n_ops", "u", &n_ops);
  g_variant_lookup (info, "op", "u", &op);
  g_variant_lookup (info, "progress", "u", &progress);
  g_variant_lookup (info, "status", "u", &status);
  if (status == XDP_UPDATE_STATUS_FAILED)
    {
      g_variant_lookup (info, "error", "&s", &error);
      g_variant_lookup (info, "error_message", "&s", &error_message);
    }
  g_debug ("update progress received %u/%u %u%% %d", op, n_ops, progress, status);

  g_signal_emit_by_name (portal, "update-progress",
                         n_ops,
                         op,
                         progress,
                         status,
                         error,
                         error_message);
}

static void
ensure_update_monitor_connection (XdpPortal *portal)
{
  if (portal->update_available_signal == 0)
    portal->update_available_signal =
       g_dbus_connection_signal_subscribe (portal->bus,
                                           FLATPAK_PORTAL_BUS_NAME,
                                           UPDATE_MONITOR_INTERFACE,
                                           "UpdateAvailable",
                                           portal->update_monitor_handle,
                                           NULL,
                                           G_DBUS_SIGNAL_FLAGS_NO_MATCH_RULE,
                                           update_available_received,
                                           portal,
                                           NULL);

  if (portal->update_progress_signal == 0)
    portal->update_progress_signal =
       g_dbus_connection_signal_subscribe (portal->bus,
                                           FLATPAK_PORTAL_BUS_NAME,
                                           UPDATE_MONITOR_INTERFACE,
                                           "Progress",
                                           portal->update_monitor_handle,
                                           NULL,
                                           G_DBUS_SIGNAL_FLAGS_NO_MATCH_RULE,
                                           update_progress_received,
                                           portal,
                                           NULL);
}

static void
monitor_created (GObject *object,
                 GAsyncResult *result,
                 gpointer data)
{
  CreateMonitorCall *call = data;
  GError *error = NULL;
  g_autoptr(GVariant) ret = NULL;

  ret = g_dbus_connection_call_finish (G_DBUS_CONNECTION (object), result, &error);
  if (error)
    {
      g_task_return_error (call->task, error);
    }
  else
    {
      call->portal->update_monitor_handle = g_strdup (call->id);
      ensure_update_monitor_connection (call->portal);
      g_task_return_boolean (call->task, TRUE);
    }

  create_monitor_call_free (call);
}

static void
create_monitor (CreateMonitorCall *call)
{
  GVariantBuilder options;
  g_autofree char *token = NULL;
  GCancellable *cancellable;

  if (call->portal->update_monitor_handle)
    {
      g_task_return_boolean (call->task, TRUE);
      create_monitor_call_free (call);
      return;
    }

  token = g_strdup_printf ("portal%d", g_random_int_range (0, G_MAXINT));
  call->id = g_strconcat (UPDATE_MONITOR_PATH_PREFIX, call->portal->sender, "/", token, NULL);

  cancellable = g_task_get_cancellable (call->task);

  g_variant_builder_init (&options, G_VARIANT_TYPE_VARDICT);
  g_variant_builder_add (&options, "{sv}", "handle_token", g_variant_new_string (token));
  g_dbus_connection_call (call->portal->bus,
                          FLATPAK_PORTAL_BUS_NAME,
                          FLATPAK_PORTAL_OBJECT_PATH,
                          FLATPAK_PORTAL_INTERFACE,
                          "CreateUpdateMonitor",
                          g_variant_new ("(a{sv})", &options),
                          NULL,
                          G_DBUS_CALL_FLAGS_NONE,
                          -1,
                          cancellable,
                          monitor_created,
                          call);

}

/**
 * xdp_portal_update_monitor_start:
 * @portal: a [class@Portal]
 * @flags: options for this cal..
 * @cancellable: (nullable): optional [class@Gio.Cancellable]
 * @callback: (scope async): a callback to call when the request is done
 * @data: (closure): data to pass to @callback
 *
 * Makes `XdpPortal` start monitoring for available software updates.
 *
 * When a new update is available, the [signal@Portal::update-available].
 * signal is emitted.
 *
 * Use [method@Portal.update_monitor_stop] to stop monitoring.
 */
void
xdp_portal_update_monitor_start (XdpPortal *portal,
                                 XdpUpdateMonitorFlags flags,
                                 GCancellable *cancellable,
                                 GAsyncReadyCallback callback,
                                 gpointer data)
{
  CreateMonitorCall *call;

  g_return_if_fail (XDP_IS_PORTAL (portal));
  g_return_if_fail (flags == XDP_UPDATE_MONITOR_FLAG_NONE);

  call = g_new0 (CreateMonitorCall, 1);
  call->portal = g_object_ref (portal);
  call->task = g_task_new (portal, cancellable, callback, data);
  g_task_set_source_tag (call->task, xdp_portal_update_monitor_start);

  create_monitor (call);
}

/**
 * xdp_portal_update_monitor_start_finish:
 * @portal: a [class@Portal]
 * @result: a [iface@Gio.AsyncResult]
 * @error: return location for an error
 *
 * Finishes an update-monitor request.
 *
 * Returns the result in the form of boolean.
 *
 * Returns: `TRUE` if the request succeeded
 */
gboolean
xdp_portal_update_monitor_start_finish (XdpPortal *portal,
                                        GAsyncResult *result,
                                        GError **error)
{
  g_return_val_if_fail (XDP_IS_PORTAL (portal), FALSE);
  g_return_val_if_fail (g_task_is_valid (result, portal), FALSE);
  g_return_val_if_fail (g_task_get_source_tag (G_TASK (result)) == xdp_portal_update_monitor_start, FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

/**
 * xdp_portal_update_monitor_stop:
 * @portal: a [class@Portal]
 *
 * Stops update monitoring that was started with
 * [method@Portal.update_monitor_start].
 */
void
xdp_portal_update_monitor_stop (XdpPortal *portal)
{
  g_return_if_fail (XDP_IS_PORTAL (portal));

  if (portal->update_available_signal)
    {
      g_dbus_connection_signal_unsubscribe (portal->bus, portal->update_available_signal);
      portal->update_available_signal = 0;
    }

  if (portal->update_progress_signal)
    {
      g_dbus_connection_signal_unsubscribe (portal->bus, portal->update_progress_signal);
      portal->update_progress_signal = 0;
    }

  if (portal->update_monitor_handle)
    {
      g_dbus_connection_call (portal->bus,
                              FLATPAK_PORTAL_BUS_NAME,
                              portal->update_monitor_handle,
                              UPDATE_MONITOR_INTERFACE,
                              "Close",
                              NULL,
                              NULL, 0, -1, NULL, NULL, NULL);
      g_clear_pointer (&portal->update_monitor_handle, g_free);
    }
}

typedef struct {
  XdpPortal *portal;
  XdpParent *parent;
  char *parent_handle;
  GTask *task;
} InstallUpdateCall;

static void
install_update_call_free (InstallUpdateCall *call)
{
  if (call->parent)
    {
      call->parent->parent_unexport (call->parent);
      xdp_parent_free (call->parent);
    }
  g_free (call->parent_handle);

  g_object_unref (call->portal);
  g_object_unref (call->task);

  g_free (call);
}

static void install_update (InstallUpdateCall *call);

static void
create_parent_exported (XdpParent *parent,
                        const char *handle,
                        gpointer data)
{
  InstallUpdateCall *call = data;
  call->parent_handle = g_strdup (handle);
  install_update (call);
}

static void
update_started (GObject *object,
                GAsyncResult *result,
                gpointer data)
{
  InstallUpdateCall *call = data;
  g_autoptr(GError) error = NULL;
  g_autoptr(GVariant) ret = NULL;

  ret = g_dbus_connection_call_finish (G_DBUS_CONNECTION (object), result, &error);
  if (error)
    g_task_return_error (call->task, error);
  else
    g_task_return_boolean (call->task, TRUE);

  install_update_call_free (call);
}

static void
install_update (InstallUpdateCall *call)
{
  GVariantBuilder options;
  GCancellable *cancellable;

  if (call->portal->update_monitor_handle == NULL)
    {
      g_task_return_new_error (call->task, G_IO_ERROR, G_IO_ERROR_FAILED, "Not monitoring updates");
      install_update_call_free (call);
      return;
    }

  if (call->parent_handle == NULL)
    {
      call->parent->parent_export (call->parent, create_parent_exported, call);
      return;
    }

  cancellable = g_task_get_cancellable (call->task);

  g_variant_builder_init (&options, G_VARIANT_TYPE_VARDICT);
  g_dbus_connection_call (call->portal->bus,
                          FLATPAK_PORTAL_BUS_NAME,
                          call->portal->update_monitor_handle,
                          UPDATE_MONITOR_INTERFACE,
                          "Update",
                          g_variant_new ("(sa{sv})",
                                         call->parent_handle,
                                         &options),
                          NULL, 0, -1, cancellable, update_started, call);
}

/**
 * xdp_portal_update_install:
 * @portal: a [class@Portal]
 * @parent: a [struct@Parent]
 * @flags: options for this call
 * @cancellable: (nullable): optional [class@Gio.Cancellable]
 * @callback: (scope async): a callback to call when the request is done
 * @data: (closure): data to pass to @callback
 *
 * Installs an available software update.
 *
 * This should be called in response to a [signal@Portal::update-available]
 * signal.
 *
 * During the update installation, the [signal@Portal::update-progress]
 * signal will be emitted to provide progress information.
 */
void
xdp_portal_update_install (XdpPortal *portal,
                           XdpParent *parent,
                           XdpUpdateInstallFlags flags,
                           GCancellable *cancellable,
                           GAsyncReadyCallback callback,
                           gpointer data)
{
  InstallUpdateCall *call;

  g_return_if_fail (XDP_IS_PORTAL (portal));
  g_return_if_fail (flags == XDP_UPDATE_INSTALL_FLAG_NONE);

  call = g_new0 (InstallUpdateCall, 1);
  call->portal = g_object_ref (portal);
  if (parent)
    call->parent = xdp_parent_copy (parent);
  else
    call->parent_handle = g_strdup ("");
  call->task = g_task_new (portal, cancellable, callback, data);
  g_task_set_source_tag (call->task, xdp_portal_update_install);

  install_update (call);
}

/**
 * xdp_portal_update_install_finish:
 * @portal: a [class@Portal]
 * @result: a [iface@Gio.AsyncResult]
 * @error: return location for an error
 *
 * Finishes an update-installation request.
 *
 * Returns the result in the form of boolean.
 *
 * Note that the update may not be completely installed
 * by the time this function is called. You need to
 * listen to the [signal@Portal::update-progress] signal
 * to learn when the installation is complete.
 *
 * Returns: `TRUE` if the update is being installed
 */
gboolean
xdp_portal_update_install_finish (XdpPortal *portal,
                                  GAsyncResult *result,
                                  GError **error)
{
  g_return_val_if_fail (XDP_IS_PORTAL (portal), FALSE);
  g_return_val_if_fail (g_task_is_valid (result, portal), FALSE);
  g_return_val_if_fail (g_task_get_source_tag (G_TASK (result)) == xdp_portal_update_install, FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}
