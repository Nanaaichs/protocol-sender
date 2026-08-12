#ifndef TRANSMISSIONREPOSITORY_H
#define TRANSMISSIONREPOSITORY_H

#include <QList>
#include <QSqlDatabase>

struct TransmissionLogEntry {
    QString createdAt;
    QString protocolName;
    QString ip;
    int port;
    int sequence;
    QString payload;
};

class TransmissionRepository
{
public:
    TransmissionRepository(const QString &databasePath, const QString &connectionName);
    ~TransmissionRepository();

    bool isOpen() const;
    QString lastError() const;
    bool insert(const TransmissionLogEntry &entry);
    QList<TransmissionLogEntry> search(const QString &timeLike, const QString &protocolLike, const QString &ipLike);

private:
    void initialize();

    QSqlDatabase m_db;
    QString m_lastError;
};

#endif
