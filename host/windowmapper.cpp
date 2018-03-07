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

#include "windowmapper.h"

#include <QJsonDocument>
#include <QJsonObject>

#include <QDebug>
#include <QWindow>

#include <KWindowInfo>
#include <KWindowSystem>

static const QString s_keyword = QLatin1String("@PLASMABROWSERINTEGRATION");

Window::Window(int browserId, QObject *parent)
    : QObject(parent)
    , m_browserId(browserId)
{

}

int Window::browserId() const
{
    return m_browserId;
}

WId Window::windowId() const
{
    return m_windowId;
}

void Window::setWindowId(WId windowId)
{
    if (m_windowId != windowId) {
        m_windowId = windowId;
        emit windowIdChanged(windowId);
    }
}

QString Window::title() const
{
    return m_title;
}

int Window::activeTabId() const
{
    return m_activeTabId;
}

QUrl Window::activeTabUrl() const
{
    return m_activeTabUrl;
}

bool Window::audible() const
{
    return m_audible;
}

bool Window::muted() const
{
    return m_muted;
}

void Window::unminimize()
{
    KWindowSystem::unminimizeWindow(windowId());
}

void Window::update(const QJsonObject &payload)
{
    auto end = payload.constEnd();

    auto it = payload.constFind(QStringLiteral("title"));
    if (it != end) {
        const QString &newTitle = it->toString();

        // abort when our window mapper tab opened
        if (newTitle.contains(s_keyword)) {
            return;
        }

        if (m_title != newTitle) {
            m_title = newTitle;
            emit titleChanged(newTitle);
        }
    }

    it = payload.constFind(QStringLiteral("activeTabId"));
    if (it != end) {
        const int newTabId = it->toInt();
        if (m_activeTabId != newTabId) {
            m_activeTabId = newTabId;
            emit activeTabIdChanged(newTabId);
        }
    }

    it = payload.constFind(QStringLiteral("activeTabUrl"));
    if (it != end) {
        const QUrl newTabUrl(it->toString());
        if (m_activeTabUrl != newTabUrl) {
            m_activeTabUrl = newTabUrl;
            emit activeTabUrlChanged(newTabUrl);
        }
    }
}

WindowMapper::WindowMapper()
    : AbstractBrowserPlugin(QStringLiteral("windows"), 1, nullptr)
{
    connect(KWindowSystem::self(),
            static_cast<void(KWindowSystem::*)(WId, NET::Properties, NET::Properties2)>(&KWindowSystem::windowChanged),
            this, &WindowMapper::windowChanged);

    // kindly ask the extension to tell us about all its windows
    sendData(QStringLiteral("getAll"));
}

WindowMapper &WindowMapper::self()
{
    static WindowMapper s_self;
    return s_self;
}

void WindowMapper::handleData(const QString &event, const QJsonObject &data)
{
    //qDebug() << "window maper handle" << event << data;
    const int browserId = data.value(QStringLiteral("browserId")).toInt();
    if (browserId <= 0) {
        qWarning() << "Got window event for invalid browser id" << browserId;
        return;
    }

    if (event == QLatin1String("removed")) {
        Window *window = m_windows.take(browserId);
        if (window) {
            emit windowRemoved(window);
            delete window;
        }  else {
            qWarning() << "Got told that window" << browserId << "was removed but we don't know it";
        }
    } else if (event == QLatin1String("added")) {
        // This used to be an assert but apparently we can't do that (Bug 380186)
        if (m_windows.contains(browserId)) {
            qWarning() << "Got told that window" << browserId << "was added but we already know it";
            return; // TODO should we still "resolveWindow" in this case?
        }

        Window *window = new Window(browserId, this);
        // TODO have a constructor that takes QJsonObject and populates without emitting?
        window->update(data);
        m_windows.insert(browserId, window);
        emit windowAdded(window);

        resolveWindow(browserId);
    } else if (event == QLatin1String("update")) {
        Window *window = m_windows.value(browserId);
        if (window) {
            window->update(data);
        }  else {
            qWarning() << "Got told that window" << browserId << "was updated but we don't know it";
        }
    } else if (event == QLatin1String("unminimize")) {
        if (Window *window = m_windows.value(browserId)) {
            window->unminimize();
        } else {
            qWarning() << "Got told to unminimize" << browserId << "but we don't know it";
        }
    }

    //qDebug() << "we know the following windows" << m_windows;
}

Window *WindowMapper::getWindow(int browserId) const
{
    return m_windows.value(browserId);
}

// should we just make this thing return the QHash to avoid creating a list just to then later iterate it?
QList<Window *> WindowMapper::windows() const
{
    return m_windows.values();
}

void WindowMapper::resolveWindow(int browserId)
{
    // Disable for now as it causes an annoying tab to appear
    return;
    qDebug() << "will resolve window" << browserId;
    sendData(QStringLiteral("resolve"), {
        {QStringLiteral("browserId"), browserId}
    });
}

// code below is horrible
void WindowMapper::windowChanged(WId id, NET::Properties properties, NET::Properties2 properties2)
{
    Q_UNUSED(properties2);

    // TODO what about VisibleName?
    if (!(properties & NET::WMName)) {
        return;
    }

    // don't bother if we have already resolved this window
    auto it = std::find_if(m_windows.constBegin(), m_windows.constEnd(), [id](Window *window) {
        return window->windowId() == id;
    });

    if (it != m_windows.constEnd()) {
        return;
    }

    // TODO should we check for whether it's actually a chrome window?
    KWindowInfo info(id, NET::WMName);

    const QString &name = info.name();

    const int idx = name.indexOf(s_keyword);
    if (idx == -1) {
        return;
    }

    const int beginIdx = idx + s_keyword.length();
    const int endIdx = name.indexOf(QLatin1String("@"), beginIdx);
    if (endIdx == -1 || (endIdx - beginIdx <= 0)) {
        qWarning() << "Got window title of" << id << "with" << name << "and keyword but no ending at sign";
        return;
    }

    const QStringRef &browserIdString = name.midRef(beginIdx, endIdx - beginIdx);
    if (browserIdString.isEmpty()) {
        return;
    }

    bool ok;
    const int browserId = browserIdString.toInt(&ok);

    if (!ok) {
        qWarning() << "Failed to convert" << browserIdString.toString() << "to int";
        return;
    }

    if (browserId <= 0) {
        qWarning() << "Got implausible window id" << browserId;
        return;
    }

    //qDebug() << "Mapped" << id << "to" << browserId;

    Window *window = m_windows.value(browserId);
    if (!window) {
        qWarning() << "Got mapping result for" << browserId << "but we don't know this window";
        return;
    }

    window->setWindowId(id);

    // now tell the extension that it may close the tab again
    sendData(QStringLiteral("resolved"), {
        {QStringLiteral("browserId"), browserId}
    });
}
