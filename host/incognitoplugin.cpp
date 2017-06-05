/*
    Copyright (C) 2017 by Kai Uwe Broulik <kde@privat.broulik.de>
    Copyright (C) 2017 by David Edmundson <davidedmundson@kde.org>

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
    THE SOFTWARE.
*/

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
