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

#include "spawn.h"

#define GNU_SOURCE 1

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <glib/gstdio.h>
#include <gio/gunixfdlist.h>

#include "portal-private.h"

typedef struct {
  XdpPortal *portal;
  GTask *task;

  char *cwd;
  char **argv;
  int *fds;
  int *map_to;
  int n_fds;
  char **env;
  char **sandbox_expose;
  char **sandbox_expose_ro;
  XdpSpawnFlags flags;
} SpawnCall;

static void
spawn_call_free (SpawnCall *call)
{
  g_object_unref (call->portal);
  g_object_unref (call->task);

  g_free (call->cwd);
  g_strfreev (call->env);
  g_strfreev (call->sandbox_expose);
  g_strfreev (call->sandbox_expose_ro);

  g_free (call);
}

static void
spawned (GObject      *bus,
         GAsyncResult *result,
         gpointer      data)
{
  SpawnCall *call = data;
  GError *error = NULL;
  g_autoptr(GVariant) ret = NULL;

  ret = g_dbus_connection_call_with_unix_fd_list_finish (G_DBUS_CONNECTION (bus), NULL, result, &error);

  if (error)
    g_task_return_error (call->task, error);
  else
    {
      pid_t pid;

      g_variant_get (ret, "(u)", &pid);
      g_task_return_int (call->task, (gssize)pid);
    }

  spawn_call_free (call);
}

static void
spawn_exited (GDBusConnection *bus,
              const char *sender_name,
              const char *object_path,
              const char *interface_name,
              const char *signal_name,
              GVariant *parameters,
              gpointer data)
{
  XdpPortal *portal = data;
  guint pid;
  guint exit_status;

  g_variant_get (parameters, "(uu)", &pid, &exit_status);
  g_signal_emit_by_name (portal, "spawn-exited", pid, exit_status);
}

static void
ensure_spawn_exited_connection (XdpPortal *portal)
{
  if (portal->spawn_exited_signal == 0)
    {
      portal->spawn_exited_signal =
         g_dbus_connection_signal_subscribe (portal->bus,
                                             FLATPAK_PORTAL_BUS_NAME,
                                             FLATPAK_PORTAL_INTERFACE,
                                             "SpawnExited",
                                             FLATPAK_PORTAL_OBJECT_PATH,
                                             NULL,
                                             G_DBUS_SIGNAL_FLAGS_NO_MATCH_RULE,
                                             spawn_exited,
                                             portal,
                                             NULL);
    }
}

void
do_spawn (SpawnCall *call)
{
  g_autoptr(GUnixFDList) fd_list = NULL;
  GVariantBuilder fds_builder;
  GVariantBuilder env_builder;
  GVariantBuilder opt_builder;

  ensure_spawn_exited_connection (call->portal);

  g_variant_builder_init (&opt_builder, G_VARIANT_TYPE_VARDICT);

  g_variant_builder_init (&fds_builder, G_VARIANT_TYPE ("a{uh}"));
  if (call->n_fds > 0)
    {
      int i;

      fd_list = g_unix_fd_list_new_from_array (call->fds, call->n_fds);

      for (i = 0; i < call->n_fds; i++)
        g_variant_builder_add (&fds_builder,"{uh}", call->map_to[i], i);
    }

  g_variant_builder_init (&env_builder, G_VARIANT_TYPE ("a{ss}"));
  if (call->env != NULL)
    {
      int i;

      for (i = 0; call->env[i]; i++)
        {
          g_auto(GStrv) s = g_strsplit (call->env[i], "=", 2);
          if (s[0] && s[1])
            g_variant_builder_add (&env_builder, "{ss}", s[0], s[1]);
        }
    }
  g_variant_builder_init (&env_builder, G_VARIANT_TYPE_VARDICT);
  if (call->sandbox_expose)
    g_variant_builder_add (&env_builder, "{sv}", "sandbox-expose",
                           g_variant_new_strv ((const char *const*)call->sandbox_expose, -1));
  if (call->sandbox_expose_ro)
    g_variant_builder_add (&env_builder, "{sv}", "sandbox-expose-ro",
                           g_variant_new_strv ((const char *const*)call->sandbox_expose_ro, -1));

  g_dbus_connection_call_with_unix_fd_list (call->portal->bus,
                                            FLATPAK_PORTAL_BUS_NAME,
                                            FLATPAK_PORTAL_OBJECT_PATH,
                                            FLATPAK_PORTAL_INTERFACE,
                                            "Spawn",
                                            g_variant_new ("(ay^aaya{uh}a{ss}ua{sv})",
                                                           call->cwd,
                                                           call->argv,
                                                           fds_builder,
                                                           env_builder,
                                                           call->flags,
                                                           opt_builder),
                                            G_VARIANT_TYPE ("(u)"),
                                            G_DBUS_CALL_FLAGS_NONE,
                                            -1,
                                            fd_list,
                                            NULL,
                                            spawned,
                                            call);
}

