#include "datagenerator.h"

#include <QDateTime>
#include <QStringList>
#include <QtGlobal>

DataGenerator::DataGenerator()
    : m_seed(static_cast<quint32>(QDateTime::currentMSecsSinceEpoch() & 0xffffffff))
{
}

QString DataGenerator::generatePayload(const ProtocolDefinition &definition)
{
    QStringList groups;
    QStringList currentGroup;
    for (int i = 0; i < definition.fields.size(); ++i) {
        const ProtocolField &field = definition.fields.at(i);
        if (!field.isSelected) {
            continue;
        }
        currentGroup.append(QString("%1=%2").arg(field.name, generateFieldValue(field)));
        if (field.loopEnd) {
            groups.append(currentGroup.join(";"));
            currentGroup.clear();
        }
    }
    if (!currentGroup.isEmpty()) {
        groups.append(currentGroup.join(";"));
    }
    return groups.join(" || ");
}

QStringList DataGenerator::supportedTypes() const
{
    return supportedProtocolTypes();
}

QString DataGenerator::generateFieldValue(const ProtocolField &field)
{
    const QString type = field.dataType.isEmpty() ? field.type.toUpper() : field.dataType.toUpper();
    QString baseValue;
    if (type == "DEC" || type == "INT") {
        baseValue = QString::number(boundedSigned(field.minValue, field.maxValue));
    } else if (type == "UINT") {
        baseValue = QString::number(boundedUnsigned(field.minValue, field.maxValue));
    } else if (type == "INT8") {
        baseValue = QString::number(boundedSigned(qMax(-128.0, qMin(127.0, field.minValue)),
                                                  qMax(-128.0, qMin(127.0, field.maxValue))));
    } else if (type == "UINT8") {
        baseValue = QString::number(boundedUnsigned(qMax(0.0, qMin(255.0, field.minValue)),
                                                    qMax(0.0, qMin(255.0, field.maxValue))));
    } else if (type == "INT16") {
        baseValue = QString::number(boundedSigned(qMax(-32768.0, qMin(32767.0, field.minValue)),
                                                  qMax(-32768.0, qMin(32767.0, field.maxValue))));
    } else if (type == "UINT16") {
        baseValue = QString::number(boundedUnsigned(qMax(0.0, qMin(65535.0, field.minValue)),
                                                    qMax(0.0, qMin(65535.0, field.maxValue))));
    } else if (type == "UINT32") {
        baseValue = QString::number(static_cast<qulonglong>(
            boundedUnsigned(qMax(0.0, qMin(4294967295.0, field.minValue)),
                            qMax(0.0, qMin(4294967295.0, field.maxValue)))));
    } else if (type == "BIN") {
        baseValue = QString::number(static_cast<qulonglong>(boundedUnsigned(field.minValue, field.maxValue)), 2);
    } else if (type == "OCT") {
        baseValue = QString::number(static_cast<qulonglong>(boundedUnsigned(field.minValue, field.maxValue)), 8);
    } else if (type == "HEX") {
        baseValue = QString::number(static_cast<qulonglong>(boundedUnsigned(field.minValue, field.maxValue)), 16).toUpper();
    } else if (type == "FLT") {
        const double span = field.maxValue - field.minValue;
        const double ratio = static_cast<double>(boundedUnsigned(0, 10000)) / 10000.0;
        baseValue = QString::number(field.minValue + span * ratio, 'f', 3);
    } else if (type == "DBL") {
        const double span = field.maxValue - field.minValue;
        const double ratio = static_cast<double>(boundedUnsigned(0, 100000)) / 100000.0;
        baseValue = QString::number(field.minValue + span * ratio, 'f', 6);
    } else if (type == "STRING") {
        baseValue = randomString(field.length > 0 ? field.length : 8);
    } else if (type == "FLAG") {
        baseValue = QString::number(boundedUnsigned(0, 1));
    } else {
        baseValue = "UNSUPPORTED";
    }

    const bool literalData = !field.data.isEmpty() && !field.data.contains("${");
    if (literalData) {
        baseValue = field.data;
    }

    if (field.bitIndex >= 0) {
        bool ok = false;
        int base = 10;
        if (type == "BIN") {
            base = 2;
        } else if (type == "OCT") {
            base = 8;
        } else if (type == "HEX") {
            base = 16;
        }
        const qulonglong value = baseValue.toULongLong(&ok, base);
        if (ok) {
            baseValue = QString::number((value >> field.bitIndex) & 1ULL);
        }
    }

    return literalData ? baseValue : applyTemplate(field, baseValue);
}

QString DataGenerator::applyTemplate(const ProtocolField &field, const QString &baseValue)
{
    if (field.data.isEmpty()) {
        return baseValue;
    }
    QString output = field.data;
    output.replace("${value}", baseValue);
    output.replace("${name}", field.name);
    output.replace("${type}", field.dataType.isEmpty() ? field.type : field.dataType);
    output.replace("${timestamp}", QString::number(QDateTime::currentMSecsSinceEpoch()));
    return output;
}

QString DataGenerator::randomString(int length)
{
    const QString alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    QString result;
    for (int i = 0; i < length; ++i) {
        result.append(alphabet.at(static_cast<int>(boundedUnsigned(0, alphabet.size() - 1))));
    }
    return result;
}

qint64 DataGenerator::boundedSigned(double minValue, double maxValue)
{
    qint64 minInt = static_cast<qint64>(minValue);
    qint64 maxInt = static_cast<qint64>(maxValue);
    if (maxInt < minInt) {
        qSwap(minInt, maxInt);
    }
    const quint64 range = static_cast<quint64>(maxInt - minInt + 1);
    m_seed = m_seed * 1664525u + 1013904223u;
    return minInt + static_cast<qint64>(m_seed % (range == 0 ? 1 : range));
}

quint64 DataGenerator::boundedUnsigned(double minValue, double maxValue)
{
    quint64 minInt = static_cast<quint64>(qMax(0.0, minValue));
    quint64 maxInt = static_cast<quint64>(qMax(0.0, maxValue));
    if (maxInt < minInt) {
        qSwap(minInt, maxInt);
    }
    const quint64 range = maxInt - minInt + 1;
    m_seed = m_seed * 22695477u + 1u;
    return minInt + (range == 0 ? 0 : (m_seed % range));
}
