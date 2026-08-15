#ifndef PROTOCOLPARSER_H
#define PROTOCOLPARSER_H

#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>

QStringList supportedProtocolTypes();

struct ProtocolField {
    QString name;
    QString type;
    QString dataType;
    QString data;
    double minValue = 0.0;
    double maxValue = 100.0;
    int length = 8;
    bool isSelected = true;
    bool isKey = false;
    int bitIndex = -1;
    bool loopEnd = false;
    int endBit = -1;
    QString minimum;
    QString maximum;
    QString precision;
    QString comment;
};

struct ProtocolDefinition {
    QString name;
    QList<ProtocolField> fields;
    bool packedBitLayout = false;
    QString sourceIp;
    QString destinationIp;
    int sourcePort = 0;
    int destinationPort = 0;
    int messageType = 0;
    QString system;
    QMap<QString, QString> metadata;
};

class ProtocolParser
{
public:
    bool load(const QString &filePath);
    QString lastError() const;
    ProtocolDefinition definition() const;

private:
    QString m_lastError;
    ProtocolDefinition m_definition;
};

#endif
