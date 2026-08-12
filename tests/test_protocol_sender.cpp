#include <QtTest>

#include <QDir>
#include <QDebug>
#include <QFile>
#include <QSignalSpy>
#include <QUdpSocket>

#include "../src/datagenerator.h"
#include "../src/protocolparser.h"
#include "../src/transmissionrepository.h"
#include "../src/udpsendcontroller.h"

namespace {
ProtocolDefinition simpleDefinition()
{
    ProtocolDefinition definition;
    definition.name = "ControllerDemo";
    ProtocolField field;
    field.name = "value";
    field.type = "INT";
    field.dataType = "INT";
    field.minValue = 1;
    field.maxValue = 9;
    field.length = 1;
    field.isSelected = true;
    field.isKey = true;
    field.bitIndex = -1;
    field.loopEnd = false;
    definition.fields << field;
    return definition;
}
}

class ProtocolSenderTests : public QObject
{
    Q_OBJECT

private slots:
    void parseProtocol();
    void rejectInvalidProtocol();
    void generateAllSupportedTypes();
    void generatePayloadGroups();
    void literalDataSupportsBitExtraction();
    void repositoryQuery();
    void controllerRejectsInvalidParameters();
    void controllerSendsAndLogs();
    void controllerStopsContinuousRun();
    void loopbackBenchmarkDeliversDatagrams();
};

void ProtocolSenderTests::parseProtocol()
{
    const QString xmlPath = QDir::temp().absoluteFilePath("citel_t007_protocol.xml");
    QFile file(xmlPath);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
    file.write("<?xml version=\"1.0\"?><protocol name=\"Demo\"><field name=\"a\" type=\"INT\" dataType=\"INT16\" min=\"1\" max=\"9\" isSelected=\"true\" isKey=\"true\" data=\"X-${value}\" bitIndex=\"-1\" loopEnd=\"true\" /></protocol>");
    file.close();

    ProtocolParser parser;
    QVERIFY(parser.load(xmlPath));
    QCOMPARE(parser.definition().name, QString("Demo"));
    QCOMPARE(parser.definition().fields.size(), 1);
    QCOMPARE(parser.definition().fields.first().dataType, QString("INT16"));
    QVERIFY(parser.definition().fields.first().isSelected);
    QVERIFY(parser.definition().fields.first().isKey);
    QVERIFY(parser.definition().fields.first().loopEnd);
}

void ProtocolSenderTests::rejectInvalidProtocol()
{
    const QString xmlPath = QDir::temp().absoluteFilePath("citel_t007_invalid_protocol.xml");
    QFile file(xmlPath);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
    file.write("<protocol name=\"Bad\"><field name=\"value\" type=\"UNKNOWN\" /></protocol>");
    file.close();

    ProtocolParser parser;
    QVERIFY(!parser.load(xmlPath));
    QVERIFY(parser.lastError().contains("UNKNOWN"));
}

void ProtocolSenderTests::generateAllSupportedTypes()
{
    DataGenerator generator;
    const QStringList types = generator.supportedTypes();
    QCOMPARE(types.size(), 15);

    for (int i = 0; i < types.size(); ++i) {
        ProtocolDefinition definition;
        definition.name = "AllTypes";
        ProtocolField field;
        field.name = types.at(i).toLower();
        field.type = types.at(i);
        field.dataType = types.at(i);
        field.minValue = 1;
        field.maxValue = 9;
        field.length = 6;
        field.isSelected = true;
        field.isKey = false;
        field.bitIndex = -1;
        field.loopEnd = false;
        definition.fields << field;

        const QString payload = generator.generatePayload(definition);
        QVERIFY2(!payload.contains("UNSUPPORTED"), qPrintable(types.at(i)));
        const QString value = payload.section('=', 1);
        bool ok = false;
        if (types.at(i) == "STRING") {
            QCOMPARE(value.size(), 6);
        } else if (types.at(i) == "BIN") {
            value.toULongLong(&ok, 2);
            QVERIFY(ok);
        } else if (types.at(i) == "OCT") {
            value.toULongLong(&ok, 8);
            QVERIFY(ok);
        } else if (types.at(i) == "HEX") {
            value.toULongLong(&ok, 16);
            QVERIFY(ok);
        } else if (types.at(i) == "FLT" || types.at(i) == "DBL") {
            value.toDouble(&ok);
            QVERIFY(ok);
        } else if (types.at(i) == "FLAG") {
            QVERIFY(value == "0" || value == "1");
        } else {
            const qlonglong number = value.toLongLong(&ok);
            QVERIFY2(ok, qPrintable(types.at(i)));
            QVERIFY(number >= 1 && number <= 9);
        }
    }
}

