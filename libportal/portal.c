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

#include "portal-helpers.h"
#include "portal-private.h"
#include "portal-enums.h"

#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#ifdef HAVE_SYS_VFS_H
#include <sys/vfs.h>
#endif
#include <stdio.h>

const char *
portal_get_bus_name (void)
{
  static const char *busname = NULL;

  if (g_once_init_enter (&busname))
    {
      const char *env = g_getenv ("LIBPORTAL_PORTAL_BUS_NAME");
      g_once_init_leave (&busname, env ?: "org.freedesktop.portal.Desktop");
    }

  return busname;
}

/**
 * XdpPortal
 *
 * Context for portal calls.
 *
 * The XdpPortal object provides the main context object
 * for the portal operations of libportal.
 *
 * Typically, an application will create a single XdpPortal
 * object with [ctor@Portal.new] and use it throughout its lifetime.
 */

enum {
  SPAWN_EXITED,
  SESSION_STATE_CHANGED,
  UPDATE_AVAILABLE,
  UPDATE_PROGRESS,
  LOCATION_UPDATED,
  NOTIFICATION_ACTION_INVOKED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];
static void xdp_portal_initable_iface_init (GInitableIface  *iface);

G_DEFINE_TYPE_WITH_CODE (XdpPortal, xdp_portal, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
                                                xdp_portal_initable_iface_init))

static void
xdp_portal_finalize (GObject *object)
{
  XdpPortal *portal = XDP_PORTAL (object);

  /* inhibit */
  if (portal->inhibit_handles)
    g_hash_table_unref (portal->inhibit_handles);

  if (portal->state_changed_signal)
    g_dbus_connection_signal_unsubscribe (portal->bus, portal->state_changed_signal);

  g_free (portal->session_monitor_handle);

  /* spawn */
  if (portal->spawn_exited_signal)
    g_dbus_connection_signal_unsubscribe (portal->bus, portal->spawn_exited_signal);

  /* updates */
  if (portal->update_available_signal)
    g_dbus_connection_signal_unsubscribe (portal->bus, portal->update_available_signal);
  if (portal->update_progress_signal)
    g_dbus_connection_signal_unsubscribe (portal->bus, portal->update_progress_signal);
  g_free (portal->update_monitor_handle);

  /* location */
  if (portal->location_updated_signal)
    g_dbus_connection_signal_unsubscribe (portal->bus, portal->location_updated_signal);
  g_free (portal->location_monitor_handle);

  /* notification */
  if (portal->action_invoked_signal)
    g_dbus_connection_signal_unsubscribe (portal->bus, portal->action_invoked_signal);

  g_clear_object (&portal->bus);
  g_free (portal->sender);

  G_OBJECT_CLASS (xdp_portal_parent_class)->finalize (object);
}

