#include "abstractbrowserplugin.h"

#include <KStatusNotifierItem>
#include <QPointer>

class IncognitoPlugin : public AbstractBrowserPlugin
{
    Q_OBJECT
public:
    IncognitoPlugin(QObject *parent);
    void handleData(const QString &event, const QJsonObject &data);
private:
    QPointer<KStatusNotifierItem> m_ksni;
};
