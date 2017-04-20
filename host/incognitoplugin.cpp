#include "incognitoplugin.h"

#include <QDebug>
#include <QMenu>

IncognitoPlugin::IncognitoPlugin(QObject* parent) :
    AbstractBrowserPlugin(QStringLiteral("incognito"), 1, parent)
{
}

bool IncognitoPlugin::onUnload()
{
    if (m_ksni) {
        m_ksni->deleteLater();
    }
    return true;
}

void IncognitoPlugin::handleData(const QString& event, const QJsonObject& data)
{
    if (event == QLatin1String("show")) {
        if (m_ksni) {
            debug() << "incognito already there";
            return;
        }
        debug() << "adding incongito icon";

        m_ksni = new KStatusNotifierItem(this);

        m_ksni->setIconByName("face-smirk");
        m_ksni->setTitle("Incognito Tabs");
        m_ksni->setStandardActionsEnabled(false);
        m_ksni->setStatus(KStatusNotifierItem::Active);

        QMenu *menu = new QMenu();
        QAction *closeAllAction = menu->addAction(QIcon::fromTheme("window-close"), "Close all Incognito Tabs");
        QObject::connect(closeAllAction, &QAction::triggered, this, [this] {
            sendData(QStringLiteral("close"));
        });

        m_ksni->setContextMenu(menu);
    } else if (event == QLatin1String("hide")) {
        if (!m_ksni) {
            debug() << "no incongito there but wanted to hide";
            return;
        }
        debug() << "removing incognito icon";

        delete m_ksni.data();
    }
}