static void
xdp_portal_class_init (XdpPortalClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = xdp_portal_finalize;

  /**
   * XdpPortal::spawn-exited:
   * @portal: the [class@Portal] object
   * @pid: the pid of the process
   * @exit_status: the exit status of the process
   *
   * Emitted when a process that was spawned with [method@Portal.spawn] exits.
   */
  signals[SPAWN_EXITED] = g_signal_new ("spawn-exited",
                                        G_TYPE_FROM_CLASS (object_class),
                                        G_SIGNAL_RUN_FIRST,
                                        0,
                                        NULL, NULL,
                                        NULL,
                                        G_TYPE_NONE, 2,
                                        G_TYPE_UINT,
                                        G_TYPE_UINT);

  /**
   * XdpPortal::session-state-changed:
   * @portal: the [class@Portal] object
   * @screensaver_active: whether the screensaver is active
   * @session_state: the current state of the login session
   *
   * Emitted when session state monitoring is
   * enabled and the state of the login session changes or
   * the screensaver is activated or deactivated.
   */
  signals[SESSION_STATE_CHANGED] = g_signal_new ("session-state-changed",
                                                 G_TYPE_FROM_CLASS (object_class),
                                                 G_SIGNAL_RUN_FIRST,
                                                 0,
                                                 NULL, NULL,
                                                 NULL,
                                                 G_TYPE_NONE, 2,
                                                 G_TYPE_BOOLEAN,
                                                 XDP_TYPE_LOGIN_SESSION_STATE);

  /**
   * XdpPortal::update-available:
   * @portal: the [class@Portal] object
   * @running_commit: the commit that the sandbox is running with
   * @local_commit: the commit that is currently deployed. Restarting
   *     the app will use this commit
   * @remote_commit: the commit that is available as an update.
   *     Updating the app will deloy this commit
   *
   * Emitted when updates monitoring is enabled
   * and a new update is available.
   *
   * It is only sent once with the same information, but it can be sent many
   * times if new updates appear.
   */
  signals[UPDATE_AVAILABLE] = g_signal_new ("update-available",
                                            G_TYPE_FROM_CLASS (object_class),
                                            G_SIGNAL_RUN_FIRST,
                                            0,
                                            NULL, NULL,
                                            NULL,
                                            G_TYPE_NONE, 3,
                                            G_TYPE_STRING,
                                            G_TYPE_STRING,
                                            G_TYPE_STRING);

  /**
   * XdpPortal::update-progress:
   * @portal: the [class@Portal] object
   * @n_ops: the number of operations that the update consists of
   * @op: the position of the currently active operation
   * @progress: the progress of the currently active operation, as
   *   a number between 0 and 100
   * @status: the overall status of the update
   * @error: the error name if the status is 'failed'
   * @error_message: the error message if the status is 'failed'
   *
   * Emitted to indicate progress of an update installation.
   *
   * It is undefined exactly how often it is sent, but it will be emitted at
   * least once at the end with a non-zero @status. For each successful
   * operation in the update, we're also guaranteed to send exactly one signal
   * with @progress 100.
   */
  signals[UPDATE_PROGRESS] = g_signal_new ("update-progress",
                                           G_TYPE_FROM_CLASS (object_class),
                                           G_SIGNAL_RUN_FIRST,
                                           0,
                                           NULL, NULL,
                                           NULL,
                                           G_TYPE_NONE, 6,
                                           G_TYPE_UINT,
                                           G_TYPE_UINT,
                                           G_TYPE_UINT,
                                           XDP_TYPE_UPDATE_STATUS,
                                           G_TYPE_STRING,
                                           G_TYPE_STRING);

 /**
   * XdpPortal::location-updated:
   * @portal: the [class@Portal]
   * @latitude: the latitude, in degrees
   * @longitude: the longitude, in degrees
   * @altitude: the altitude, in meters
   * @accuracy: the accuracy, in meters
   * @speed: the speed, in meters per second
   * @heading: the heading, in degrees
   * @description: the description
   * @timestamp_s: the timestamp seconds since the Unix epoch
   * @timestamp_ms: the microseconds fraction of the timestamp
   *
   * Emitted when location monitoring is enabled and the location changes.
   */
  signals[LOCATION_UPDATED] =
    g_signal_new ("location-updated",
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_FIRST,
                  0,
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 9,
                  G_TYPE_DOUBLE,
                  G_TYPE_DOUBLE,
                  G_TYPE_DOUBLE,
                  G_TYPE_DOUBLE,
                  G_TYPE_DOUBLE,
                  G_TYPE_DOUBLE,
                  G_TYPE_STRING,
                  G_TYPE_INT64,
                  G_TYPE_INT64);

  /**
   * XdpPortal::notification-action-invoked:
   * @portal: the [class@Portal]
   * @id: the notification ID
   * @action: the action name
   * @parameter: (nullable): the target parameter for the action
   *
   * Emitted when a non-exported action is activated on a notification.
   */
  signals[NOTIFICATION_ACTION_INVOKED] =
    g_signal_new ("notification-action-invoked",
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_FIRST,
                  0,
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 3,
                  G_TYPE_STRING,
                  G_TYPE_STRING,
                  G_TYPE_VARIANT);
}

