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

#include "portal-private.h"
#include "utils.h"

typedef struct {
  XdpPortal *portal;
  char *id;
  GVariant *notification;
  gboolean add;
} NotificationCall;

static void
notification_call_free (NotificationCall *call)
{
  g_object_unref (call->portal);
  g_free (call->id);
  if (call->notification)
    g_variant_unref (call->notification);

  g_free (call);
}

static void do_notification (NotificationCall *call);

static void
got_proxy (GObject *source,
           GAsyncResult *res,
           gpointer data)
{
  NotificationCall *call = data;
  g_autoptr(GError) error = NULL;

  call->portal->notification = _xdp_notification_proxy_new_for_bus_finish (res, &error);
  if (call->portal->notification == NULL)
    {
      g_warning ("Failed to get notification proxy: %s", error->message);
      notification_call_free (call);
      return;
    }
  do_notification (call);
}

static void
call_done (GObject *source,
           GAsyncResult *result,
           gpointer data)
{
  NotificationCall *call = data;
  g_autoptr(GError) error = NULL;

g_print ("call done\n");
  if (!_xdp_notification_call_add_notification_finish (call->portal->notification, result, &error))
    g_warning ("%s failed: %s", call->add ? "AddNotification" : "RemoveNotification", error->message);

 notification_call_free (call);
}

static void
do_notification (NotificationCall *call)
{
g_print ("do_notification\n");
  if (call->portal->notification == NULL)
    {
g_print ("getting proxy\n");
       _xdp_notification_proxy_new_for_bus (G_BUS_TYPE_SESSION,
                                            G_DBUS_PROXY_FLAGS_NONE,
                                            PORTAL_BUS_NAME,
                                            PORTAL_OBJECT_PATH,
                                            NULL,
                                            got_proxy,
                                            call);
       return;
    }

  if (call->add)
    _xdp_notification_call_add_notification (call->portal->notification,
                                             call->id,
                                             call->notification,
                                             NULL,
                                             call_done,
                                             call);
  else
    _xdp_notification_call_remove_notification (call->portal->notification,
                                             call->id,
                                             NULL,
                                             call_done,
                                             call);
}

void
xdp_portal_add_notification (XdpPortal  *portal,
                             const char *id,
                             GVariant   *notification)
{
  NotificationCall *call;

g_print ("call add_notification\n");
  call = g_new (NotificationCall, 1);
  call->portal = g_object_ref (portal);
  call->id = g_strdup (id);
  call->add = TRUE;
  call->notification = g_variant_ref (notification);

  do_notification (call);
}

void
xdp_portal_remove_notification (XdpPortal  *portal,
                                const char *id)
{
  NotificationCall *call;

  call = g_new (NotificationCall, 1);
  call->portal = g_object_ref (portal);
  call->id = g_strdup (id);
  call->add = FALSE;
  call->notification = NULL;

  do_notification (call);
}
