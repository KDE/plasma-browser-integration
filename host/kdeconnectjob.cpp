/*
    Copyright (C) 2020 by Kai Uwe Broulik <kde@privat.broulik.de>

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

#include "kdeconnectjob.h"

#include <QDBusConnection>
#include <QDBusError>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>

KdeConnectJob::KdeConnectJob(const QString &deviceId, const QString &module, const QString &methodName, QObject *parent)
    : KJob(parent)
    , m_deviceId(deviceId)
    , m_module(module)
    , m_methodName(methodName)
{

}

KdeConnectJob::~KdeConnectJob() = default;

QVariantList KdeConnectJob::arguments() const
{
    return m_arguments;
}

void KdeConnectJob::setArguments(const QVariantList &arguments)
{
    m_arguments = arguments;
}

void KdeConnectJob::start()
{
    QMetaObject::invokeMethod(this, &KdeConnectJob::doStart, Qt::QueuedConnection);
}

void KdeConnectJob::doStart()
{
    // DBus will moan at us if we send a rubbish path
    /*if (m_deviceId.isEmpty() || m_module.isEmpty() || m_methodName.isEmpty()) {
        setError(QDBusError::Other);
        emitResult();
        return;
    }*/

    QDBusMessage msg = QDBusMessage::createMethodCall(
        QStringLiteral("org.kde.kdeconnect"),
        QStringLiteral("/modules/kdeconnect/devices/") + m_deviceId + QStringLiteral("/") + m_module,
        QStringLiteral("org.kde.kdeconnect.device.") + m_module,
        m_methodName
    );
    msg.setArguments(m_arguments);

    // TODO what about methods that return something?
    QDBusPendingReply<> reply = QDBusConnection::sessionBus().asyncCall(msg);
    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(reply, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *watcher) {
        QDBusPendingReply<> reply = *watcher;
        watcher->deleteLater();

        if (reply.isError()) {
            setError(reply.error().type());
            setErrorText(reply.error().message());
            emitResult();
            return;
        }

        emitResult();
    });

    QDBusConnection::sessionBus().asyncCall(msg);
}
