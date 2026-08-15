#ifndef UDPSENDCONTROLLER_H
#define UDPSENDCONTROLLER_H

#include "datagenerator.h"
#include "protocolparser.h"
#include "transmissionrepository.h"

#include <QDateTime>
#include <QElapsedTimer>
#include <QObject>
#include <QTimer>
#include <QUdpSocket>

struct LoopbackBenchmarkResult
{
    LoopbackBenchmarkResult();

    bool isValid() const;
    QString summary() const;

    int requestedCount;
    int sentCount;
    int receivedCount;
    int malformedCount;
    double sendElapsedMs;
    double totalElapsedMs;
    double sendRateHz;
    double receiveRateHz;
    double lossRatePercent;
    QString error;
};

class UdpSendController : public QObject
{
    Q_OBJECT

public:
    explicit UdpSendController(TransmissionRepository *repository, QObject *parent = 0);

    void setProtocol(const ProtocolDefinition &definition);
    bool start(const QString &ip, quint16 port, int frequencyHz, int count);
    void stop();
    bool isRunning() const;
    QString lastError() const;

    LoopbackBenchmarkResult runLoopbackBenchmark(const ProtocolDefinition &definition,
                                                 int messageCount,
                                                 int timeoutMs = 3000);

signals:
    void progressChanged(int sentCount, int totalCount);
    void messageGenerated(const QString &payload);
    void runFinished(const QString &message);

private slots:
    void sendOnce();

private:
    void failRun(const QString &error);
    void finishRun(const QString &status, const QString &message, const QString &error = QString());
    void scheduleNext();

    TransmissionRepository *m_repository;
    ProtocolDefinition m_definition;
    DataGenerator m_generator;
    QUdpSocket m_socket;
    QTimer m_timer;
    QElapsedTimer m_runClock;
    QDateTime m_startTime;
    QString m_ip;
    QString m_lastError;
    quint16 m_port;
    int m_frequencyHz;
    int m_totalCount;
    int m_sentCount;
    bool m_running;
};

#endif
