#include "transmissionrepository.h"
// 负责创建/打开SQLite数据库、建表、插入发送日志、查询发送日志、关闭数据库
#include <QDir>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

TransmissionRepository::TransmissionRepository(const QString &databasePath, const QString &connectionName)
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
    if (m_db.isOpen())
    {
        m_db.close();
    }
    m_db = QSqlDatabase();
    QSqlDatabase::removeDatabase(connectionName);
}

bool TransmissionRepository::isOpen() const
{
    return m_db.isOpen();
}

QString TransmissionRepository::lastError() const
{
    return m_lastError;
}

bool TransmissionRepository::insert(const TransmissionLogEntry &entry)
{
    if (!m_db.isOpen())
    {
        m_lastError = "SQLite 数据库未打开";
        return false;
    }
    QSqlQuery query(m_db);
    query.prepare(
        "INSERT INTO transmission_logs(created_at, protocol_name, ip, port, sequence_no, payload) "
        "VALUES(?, ?, ?, ?, ?, ?)");
    query.addBindValue(entry.createdAt);
    query.addBindValue(entry.protocolName);
    query.addBindValue(entry.ip);
    query.addBindValue(entry.port);
    query.addBindValue(entry.sequence);
    query.addBindValue(entry.payload);
    if (!query.exec())
    {
        m_lastError = query.lastError().text();
        return false;
    }
    m_lastError.clear();
    return true;
}

QList<TransmissionLogEntry> TransmissionRepository::search(const QString &timeLike, const QString &protocolLike, const QString &ipLike)
{
    QList<TransmissionLogEntry> entries;
    QSqlQuery query(m_db);
    query.prepare(
        "SELECT created_at, protocol_name, ip, port, sequence_no, payload "
        "FROM transmission_logs "
        "WHERE created_at LIKE ? AND protocol_name LIKE ? AND ip LIKE ? "
        "ORDER BY id DESC");
    query.addBindValue(timeLike.isEmpty() ? "%" : "%" + timeLike + "%");
    query.addBindValue(protocolLike.isEmpty() ? "%" : "%" + protocolLike + "%");
    query.addBindValue(ipLike.isEmpty() ? "%" : "%" + ipLike + "%");
    if (!query.exec())
    {
        m_lastError = query.lastError().text();
        return entries;
    }
    while (query.next())
    {
        TransmissionLogEntry entry;
        entry.createdAt = query.value(0).toString();
        entry.protocolName = query.value(1).toString();
        entry.ip = query.value(2).toString();
        entry.port = query.value(3).toInt();
        entry.sequence = query.value(4).toInt();
        entry.payload = query.value(5).toString();
        entries.append(entry);
    }
    m_lastError.clear();
    return entries;
}

void TransmissionRepository::initialize()
{
    QSqlQuery query(m_db);
    if (!query.exec(
            "CREATE TABLE IF NOT EXISTS transmission_logs ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "created_at TEXT NOT NULL,"
            "protocol_name TEXT NOT NULL,"
            "ip TEXT NOT NULL,"
            "port INTEGER NOT NULL,"
            "sequence_no INTEGER NOT NULL,"
            "payload TEXT NOT NULL)"))
    {
        m_lastError = query.lastError().text();
        return;
    }
    m_lastError.clear();
}
