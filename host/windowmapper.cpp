#include "windowmapper.h"

#include <QJsonDocument>
#include <QJsonObject>

#include <QDebug>
#include <QWindow>

#include <KWindowInfo>
#include <KWindowSystem>

static const QString s_keyword = QLatin1String("@PLASMABROWSERINTEGRATION");

WindowMapper::WindowMapper()
    : AbstractBrowserPlugin(QStringLiteral("windows"), 1, nullptr)
{
    connect(KWindowSystem::self(),
            static_cast<void(KWindowSystem::*)(WId, NET::Properties, NET::Properties2)>(&KWindowSystem::windowChanged),
            this, &WindowMapper::windowChanged);

    // kindly ask the extension to tell us about all its windows
    sendData(QStringLiteral("getAll"));
}

WindowMapper &WindowMapper::self()
{
    static WindowMapper s_self;
    return s_self;
}

void WindowMapper::handleData(const QString &event, const QJsonObject &data)
{
    qDebug() << "window maper handle" << event << data;
    const int browserId = data.value(QStringLiteral("browserId")).toInt();
    if (browserId <= 0) {
        qWarning() << "Got window event for invalid browser id" << browserId;
        return;
    }

    if (event == QLatin1String("removed")) {
        m_windows.remove(browserId);
        emit windowRemoved(browserId);
    } else if (event == QLatin1String("added")) {
        emit windowAdded(browserId);
        resolveWindow(browserId);
    }

    //qDebug() << "we know the following windows" << m_windows;
}

WId WindowMapper::winIdForBrowserId(int browserId) const
{
    return m_windows.value(browserId);
}

void WindowMapper::resolveWindow(int browserId)
{
    qDebug() << "will resolve window" << browserId;
    sendData(QStringLiteral("resolve"), {
        {QStringLiteral("browserId"), browserId}
    });
}

// code below is horrible
void WindowMapper::windowChanged(WId id, NET::Properties properties, NET::Properties2 properties2)
{
    Q_UNUSED(properties2);

    // TODO what about VisibleName?
    if (!(properties & NET::WMName)) {
        return;
    }

    if (m_windows.key(id)) { // "hasKey"
        return;
    }

    // TODO should we check for whether it's actually a chrome window?
    KWindowInfo info(id, NET::WMName);

    const QString &name = info.name();

    const int idx = name.indexOf(s_keyword);
    if (idx == -1) {
        return;
    }

    const int beginIdx = idx + s_keyword.length();
    const int endIdx = name.indexOf(QLatin1String("@"), beginIdx);
    if (endIdx == -1 || (endIdx - beginIdx <= 0)) {
        qWarning() << "Got window title of" << id << "with" << name << "and keyword but no ending at sign";
        return;
    }

    const QStringRef &browserIdString = name.midRef(beginIdx, endIdx - beginIdx);
    if (browserIdString.isEmpty()) {
        return;
    }

    bool ok;
    const int browserId = browserIdString.toInt(&ok);

    if (!ok) {
        qWarning() << "Failed to convert" << browserIdString.toString() << "to int";
        return;
    }

    if (browserId <= 0) {
        qWarning() << "Got implausible window id" << browserId;
        return;
    }

    //qDebug() << "Mapped" << id << "to" << browserId;

    m_windows.insert(browserId, id);
    emit windowResolved(browserId, id);

    // now tell the extension that it may close the tab again
    sendData(QStringLiteral("resolved"), {
        {QStringLiteral("browserId"), browserId}
    });
}
