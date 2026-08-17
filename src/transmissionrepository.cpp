#include "transmissionrepository.h"

#include <QDir>      // 目录操作
#include <QFileInfo> // 文件信息
#include <QSqlError> // SQL错误信息
#include <QSqlQuery> // SQL查询
#include <QVariant>  // Qt变体类型

/**
 * @brief TransmissionRepository构造函数
 * @param databasePath SQLite数据库文件路径
 * @param connectionName 数据库连接名称（用于多连接管理）
 *
 * 初始化流程：
 * 1. 创建SQLite数据库连接
 * 2. 确保数据库文件所在目录存在
 * 3. 打开数据库
 * 4. 初始化表结构和索引
 */
TransmissionRepository::TransmissionRepository(const QString &databasePath,
                                               const QString &connectionName)
{
    // 添加SQLite数据库连接，使用指定的连接名称
    m_db = QSqlDatabase::addDatabase("QSQLITE", connectionName);

    // 创建数据库文件所在目录（如果不存在）
    QDir().mkpath(QFileInfo(databasePath).absolutePath());

    // 设置数据库文件路径
    m_db.setDatabaseName(databasePath);

    // 打开数据库
    if (!m_db.open())
    {
        m_lastError = m_db.lastError().text(); // 记录打开失败的错误
        return;
    }

    // 初始化表结构
    initialize();
}

/**
 * @brief TransmissionRepository析构函数
 *
 * 清理数据库连接：
 * 1. 关闭数据库
 * 2. 重置QSqlDatabase对象
 * 3. 移除数据库连接
 */
TransmissionRepository::~TransmissionRepository()
{
    const QString connectionName = m_db.connectionName(); // 保存连接名称
    if (m_db.isOpen())
        m_db.close();                             // 关闭数据库
    m_db = QSqlDatabase();                        // 重置对象
    QSqlDatabase::removeDatabase(connectionName); // 移除连接
}

/**
 * @brief 检查数据库是否可用
 * @return 数据库是否已打开且初始化成功
 */
bool TransmissionRepository::isOpen() const
{
    return m_db.isOpen() && m_initialized; // 需要同时满足两个条件
}

/**
 * @brief 获取最后一次错误信息
 * @return 错误信息字符串
 */
QString TransmissionRepository::lastError() const
{
    return m_lastError;
}

/**
 * @brief 插入一条传输运行日志
 * @param entry 传输运行日志条目
 * @return 插入是否成功
 *
 * 使用预处理语句防止SQL注入
 */
bool TransmissionRepository::insertRun(const TransmissionRunEntry &entry)
{
    // 检查数据库是否打开
    if (!m_db.isOpen())
    {
        m_lastError = QString::fromUtf8("SQLite 数据库未打开");
        return false;
    }

    // 创建SQL查询对象
    QSqlQuery query(m_db);

    // 准备INSERT语句，使用?作为占位符
    query.prepare(
        "INSERT INTO transmission_runs("
        "protocol_name, requested_count, total_count, frequency_hz, start_time, end_time, "
        "target_ip, target_port, status, error_message) "
        "VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");

    // 绑定参数（按顺序）
    query.addBindValue(entry.protocolName);   // 协议名称
    query.addBindValue(entry.requestedCount); // 请求数量
    query.addBindValue(entry.totalCount);     // 实际发送数量
    query.addBindValue(entry.frequencyHz);    // 频率
    query.addBindValue(entry.startTime);      // 开始时间
    query.addBindValue(entry.endTime);        // 结束时间
    query.addBindValue(entry.targetIp);       // 目标IP
    query.addBindValue(entry.targetPort);     // 目标端口
    query.addBindValue(entry.status);         // 状态
    // 错误信息：如果为空则存储空字符串
    query.addBindValue(entry.errorMessage.isEmpty() ? QString("") : entry.errorMessage);

    // 执行插入
    if (!query.exec())
    {
        m_lastError = query.lastError().text(); // 记录错误
        return false;
    }

    m_lastError.clear(); // 清除错误信息
    return true;
}

/**
 * @brief 搜索传输运行日志
 * @param fromTime 起始时间（格式：yyyy-MM-dd HH:mm:ss.zzz）
 * @param toTime 结束时间（格式：yyyy-MM-dd HH:mm:ss.zzz）
 * @param protocolLike 协议名称模糊匹配（支持%通配符）
 * @param ipLike IP地址模糊匹配（支持%通配符）
 * @return 匹配的日志条目列表
 *
 * 时间条件采用区间相交语义：
 * - 任务结束时间 >= 查询起始时间
 * - 任务开始时间 <= 查询结束时间
 * 这样可以找到与查询时间段有任何重叠的任务
 */
