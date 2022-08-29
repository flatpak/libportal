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

#include "portal-private.h"
#include "session-private.h"
#include "inputcapture-pointerbarrier.h"
#include "inputcapture-private.h"

/**
 * XdpInputCapturePointerBarrier
 *
 * A representation of a pointer barrier on an [class@InputCaptureZone].
 * Barriers can be assigned with
 * [method@InputCaptureSession.set_pointer_barriers], once the Portal
 * interaction is complete the barrier's "is-active" state indicates whether
 * the barrier is active. Barriers can only be used once, subsequent calls to
 * [method@InputCaptureSession.set_pointer_barriers] will invalidate all
 * current barriers.
 */

enum
{
  PROP_0,

  PROP_X1,
  PROP_X2,
  PROP_Y1,
  PROP_Y2,
  PROP_ID,
  PROP_IS_ACTIVE,

  N_PROPERTIES
};

enum
{
  LAST_SIGNAL
};

enum barrier_state
{
  BARRIER_STATE_NEW,
  BARRIER_STATE_ACTIVE,
  BARRIER_STATE_FAILED,
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

struct  _XdpInputCapturePointerBarrier {
    GObject parent_instance;

    unsigned int id;
    int x1, y1;
    int x2, y2;

    enum barrier_state state;
};

G_DEFINE_TYPE (XdpInputCapturePointerBarrier, xdp_input_capture_pointer_barrier, G_TYPE_OBJECT)

static void
xdp_input_capture_pointer_barrier_get_property (GObject      *object,
                                                unsigned int  property_id,
                                                GValue       *value,
                                                GParamSpec   *pspec)
{
  XdpInputCapturePointerBarrier *barrier = XDP_INPUT_CAPTURE_POINTER_BARRIER (object);

  switch (property_id)
    {
    case PROP_X1:
      g_value_set_int (value, barrier->x1);
      break;
    case PROP_Y1:
      g_value_set_int (value, barrier->y1);
      break;
    case PROP_X2:
      g_value_set_int (value, barrier->x2);
      break;
    case PROP_Y2:
      g_value_set_int (value, barrier->y2);
      break;
    case PROP_ID:
      g_value_set_uint (value, barrier->id);
      break;
    case PROP_IS_ACTIVE:
      g_value_set_boolean (value, barrier->state == BARRIER_STATE_ACTIVE);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
xdp_input_capture_pointer_barrier_set_property (GObject      *object,
                                                unsigned int  property_id,
                                                const GValue *value,
                                                GParamSpec   *pspec)
{
  XdpInputCapturePointerBarrier *pointerbarrier = XDP_INPUT_CAPTURE_POINTER_BARRIER (object);

  switch (property_id)
    {
    case PROP_X1:
      pointerbarrier->x1 = g_value_get_int (value);
      break;
    case PROP_Y1:
      pointerbarrier->y1 = g_value_get_int (value);
      break;
    case PROP_X2:
      pointerbarrier->x2 = g_value_get_int (value);
      break;
    case PROP_Y2:
      pointerbarrier->y2 = g_value_get_int (value);
      break;
    case PROP_ID:
      pointerbarrier->id = g_value_get_uint (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
xdp_input_capture_pointer_barrier_class_init (XdpInputCapturePointerBarrierClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = xdp_input_capture_pointer_barrier_get_property;
  object_class->set_property = xdp_input_capture_pointer_barrier_set_property;

  /**
   * XdpInputCapturePointerBarrier:x1:
   *
   * The pointer barrier x offset in logical pixels
   */
  properties[PROP_X1] =
    g_param_spec_int ("x1",
                      "Pointer barrier x offset",
                      "The pointer barrier x offset in logical pixels",
                      INT_MIN, INT_MAX, 0,
                      G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);

  /**
   * XdpInputCapturePointerBarrier:y1:
   *
   * The pointer barrier y offset in logical pixels
   */
  properties[PROP_Y1] =
    g_param_spec_int ("y1",
                      "Pointer barrier y offset",
                      "The pointer barrier y offset in logical pixels",
                      INT_MIN, INT_MAX, 0,
                      G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);
  /**
   * XdpInputCapturePointerBarrier:x2:
   *
   * The pointer barrier x offset in logical pixels
   */
  properties[PROP_X2] =
    g_param_spec_int ("x2",
                      "Pointer barrier x offset",
                      "The pointer barrier x offset in logical pixels",
                      INT_MIN, INT_MAX, 0,
                      G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);
  /**
   * XdpInputCapturePointerBarrier:y2:
   *
   * The pointer barrier y offset in logical pixels
   */
  properties[PROP_Y2] =
    g_param_spec_int ("y2",
                      "Pointer barrier y offset",
                      "The pointer barrier y offset in logical pixels",
                      INT_MIN, INT_MAX, 0,
                      G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);
  /**
   * XdpInputCapturePointerBarrier:id:
   *
   * The caller-assigned unique id of this barrier
   */
  properties[PROP_ID] =
    g_param_spec_uint ("id",
                       "Pointer barrier unique id",
                       "The id assigned to this barrier by the caller",
                       0, UINT_MAX, 0,
                       G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);
  /**
   * XdpInputCapturePointerBarrier:is-active:
   *
   * A boolean indicating whether this barrier is active. A barrier cannot
   * become active once it failed to apply, barriers that are not active can
   * be thus cleaned up by the caller.
   */
  properties[PROP_IS_ACTIVE] =
    g_param_spec_boolean ("is-active",
                          "true if active, false otherwise",
                          "true if active, false otherwise",
                          FALSE,
                          G_PARAM_READABLE);

  g_object_class_install_properties (object_class, N_PROPERTIES, properties);
}

static void
xdp_input_capture_pointer_barrier_init (XdpInputCapturePointerBarrier *barrier)
{
    barrier->state = BARRIER_STATE_NEW;
}

unsigned int
_xdp_input_capture_pointer_barrier_get_id (XdpInputCapturePointerBarrier *barrier)
{
  return barrier->id;
}

void
_xdp_input_capture_pointer_barrier_set_is_active (XdpInputCapturePointerBarrier *barrier, gboolean active)
{
  g_return_if_fail (barrier->state == BARRIER_STATE_NEW);

  if (active)
    barrier->state = BARRIER_STATE_ACTIVE;
  else
    barrier->state = BARRIER_STATE_FAILED;

  g_object_notify_by_pspec (G_OBJECT (barrier), properties[PROP_IS_ACTIVE]);
}
