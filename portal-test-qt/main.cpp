
#include <QApplication>

#include "portal-test-qt.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    PortalTestQt *portalTest = new PortalTestQt(nullptr);
    portalTest->show();

    return a.exec();
}
