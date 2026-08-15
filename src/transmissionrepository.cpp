#include "transmissionrepository.h"

#include <QDir>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

TransmissionRepository::TransmissionRepository(const QString &databasePath,
                                                 const QString &connectionName)
{
    m_db = QSqlDatabase::addDatabase("QSQLITE", connectionName);
    QDir().mkpath(QFileInfo(databasePath).absolutePath());
    m_db.setDatabaseName(databasePath);
    if (!m_db.open())
    {
        m_lastError = m_db.lastError().text();
        return;
    }
    initialize();
}

TransmissionRepository::~TransmissionRepository()
{
    const QString connectionName = m_db.connectionName();
    if (m_db.isOpen()) m_db.close();
    m_db = QSqlDatabase();
    QSqlDatabase::removeDatabase(connectionName);
}

bool TransmissionRepository::isOpen() const
{
    return m_db.isOpen() && m_initialized;
}

QString TransmissionRepository::lastError() const
{
    return m_lastError;
}

bool TransmissionRepository::insertRun(const TransmissionRunEntry &entry)
{
    if (!m_db.isOpen())
    {
        m_lastError = QString::fromUtf8("SQLite 数据库未打开");
        return false;
    }

    QSqlQuery query(m_db);
    query.prepare(
        "INSERT INTO transmission_runs("
        "protocol_name, requested_count, total_count, frequency_hz, start_time, end_time, "
        "target_ip, target_port, status, error_message) "
        "VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
    query.addBindValue(entry.protocolName);
    query.addBindValue(entry.requestedCount);
    query.addBindValue(entry.totalCount);
    query.addBindValue(entry.frequencyHz);
    query.addBindValue(entry.startTime);
    query.addBindValue(entry.endTime);
    query.addBindValue(entry.targetIp);
    query.addBindValue(entry.targetPort);
    query.addBindValue(entry.status);
    query.addBindValue(entry.errorMessage.isEmpty() ? QString("") : entry.errorMessage);
    if (!query.exec())
    {
        m_lastError = query.lastError().text();
        return false;
    }
    m_lastError.clear();
    return true;
}

QList<TransmissionRunEntry> TransmissionRepository::searchRuns(const QString &fromTime,
                                                                const QString &toTime,
                                                                const QString &protocolLike,
                                                                const QString &ipLike)
{
    QList<TransmissionRunEntry> entries;
    if (!m_db.isOpen())
    {
        m_lastError = QString::fromUtf8("SQLite 数据库未打开");
        return entries;
    }

    // 时间条件采用区间相交语义：任务结束不早于起点，且任务开始不晚于终点。
    QSqlQuery query(m_db);
    query.prepare(
        "SELECT protocol_name, requested_count, total_count, frequency_hz, start_time, end_time, "
        "target_ip, target_port, status, error_message "
        "FROM transmission_runs "
        "WHERE (? = '' OR end_time >= ?) "
        "AND (? = '' OR start_time <= ?) "
        "AND protocol_name LIKE ? "
        "AND target_ip LIKE ? "
        "ORDER BY start_time DESC, id DESC");
    const QString normalizedFrom = fromTime.isEmpty() ? QString("") : fromTime;
    const QString normalizedTo = toTime.isEmpty() ? QString("") : toTime;
    query.addBindValue(normalizedFrom);
    query.addBindValue(normalizedFrom);
    query.addBindValue(normalizedTo);
    query.addBindValue(normalizedTo);
    query.addBindValue(protocolLike.isEmpty() ? "%" : "%" + protocolLike + "%");
    query.addBindValue(ipLike.isEmpty() ? "%" : "%" + ipLike + "%");
    if (!query.exec())
    {
        m_lastError = query.lastError().text();
        return entries;
    }

    while (query.next())
    {
        TransmissionRunEntry entry;
        entry.protocolName = query.value(0).toString();
        entry.requestedCount = query.value(1).toInt();
        entry.totalCount = query.value(2).toInt();
        entry.frequencyHz = query.value(3).toInt();
        entry.startTime = query.value(4).toString();
        entry.endTime = query.value(5).toString();
        entry.targetIp = query.value(6).toString();
        entry.targetPort = query.value(7).toInt();
        entry.status = query.value(8).toString();
        entry.errorMessage = query.value(9).toString();
        entries.append(entry);
    }
    m_lastError.clear();
    return entries;
}

void TransmissionRepository::initialize()
{
    m_initialized = false;
    QSqlQuery query(m_db);
    if (!query.exec(
            "CREATE TABLE IF NOT EXISTS transmission_runs ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "protocol_name TEXT NOT NULL,"
            "requested_count INTEGER NOT NULL,"
            "total_count INTEGER NOT NULL,"
            "frequency_hz INTEGER NOT NULL,"
            "start_time TEXT NOT NULL,"
            "end_time TEXT NOT NULL,"
            "target_ip TEXT NOT NULL,"
            "target_port INTEGER NOT NULL,"
            "status TEXT NOT NULL,"
            "error_message TEXT NOT NULL DEFAULT '')"))
    {
        m_lastError = query.lastError().text();
        return;
    }

    // 旧版本的 transmission_logs 表如果存在会被原样保留。
    if (!query.exec(
            "CREATE INDEX IF NOT EXISTS idx_transmission_runs_search "
            "ON transmission_runs(start_time, end_time, protocol_name, target_ip)"))
    {
        m_lastError = query.lastError().text();
        return;
    }
    m_initialized = true;
    m_lastError.clear();
}
