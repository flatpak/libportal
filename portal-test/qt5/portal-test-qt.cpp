
#include "portal-test-qt.h"
#include "ui_portal-test-qt.h"

#include <QStringLiteral>

PortalTestQt::PortalTestQt(QWidget *parent, Qt::WindowFlags f)
    : QMainWindow(parent, f)
    , m_mainWindow(new Ui_PortalTestQt)
    , m_portal(xdp_portal_new())
{
    m_mainWindow->setupUi(this);

    connect(m_mainWindow->openFileButton, &QPushButton::clicked, [=] (bool clicked) {
        XdpParent *parent;
        XdpOpenFileFlags flags = XDP_OPEN_FILE_FLAG_NONE;

        parent = xdp_parent_new_qt(windowHandle());
        xdp_portal_open_file (m_portal, parent, "Portal Test Qt", nullptr /*filters*/, nullptr /*current_filters*/,
                              nullptr /*choices*/, flags, nullptr /*cancellable*/, openedFile, this);
        xdp_parent_free (parent);
    });
}

PortalTestQt::~PortalTestQt()
{
    delete m_mainWindow;
    g_object_unref( m_portal);
}

void PortalTestQt::updateLastOpenedFile(const QString &file)
{
    if (!file.isEmpty()) {
        m_mainWindow->openedFileLabel->setText(QStringLiteral("Opened file: %1").arg(file));
    } else {
        m_mainWindow->openedFileLabel->setText(QStringLiteral("Failed to open a file!!!"));
    }
}

void PortalTestQt::openedFile(GObject *object, GAsyncResult *result, gpointer data)
{
    Q_UNUSED(data);
    XdpPortal *portal = XDP_PORTAL (object);
    PortalTestQt *win = static_cast<PortalTestQt*>(data);
    g_autoptr(GError) error = nullptr;
    g_autoptr(GVariant) ret = nullptr;

    ret = xdp_portal_open_file_finish(portal, result, &error);

    if (ret) {
        const char **uris;
        if (g_variant_lookup(ret, "uris", "^a&s", &uris)) {
            win->updateLastOpenedFile(uris[0]);
        }
    } else {
            win->updateLastOpenedFile(QString());
    }
}
