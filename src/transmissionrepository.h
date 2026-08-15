#ifndef TRANSMISSIONREPOSITORY_H
#define TRANSMISSIONREPOSITORY_H

#include <QList>
#include <QSqlDatabase>
#include <QString>

// 一条记录对应一次完整的发送任务，而不是一个 UDP 数据包。
struct TransmissionRunEntry
{
    QString protocolName;
    int requestedCount = 0; // 0 表示配置为持续发送
    int totalCount = 0;     // 任务最终实际成功发送的报文数
    int frequencyHz = 0;
    QString startTime;
    QString endTime;
    QString targetIp;
    int targetPort = 0;
    QString status;         // completed / stopped / failed
    QString errorMessage;
};

class TransmissionRepository
{
public:
    TransmissionRepository(const QString &databasePath, const QString &connectionName);
    ~TransmissionRepository();

    bool isOpen() const;
    QString lastError() const;

    bool insertRun(const TransmissionRunEntry &entry);
    QList<TransmissionRunEntry> searchRuns(const QString &fromTime,
                                           const QString &toTime,
                                           const QString &protocolLike,
                                           const QString &ipLike);

private:
    void initialize();

    QSqlDatabase m_db;
    QString m_lastError;
    bool m_initialized = false;
};

#endif
