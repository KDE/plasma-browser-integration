#pragma once

#include "abstractbrowserplugin.h"

#include <KActivities/ResourceInstance>
#include <QWindow> // for WId

class Window;

class SlcPlugin : public AbstractBrowserPlugin
{
    Q_OBJECT

public:
    SlcPlugin(QObject *parent);
    ~SlcPlugin() override;

    void handleData(const QString &event, const QJsonObject &data) override;

private:
    void windowAdded(const Window *window);
    KActivities::ResourceInstance *addResource(const Window *window);

    void updateResource(const Window *window);

    QHash<const Window *, KActivities::ResourceInstance *> m_resources;

};
