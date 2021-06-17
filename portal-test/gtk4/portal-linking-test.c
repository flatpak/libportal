#include <libportal/portal-gtk4.h>

int
main (int argc, char **argv)
{
  XdpPortal *portal;

  portal = xdp_portal_new ();
  (void) xdp_portal_is_camera_present (portal);

  return 0;
}
