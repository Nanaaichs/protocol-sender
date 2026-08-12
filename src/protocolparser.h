#ifndef PROTOCOLPARSER_H
#define PROTOCOLPARSER_H

#include <QList>
#include <QString>
#include <QStringList>

QStringList supportedProtocolTypes();

struct ProtocolField {
    QString name;
    QString type;
    QString dataType;
    QString data;
    double minValue;
    double maxValue;
    int length;
    bool isSelected;
    bool isKey;
    int bitIndex;
    bool loopEnd;
};

struct ProtocolDefinition {
    QString name;
    QList<ProtocolField> fields;
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
