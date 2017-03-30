#include "connection.h"
#include <QJsonDocument>
#include <QJsonObject>

Connection::Connection() :
    QObject()
{
    m_stdOut.open(stdout, QIODevice::WriteOnly);
    m_stdIn.open(stdin, QIODevice::ReadOnly | QIODevice::Unbuffered);

    connect(&m_stdIn, &QIODevice::readyRead, this, &Connection::readData);
}

void Connection::sendData(const QJsonObject &data)
{
    const QByteArray rawData = QJsonDocument(data).toJson(QJsonDocument::Compact);

    //note, don't use QDataStream as we need to control the binary format used
    quint32 len = rawData.count();
    m_stdOut.write((char*)&len, sizeof(len));
    m_stdOut.write(rawData);
}

void Connection::sendError(const QString &error, const QJsonObject &info)
{
    QJsonObject data = info;
    data.insert(QStringLiteral("error"), error);
    sendData(data);
}


Connection* Connection::self()
{
    static Connection *s = 0;
    if (!s) {
        s = new Connection();
    }
    return s;
}

void Connection::readData()
{
    m_stdIn.startTransaction();
    quint32 length;
    auto rc = m_stdIn.read((char*)(&length), sizeof(quint32));
    if (rc == -1) {
        m_stdIn.rollbackTransaction();
        return;
    }

    if (m_stdIn.bytesAvailable() < length) {
        m_stdIn.rollbackTransaction();
        return;
    }

    QByteArray data = m_stdIn.read(length);
    if (data.length() != length) {
        m_stdIn.rollbackTransaction();
        return;
    }

    m_stdIn.commitTransaction();
    const QJsonObject json = QJsonDocument::fromJson(data).object();
    emit dataReceived(json);
}