static GDBusConnection *
create_bus_from_address (const char *address,
                         GError    **error)
{
  g_autoptr(GDBusConnection) bus = NULL;

  if (!address)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED, "Missing D-Bus session bus address");
      return NULL;
    }

  bus = g_dbus_connection_new_for_address_sync (address,
                                                G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT |
                                                G_DBUS_CONNECTION_FLAGS_MESSAGE_BUS_CONNECTION,
                                                NULL, NULL,
                                                error);
  return g_steal_pointer (&bus);
}

static gboolean
xdp_portal_initable_init (GInitable     *initable,
                          GCancellable  *cancellable,
                          GError       **out_error)
{
  int i;

  XdpPortal *portal = (XdpPortal*) initable;

  /* g_bus_get_sync() returns a singleton. In the test suite we may restart
   * the session bus, so we have to manually connect to the new bus */
  if (getenv ("LIBPORTAL_TEST_SUITE"))
    portal->bus = create_bus_from_address (getenv ("DBUS_SESSION_BUS_ADDRESS"), out_error);
  else
    portal->bus = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, out_error);

  if (portal->bus == NULL)
    return FALSE;

  portal->sender = g_strdup (g_dbus_connection_get_unique_name (portal->bus) + 1);
  for (i = 0; portal->sender[i]; i++)
    if (portal->sender[i] == '.')
      portal->sender[i] = '_';

  return TRUE;
}

static void
xdp_portal_initable_iface_init (GInitableIface *iface)
{
  iface->init = xdp_portal_initable_init;
}

static void
xdp_portal_init (XdpPortal *portal)
{
}

/**
 * xdp_portal_initable_new:
 * @error: A GError location to store the error occurring, or NULL to ignore.
 *
 * Creates a new [class@Portal] object.
 *
 * Returns: (nullable): a newly created [class@Portal] object or NULL on error
 */
XdpPortal *
xdp_portal_initable_new (GError **error)
{
  return g_initable_new (XDP_TYPE_PORTAL, NULL, error, NULL);
}

/**
 * xdp_portal_new:
 *
 * Creates a new [class@Portal] object. If D-Bus is unavailable this API will abort.
 * We recommend using xdp_portal_initable_new() to safely handle this failure.
 *
 * Returns: a newly created [class@Portal] object
 */
XdpPortal *
xdp_portal_new (void)
{
  g_autoptr(GError) error = NULL;
  XdpPortal *portal = xdp_portal_initable_new (&error);
  if (error)
    {
      g_critical ("Failed to create XdpPortal instance: %s\n", error->message);
      abort ();
    }

  return portal;
}

/* This function is copied from xdg-desktop-portal */
static int
_xdp_parse_cgroup_file (FILE *f, gboolean *is_snap)
{
  ssize_t n;
  g_autofree char *id = NULL;
  g_autofree char *controller = NULL;
  g_autofree char *cgroup = NULL;
  size_t id_len = 0, controller_len = 0, cgroup_len = 0;

  g_return_val_if_fail (f != NULL, -1);
  g_return_val_if_fail (is_snap != NULL, -1);

  *is_snap = FALSE;
  do
    {
      n = getdelim (&id, &id_len, ':', f);
      if (n == -1) break;
      n = getdelim (&controller, &controller_len, ':', f);
      if (n == -1) break;
      n = getdelim (&cgroup, &cgroup_len, '\n', f);
      if (n == -1) break;

      /* Only consider the freezer, systemd group or unified cgroup
       * hierarchies */
      if (controller != NULL &&
          (g_str_equal (controller, "freezer:") ||
           g_str_equal (controller, "name=systemd:") ||
           g_str_equal (controller, ":")) &&
          strstr (cgroup, "/snap.") != NULL)
        {
          *is_snap = TRUE;
          break;
        }
    }
  while (n >= 0);

  if (n < 0 && !feof(f)) return -1;

  return 0;
}

