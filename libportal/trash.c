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

#include "trash.h"

#define GNU_SOURCE 1

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <glib/gstdio.h>
#include <gio/gunixfdlist.h>

#include "portal-private.h"

typedef struct {
  XdpPortal *portal;
  char *path;
  GTask *task;
} TrashCall;

static void
trash_call_free (TrashCall *call)
{
  g_object_unref (call->portal);
  g_object_unref (call->task);
  g_free (call->path);

  g_free (call);
}

#ifndef O_PATH
#define O_PATH 0
#endif

static void
file_trashed (GObject      *bus,
              GAsyncResult *result,
              gpointer      data)
{
  GError *error = NULL;
  g_autoptr(GVariant) ret = NULL;
  TrashCall *call = data;

  ret = g_dbus_connection_call_with_unix_fd_list_finish (G_DBUS_CONNECTION (bus),
                                                         NULL,
                                                         result,
                                                         &error);
  if (error)
    g_task_return_error (call->task, error);
  else
    {
      guint retval;

      g_variant_get (ret, "(u)", &retval);

      if (retval == 1)
        g_task_return_boolean (call->task, TRUE);
      else
        g_task_return_new_error (call->task, G_IO_ERROR, G_IO_ERROR_FAILED, "Failed to trash");
    }

  trash_call_free (call);
}

static void
trash_file (TrashCall *call)
{
  g_autoptr(GUnixFDList) fd_list = NULL;
  int fd, fd_in;
  GCancellable *cancellable;

  fd = g_open (call->path, O_PATH | O_CLOEXEC);
  if (fd == -1)
    {
      g_task_return_new_error (call->task, G_IO_ERROR, G_IO_ERROR_FAILED, "Failed to open '%s'", call->path);
      trash_call_free (call);
      return;
    }

  fd_list = g_unix_fd_list_new_from_array (&fd, 1);
  fd = -1;
  fd_in = 0;

  cancellable = g_task_get_cancellable (call->task);

  g_dbus_connection_call_with_unix_fd_list (call->portal->bus,
                                            PORTAL_BUS_NAME,
                                            PORTAL_OBJECT_PATH,
                                            "org.freedesktop.portal.Trash",
                                            "TrashFile",
                                            g_variant_new ("(h)", fd_in),
                                            NULL,
                                            G_DBUS_CALL_FLAGS_NONE,
                                            -1,
                                            fd_list,
                                            cancellable,
                                            file_trashed,
                                            call);

}

/**
 * xdp_portal_trash_file:
 * @portal: a [class@Portal]
 * @path: the path for a local file
 * @cancellable: (nullable): optional [class@Gio.Cancellable]
 * @callback: (scope async): a callback to call when the request is done
 * @data: (closure): data to pass to @callback
 *
 * Sends the file at @path to the trash can.
 */
void
xdp_portal_trash_file (XdpPortal           *portal,
                       const char          *path,
                       GCancellable        *cancellable,
                       GAsyncReadyCallback  callback,
                       gpointer             data)

{
  TrashCall *call;

  g_return_if_fail (XDP_IS_PORTAL (portal));
  g_return_if_fail (path != NULL);

  call = g_new0 (TrashCall, 1);
  call->portal = g_object_ref (portal);
  call->path = g_strdup (path);
  call->task = g_task_new (portal, cancellable, callback, data);
  g_task_set_source_tag (call->task, xdp_portal_trash_file);

  trash_file (call);
}

/**
 * xdp_portal_trash_file_finish:
 * @portal: a [class@Portal]
 * @result: a [iface@Gio.AsyncResult]
 * @error: return location for an error
 *
 * Finishes the trash-file request.
 *
 * Returns the result in the form of a boolean.
 *
 * Returns: `TRUE` if the call succeeded
 */
gboolean
xdp_portal_trash_file_finish (XdpPortal *portal,
                              GAsyncResult *result,
                              GError **error)
{
  g_return_val_if_fail (XDP_IS_PORTAL (portal), FALSE);
  g_return_val_if_fail (g_task_is_valid (result, portal), FALSE);
  g_return_val_if_fail (g_task_get_source_tag (G_TASK (result)) == xdp_portal_trash_file, FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}
