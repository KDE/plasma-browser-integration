#include <QObject>
#include <QFile>
#include <QJsonObject>

/*
 * This class is responsible for managing all stdout/stdin connections emitting JSON
 */
class Connection : public QObject
{
    Q_OBJECT
public:
    static Connection* self();
    void sendData(const QJsonObject &data);
    void sendError(const QString &error, const QJsonObject &info = QJsonObject());

signals:
    void dataReceived(const QJsonObject &data);

private:
    Connection();
    ~Connection() = default;
    void readData();
    QFile m_stdOut;
    QFile m_stdIn;
};
