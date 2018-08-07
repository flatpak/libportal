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

G_DECLARE_FINAL_TYPE (AccountCall, account_call, ACCOUNT, CALL, GObject)

typedef struct {
  XdpPortal *portal;
  GtkWindow *parent;
  char *parent_handle;
  XdpInhibitFlags inhibit;
  char *reason;
  guint response_signal; 
  char *id;
  char *handle;
} InhibitCall;

static void
inhibit_call_free (InhibitCall *call)
{
 if (call->response_signal)
    {
      GDBusConnection *bus;

      bus = g_dbus_proxy_get_connection (G_DBUS_PROXY (call->portal->account));
      g_dbus_connection_signal_unsubscribe (bus, call->response_signal);
    }
  g_object_unref (call->portal);
  if (call->parent)
    g_object_unref (call->parent);
  g_free (call->parent_handle);
  g_free (call->reason);
  g_free (call->handle);
  g_free (call->id);

  g_free (call);
}

static void do_inhibit (InhibitCall *call);

static void
got_proxy (GObject *source,
           GAsyncResult *res,
           gpointer data)
{
  InhibitCall *call = data;
  g_autoptr(GError) error = NULL;

  call->portal->inhibit = _xdp_inhibit_proxy_new_for_bus_finish (res, &error);
  if (call->portal->inhibit == NULL)
    {
      inhibit_call_free (call);
      return;
    }

  call->portal->inhibit_handles = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
  do_inhibit (call);
}

static void
window_handle_exported (GtkWindow *window,
                        const char *window_handle,
                        gpointer data)
{
  InhibitCall *call = data;

  call->parent_handle = g_strdup (window_handle);

  do_inhibit (call);
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

  g_dbus_connection_signal_unsubscribe (bus, call->response_signal);
  call->response_signal = 0;

  g_variant_get (parameters, "(u@a{sv})", &response, &ret);

  if (response == 0)
    {
      g_hash_table_insert (call->portal->inhibit_handles,
                           g_strdup (call->id),
                           g_strdup (call->handle));
    }
  else if (response == 1)
    g_warning ("Inhibit canceled");
  else
    g_warning ("Inhibit failed");

  inhibit_call_free (call);
}

static void
do_inhibit (InhibitCall *call)
{
  GVariantBuilder options;
  GDBusConnection *bus;
  g_autofree char *token = NULL;
  g_autofree char *sender = NULL;
  int i;

  if (call->portal->inhibit == NULL)
    {
       _xdp_inhibit_proxy_new_for_bus (G_BUS_TYPE_SESSION,
                                       G_DBUS_PROXY_FLAGS_NONE,
                                       PORTAL_BUS_NAME,
                                       PORTAL_OBJECT_PATH,
                                       NULL,
                                       got_proxy,
                                       call);
       return;
    }

  if (call->parent_handle == NULL)
    {
      if (call->parent != NULL)
        {
          _gtk_window_export_handle (call->parent,
                                     window_handle_exported,
                                     call);
          return;
        }

      call->parent_handle = g_strdup ("");
    }

  if (g_hash_table_lookup (call->portal->inhibit_handles, call->id))
    {
      g_warning ("Duplicate Inhibit id: %s", call->id);
      inhibit_call_free (call);
      return;
    }

  bus = g_dbus_proxy_get_connection (G_DBUS_PROXY (call->portal->inhibit));

  token = g_strdup_printf ("portal%d", g_random_int_range (0, G_MAXINT));
  sender = g_strdup (g_dbus_connection_get_unique_name (bus) + 1);
  for (i = 0; sender[i]; i++)
    if (sender[i] == '.')
      sender[i] = '_';

  call->handle = g_strdup_printf ("/org/freedesktop/portal/desktop/request/%s/%s", sender, token);
  call->response_signal = g_dbus_connection_signal_subscribe (bus,
                                                              PORTAL_BUS_NAME,
                                                              REQUEST_INTERFACE,
                                                              "Response",
                                                              call->handle,
                                                              NULL,
                                                              G_DBUS_SIGNAL_FLAGS_NO_MATCH_RULE,
                                                              response_received,
                                                              call,
                                                              NULL);

  g_hash_table_insert (call->portal->inhibit_handles,
                       g_strdup (call->id),
                       g_strdup (call->handle));

  g_variant_builder_init (&options, G_VARIANT_TYPE_VARDICT);
  g_variant_builder_add (&options, "{sv}", "handle_token", g_variant_new_string (token));
  g_variant_builder_add (&options, "{sv}", "reason", g_variant_new_string (call->reason));

  _xdp_inhibit_call_inhibit (call->portal->inhibit,
                             call->parent_handle,
                             call->inhibit,
                             g_variant_builder_end (&options),
                             NULL,
                             NULL,
                             NULL);
}

void
xdp_portal_inhibit (XdpPortal       *portal,
                    GtkWindow       *parent,
                    XdpInhibitFlags  inhibit,
                    const char      *reason,
                    const char      *id)
{
  InhibitCall *call = NULL;

  call = g_new (InhibitCall, 1);
  call->portal = g_object_ref (portal);
  call->parent = parent ? g_object_ref (parent) : NULL;
  call->parent_handle = NULL;
  call->inhibit = inhibit;
  call->reason = g_strdup (reason);  
  call->id = g_strdup (id);

  do_inhibit (call);
}

void
xdp_portal_uninhibit (XdpPortal  *portal,
                      const char *id)
{
  GDBusConnection *bus;
  g_autofree char *key = NULL;
  g_autofree char *value = NULL;

  if (!g_hash_table_steal_extended (portal->inhibit_handles,
                                    id,
                                    (gpointer *)&key,
                                    (gpointer *)&value))
    {
      g_warning ("No inhibit handle found");
      return;
    }

  bus = g_dbus_proxy_get_connection (G_DBUS_PROXY (portal->inhibit));
  g_dbus_connection_call (bus,
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
