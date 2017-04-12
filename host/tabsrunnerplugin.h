#pragma once

#include "abstractbrowserplugin.h"

#include <QDBusContext>
#include <QDBusMessage>

class TabsRunnerPlugin : public AbstractBrowserPlugin, protected QDBusContext
{
    Q_OBJECT

public:
    TabsRunnerPlugin(QObject *parent);
    void handleData(const QString &event, const QJsonObject &data) override;

    // dbus-exported
    QList<QHash<QString, QVariant>> GetTabs();
    void Activate(int tabId);

private:
    QDBusMessage m_tabsReplyMessage;

};
