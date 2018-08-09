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
#include "utils-private.h"

/**
 * SECTION:inhibit
 * @title: Inhibit
 * @short_description: prevent session state changes
 *
 * These functions let applications inhibit certain session state changes.
 * A typical use for this functionality is to prevent the session from
 * locking while a video is playing.
 *
 * The underlying portal is org.freedesktop.portal.Inhibit.
 */

typedef struct {
  XdpPortal *portal;
  XdpParent *parent;
  char *parent_handle;
  XdpInhibitFlags inhibit;
  char *reason;
  char *id;
  guint signal_id; 
} InhibitCall;

static void
inhibit_call_free (InhibitCall *call)
{
  if (call->parent)
    {
      call->parent->unexport (call->parent);
      _xdp_parent_free (call->parent);
    }
 g_free (call->parent_handle);

 if (call->signal_id)
   g_dbus_connection_signal_unsubscribe (call->portal->bus, call->signal_id);

  g_object_unref (call->portal);

  g_free (call->reason);
  g_free (call->id);

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
  InhibitCall *call = data;
  guint32 response;
  g_autoptr(GVariant) ret = NULL;

  g_variant_get (parameters, "(u@a{sv})", &response, &ret);

  if (response == 1)
    g_warning ("Inhibit canceled");
  else if (response == 2)
    g_warning ("Inhibit failed");

  if (response != 0)
    g_hash_table_remove (call->portal->inhibit_handles, call->id);

  inhibit_call_free (call);
}

static void do_inhibit (InhibitCall *call);

static void
parent_exported (XdpParent *parent,
                 const char *handle,
                 gpointer data)
{
  InhibitCall *call = data;
  call->parent_handle = g_strdup (handle);
  do_inhibit (call);
}

static void
do_inhibit (InhibitCall *call)
{
  GVariantBuilder options;
  g_autofree char *token = NULL;
  g_autofree char *handle = NULL;

  if (call->parent_handle == NULL)
    {
      call->parent->export (call->parent, parent_exported, call);
      return;
    }

  token = g_strdup_printf ("portal%d", g_random_int_range (0, G_MAXINT));
  handle = g_strconcat (REQUEST_PATH_PREFIX, call->portal->sender, "/", token, NULL);
  call->signal_id = g_dbus_connection_signal_subscribe (call->portal->bus,
                                                        PORTAL_BUS_NAME,
                                                        REQUEST_INTERFACE,
                                                        "Response",
                                                        handle,
                                                        NULL,
                                                        G_DBUS_SIGNAL_FLAGS_NO_MATCH_RULE,
                                                        response_received,
                                                        call,
                                                        NULL);

  g_hash_table_insert (call->portal->inhibit_handles,
                       g_strdup (call->id),
                       g_strdup (handle));

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
                          NULL, NULL, NULL);
}

/**
 * xdp_portal_inhibit:
 * @portal: a #XdpPortal
 * @parent: (nullable): parent window information
 * @inhibit: information about what to inhibit
 * @reason: (nullable): user-visible reason for the inhibition
 * @id: unique ID for this inhibition
 *
 * Inhibits various session status changes.
 *
 * It is the callers responsibility to ensure that the ID is unique among
 * all active inhibitors.
 *
 * To remove an active inhibitor, call xdp_portal_uninhibit() with the same ID.
 */
void
xdp_portal_inhibit (XdpPortal *portal,
                    XdpParent *parent,
                    XdpInhibitFlags inhibit,
                    const char *reason,
                    const char *id)
{
  InhibitCall *call = NULL;

  g_return_if_fail (XDP_IS_PORTAL (portal));

  if (portal->inhibit_handles == NULL)
    portal->inhibit_handles = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

  if (g_hash_table_lookup (portal->inhibit_handles, id))
    {
      g_warning ("Duplicate Inhibit id: %s", id);
      return;
    }

  call = g_new0 (InhibitCall, 1);
  call->portal = g_object_ref (portal);
  if (parent)
    call->parent = _xdp_parent_copy (parent);
  else
    call->parent_handle = g_strdup ("");
  call->inhibit = inhibit;
  call->reason = g_strdup (reason);  
  call->id = g_strdup (id);

  do_inhibit (call);
}

/**
 * xdp_portal_uninhibit:
 * @portal: a #XdpPortal
 * @id: unique ID for an active inhibition
 *
 * Removes an inhibitor that was created by a call to xdp_portal_inhibit().
 */
void
xdp_portal_uninhibit (XdpPortal *portal,
                      const char *id)
{
  g_autofree char *key = NULL;
  g_autofree char *value = NULL;

  g_return_if_fail (XDP_IS_PORTAL (portal));

  if (portal->inhibit_handles == NULL || 
      !g_hash_table_steal_extended (portal->inhibit_handles,
                                    id,
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
