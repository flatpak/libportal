/*
 * Copyright (C) 2018, Matthias Clasen
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
 * SECTION:open
 * @title: Open
 * @short_description: handle URIs
 *
 * These functions let applications open URIs in external handlers.
 * A typical example is to open a pdf file in a document viewer.
 *
 * The underlying portal is org.freedesktop.portal.OpenURI.
 */

typedef struct {
  XdpPortal *portal;
  XdpParent *parent;
  char *parent_handle;
  char *uri;
  gboolean writable;
} OpenCall;

static void
open_call_free (OpenCall *call)
{
  if (call->parent)
    {
      call->parent->unexport (call->parent);
      _xdp_parent_free (call->parent);
    }
  g_free (call->parent_handle);

  g_object_unref (call->portal);
  g_free (call->uri);

  g_free (call);
}

static void do_open (OpenCall *call);

static void
parent_exported (XdpParent *parent,
                 const char *handle,
                 gpointer data)
{
  OpenCall *call = data;
  call->parent_handle = g_strdup (handle);
  do_open (call);
}

#ifndef O_PATH
#define O_PATH 0
#endif

static void
do_open (OpenCall *call)
{
  GVariantBuilder options;
  g_autofree char *token = NULL;
  g_autoptr(GFile) file = NULL;

  if (call->parent_handle == NULL)
    {
      call->parent->export (call->parent, parent_exported, call);
      return;
    }

  token = g_strdup_printf ("portal%d", g_random_int_range (0, G_MAXINT));

  g_variant_builder_init (&options, G_VARIANT_TYPE_VARDICT);
  g_variant_builder_add (&options, "{sv}", "handle_token", g_variant_new_string (token));
  g_variant_builder_add (&options, "{sv}", "writable", g_variant_new_boolean (call->writable));

  file = g_file_new_for_uri (call->uri);

  if (g_file_is_native (file))
    {
      g_autoptr(GUnixFDList) fd_list = NULL;
      g_autofree char *path = NULL;
      int fd, fd_in;

      path = g_file_get_path (file);

      fd = g_open (path, O_PATH | O_CLOEXEC);
      if (fd == -1)
        {
          g_warning ("Failed to open '%s'", call->uri);
          return;
        }

      fd_list = g_unix_fd_list_new_from_array (&fd, 1);
      fd = -1;
      fd_in = 0;

      g_dbus_connection_call_with_unix_fd_list (call->portal->bus,
                                                PORTAL_BUS_NAME,
                                                PORTAL_OBJECT_PATH,
                                                "org.freedesktop.portal.OpenURI",
                                                "OpenFile",
                                                g_variant_new ("(sha{sv})", call->parent_handle, fd_in, &options),
                                                NULL,
                                                G_DBUS_CALL_FLAGS_NONE,
                                                -1,
                                                fd_list,
                                                NULL, NULL, NULL);
    }
  else
    {
      g_dbus_connection_call (call->portal->bus,
                              PORTAL_BUS_NAME,
                              PORTAL_OBJECT_PATH,
                              "org.freedesktop.portal.OpenURI",
                              "OpenUri",
                              g_variant_new ("(ssa{sv})", call->parent_handle, call->uri, &options),
                              NULL,
                              G_DBUS_CALL_FLAGS_NONE,
                              -1,
                              NULL, NULL, NULL);
    }

  open_call_free (call);
}

/**
 * xdp_portal_open_uri:
 * @portal: a #XdpPortal
 * @parent: parent window information
 * @uri: the URI to open
 * @writable: whether to make the file writable
 *
 * Opens @uri with an external hamdler.
 */
void
xdp_portal_open_uri (XdpPortal *portal,
                     XdpParent *parent,
                     const char *uri,
                     gboolean writable)
{
  OpenCall *call = NULL;

  g_return_if_fail (XDP_IS_PORTAL (portal));

  call = g_new0 (OpenCall, 1);
  call->portal = g_object_ref (portal);
  if (parent)
    call->parent = _xdp_parent_copy (parent);
  else
    call->parent_handle = g_strdup ("");
  call->uri = g_strdup (uri);
  call->writable = writable;

  do_open (call);
}