void ProtocolSenderTests::generatePayloadGroups()
{
    DataGenerator generator;

    ProtocolDefinition definition;
    definition.name = "Demo";
    ProtocolField field1;
    field1.name = "first";
    field1.type = "FLAG";
    field1.dataType = "FLAG";
    field1.minValue = 0;
    field1.maxValue = 1;
    field1.length = 1;
    field1.isSelected = true;
    field1.isKey = false;
    field1.bitIndex = -1;
    field1.loopEnd = true;
    ProtocolField field2;
    field2.name = "second";
    field2.type = "STRING";
    field2.dataType = "STRING";
    field2.data = "MODE-${value}";
    field2.length = 4;
    field2.isSelected = true;
    field2.isKey = false;
    field2.bitIndex = -1;
    field2.loopEnd = false;
    ProtocolField field3;
    field3.name = "hidden";
    field3.type = "STRING";
    field3.dataType = "STRING";
    field3.length = 5;
    field3.isSelected = false;
    field3.isKey = false;
    field3.bitIndex = -1;
    field3.loopEnd = false;
    definition.fields << field1 << field2 << field3;

    const QString payload = generator.generatePayload(definition);
    QVERIFY(payload.contains("first="));
    QVERIFY(payload.contains("second=MODE-"));
    QVERIFY(!payload.contains("hidden="));
    QVERIFY(payload.contains(" || "));
}

void ProtocolSenderTests::literalDataSupportsBitExtraction()
{
    ProtocolDefinition definition;
    definition.name = "Bits";
    ProtocolField field;
    field.name = "flags";
    field.type = "UINT8";
    field.dataType = "UINT8";
    field.data = "13";
    field.minValue = 0;
    field.maxValue = 255;
    field.length = 1;
    field.isSelected = true;
    field.isKey = false;
    field.bitIndex = 2;
    field.loopEnd = false;
    definition.fields << field;

    DataGenerator generator;
    QCOMPARE(generator.generatePayload(definition), QString("flags=1"));
}

void ProtocolSenderTests::repositoryQuery()
{
    const QString dbPath = QDir::temp().absoluteFilePath("citel_t007_logs.db");
    QFile::remove(dbPath);
    TransmissionRepository repository(dbPath, "protocol_sender_tests_repo");
    QVERIFY(repository.isOpen());

    TransmissionLogEntry entry;
    entry.createdAt = "2026-07-29T10:00:00";
    entry.protocolName = "LoopbackDemo";
    entry.ip = "127.0.0.1";
    entry.port = 39001;
    entry.sequence = 1;
    entry.payload = "device_id=1";
    QVERIFY(repository.insert(entry));

    const QList<TransmissionLogEntry> entries = repository.search("2026-07-29", "Loopback", "127.0.0.1");
    QCOMPARE(entries.size(), 1);
}

void ProtocolSenderTests::controllerRejectsInvalidParameters()
{
    const QString dbPath = QDir::temp().absoluteFilePath("citel_t007_invalid_controller.db");
    QFile::remove(dbPath);
    TransmissionRepository repository(dbPath, "protocol_sender_invalid_controller_repo");
    UdpSendController controller(&repository);

    QVERIFY(!controller.start("127.0.0.1", 39001, 10, 1));
    QVERIFY(!controller.lastError().isEmpty());

    controller.setProtocol(simpleDefinition());
    QVERIFY(!controller.start("not-an-ip", 39001, 10, 1));
    QVERIFY(!controller.start("127.0.0.1", 0, 10, 1));
    QVERIFY(!controller.start("127.0.0.1", 39001, 0, 1));
    QVERIFY(!controller.start("127.0.0.1", 39001, 1001, 1));
    QVERIFY(!controller.start("127.0.0.1", 39001, 10, -1));
}

