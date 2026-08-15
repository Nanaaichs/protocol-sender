#ifndef DATAGENERATOR_H
#define DATAGENERATOR_H

#include "protocolparser.h"

#include <QByteArray>
#include <QMap>
#include <QString>

struct GeneratedPayload {
    QByteArray datagram;
    QString displayText;
};

class DataGenerator
{
public:
    DataGenerator();

    GeneratedPayload generate(const ProtocolDefinition &definition);
    QString generatePayload(const ProtocolDefinition &definition);
    QStringList supportedTypes() const;

private:
    QString generateTextPayload(const ProtocolDefinition &definition);
    QString generateFieldValue(const ProtocolField &field);
    QString applyTemplate(const ProtocolField &field, const QString &baseValue);
    QString randomString(int length);
    QString randomIp(const QString &minimum, const QString &maximum);
    qint64 boundedSigned(double minValue, double maxValue);
    quint64 boundedUnsigned(double minValue, double maxValue);

    quint32 m_seed;
};

#endif
