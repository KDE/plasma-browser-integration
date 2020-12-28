#pragma once

#include <QList>
#include <QString>
#include <QDBusArgument>
#include <QVariantMap>
#include <KRunner/QueryMatch>

struct RemoteMatch
{
    //sssuda{sv}
    QString id;
    QString text;
    QString iconName;
    Plasma::QueryMatch::Type type = Plasma::QueryMatch::NoMatch;
    qreal relevance = 0;
    QVariantMap properties;
};

typedef QList<RemoteMatch> RemoteMatches;

struct RemoteAction
{
    QString id;
    QString text;
    QString iconName;
};

typedef QList<RemoteAction> RemoteActions;

struct RemoteImage
{
    //iiibiiay (matching notification spec image-data attribute)
    int width;
    int height;
    int rowStride;
    bool hasAlpha;
    int bitsPerSample;
    int channels;
    QByteArray data;
};

inline QDBusArgument &operator<< (QDBusArgument &argument, const RemoteMatch &match) {
    argument.beginStructure();
    argument << match.id;
    argument << match.text;
    argument << match.iconName;
    argument << match.type;
    argument << match.relevance;
    argument << match.properties;
    argument.endStructure();
    return argument;
}

inline const QDBusArgument &operator>>(const QDBusArgument &argument, RemoteMatch &match) {
    argument.beginStructure();
    argument >> match.id;
    argument >> match.text;
    argument >> match.iconName;
    uint type;
    argument >> type;
    match.type = static_cast<Plasma::QueryMatch::Type>(type);
    argument >> match.relevance;
    argument >> match.properties;
    argument.endStructure();

    return argument;
}

inline QDBusArgument &operator<< (QDBusArgument &argument, const RemoteAction &action)
{
    argument.beginStructure();
    argument << action.id;
    argument << action.text;
    argument << action.iconName;
    argument.endStructure();
    return argument;
}

inline const QDBusArgument &operator>>(const QDBusArgument &argument, RemoteAction &action) {
    argument.beginStructure();
    argument >> action.id;
    argument >> action.text;
    argument >> action.iconName;
    argument.endStructure();
    return argument;
}

inline QDBusArgument &operator<< (QDBusArgument &argument, const RemoteImage &image) {
    argument.beginStructure();
    argument << image.width;
    argument << image.height;
    argument << image.rowStride;
    argument << image.hasAlpha;
    argument << image.bitsPerSample;
    argument << image.channels;
    argument << image.data;
    argument.endStructure();
    return argument;
}

inline const QDBusArgument &operator>>(const QDBusArgument &argument, RemoteImage &image) {
    argument.beginStructure();
    argument >> image.width;
    argument >> image.height;
    argument >> image.rowStride;
    argument >> image.hasAlpha;
    argument >> image.bitsPerSample;
    argument >> image.channels;
    argument >> image.data;
    argument.endStructure();
    return argument;
}

Q_DECLARE_METATYPE(RemoteMatch)
Q_DECLARE_METATYPE(RemoteMatches)
Q_DECLARE_METATYPE(RemoteAction)
Q_DECLARE_METATYPE(RemoteActions)
Q_DECLARE_METATYPE(RemoteImage)
