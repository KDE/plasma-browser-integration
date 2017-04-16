#pragma once

#include "abstractbrowserplugin.h"

#include <QJsonObject>

#include <KWindowSystem>

/*
 * This class is responsible for mapping browser window IDs to actual windows
 */
class WindowMapper : public AbstractBrowserPlugin
{
    Q_OBJECT

public:
    static WindowMapper &self();

    void handleData(const QString &event, const QJsonObject &data);

    WId winIdForBrowserId(int browserId) const;
    void resolveWindow(int browserId);

signals:
    void windowAdded(int browserId);
    void windowResolved(int browserId, WId winId);
    void windowRemoved(int browserId);

private:
    WindowMapper();
    ~WindowMapper() override = default;

    void windowChanged(WId id, NET::Properties properties, NET::Properties2 properties2);

    QHash<int, WId> m_windows;

};
