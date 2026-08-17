#include "udpsendcontroller.h"

#include <QDateTime>     // 日期时间处理
#include <QElapsedTimer> // 高精度计时器
#include <QHostAddress>  // IP地址处理
#include <QSet>          // 集合容器，用于存储序列号

// 匿名命名空间：内部工具函数
namespace
{
    /**
     * @brief 排空接收缓冲区中的所有待处理数据报
     * @param receiver UDP接收套接字
     * @param sequences 输出参数，存储收到的有效序列号集合
     * @param malformedCount 输出参数，异常报文计数器
     *
     * 处理流程：
     * 1. 循环读取所有待处理的数据报
     * 2. 解析报文格式：BENCH:序列号|数据
     * 3. 验证报文有效性
     * 4. 将有效序列号加入集合
     */
    void drainBenchmarkDatagrams(QUdpSocket *receiver, QSet<int> *sequences, int *malformedCount)
    {
        // 循环处理所有待处理的数据报
        while (receiver->hasPendingDatagrams())
        {
            QByteArray datagram;
            // 根据待处理数据报大小调整缓冲区
            datagram.resize(static_cast<int>(receiver->pendingDatagramSize()));
            // 读取数据报
            if (receiver->readDatagram(datagram.data(), datagram.size()) < 0)
            {
                ++(*malformedCount); // 读取失败，计数异常
                continue;
            }
            // 查找分隔符'|'
            const int separator = datagram.indexOf('|');
            // 提取前缀部分（分隔符之前的内容）
            const QByteArray prefix = separator < 0 ? datagram : datagram.left(separator);
            bool ok = false;
            // 从"BENCH:"之后提取序列号
            const int sequence = prefix.mid(sizeof("BENCH:") - 1).toInt(&ok);
            // 验证报文格式：必须以"BENCH:"开头，序列号有效且非负
            if (!prefix.startsWith("BENCH:") || !ok || sequence < 0)
            {
                ++(*malformedCount); // 格式错误，计数异常
                continue;
            }
            sequences->insert(sequence); // 插入有效序列号
        }
    }
}

/**
 * @brief LoopbackBenchmarkResult构造函数
 *
 * 初始化所有统计字段为0或默认值
 */
LoopbackBenchmarkResult::LoopbackBenchmarkResult()
    : requestedCount(0) // 请求发送数量
      ,
      sentCount(0) // 实际发送数量
      ,
      receivedCount(0) // 实际接收数量
      ,
      malformedCount(0) // 异常报文数量
      ,
      sendElapsedMs(0.0) // 发送阶段耗时（毫秒）
      ,
      totalElapsedMs(0.0) // 总耗时（毫秒）
      ,
      sendRateHz(0.0) // 发送速率（Hz）
      ,
      receiveRateHz(0.0) // 接收速率（Hz）
      ,
      lossRatePercent(0.0) // 丢包率（%）
{
}

/**
 * @brief 判断回环测试结果是否有效
 * @return 测试是否完全成功
 *
 * 有效的条件：
 * - 无错误信息
 * - 请求数量大于0
 * - 发送数量等于请求数量
 * - 接收数量等于发送数量（无丢包）
 * - 无异常报文
 */
bool LoopbackBenchmarkResult::isValid() const
{
    return error.isEmpty() && requestedCount > 0 && sentCount == requestedCount && receivedCount == sentCount && malformedCount == 0;
}

/**
 * @brief 生成回环测试结果摘要
 * @return 格式化的结果摘要字符串
 *
 * 包含：请求数、发送数、接收数、异常数、发送耗时、发送速率、
 * 端到端耗时、接收速率、丢包率
 */
QString LoopbackBenchmarkResult::summary() const
{
    // 如果有错误，返回错误信息
    if (!error.isEmpty())
    {
        return QString::fromUtf8("loopback 基准失败：%1").arg(error);
    }
    // 返回详细统计信息
    return QString::fromUtf8(
               "loopback 实测：请求 %1 条，成功写入 %2 条，接收 %3 条，异常报文 %4 条；"
               "发送阶段 %5 ms（%6 Hz），端到端 %7 ms（%8 Hz），丢包率 %9%。")
        .arg(requestedCount)
        .arg(sentCount)
        .arg(receivedCount)
        .arg(malformedCount)
        .arg(QString::number(sendElapsedMs, 'f', 3)) // 保留3位小数
        .arg(QString::number(sendRateHz, 'f', 2))    // 保留2位小数
        .arg(QString::number(totalElapsedMs, 'f', 3))
        .arg(QString::number(receiveRateHz, 'f', 2))
        .arg(QString::number(lossRatePercent, 'f', 2));
}

