#include "incognitoplugin.h"

#include <QDebug>
#include <QMenu>

IncognitoPlugin::IncognitoPlugin(QObject* parent) :
    AbstractBrowserPlugin(QStringLiteral("incognito"), parent)
{
}

void IncognitoPlugin::handleData(const QString& event, const QJsonObject& data)
{
    if (event == QLatin1String("show")) {
        if (m_ksni) {
            qDebug() << "incognito already there";
            return;
        }
        qDebug() << "adding incongito icon";

        m_ksni = new KStatusNotifierItem(this);

        m_ksni->setIconByName("face-smirk");
        m_ksni->setTitle("Incognito Tabs");
        m_ksni->setStandardActionsEnabled(false);
        m_ksni->setStatus(KStatusNotifierItem::Active);

        QMenu *menu = new QMenu();
        QAction *closeAllAction = menu->addAction(QIcon::fromTheme("window-close"), "Close all Incognito Tabs");
        QObject::connect(closeAllAction, &QAction::triggered, this, [this] {
            sendData({ {"subsystem", "incognito"}, {"action", "close"} });
        });

        m_ksni->setContextMenu(menu);
        sendData({ {"incognito indicator", true} });
    } else if (event == QLatin1String("hide")) {
        if (!m_ksni) {
            qDebug() << "no incongito there but wanted to hide";
            return;
        }
        qDebug() << "removing incognito icon";

        delete m_ksni.data();
        sendData({{"incognito indicator", false}});
    }
}
