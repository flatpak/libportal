
#include <QApplication>

#include "portal-test-qt.h"

#define APP_ID "org.gnome.PortalTest.Qt6"


int main(int argc, char *argv[])
{
    XdpPortal *portal = xdp_portal_new();
    xdp_portal_register(portal, APP_ID, nullptr /*cancellable*/,
                        [](GObject *source_object, GAsyncResult *result, gpointer user_data)
                        {
                          Q_UNUSED(user_data)

                          XdpPortal *portal = XDP_PORTAL (source_object);
                          g_autoptr(GError) error = NULL;
                          if (!xdp_portal_register_finish (portal, result, &error))
                          {
                              if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
                                  qWarning ("Failed to register application ID: %s", error->message);
                              return;
                          }

                          qInfo ("Registered application ID");
                        },
                        nullptr);


    QApplication a(argc, argv);
    PortalTestQt *portalTest = new PortalTestQt(portal, nullptr);
    portalTest->show();

    return a.exec();
}