/**
 * @brief UdpSendController构造函数
 * @param repository 传输日志仓储指针（用于数据库记录）
 * @param parent Qt父对象
 *
 * 初始化定时器为单次触发、精确计时
 */
UdpSendController::UdpSendController(TransmissionRepository *repository, QObject *parent)
    : QObject(parent), m_repository(repository) // 日志仓储
      ,
      m_port(0) // 目标端口
      ,
      m_frequencyHz(1) // 默认频率1Hz
      ,
      m_totalCount(0) // 总发送数
      ,
      m_sentCount(0) // 已发送数
      ,
      m_running(false) // 运行状态
{
    // 连接定时器超时信号到发送槽函数
    connect(&m_timer, SIGNAL(timeout()), this, SLOT(sendOnce()));
    m_timer.setSingleShot(true);            // 单次触发模式
    m_timer.setTimerType(Qt::PreciseTimer); // 精确计时（毫秒级精度）
}

/**
 * @brief 设置协议定义
 * @param definition 协议定义对象
 */
void UdpSendController::setProtocol(const ProtocolDefinition &definition)
{
    m_definition = definition;
}

/**
 * @brief 启动UDP发送任务
 * @param ip 目标IP地址
 * @param port 目标端口
 * @param frequencyHz 发送频率（1-1000 Hz）
 * @param count 发送总数（0表示无限发送）
 * @return 启动是否成功
 *
 * 验证流程：
 * 1. 检查是否已在运行
 * 2. 检查协议是否已加载
 * 3. 验证IP地址
 * 4. 验证端口号
 * 5. 验证频率范围
 * 6. 验证发送数量
 * 7. 检查数据库连接
 */
bool UdpSendController::start(const QString &ip, quint16 port, int frequencyHz, int count)
{
    // 检查是否已在运行
    if (m_running)
    {
        m_lastError = QString::fromUtf8("发送任务正在运行");
        return false;
    }
    // 检查协议是否已加载
    if (m_definition.fields.isEmpty())
    {
        m_lastError = QString::fromUtf8("协议尚未加载");
        return false;
    }
    // 验证IP地址
    QHostAddress address;
    if (!address.setAddress(ip) || address.isNull())
    {
        m_lastError = QString::fromUtf8("目标 IP 无效");
        return false;
    }
    // 验证端口（0为无效端口）
    if (port == 0)
    {
        m_lastError = QString::fromUtf8("目标端口必须在 1 到 65535 之间");
        return false;
    }
    // 验证频率范围
    if (frequencyHz < 1 || frequencyHz > 1000)
    {
        m_lastError = QString::fromUtf8("常规发送频率必须在 1 到 1000 Hz 之间");
        return false;
    }
    // 验证发送数量（不能为负）
    if (count < 0)
    {
        m_lastError = QString::fromUtf8("发送数量不能为负数");
        return false;
    }
    // 检查数据库仓储
    if (!m_repository || !m_repository->isOpen())
    {
        m_lastError = m_repository
                          ? QString::fromUtf8("SQLite 日志不可用：%1").arg(m_repository->lastError())
                          : QString::fromUtf8("SQLite 日志仓储未配置");
        return false;
    }

    // 保存发送参数
    m_ip = address.toString();
    m_port = port;
    m_frequencyHz = frequencyHz;
    m_totalCount = count;
    m_sentCount = 0;
    m_startTime = QDateTime::currentDateTime();
    m_running = true;
    m_lastError.clear();
    m_runClock.start(); // 启动计时器
    m_timer.start(0);   // 立即触发第一次发送
    return true;
}

/**
 * @brief 停止UDP发送任务
 *
 * 如果正在运行，则结束任务并记录日志
 */
void UdpSendController::stop()
{
    if (m_running)
    {
        finishRun("stopped", QString::fromUtf8("发送已停止，共发送 %1 条").arg(m_sentCount));
    }
}

/**
 * @brief 检查是否正在运行
 * @return 运行状态
 */
bool UdpSendController::isRunning() const
{
    return m_running;
}

/**
 * @brief 获取最后一次错误信息
 * @return 错误信息字符串
 */