QList<TransmissionRunEntry> TransmissionRepository::searchRuns(const QString &fromTime,
                                                               const QString &toTime,
                                                               const QString &protocolLike,
                                                               const QString &ipLike)
{
    QList<TransmissionRunEntry> entries;

    // 检查数据库是否打开
    if (!m_db.isOpen())
    {
        m_lastError = QString::fromUtf8("SQLite 数据库未打开");
        return entries;
    }

    // 准备SELECT查询语句
    // 时间条件采用区间相交语义：任务结束不早于起点，且任务开始不晚于终点
    QSqlQuery query(m_db);
    query.prepare(
        "SELECT protocol_name, requested_count, total_count, frequency_hz, start_time, end_time, "
        "target_ip, target_port, status, error_message "
        "FROM transmission_runs "
        "WHERE (? = '' OR end_time >= ?) "    // 结束时间条件
        "AND (? = '' OR start_time <= ?) "    // 开始时间条件
        "AND protocol_name LIKE ? "           // 协议名模糊匹配
        "AND target_ip LIKE ? "               // IP模糊匹配
        "ORDER BY start_time DESC, id DESC"); // 按开始时间降序，同时间按ID降序

    // 规范化时间参数（空字符串表示不限制）
    const QString normalizedFrom = fromTime.isEmpty() ? QString("") : fromTime;
    const QString normalizedTo = toTime.isEmpty() ? QString("") : toTime;

    // 绑定时间参数（每个时间条件需要绑定两次：一次用于判断是否为空，一次用于比较）
    query.addBindValue(normalizedFrom);
    query.addBindValue(normalizedFrom);
    query.addBindValue(normalizedTo);
    query.addBindValue(normalizedTo);

    // 绑定模糊匹配参数（自动添加%通配符）
    query.addBindValue(protocolLike.isEmpty() ? "%" : "%" + protocolLike + "%");
    query.addBindValue(ipLike.isEmpty() ? "%" : "%" + ipLike + "%");

    // 执行查询
    if (!query.exec())
    {
        m_lastError = query.lastError().text();
        return entries;
    }

    // 遍历查询结果
    while (query.next())
    {
        TransmissionRunEntry entry;
        entry.protocolName = query.value(0).toString(); // 协议名称
        entry.requestedCount = query.value(1).toInt();  // 请求数量
        entry.totalCount = query.value(2).toInt();      // 实际数量
        entry.frequencyHz = query.value(3).toInt();     // 频率
        entry.startTime = query.value(4).toString();    // 开始时间
        entry.endTime = query.value(5).toString();      // 结束时间
        entry.targetIp = query.value(6).toString();     // 目标IP
        entry.targetPort = query.value(7).toInt();      // 目标端口
        entry.status = query.value(8).toString();       // 状态
        entry.errorMessage = query.value(9).toString(); // 错误信息
        entries.append(entry);                          // 添加到结果列表
    }

    m_lastError.clear(); // 清除错误信息
    return entries;
}

/**
 * @brief 初始化数据库表结构和索引
 *
 * 创建：
 * 1. transmission_runs表（如果不存在）
 * 2. 搜索优化索引（如果不存在）
 */
void TransmissionRepository::initialize()
{
    m_initialized = false; // 标记为未初始化

    QSqlQuery query(m_db);

    // 创建transmission_runs表
    if (!query.exec(
            "CREATE TABLE IF NOT EXISTS transmission_runs ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"     // 自增主键
            "protocol_name TEXT NOT NULL,"              // 协议名称（非空）
            "requested_count INTEGER NOT NULL,"         // 请求数量（非空）
            "total_count INTEGER NOT NULL,"             // 实际数量（非空）
            "frequency_hz INTEGER NOT NULL,"            // 频率（非空）
            "start_time TEXT NOT NULL,"                 // 开始时间（非空）
            "end_time TEXT NOT NULL,"                   // 结束时间（非空）
            "target_ip TEXT NOT NULL,"                  // 目标IP（非空）
            "target_port INTEGER NOT NULL,"             // 目标端口（非空）
            "status TEXT NOT NULL,"                     // 状态（非空）
            "error_message TEXT NOT NULL DEFAULT '')")) // 错误信息（默认空字符串）
    {
        m_lastError = query.lastError().text();
        return;
    }

    // 旧版本的 transmission_logs 表如果存在会被原样保留
    // 创建搜索优化索引
    if (!query.exec(
            "CREATE INDEX IF NOT EXISTS idx_transmission_runs_search "
            "ON transmission_runs(start_time, end_time, protocol_name, target_ip)"))
    {
        m_lastError = query.lastError().text();
        return;
    }

    // 标记初始化成功
    m_initialized = true;
    m_lastError.clear();
}