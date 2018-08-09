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

/**
 * SECTION:portal
 * @title: XdpPortal
 * @short_description: context for portal calls
 *
 * The XdpPortal object provides the main context object
 * for the portal operations of libportal.
 *
 * Typically, an application will create a single XdpPortal
 * object with xdp_portal_new() and use it throughout its lifetime.
 */

G_DEFINE_TYPE (XdpPortal, xdp_portal, G_TYPE_OBJECT)

static void
xdp_portal_finalize (GObject *object)
{
  XdpPortal *portal = XDP_PORTAL (object);

  g_clear_object (&portal->bus);
  g_free (portal->sender);

  if (portal->inhibit_handles)
    g_hash_table_unref (portal->inhibit_handles);

  G_OBJECT_CLASS (xdp_portal_parent_class)->finalize (object);
}

static void
xdp_portal_class_init (XdpPortalClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = xdp_portal_finalize;
}

static void
xdp_portal_init (XdpPortal *portal)
{
  int i;

  portal->bus = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, NULL);
  portal->sender = g_strdup (g_dbus_connection_get_unique_name (portal->bus) + 1);
  for (i = 0; portal->sender[i]; i++)
    if (portal->sender[i] == '.') 
      portal->sender[i] = '_';
}

/**
 * xdp_portal_new:
 *
 * Creates a new #XdpPortal object.
 *
 * Returns: a newly created #XdpPortal object
 */
XdpPortal *
xdp_portal_new (void)
{
  return g_object_new (XDP_TYPE_PORTAL, NULL);
}

/**
 * xdp_parent_new_gtk:
 * @window: a #GtkWindow
 *
 * Creates an #XdpParent for @window.
 *
 * Returns: a newly created #XdpParent. Free with xdp_parent_free
 */

/**
 * xdp_parent_free:
 * @parent: a #XdpParent
 *
 * Frees an #XdpParent.
 */