QString UdpSendController::lastError() const
{
    return m_lastError;
}

/**
 * @brief 执行回环基准测试
 * @param definition 协议定义
 * @param messageCount 测试报文数量
 * @param timeoutMs 接收超时时间（毫秒）
 * @return 测试结果
 *
 * 测试流程：
 * 1. 创建接收套接字并绑定到本地随机端口
 * 2. 创建发送套接字
 * 3. 循环发送带序列号的测试报文
 * 4. 定期排空接收缓冲区
 * 5. 等待接收所有报文或超时
 * 6. 计算统计数据
 */
LoopbackBenchmarkResult UdpSendController::runLoopbackBenchmark(const ProtocolDefinition &definition,
                                                                int messageCount,
                                                                int timeoutMs)
{
    LoopbackBenchmarkResult result;
    result.requestedCount = messageCount;

    // 验证协议定义
    if (definition.fields.isEmpty())
    {
        result.error = QString::fromUtf8("协议定义为空");
        return result;
    }
    // 验证报文数量
    if (messageCount <= 0)
    {
        result.error = QString::fromUtf8("测试报文数量必须为正整数");
        return result;
    }

    // 创建接收套接字
    QUdpSocket receiver;
    // 设置接收缓冲区为4MB
    receiver.setSocketOption(QAbstractSocket::ReceiveBufferSizeSocketOption, 4 * 1024 * 1024);
    // 绑定到本地回环地址的随机端口
    if (!receiver.bind(QHostAddress(QHostAddress::LocalHost), static_cast<quint16>(0)))
    {
        result.error = QString::fromUtf8("接收端绑定失败：%1").arg(receiver.errorString());
        return result;
    }

    QElapsedTimer timer;         // 高精度计时器
    QUdpSocket sender;           // 发送套接字
    QSet<int> receivedSequences; // 存储已接收的序列号

    timer.start(); // 开始计时

    // 发送测试报文
    for (int i = 0; i < messageCount; ++i)
    {
        // 生成协议数据
        const GeneratedPayload generated = m_generator.generate(definition);
        // 构造测试报文：BENCH:序列号|数据
        const QByteArray payload = QByteArray("BENCH:") + QByteArray::number(i) + '|' + generated.datagram;
        // 发送到本地回环地址
        const qint64 written = sender.writeDatagram(
            payload, QHostAddress(QHostAddress::LocalHost), receiver.localPort());
        // 检查发送是否成功
        if (written != payload.size())
        {
            result.error = QString::fromUtf8("第 %1 条 UDP 写入失败：%2").arg(i + 1).arg(sender.errorString());
            break;
        }
        ++result.sentCount; // 发送成功计数

        // 每发送32条，排空一次接收缓冲区
        if ((i + 1) % 32 == 0)
        {
            receiver.waitForReadyRead(1); // 等待1ms
            drainBenchmarkDatagrams(&receiver, &receivedSequences, &result.malformedCount);
        }
    }
    const qint64 sendElapsedNs = timer.nsecsElapsed(); // 发送阶段耗时

    // 等待接收所有报文
    const int effectiveTimeoutMs = qMax(100, timeoutMs); // 至少100ms超时
    while (receivedSequences.size() < result.sentCount && timer.elapsed() < effectiveTimeoutMs)
    {
        receiver.waitForReadyRead(10); // 等待10ms
        drainBenchmarkDatagrams(&receiver, &receivedSequences, &result.malformedCount);
    }
    // 最后排空一次
    drainBenchmarkDatagrams(&receiver, &receivedSequences, &result.malformedCount);

    // 计算统计数据
    const qint64 totalElapsedNs = timer.nsecsElapsed();
    result.receivedCount = receivedSequences.size();
    result.sendElapsedMs = qMax(0.001, sendElapsedNs / 1000000.0); // 纳秒转毫秒
    result.totalElapsedMs = qMax(0.001, totalElapsedNs / 1000000.0);
    result.sendRateHz = result.sentCount * 1000.0 / result.sendElapsedMs;         // 发送速率
    result.receiveRateHz = result.receivedCount * 1000.0 / result.totalElapsedMs; // 接收速率
    // 计算丢包率
    result.lossRatePercent = result.sentCount == 0
                                 ? 0.0
                                 : (result.sentCount - result.receivedCount) * 100.0 / result.sentCount;
    return result;
}

