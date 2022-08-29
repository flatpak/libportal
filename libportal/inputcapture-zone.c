/*
 * Copyright (C) 2022, Red Hat, Inc.
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

#include "inputcapture-zone.h"

/**
 * XdpInputCaptureZone
 *
 * A representation of a zone that supports input capture.
 *
 * The [class@XdpInputCaptureZone] object is used to represent a zone on the
 * user-visible desktop that may be used to set up
 * [class@XdpInputCapturePointerBarrier] objects. In most cases, the set of
 * [class@XdpInputCaptureZone] objects represent the available monitors but the
 * exact implementation is up to the implementation.
 */

enum
{
  PROP_0,

  PROP_WIDTH,
  PROP_HEIGHT,
  PROP_X,
  PROP_Y,
  PROP_ZONE_SET,
  PROP_IS_VALID,

  N_PROPERTIES
};

static GParamSpec *zone_properties[N_PROPERTIES] = { NULL, };

struct  _XdpInputCaptureZone {
    GObject parent_instance;

    unsigned int width;
    unsigned int height;
    int x;
    int y;

    unsigned int zone_set;

    gboolean is_valid;
};

G_DEFINE_TYPE (XdpInputCaptureZone, xdp_input_capture_zone, G_TYPE_OBJECT)

static void
xdp_input_capture_zone_get_property (GObject      *object,
                                     unsigned int  property_id,
                                     GValue       *value,
                                     GParamSpec   *pspec)
{

  XdpInputCaptureZone *zone = XDP_INPUT_CAPTURE_ZONE (object);

  switch (property_id)
    {
    case PROP_WIDTH:
      g_value_set_uint (value, zone->width);
      break;
    case PROP_HEIGHT:
      g_value_set_uint (value, zone->height);
      break;
    case PROP_X:
      g_value_set_int (value, zone->x);
      break;
    case PROP_Y:
      g_value_set_int (value, zone->y);
      break;
    case PROP_ZONE_SET:
      g_value_set_uint (value, zone->zone_set);
      break;
    case PROP_IS_VALID:
      g_value_set_boolean (value, zone->is_valid);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
xdp_input_capture_zone_set_property (GObject      *object,
                                     unsigned int  property_id,
                                     const GValue *value,
                                     GParamSpec     *pspec)
{
  XdpInputCaptureZone *zone = XDP_INPUT_CAPTURE_ZONE (object);

  switch (property_id)
    {
    case PROP_WIDTH:
      zone->width = g_value_get_uint (value);
      break;
    case PROP_HEIGHT:
      zone->height = g_value_get_uint (value);
      break;
    case PROP_X:
      zone->x =  g_value_get_int (value);
      break;
    case PROP_Y:
      zone->y = g_value_get_int (value);
      break;
    case PROP_ZONE_SET:
      zone->zone_set = g_value_get_uint (value);
      break;
    case PROP_IS_VALID:
      zone->is_valid = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
xdp_input_capture_zone_class_init (XdpInputCaptureZoneClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = xdp_input_capture_zone_get_property;
  object_class->set_property = xdp_input_capture_zone_set_property;

  /**
   * XdpInputCaptureZone:width:
   *
   * The width of this zone in logical pixels
   */
  zone_properties[PROP_WIDTH] =
        g_param_spec_uint ("width",
                           "zone width",
                           "The zone width in logical pixels",
                           0, UINT_MAX, 0,
                           G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);

  /**
   * XdpInputCaptureZone:height:
   *
   * The height of this zone in logical pixels
   */
  zone_properties[PROP_HEIGHT] =
        g_param_spec_uint ("height",
                           "zone height",
                           "The zone height in logical pixels",
                           0, UINT_MAX, 0,
                           G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);

  /**
   * XdpInputCaptureZone:x:
   *
   * The x offset of this zone in logical pixels
   */
  zone_properties[PROP_X] =
        g_param_spec_int ("x",
                          "zone x offset",
                          "The zone x offset in logical pixels",
                          INT_MIN, INT_MAX, 0,
                          G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);
  /**
   * XdpInputCaptureZone:y:
   *
   * The x offset of this zone in logical pixels
   */
  zone_properties[PROP_Y] =
        g_param_spec_int ("y",
                          "zone y offset",
                          "The zone y offset in logical pixels",
                          INT_MIN, INT_MAX, 0,
                          G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);

  /**
   * XdpInputCaptureZone:zone_set:
   *
   * The unique zone_set number assigned to this set of zones. A set of zones as
   * returned by [method@InputCaptureSession.get_zones] have the same zone_set
   * number and only one set of zones may be valid at any time (the most
   * recently returned set).
   */
  zone_properties[PROP_ZONE_SET] =
        g_param_spec_uint ("zone_set",
                           "zone set number",
                           "The zone_set number when this zone was retrieved",
                           0, UINT_MAX, 0,
                           G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);

  /**
   * XdpInputCaptureZone:is-valid:
   *
   * A boolean indicating whether this zone is currently valid. Zones are
   * invalidated by the Portal's ZonesChanged signal, see
   * [signal@InputCaptureSession::zones-changed].
   *
   * Once invalidated, a Zone can be discarded by the caller, it cannot become
   * valid again.
   */
  zone_properties[PROP_IS_VALID] =
        g_param_spec_boolean ("is-valid",
                              "validity check",
                              "True if this zone is currently valid",
                              TRUE,
                              G_PARAM_READWRITE);

  g_object_class_install_properties (object_class,
                                     N_PROPERTIES,
                                     zone_properties);
}

static void
xdp_input_capture_zone_init (XdpInputCaptureZone *zone)
{
}

void
_xdp_input_capture_zone_invalidate_and_free (XdpInputCaptureZone *zone)
{
  g_object_set (zone, "is-valid", FALSE, NULL);
  g_object_unref (zone);
}
