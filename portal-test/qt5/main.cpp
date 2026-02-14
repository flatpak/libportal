
#include <QApplication>

#include "portal-test-qt.h"

#define APP_ID "org.freedesktop.PortalTest.Qt5"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    a.setDesktopFileName(APP_ID);

    PortalTestQt *portalTest = new PortalTestQt(&a, nullptr);
    portalTest->show();

    return a.exec();
}
