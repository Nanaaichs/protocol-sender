#ifndef DATAGENERATOR_H
#define DATAGENERATOR_H

#include "protocolparser.h"

#include <QMap>
#include <QString>

class DataGenerator
{
public:
    DataGenerator();

    QString generatePayload(const ProtocolDefinition &definition);
    QStringList supportedTypes() const;

private:
    QString generateFieldValue(const ProtocolField &field);
    QString applyTemplate(const ProtocolField &field, const QString &baseValue);
    QString randomString(int length);
    qint64 boundedSigned(double minValue, double maxValue);
    quint64 boundedUnsigned(double minValue, double maxValue);

    quint32 m_seed;
};

#endif
