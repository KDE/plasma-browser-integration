#include "abstractbrowserplugin.h"

#include <QHash>
#include "downloadjob.h"

class DownloadPlugin : public AbstractBrowserPlugin
{
    Q_OBJECT
public:
    DownloadPlugin(QObject *parent);
    bool onUnload() override;
    void handleData(const QString &event, const QJsonObject &data);
private:
    QHash<int, DownloadJob *> m_jobs;
};
