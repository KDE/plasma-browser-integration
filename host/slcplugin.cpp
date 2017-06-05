/*
    Copyright (C) 2017 by Kai Uwe Broulik <kde@privat.broulik.de>
    Copyright (C) 2017 by David Edmundson <davidedmundson@kde.org>

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
    THE SOFTWARE.
*/

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
    m_resources.clear();
}

bool SlcPlugin::onUnload()
{
    qDeleteAll(m_resources);
    m_resources.clear();
    return true;
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