/**
 * @brief 执行一次UDP发送（定时器触发）
 *
 * 处理流程：
 * 1. 检查运行状态
 * 2. 检查协议定义
 * 3. 生成数据报
 * 4. 发送数据报
 * 5. 更新计数
 * 6. 发出信号
 * 7. 检查是否完成
 * 8. 调度下一次发送
 */
void UdpSendController::sendOnce()
{
    // 检查运行状态
    if (!m_running)
    {
        return;
    }
    // 检查协议定义
    if (m_definition.fields.isEmpty())
    {
        failRun(QString::fromUtf8("协议定义为空"));
        return;
    }

    // 生成数据报
    const GeneratedPayload generated = m_generator.generate(m_definition);
    const QString payloadText = generated.displayText;
    const QByteArray payload = generated.datagram;

    // 发送数据报
    const qint64 written = m_socket.writeDatagram(payload, QHostAddress(m_ip), m_port);
    if (written != payload.size())
    {
        // 发送失败
        failRun(QString::fromUtf8("UDP 写入失败：%1").arg(m_socket.errorString()));
        return;
    }
    ++m_sentCount; // 发送成功计数

    // 发出信号
    emit messageGenerated(payloadText);              // 报文生成信号
    emit progressChanged(m_sentCount, m_totalCount); // 进度更新信号

    // 检查是否完成（达到指定数量）
    if (m_totalCount > 0 && m_sentCount >= m_totalCount)
    {
        finishRun("completed", QString::fromUtf8("发送完成，共 %1 条").arg(m_sentCount));
        return;
    }

    // 调度下一次发送
    scheduleNext();
}

/**
 * @brief 以失败状态结束发送任务
 * @param error 错误信息
 */
void UdpSendController::failRun(const QString &error)
{
    finishRun("failed",
              QString::fromUtf8("发送失败：%1；已成功发送 %2 条").arg(error).arg(m_sentCount),
              error);
}

/**
 * @brief 结束发送任务并记录日志
 * @param status 任务状态（completed/stopped/failed）
 * @param message 完成消息
 * @param error 错误信息（可选）
 *
 * 处理流程：
 * 1. 检查是否在运行
 * 2. 停止定时器
 * 3. 创建日志条目
 * 4. 写入数据库
 * 5. 发出完成信号
 */
void UdpSendController::finishRun(const QString &status,
                                  const QString &message,
                                  const QString &error)
{
    // 防止重复结束
    if (!m_running)
    {
        return;
    }

    // 停止定时器
    m_timer.stop();

    // 创建日志条目
    TransmissionRunEntry entry;
    entry.protocolName = m_definition.name;
    entry.requestedCount = m_totalCount;
    entry.totalCount = m_sentCount;
    entry.frequencyHz = m_frequencyHz;
    entry.startTime = m_startTime.toString("yyyy-MM-dd HH:mm:ss.zzz");
    entry.endTime = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz");
    entry.targetIp = m_ip;
    entry.targetPort = m_port;
    entry.status = status;
    entry.errorMessage = error;

    m_running = false; // 标记为停止

    // 写入数据库日志
    if (!m_repository->insertRun(entry))
    {
        m_lastError = QString::fromUtf8("发送任务结束，但 SQLite 汇总日志写入失败：%1")
                          .arg(m_repository->lastError());
        emit runFinished(m_lastError);
        return;
    }

    m_lastError = error;
    emit runFinished(message); // 发出完成信号
}

/**
 * @brief 调度下一次发送
 *
 * 根据目标频率计算下一次发送的延迟时间：
 * - 计算目标时间点：sentCount / frequencyHz 秒
 * - 计算剩余时间
 * - 设置定时器延迟
 */
void UdpSendController::scheduleNext()
{
    // 检查运行状态
    if (!m_running)
    {
        return;
    }

    // 计算目标时间点（纳秒）
    // 使用向上取整确保发送不会早于目标时间
    const qint64 targetNs = (static_cast<qint64>(m_sentCount) * 1000000000LL + m_frequencyHz - 1) / m_frequencyHz;
    // 计算剩余时间
    const qint64 remainingNs = targetNs - m_runClock.nsecsElapsed();
    // 转换为毫秒（向上取整）
    const int delayMs = remainingNs <= 0
                            ? 0 // 已经超过目标时间，立即发送
                            : static_cast<int>((remainingNs + 999999LL) / 1000000LL);
    // 启动定时器
    m_timer.start(delayMs);
}