#pragma once

#include "abstractbrowserplugin.h"

#include <QDBusContext>
#include <QDBusMessage>

class TabsRunnerPlugin : public AbstractBrowserPlugin, protected QDBusContext
{
    Q_OBJECT

public:
    TabsRunnerPlugin(QObject *parent);
    void onLoad() override;
    void onUnload() override;
    void handleData(const QString &event, const QJsonObject &data) override;

    // dbus-exported
    QList<QHash<QString, QVariant>> GetTabs();
    void Activate(int tabId);
    void SetMuted(int tabId, bool muted);

private:
    QDBusMessage m_tabsReplyMessage;

};
