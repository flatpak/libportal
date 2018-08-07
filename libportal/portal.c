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

G_DEFINE_TYPE (XdpPortal, xdp_portal, G_TYPE_OBJECT)

static void
xdp_portal_finalize (GObject *object)
{
  XdpPortal *portal = XDP_PORTAL (object);

  g_clear_object (&portal->screenshot);
  g_clear_object (&portal->notification);
  g_clear_object (&portal->email);
  g_clear_object (&portal->account);

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
}

XdpPortal *
xdp_portal_new (void)
{
  return g_object_new (XDP_TYPE_PORTAL, NULL);
}
