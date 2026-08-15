#include "udpsendcontroller.h"

#include <QDateTime>
#include <QElapsedTimer>
#include <QHostAddress>
#include <QSet>

namespace {
void drainBenchmarkDatagrams(QUdpSocket *receiver, QSet<int> *sequences, int *malformedCount)
{
    while (receiver->hasPendingDatagrams()) {
        QByteArray datagram;
        datagram.resize(static_cast<int>(receiver->pendingDatagramSize()));
        if (receiver->readDatagram(datagram.data(), datagram.size()) < 0) {
            ++(*malformedCount);
            continue;
        }
        const int separator = datagram.indexOf('|');
        const QByteArray prefix = separator < 0 ? datagram : datagram.left(separator);
        bool ok = false;
        const int sequence = prefix.mid(sizeof("BENCH:") - 1).toInt(&ok);
        if (!prefix.startsWith("BENCH:") || !ok || sequence < 0) {
            ++(*malformedCount);
            continue;
        }
        sequences->insert(sequence);
    }
}
}

LoopbackBenchmarkResult::LoopbackBenchmarkResult()
    : requestedCount(0)
    , sentCount(0)
    , receivedCount(0)
    , malformedCount(0)
    , sendElapsedMs(0.0)
    , totalElapsedMs(0.0)
    , sendRateHz(0.0)
    , receiveRateHz(0.0)
    , lossRatePercent(0.0)
{
}

bool LoopbackBenchmarkResult::isValid() const
{
    return error.isEmpty()
        && requestedCount > 0
        && sentCount == requestedCount
        && receivedCount == sentCount
        && malformedCount == 0;
}

QString LoopbackBenchmarkResult::summary() const
{
    if (!error.isEmpty()) {
        return QString::fromUtf8("loopback 基准失败：%1").arg(error);
    }
    return QString::fromUtf8(
        "loopback 实测：请求 %1 条，成功写入 %2 条，接收 %3 条，异常报文 %4 条；"
        "发送阶段 %5 ms（%6 Hz），端到端 %7 ms（%8 Hz），丢包率 %9%。")
        .arg(requestedCount)
        .arg(sentCount)
        .arg(receivedCount)
        .arg(malformedCount)
        .arg(QString::number(sendElapsedMs, 'f', 3))
        .arg(QString::number(sendRateHz, 'f', 2))
        .arg(QString::number(totalElapsedMs, 'f', 3))
        .arg(QString::number(receiveRateHz, 'f', 2))
        .arg(QString::number(lossRatePercent, 'f', 2));
}

UdpSendController::UdpSendController(TransmissionRepository *repository, QObject *parent)
    : QObject(parent)
    , m_repository(repository)
    , m_port(0)
    , m_frequencyHz(1)
    , m_totalCount(0)
    , m_sentCount(0)
    , m_running(false)
{
    connect(&m_timer, SIGNAL(timeout()), this, SLOT(sendOnce()));
    m_timer.setSingleShot(true);
    m_timer.setTimerType(Qt::PreciseTimer);
}

void UdpSendController::setProtocol(const ProtocolDefinition &definition)
{
    m_definition = definition;
}

bool UdpSendController::start(const QString &ip, quint16 port, int frequencyHz, int count)
{
    if (m_running) {
        m_lastError = QString::fromUtf8("发送任务正在运行");
        return false;
    }
    if (m_definition.fields.isEmpty()) {
        m_lastError = QString::fromUtf8("协议尚未加载");
        return false;
    }
    QHostAddress address;
    if (!address.setAddress(ip) || address.isNull()) {
        m_lastError = QString::fromUtf8("目标 IP 无效");
        return false;
    }
    if (port == 0) {
        m_lastError = QString::fromUtf8("目标端口必须在 1 到 65535 之间");
        return false;
    }
    if (frequencyHz < 1 || frequencyHz > 1000) {
        m_lastError = QString::fromUtf8("常规发送频率必须在 1 到 1000 Hz 之间");
        return false;
    }
    if (count < 0) {
        m_lastError = QString::fromUtf8("发送数量不能为负数");
        return false;
    }
    if (!m_repository || !m_repository->isOpen()) {
        m_lastError = m_repository
            ? QString::fromUtf8("SQLite 日志不可用：%1").arg(m_repository->lastError())
            : QString::fromUtf8("SQLite 日志仓储未配置");
        return false;
    }

    m_ip = address.toString();
    m_port = port;
    m_frequencyHz = frequencyHz;
    m_totalCount = count;
    m_sentCount = 0;
    m_startTime = QDateTime::currentDateTime();
    m_running = true;
    m_lastError.clear();
    m_runClock.start();
    m_timer.start(0);
    return true;
}

void UdpSendController::stop()
{
    if (m_running) {
        finishRun("stopped", QString::fromUtf8("发送已停止，共发送 %1 条").arg(m_sentCount));
    }
}

bool UdpSendController::isRunning() const
{
    return m_running;
}

QString UdpSendController::lastError() const
{
    return m_lastError;
}

