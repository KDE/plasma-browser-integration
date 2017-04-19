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