/* This function is copied from xdg-desktop-portal pid_is_snap() */
static gpointer
get_under_snap (gpointer user_data)
{
  g_autofree char *cgroup_path = NULL;;
  int fd;
  FILE *f = NULL;
  gboolean is_snap = FALSE;
  int err = 0;
  pid_t pid = getpid ();
  GError **error = user_data;

  cgroup_path = g_strdup_printf ("/proc/%u/cgroup", (guint) pid);
  fd = open (cgroup_path, O_RDONLY | O_CLOEXEC | O_NOCTTY);
  if (fd == -1)
    {
      err = errno;
      goto end;
    }

  f = fdopen (fd, "r");
  if (f == NULL)
    {
      err = errno;
      goto end;
    }

  fd = -1; /* fd is now owned by f */

  if (_xdp_parse_cgroup_file (f, &is_snap) == -1)
    err = errno;

  fclose (f);

end:
  /* Silence ENOENT, treating it as "not a snap" */
  if (err != 0 && err != ENOENT)
    {
      g_set_error (error, G_IO_ERROR, g_io_error_from_errno (err),
                   "Could not parse cgroup info for pid %u: %s", (guint) pid,
                   g_strerror (err));
      return GINT_TO_POINTER (FALSE);
    }
  return GINT_TO_POINTER (is_snap);
}

/**
 * xdp_portal_running_under_flatpak:
 *
 * Detects if running inside of a Flatpak or WebKit sandbox.
 *
 * See also: [func@Portal.running_under_sandbox].
 *
 * Returns: %TRUE if the current process is running under a Flatpak sandbox
 */
gboolean
xdp_portal_running_under_flatpak (void)
{
  static gsize under_flatpak = 0;
  enum {
    NOT_FLATPAK = 1,
    IS_FLATPAK  = 2
  };

  if (g_once_init_enter (&under_flatpak))
    {
      gboolean flatpak_info_exists = g_file_test ("/.flatpak-info", G_FILE_TEST_EXISTS);
      if (flatpak_info_exists)
        g_once_init_leave (&under_flatpak, IS_FLATPAK);
      else
        g_once_init_leave (&under_flatpak, NOT_FLATPAK);
    }

  return under_flatpak == IS_FLATPAK;
}

/**
 * xdp_portal_running_under_snap:
 * @error: return location for a #GError pointer
 *
 * Detects if you are running inside of a Snap sandbox.
 *
 * See also: [func@Portal.running_under_sandbox].
 *
 * Returns: %TRUE if the current process is running under a Snap sandbox, or
 *   %FALSE if either unsandboxed or an error was encountered in which case
 *   @error will be set
 */
gboolean
xdp_portal_running_under_snap (GError **error)
{
  static GOnce under_snap_once = G_ONCE_INIT;
  static GError *cached_error = NULL;
  gboolean under_snap;

  under_snap = GPOINTER_TO_INT (g_once (&under_snap_once, get_under_snap, &cached_error));

  if (error != NULL && cached_error != NULL)
    g_propagate_error (error, g_error_copy (cached_error));

  return under_snap;
}

/**
 * xdp_portal_running_under_sandbox:
 *
 * This function tries to determine if the current process is running under a
 * sandbox that requires the use of portals.
 *
 * If you need to check error conditions see [func@Portal.running_under_snap].
 *
 * Note that these functions are all cached and will always return the same result.
 *
 * Returns: %TRUE if the current process should use portals to access resources
 *   on the host system, or %FALSE if either an error was encountered or the
 *   process is running unsandboxed
 */
gboolean
xdp_portal_running_under_sandbox (void)
{
  return xdp_portal_running_under_flatpak () || xdp_portal_running_under_snap (NULL);
}
