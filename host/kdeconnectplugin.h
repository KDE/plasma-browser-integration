#include "abstractbrowserplugin.h"


class KDEConnectPlugin : public AbstractBrowserPlugin
{
    Q_OBJECT
public:
    KDEConnectPlugin(QObject *parent);
    void handleData(const QString &event, const QJsonObject &data);
private:
};