/**
 * xdp_portal_spawn:
 * @portal: a [class@Portal]
 * @cwd: the cwd for the new process
 * @argv: (array zero-terminated): the argv for the new process
 * @fds: (array length=n_fds) (nullable): an array of open fds to pass to the new process, or `NULL`
 * @map_to: (array length=n_fds) (nullable): an array of integers to map the @fds to, or `NULL`. Must be the same
 *     length as @fds
 * @n_fds: the length of @fds and @map_to arrays
 * @env: (array zero-terminated) (nullable): an array of KEY=VALUE environment settings, or `NULL`
 * @flags: flags influencing the spawn operation
 * @sandbox_expose: (array zero-terminated) (nullable): paths to expose rw in the new sandbox, or `NULL`
 * @sandbox_expose_ro: (array zero-terminated) (nullable): paths to expose ro in the new sandbox, or `NULL`
 * @cancellable: (nullable): optional [class@Gio.Cancellable]
 * @callback: (scope async): a callback to call when the request is done
 * @data: (closure): data to pass to @callback
 *
 * Creates a new copy of the applications sandbox, and runs
 * a process in, with the given arguments.
 *
 * The learn when the spawned process exits, connect to the
 * [signal@Portal::spawn-exited] signal.
 */
void
xdp_portal_spawn (XdpPortal            *portal,
                  const char           *cwd,
                  const char * const   *argv,
                  int                  *fds,
                  int                  *map_to,
                  int                   n_fds,
                  const char * const   *env,
                  XdpSpawnFlags         flags,
                  const char * const   *sandbox_expose,
                  const char * const   *sandbox_expose_ro,
                  GCancellable         *cancellable,
                  GAsyncReadyCallback   callback,
                  gpointer              data)
{
  SpawnCall *call;

  g_return_if_fail (XDP_IS_PORTAL (portal));
  g_return_if_fail ((flags & !(XDP_SPAWN_FLAG_CLEARENV |
                               XDP_SPAWN_FLAG_LATEST |
                               XDP_SPAWN_FLAG_SANDBOX |
                               XDP_SPAWN_FLAG_NO_NETWORK |
                               XDP_SPAWN_FLAG_WATCH)) == 0);

  call = g_new (SpawnCall, 1);
  call->portal = g_object_ref (portal);
  call->cwd = g_strdup (cwd);
  call->argv = g_strdupv ((char **)argv);
  call->fds = fds;
  call->map_to = map_to;
  call->n_fds = n_fds;
  call->env = g_strdupv ((char **)env);
  call->flags = flags;
  call->sandbox_expose = g_strdupv ((char **)sandbox_expose);
  call->sandbox_expose_ro = g_strdupv ((char **)sandbox_expose_ro);
  call->task = g_task_new (portal, cancellable, callback, data);
  g_task_set_source_tag (call->task, xdp_portal_spawn);

  do_spawn (call);
}

/**
 * xdp_portal_spawn_finish:
 * @portal: a [class@Portal]
 * @result: a [iface@Gio.AsyncResult]
 * @error: return location for an error
 *
 * Finishes the spawn request.
 *
 * Returns the pid of the newly spawned process.
 *
 * Returns: the pid of the spawned process.
 */

pid_t
xdp_portal_spawn_finish (XdpPortal     *portal,
                         GAsyncResult  *result,
                         GError       **error)
{
  g_return_val_if_fail (XDP_IS_PORTAL (portal), 0);
  g_return_val_if_fail (g_task_is_valid (result, portal), 0);
  g_return_val_if_fail (g_task_get_source_tag (G_TASK (result)) == xdp_portal_spawn, 0);

  return (pid_t) g_task_propagate_int (G_TASK (result), error);
}

/**
 * xdp_portal_spawn_signal:
 * @portal: a [class@Portal]
 * @pid: the pid of the process to send a signal to
 * @signal: the Unix signal to send (see signal(7))
 * @to_process_group: whether to send the signal to the process
 *     group of the process
 *
 * Sends a Unix signal to a process that has been spawned
 * by [method@Portal.spawn].
 */
void
xdp_portal_spawn_signal (XdpPortal *portal,
                         pid_t      pid,
                         int        signal,
                         gboolean   to_process_group)
{
  g_return_if_fail (XDP_IS_PORTAL (portal));

  g_dbus_connection_call (portal->bus,
                          FLATPAK_PORTAL_BUS_NAME,
                          FLATPAK_PORTAL_OBJECT_PATH,
                          FLATPAK_PORTAL_INTERFACE,
                          "SpawnSignal",
                          g_variant_new ("(uub)", (guint)pid, (guint)signal, to_process_group),
                          NULL,
                          G_DBUS_CALL_FLAGS_NONE,
                          -1,
                          NULL, NULL, NULL);
}
