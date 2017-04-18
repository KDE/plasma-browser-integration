#include "slcplugin.h"

#include "windowmapper.h"

#include <KActivities/ResourceInstance>

SlcPlugin::SlcPlugin(QObject *parent)
    : AbstractBrowserPlugin(QStringLiteral("slc"), 1, parent)
{
    connect(&WindowMapper::self(), &WindowMapper::windowAdded, this, &SlcPlugin::windowAdded);
    connect(&WindowMapper::self(), &WindowMapper::windowRemoved, this, [this](const Window *window) {
        delete m_resources.take(window);
    });

    foreach (Window *window, WindowMapper::self().windows()) {
        windowAdded(window);
    }
}

SlcPlugin::~SlcPlugin()
{
    qDeleteAll(m_resources);
}

void SlcPlugin::onUnload()
{
    qDeleteAll(m_resources);
}

void SlcPlugin::handleData(const QString &event, const QJsonObject &data)
{
    Q_UNUSED(event)
    Q_UNUSED(data)
}

void SlcPlugin::windowAdded(const Window *window)
{
    auto *resource = addResource(window);
    if (!resource) {
        connect(window, &Window::windowIdChanged, this, [this, window] {
            addResource(window);
        });
    }

    connect(window, &Window::titleChanged, this, [this, window] {
        updateResource(window);
    });

    connect(window, &Window::activeTabUrlChanged, this, [this, window] {
        updateResource(window);
    });
}

KActivities::ResourceInstance *SlcPlugin::addResource(const Window *window)
{
    if (!window || !window->windowId()) {
        return nullptr;
    }

    qDebug() << "add resource for" << window << window->title();

    // or should we use a ResourceInstance for each Tab and use notifyIn/notifyOut?
    auto *resource = new KActivities::ResourceInstance(window->windowId(), QStringLiteral("google-chrome"));
    m_resources.insert(window, resource);
    updateResource(window);
    return resource;
}

void SlcPlugin::updateResource(const Window *window)
{
    auto *resource = m_resources.value(window);
    if (!resource) {
        qWarning() << "Wanted to update resource for" << window << window->title() << "but we don't know it";
        return;
    }

    // FIXME FIXME FIXME This doesn't actually work :(
    // The quickshare applet only sees the first url (usually chrome://extensions when you restarted the extension)

    //resource->notifyFocusedOut();

    resource->setUri(window->activeTabUrl());
    resource->setTitle(window->title());
    resource->setMimetype(QStringLiteral("text/html")); // TODO

    resource->notifyModified();

    //resource->notifyFocusedIn();

    //qDebug() << "resource instance" << window << window->windowId() << resource->uri() << resource->title() << resource->mimetype();
}
