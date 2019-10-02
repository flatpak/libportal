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

#define GNU_SOURCE 1

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <glib/gstdio.h>
#include <gio/gunixfdlist.h>

#include "portal-private.h"
#include "utils-private.h"

/**
 * SECTION:trash
 * @title: Trash
 * @short_description: send files to the trash
 *
 * This function lets applications send a file to the trash can.
 *
 * The underlying portal is org.freedesktop.portal.Trash.
 */

#ifndef O_PATH
#define O_PATH 0
#endif

static void
trash_file (XdpPortal  *portal,
            const char *path)
{
  g_autoptr(GUnixFDList) fd_list = NULL;
  int fd, fd_in;

  fd = g_open (path, O_PATH | O_CLOEXEC);
  if (fd == -1)
    {
      g_warning ("Failed to open '%s'", path);
      return;
    }

  fd_list = g_unix_fd_list_new_from_array (&fd, 1);
  fd = -1;
  fd_in = 0;

  g_dbus_connection_call_with_unix_fd_list (portal->bus,
                                            PORTAL_BUS_NAME,
                                            PORTAL_OBJECT_PATH,
                                            "org.freedesktop.portal.Trash",
                                            "TrashFile",
                                            g_variant_new ("(h)", fd_in),
                                            NULL,
                                            G_DBUS_CALL_FLAGS_NONE,
                                            -1,
                                            fd_list,
                                            NULL,
                                            NULL,
                                            NULL);

}

/**
 * xdp_portal_trash_file:
 * @portal: a #XdpPortal
 * @path: the path for a local file
 *
 * Sends the file to the trash can.
 */
void
xdp_portal_trash_file (XdpPortal  *portal,
                       const char *path)
{
  g_return_if_fail (XDP_IS_PORTAL (portal));
  g_return_if_fail (path != NULL);

  trash_file (portal, path);
}
