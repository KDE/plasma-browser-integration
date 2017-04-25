#include "abstractbrowserplugin.h"

class KDEConnectPlugin : public AbstractBrowserPlugin
{
    Q_OBJECT
public:
    KDEConnectPlugin(QObject *parent);
    bool onLoad() override;
    bool onUnload() override;
    void handleData(const QString &event, const QJsonObject &data) override;
private:
    QStringList m_devices;
};

