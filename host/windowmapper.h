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

#pragma once

#include "abstractbrowserplugin.h"

#include <QJsonObject>
#include <QUrl>

#include <KWindowSystem>

/**
 * This class represents a single browser window
 */
class Window : public QObject
{
    Q_OBJECT

public:
    int browserId() const;
    WId windowId() const;

    QString title() const; // that's actually the activeTabTitle

    int activeTabId() const;
    QUrl activeTabUrl() const;

    // FIXME this stuff is whether the current tab is audible
    // but ideally it would be if there's any tab playing stuff
    bool audible() const;
    bool muted() const;

    void unminimize();

    // TODO stuff like state, geometry, blabla?

    friend class WindowMapper;

    // TODO add operator<<(QDebug) that prints browser id, winid, title?

signals:
    void windowIdChanged(WId windowId);

    void titleChanged(const QString &title);
    void activeTabIdChanged(int activeTabId);
    void activeTabUrlChanged(const QUrl &activeTabUrl);
    void audibleChanged(bool audible);
    void mutedChanged(bool muted);

private:
    Window(int browserId, QObject *parent);
    ~Window() override = default;

    void setWindowId(WId windowId);

    void update(const QJsonObject &payload);

    int m_browserId;
    WId m_windowId = 0;
    QString m_title;
    int m_activeTabId = -1;
    QUrl m_activeTabUrl;

    bool m_audible = false;
    bool m_muted = false;

};

/*
 * This class is responsible for mapping browser window IDs to actual windows
 */
class WindowMapper : public AbstractBrowserPlugin
{
    Q_OBJECT

public:
    static WindowMapper &self();

    void handleData(const QString &event, const QJsonObject &data);

    Window *getWindow(int browserId) const;
    QList<Window *> windows() const;

    void resolveWindow(int browserId);

signals:
    void windowAdded(Window *window);
    void windowRemoved(Window *window);

private:
    WindowMapper();
    ~WindowMapper() override = default;

    void windowChanged(WId id, NET::Properties properties, NET::Properties2 properties2);

    QHash<int, Window *> m_windows;

};