void ProtocolSenderTests::controllerSendsAndLogs()
{
    const QString dbPath = QDir::temp().absoluteFilePath("citel_t007_controller.db");
    QFile::remove(dbPath);
    TransmissionRepository repository(dbPath, "protocol_sender_controller_repo");
    UdpSendController controller(&repository);
    controller.setProtocol(simpleDefinition());

    QUdpSocket receiver;
    QVERIFY(receiver.bind(QHostAddress(QHostAddress::LocalHost), static_cast<quint16>(0)));
    QSignalSpy finished(&controller, SIGNAL(runFinished(QString)));
    QSignalSpy progress(&controller, SIGNAL(progressChanged(int,int)));

    QVERIFY(controller.start("127.0.0.1", receiver.localPort(), 100, 3));
    QTRY_COMPARE(finished.count(), 1);
    QCOMPARE(progress.count(), 3);
    QVERIFY(!controller.isRunning());
    QVERIFY(controller.lastError().isEmpty());

    int received = 0;
    while (receiver.hasPendingDatagrams()) {
        QByteArray datagram;
        datagram.resize(static_cast<int>(receiver.pendingDatagramSize()));
        receiver.readDatagram(datagram.data(), datagram.size());
        ++received;
    }
    QCOMPARE(received, 3);
    QCOMPARE(repository.search(QString(), "ControllerDemo", "127.0.0.1").size(), 3);
}

void ProtocolSenderTests::controllerStopsContinuousRun()
{
    const QString dbPath = QDir::temp().absoluteFilePath("citel_t007_continuous.db");
    QFile::remove(dbPath);
    TransmissionRepository repository(dbPath, "protocol_sender_continuous_repo");
    UdpSendController controller(&repository);
    controller.setProtocol(simpleDefinition());

    QUdpSocket receiver;
    QVERIFY(receiver.bind(QHostAddress(QHostAddress::LocalHost), static_cast<quint16>(0)));
    QSignalSpy finished(&controller, SIGNAL(runFinished(QString)));
    QSignalSpy progress(&controller, SIGNAL(progressChanged(int,int)));

    QVERIFY(controller.start("127.0.0.1", receiver.localPort(), 100, 0));
    QTRY_VERIFY(progress.count() >= 3);
    controller.stop();
    QCOMPARE(finished.count(), 1);
    QVERIFY(!controller.isRunning());
}

void ProtocolSenderTests::loopbackBenchmarkDeliversDatagrams()
{
    const QString dbPath = QDir::temp().absoluteFilePath("citel_t007_benchmark.db");
    QFile::remove(dbPath);
    TransmissionRepository repository(dbPath, "protocol_sender_benchmark_repo");
    UdpSendController controller(&repository);

    const LoopbackBenchmarkResult result = controller.runLoopbackBenchmark(simpleDefinition(), 2000, 3000);
    QVERIFY2(result.isValid(), qPrintable(result.error));
    QCOMPARE(result.requestedCount, 2000);
    QCOMPARE(result.sentCount, 2000);
    QCOMPARE(result.receivedCount, 2000);
    QCOMPARE(result.malformedCount, 0);
    QCOMPARE(result.lossRatePercent, 0.0);
    QVERIFY(result.sendRateHz > 0.0);
    QVERIFY(result.receiveRateHz > 0.0);
    QVERIFY(result.summary().contains(QString::fromUtf8("丢包率")));
    qInfo().noquote() << result.summary();
}

QTEST_GUILESS_MAIN(ProtocolSenderTests)

#include "test_protocol_sender.moc"
