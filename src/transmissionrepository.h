#ifndef TRANSMISSIONREPOSITORY_H
#define TRANSMISSIONREPOSITORY_H
// 负责发送日志和SQLite数据库交互的模块。
#include <QString>
#include <QList>
#include <QSqlDatabase>
// Qt SQL 模块中的数据库连接类。
//
// QSqlDatabase 负责维护数据库连接。
// 例如项目中连接 SQLite 数据库时，会通过它：
//
//     打开数据库
//     判断数据库是否连接成功
//     获取数据库连接
//
// 后面的成员：
//
//     QSqlDatabase m_db;
//
// 就是数据库连接对象。
struct TransmissionLogEntry
{
    QString createdAt;    // 发送时间，格式为 "YYYY-MM-DD HH:MM:SS"
    QString protocolName; // 协议名称，例如 "TelemetryProtocol"
    QString ip;           // 目标IP地址
    int port;             // UDP目标端口
    int sequence;         // 当前包的序列号
    QString payload;      // 实际发送给的数据内容，例如""AA550120"
};

class TransmissionRepository
{
public:
    // ========================================================
    // 构造函数
    // ========================================================

    TransmissionRepository(
        const QString &databasePath,
        const QString &connectionName);

    // 创建 TransmissionRepository 对象时调用。
    //
    // 有两个输入参数：
    //
    // 1. databasePath
    //
    //     SQLite 数据库文件路径。
    //
    //     例如：
    //
    //     C:/xxx/protocol_sender.db
    //
    //
    // 2. connectionName
    //
    //     Qt 数据库连接的名字。
    //
    //     Qt 可以同时维护多个数据库连接，
    //     所以每个连接可以有一个名字。
    //
    //     例如：
    //
    //     "protocol_sender_main"
    //
    //
    // 你之前 MainWindow 构造函数里有：
    //
    //     m_repository(
    //         ... "protocol_sender.db",
    //         "protocol_sender_main"
    //     )
    //
    // 就是在调用这个构造函数。

    // ========================================================
    // 析构函数
    // ========================================================

    ~TransmissionRepository();

    // 当 TransmissionRepository 对象销毁时自动调用。
    //
    // 一般用于清理资源，例如：
    //
    //     关闭数据库连接
    //     移除 Qt 数据库连接
    //
    // 构造函数：
    //
    //     对象出生时调用
    //
    // 析构函数：
    //
    //     对象死亡时调用

    // ========================================================
    // 判断数据库是否打开
    // ========================================================

    bool isOpen() const;

    // 输入：无
    //
    // 输出：
    //
    //     true  → 数据库当前已打开
    //     false → 数据库没有打开
    //
    // 最后的 const 表示：
    //
    //     调用这个函数不会修改 Repository 本身。

    // ========================================================
    // 获取最近一次错误
    // ========================================================

    QString lastError() const;

    // 输入：无
    //
    // 输出：
    //
    //     QString
    //
    // 例如：
    //
    //     "无法打开数据库"
    //     "SQL执行失败"
    //
    // 数据通常来自：
    //
    //     m_lastError

    // ========================================================
    // 插入一条日志
    // ========================================================

    bool insert(const TransmissionLogEntry &entry);

    // 输入：
    //
    //     const TransmissionLogEntry &entry
    //
    // 也就是“一条发送记录”。
    //
    // 例如：
    //
    //     entry.createdAt
    //     entry.protocolName
    //     entry.ip
    //     entry.port
    //     entry.sequence
    //     entry.payload
    //
    //
    // Repository 会把这条记录写入 SQLite。
    //
    // 返回：
    //
    //     true  → 插入成功
    //     false → 插入失败
    //
    //
    // 注意这里使用：
    //
    //     const TransmissionLogEntry &entry
    //
    // 而不是：
    //
    //     TransmissionLogEntry entry
    //
    // 原因是：
    //
    //     &     → 按引用传递，避免复制整个结构体
    //     const → 保证 insert() 不修改传入的 entry

    // ========================================================
    // 查询日志
    // ========================================================

    QList<TransmissionLogEntry> search(
        const QString &timeLike,
        const QString &protocolLike,
        const QString &ipLike);

    // 这个函数有三个输入：
    //
    //     timeLike
    //         时间过滤条件
    //
    //     protocolLike
    //         协议名称过滤条件
    //
    //     ipLike
    //         IP 过滤条件
    //
    //
    // 例如：
    //
    //     search(
    //         "2026-08-15",
    //         "TestProtocol",
    //         "127.0.0.1"
    //     );
    //
    //
    // “Like” 这个名字通常暗示 SQL 中可能使用：
    //
    //     LIKE
    //
    // 做模糊查询。
    //
    // 比如：
    //
    //     protocol_name LIKE '%Test%'
    //
    //
    // 返回值：
    //
    //     QList<TransmissionLogEntry>
    //
    // 意思是：
    //
    //     返回“一组符合条件的日志记录”。
// ============================================================
// private 区域
//
// 下面这些只能由 TransmissionRepository 自己直接使用。
// ============================================================

private:
    void initialize();

// 初始化数据库。
//
// 输入：无
// 输出：无
//
// 通常可能负责：
//
//     打开 SQLite 数据库
//         ↓
//     检查表是否存在
//         ↓
//     如果不存在则 CREATE TABLE
//
// 一般会由构造函数自动调用。
//
// 外部代码不应该主动：
//
//     repository.initialize();
//
// 所以放在 private。

    QSqlDatabase m_db;

// 当前 Repository 持有的数据库连接对象。
//
// 可以理解成：
//
// TransmissionRepository
//       │
//       └── m_db
//              ↓
//          SQLite 数据库
//
// 后面的 insert()、search() 等，
// 都会通过这个数据库连接执行 SQL。

    QString m_lastError;

// 保存最近一次数据库相关错误。
//
// 例如：
//
//     m_lastError = "数据库打开失败";
//
// 外部不能直接访问它，
// 需要调用：
//
//     lastError()
};

// ============================================================
// 结束头文件保护
// ============================================================

#endif