LoopbackBenchmarkResult UdpSendController::runLoopbackBenchmark(const ProtocolDefinition &definition,
                                                                int messageCount,
                                                                int timeoutMs)
{
    LoopbackBenchmarkResult result;
    result.requestedCount = messageCount;
    if (definition.fields.isEmpty()) {
        result.error = QString::fromUtf8("协议定义为空");
        return result;
    }
    if (messageCount <= 0) {
        result.error = QString::fromUtf8("测试报文数量必须为正整数");
        return result;
    }

    QUdpSocket receiver;
    receiver.setSocketOption(QAbstractSocket::ReceiveBufferSizeSocketOption, 4 * 1024 * 1024);
    if (!receiver.bind(QHostAddress(QHostAddress::LocalHost), static_cast<quint16>(0))) {
        result.error = QString::fromUtf8("接收端绑定失败：%1").arg(receiver.errorString());
        return result;
    }

    QElapsedTimer timer;
    QUdpSocket sender;
    QSet<int> receivedSequences;
    timer.start();
    for (int i = 0; i < messageCount; ++i) {
        const GeneratedPayload generated = m_generator.generate(definition);
        const QByteArray payload = QByteArray("BENCH:") + QByteArray::number(i) + '|'
            + generated.datagram;
        const qint64 written = sender.writeDatagram(
            payload, QHostAddress(QHostAddress::LocalHost), receiver.localPort());
        if (written != payload.size()) {
            result.error = QString::fromUtf8("第 %1 条 UDP 写入失败：%2").arg(i + 1).arg(sender.errorString());
            break;
        }
        ++result.sentCount;
        if ((i + 1) % 32 == 0) {
            receiver.waitForReadyRead(1);
            drainBenchmarkDatagrams(&receiver, &receivedSequences, &result.malformedCount);
        }
    }
    const qint64 sendElapsedNs = timer.nsecsElapsed();

    const int effectiveTimeoutMs = qMax(100, timeoutMs);
    while (receivedSequences.size() < result.sentCount && timer.elapsed() < effectiveTimeoutMs) {
        receiver.waitForReadyRead(10);
        drainBenchmarkDatagrams(&receiver, &receivedSequences, &result.malformedCount);
    }
    drainBenchmarkDatagrams(&receiver, &receivedSequences, &result.malformedCount);

    const qint64 totalElapsedNs = timer.nsecsElapsed();
    result.receivedCount = receivedSequences.size();
    result.sendElapsedMs = qMax(0.001, sendElapsedNs / 1000000.0);
    result.totalElapsedMs = qMax(0.001, totalElapsedNs / 1000000.0);
    result.sendRateHz = result.sentCount * 1000.0 / result.sendElapsedMs;
    result.receiveRateHz = result.receivedCount * 1000.0 / result.totalElapsedMs;
    result.lossRatePercent = result.sentCount == 0
        ? 0.0
        : (result.sentCount - result.receivedCount) * 100.0 / result.sentCount;
    return result;
}

void UdpSendController::sendOnce()
{
    if (!m_running) {
        return;
    }
    if (m_definition.fields.isEmpty()) {
        failRun(QString::fromUtf8("协议定义为空"));
        return;
    }
    const GeneratedPayload generated = m_generator.generate(m_definition);
    const QString payloadText = generated.displayText;
    const QByteArray payload = generated.datagram;
    const qint64 written = m_socket.writeDatagram(payload, QHostAddress(m_ip), m_port);
    if (written != payload.size()) {
        failRun(QString::fromUtf8("UDP 写入失败：%1").arg(m_socket.errorString()));
        return;
    }
    ++m_sentCount;

    emit messageGenerated(payloadText);
    emit progressChanged(m_sentCount, m_totalCount);

    if (m_totalCount > 0 && m_sentCount >= m_totalCount) {
        finishRun("completed", QString::fromUtf8("发送完成，共 %1 条").arg(m_sentCount));
        return;
    }

    scheduleNext();
}

void UdpSendController::failRun(const QString &error)
{
    finishRun("failed",
              QString::fromUtf8("发送失败：%1；已成功发送 %2 条").arg(error).arg(m_sentCount),
              error);
}

void UdpSendController::finishRun(const QString &status,
                                  const QString &message,
                                  const QString &error)
{
    if (!m_running) {
        return;
    }

    m_timer.stop();
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

    m_running = false;
    if (!m_repository->insertRun(entry)) {
        m_lastError = QString::fromUtf8("发送任务结束，但 SQLite 汇总日志写入失败：%1")
            .arg(m_repository->lastError());
        emit runFinished(m_lastError);
        return;
    }

    m_lastError = error;
    emit runFinished(message);
}

void UdpSendController::scheduleNext()
{
    if (!m_running) {
        return;
    }
    const qint64 targetNs = (static_cast<qint64>(m_sentCount) * 1000000000LL
                             + m_frequencyHz - 1) / m_frequencyHz;
    const qint64 remainingNs = targetNs - m_runClock.nsecsElapsed();
    const int delayMs = remainingNs <= 0
        ? 0
        : static_cast<int>((remainingNs + 999999LL) / 1000000LL);
    m_timer.start(delayMs);
}
