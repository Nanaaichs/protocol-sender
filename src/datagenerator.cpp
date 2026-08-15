#include "datagenerator.h"

#include <QDateTime>
#include <QHostAddress>
#include <QStringList>
#include <QtGlobal>

#include <cstring>
#include <limits>

namespace
{
    QString normalizedType(const ProtocolField &field)
    {
        return field.dataType.isEmpty()
            ? field.type.trimmed().toUpper()
            : field.dataType.trimmed().toUpper();
    }

    int integerBase(const QString &type)
    {
        if (type == "BIN" || type == "FLAG")
        {
            return 2;
        }
        if (type == "OCT")
        {
            return 8;
        }
        if (type == "HEX")
        {
            return 16;
        }
        return 10;
    }

    bool isSignedIntegerType(const QString &type)
    {
        return type == "DEC"
            || type == "INT"
            || type == "INT8"
            || type == "INT16";
    }

    bool isPackedIntegerType(const QString &type)
    {
        return isSignedIntegerType(type)
            || type == "UINT"
            || type == "UINT8"
            || type == "UINT16"
            || type == "UINT32"
            || type == "BIN"
            || type == "OCT"
            || type == "HEX"
            || type == "FLAG";
    }

    bool parseSignedText(const QString &text, qint64 *value)
    {
        bool ok = false;
        const qint64 parsed = text.trimmed().toLongLong(&ok, 10);
        if (ok && value)
        {
            *value = parsed;
        }
        return ok;
    }

    bool parseUnsignedText(const QString &text, const QString &type, quint64 *value)
    {
        bool ok = false;
        const quint64 parsed = text.trimmed().toULongLong(&ok, integerBase(type));
        if (ok && value)
        {
            *value = parsed;
        }
        return ok;
    }

    bool parseIPv4(const QString &text, quint32 *value)
    {
        if (text.isEmpty())
        {
            return false;
        }

        QHostAddress address;
        if (!address.setAddress(text))
        {
            return false;
        }

        const quint32 ipv4 = address.toIPv4Address();
        if (ipv4 == 0 && text.trimmed() != QLatin1String("0.0.0.0"))
        {
            return false;
        }

        if (value)
        {
            *value = ipv4;
        }
        return true;
    }

    quint64 maxUnsignedForBits(int bits)
    {
        if (bits >= 64)
        {
            return std::numeric_limits<quint64>::max();
        }
        return (Q_UINT64_C(1) << bits) - 1U;
    }

    void setBit(QByteArray *buffer, int bitIndex, bool value)
    {
        if (!buffer || bitIndex < 0)
        {
            return;
        }

        const int byteIndex = bitIndex / 8;
        if (byteIndex < 0 || byteIndex >= buffer->size())
        {
            return;
        }

        const int bitOffset = 7 - (bitIndex % 8);
        uchar byteValue = static_cast<uchar>(buffer->at(byteIndex));
        if (value)
        {
            byteValue = static_cast<uchar>(byteValue | (1U << bitOffset));
        }
        else
        {
            byteValue = static_cast<uchar>(byteValue & ~(1U << bitOffset));
        }
        (*buffer)[byteIndex] = static_cast<char>(byteValue);
    }

    void writeUnsignedBits(QByteArray *buffer, int bitIndex, int length, quint64 value)
    {
        for (int offset = 0; offset < length; ++offset)
        {
            const int shift = length - offset - 1;
            const bool bitValue = shift >= 64
                ? false
                : ((value >> shift) & 1U) != 0U;
            setBit(buffer, bitIndex + offset, bitValue);
        }
    }

    void writeByteAlignedBytes(QByteArray *buffer, int bitIndex, const QByteArray &bytes)
    {
        for (int byteIndex = 0; byteIndex < bytes.size(); ++byteIndex)
        {
            const uchar byteValue = static_cast<uchar>(bytes.at(byteIndex));
            for (int bit = 0; bit < 8; ++bit)
            {
                const bool bitValue = ((byteValue >> (7 - bit)) & 1U) != 0U;
                setBit(buffer, bitIndex + byteIndex * 8 + bit, bitValue);
            }
        }
    }

    QByteArray displayHex(const QByteArray &datagram)
    {
        return datagram.toHex(' ').toUpper();
    }
}

DataGenerator::DataGenerator()
    : m_seed(static_cast<quint32>(QDateTime::currentMSecsSinceEpoch() & 0xffffffff))
{
}

GeneratedPayload DataGenerator::generate(const ProtocolDefinition &definition)
{
    GeneratedPayload payload;
    if (!definition.packedBitLayout)
    {
        payload.displayText = generateTextPayload(definition);
        payload.datagram = payload.displayText.toUtf8();
        return payload;
    }

    int totalBits = 0;
    for (int i = 0; i < definition.fields.size(); ++i)
    {
        const ProtocolField &field = definition.fields.at(i);
        totalBits = qMax(totalBits, qMax(field.endBit, field.bitIndex + field.length));
    }

    payload.datagram = QByteArray((totalBits + 7) / 8, '\0');
    for (int i = 0; i < definition.fields.size(); ++i)
    {
        const ProtocolField &field = definition.fields.at(i);
        const QString type = normalizedType(field);

        if (type == "STRING")
        {
            const int byteLength = field.length / 8;
            QByteArray bytes = field.data.isEmpty()
                ? randomString(byteLength).toUtf8()
                : field.data.toUtf8();
            bytes = bytes.left(byteLength);
            while (bytes.size() < byteLength)
            {
                bytes.append('\0');
            }
            writeByteAlignedBytes(&payload.datagram, field.bitIndex, bytes);
            continue;
        }

        if (type == "IP")
        {
            const QString ipText = field.data.isEmpty()
                ? randomIp(field.minimum, field.maximum)
                : field.data;
            quint32 ipv4 = 0;
            if (parseIPv4(ipText, &ipv4))
            {
                QByteArray bytes(4, '\0');
                bytes[0] = static_cast<char>((ipv4 >> 24) & 0xffU);
                bytes[1] = static_cast<char>((ipv4 >> 16) & 0xffU);
                bytes[2] = static_cast<char>((ipv4 >> 8) & 0xffU);
                bytes[3] = static_cast<char>(ipv4 & 0xffU);
                writeByteAlignedBytes(&payload.datagram, field.bitIndex, bytes);
            }
            continue;
        }

        if (type == "FLT")
        {
            bool fixedOk = false;
            const double ratio = static_cast<double>(boundedUnsigned(0, 1000000)) / 1000000.0;
            const float value = field.data.isEmpty()
                ? static_cast<float>(field.minValue + (field.maxValue - field.minValue) * ratio)
                : field.data.toFloat(&fixedOk);
            if (field.data.isEmpty() || fixedOk)
            {
                quint32 rawValue = 0;
                std::memcpy(&rawValue, &value, sizeof(rawValue));
                writeUnsignedBits(&payload.datagram, field.bitIndex, 32, rawValue);
            }
            continue;
        }

        if (type == "DBL")
        {
            bool fixedOk = false;
            const double ratio = static_cast<double>(boundedUnsigned(0, 1000000)) / 1000000.0;
            const double value = field.data.isEmpty()
                ? field.minValue + (field.maxValue - field.minValue) * ratio
                : field.data.toDouble(&fixedOk);
            if (field.data.isEmpty() || fixedOk)
            {
                quint64 rawValue = 0;
                std::memcpy(&rawValue, &value, sizeof(rawValue));
                writeUnsignedBits(&payload.datagram, field.bitIndex, 64, rawValue);
            }
            continue;
        }

        if (isSignedIntegerType(type))
        {
            qint64 signedValue = 0;
            if (!field.data.isEmpty())
            {
                parseSignedText(field.data, &signedValue);
            }
            else
            {
                signedValue = boundedSigned(field.minValue, field.maxValue);
            }

            const quint64 rawValue = field.length >= 64
                ? static_cast<quint64>(signedValue)
                : (static_cast<quint64>(signedValue) & maxUnsignedForBits(field.length));
            writeUnsignedBits(&payload.datagram, field.bitIndex, field.length, rawValue);
            continue;
        }

        if (isPackedIntegerType(type))
        {
            quint64 unsignedValue = 0;
            if (!field.data.isEmpty())
            {
                parseUnsignedText(field.data, type, &unsignedValue);
            }
            else
            {
                unsignedValue = boundedUnsigned(field.minValue, field.maxValue);
            }

            if (field.length < 64)
            {
                unsignedValue &= maxUnsignedForBits(field.length);
            }
            writeUnsignedBits(&payload.datagram, field.bitIndex, field.length, unsignedValue);
        }
    }

    payload.displayText = QString("HEX: %1").arg(QString::fromLatin1(displayHex(payload.datagram)));
    return payload;
}

QString DataGenerator::generatePayload(const ProtocolDefinition &definition)
{
    if (definition.packedBitLayout)
    {
        return generate(definition).displayText;
    }
    return generateTextPayload(definition);
}

QStringList DataGenerator::supportedTypes() const
{
    return supportedProtocolTypes();
}

QString DataGenerator::generateTextPayload(const ProtocolDefinition &definition)
{
    QStringList groups;
    QStringList currentGroup;
    for (int i = 0; i < definition.fields.size(); ++i)
    {
        const ProtocolField &field = definition.fields.at(i);
        if (!field.isSelected)
        {
            continue;
        }

        currentGroup.append(QString("%1=%2").arg(field.name, generateFieldValue(field)));
        if (field.loopEnd)
        {
            groups.append(currentGroup.join(";"));
            currentGroup.clear();
        }
    }

    if (!currentGroup.isEmpty())
    {
        groups.append(currentGroup.join(";"));
    }

    return groups.join(" || ");
}

QString DataGenerator::generateFieldValue(const ProtocolField &field)
{
    const QString type = normalizedType(field);
    QString baseValue;
    if (type == "DEC" || type == "INT")
    {
        baseValue = QString::number(boundedSigned(field.minValue, field.maxValue));
    }
    else if (type == "UINT")
    {
        baseValue = QString::number(boundedUnsigned(field.minValue, field.maxValue));
    }
    else if (type == "INT8")
    {
        baseValue = QString::number(boundedSigned(qMax(-128.0, qMin(127.0, field.minValue)),
                                                  qMax(-128.0, qMin(127.0, field.maxValue))));
    }
    else if (type == "UINT8")
    {
        baseValue = QString::number(boundedUnsigned(qMax(0.0, qMin(255.0, field.minValue)),
                                                    qMax(0.0, qMin(255.0, field.maxValue))));
    }
    else if (type == "INT16")
    {
        baseValue = QString::number(boundedSigned(qMax(-32768.0, qMin(32767.0, field.minValue)),
                                                  qMax(-32768.0, qMin(32767.0, field.maxValue))));
    }
    else if (type == "UINT16")
    {
        baseValue = QString::number(boundedUnsigned(qMax(0.0, qMin(65535.0, field.minValue)),
                                                    qMax(0.0, qMin(65535.0, field.maxValue))));
    }
    else if (type == "UINT32")
    {
        baseValue = QString::number(static_cast<qulonglong>(
            boundedUnsigned(qMax(0.0, qMin(4294967295.0, field.minValue)),
                            qMax(0.0, qMin(4294967295.0, field.maxValue)))));
    }
    else if (type == "BIN")
    {
        baseValue = QString::number(static_cast<qulonglong>(boundedUnsigned(field.minValue, field.maxValue)), 2);
    }
    else if (type == "OCT")
    {
        baseValue = QString::number(static_cast<qulonglong>(boundedUnsigned(field.minValue, field.maxValue)), 8);
    }
    else if (type == "HEX")
    {
        baseValue = QString::number(static_cast<qulonglong>(boundedUnsigned(field.minValue, field.maxValue)), 16)
            .toUpper();
    }
    else if (type == "FLT")
    {
        const bool precisionOk = !field.precision.isEmpty();
        const int digits = precisionOk ? qMax(0, field.precision.toInt()) : 3;
        const double span = field.maxValue - field.minValue;
        const double ratio = static_cast<double>(boundedUnsigned(0, 10000)) / 10000.0;
        baseValue = QString::number(field.minValue + span * ratio, 'f', digits);
    }
    else if (type == "DBL")
    {
        const bool precisionOk = !field.precision.isEmpty();
        const int digits = precisionOk ? qMax(0, field.precision.toInt()) : 6;
        const double span = field.maxValue - field.minValue;
        const double ratio = static_cast<double>(boundedUnsigned(0, 100000)) / 100000.0;
        baseValue = QString::number(field.minValue + span * ratio, 'f', digits);
    }
    else if (type == "STRING")
    {
        baseValue = randomString(field.length > 0 ? field.length : 8);
    }
    else if (type == "FLAG")
    {
        baseValue = QString::number(boundedUnsigned(0, 1));
    }
    else if (type == "IP")
    {
        baseValue = randomIp(field.minimum, field.maximum);
    }
    else
    {
        baseValue = "UNSUPPORTED";
    }

    const bool literalData = !field.data.isEmpty() && !field.data.contains("${");
    if (literalData)
    {
        baseValue = field.data;
    }

    if (field.bitIndex >= 0)
    {
        bool ok = false;
        int base = integerBase(type);
        const qulonglong value = baseValue.toULongLong(&ok, base);
        if (ok)
        {
            baseValue = QString::number((value >> field.bitIndex) & 1ULL);
        }
    }

    return literalData ? baseValue : applyTemplate(field, baseValue);
}

QString DataGenerator::applyTemplate(const ProtocolField &field, const QString &baseValue)
{
    if (field.data.isEmpty())
    {
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
    for (int i = 0; i < length; ++i)
    {
        result.append(alphabet.at(static_cast<int>(boundedUnsigned(0, alphabet.size() - 1))));
    }
    return result;
}

QString DataGenerator::randomIp(const QString &minimum, const QString &maximum)
{
    quint32 minValue = 0;
    quint32 maxValue = 0;
    const bool minOk = parseIPv4(minimum, &minValue);
    const bool maxOk = parseIPv4(maximum, &maxValue);

    if (!minOk || !maxOk || minValue > maxValue)
    {
        minValue = QHostAddress(QString("127.0.0.1")).toIPv4Address();
        maxValue = QHostAddress(QString("127.255.255.254")).toIPv4Address();
    }

    const quint32 value = static_cast<quint32>(boundedUnsigned(minValue, maxValue));
    return QString("%1.%2.%3.%4")
        .arg((value >> 24) & 0xffU)
        .arg((value >> 16) & 0xffU)
        .arg((value >> 8) & 0xffU)
        .arg(value & 0xffU);
}

qint64 DataGenerator::boundedSigned(double minValue, double maxValue)
{
    qint64 minInt = static_cast<qint64>(minValue);
    qint64 maxInt = static_cast<qint64>(maxValue);
    if (maxInt < minInt)
    {
        qSwap(minInt, maxInt);
    }

    const quint64 range = static_cast<quint64>(maxInt - minInt + 1);
    m_seed = m_seed * 1664525U + 1013904223U;
    return minInt + static_cast<qint64>(m_seed % (range == 0 ? 1 : range));
}

quint64 DataGenerator::boundedUnsigned(double minValue, double maxValue)
{
    quint64 minInt = static_cast<quint64>(qMax(0.0, minValue));
    quint64 maxInt = static_cast<quint64>(qMax(0.0, maxValue));
    if (maxInt < minInt)
    {
        qSwap(minInt, maxInt);
    }

    const quint64 range = maxInt - minInt + 1;
    m_seed = m_seed * 22695477U + 1U;
    return minInt + (range == 0 ? 0 : (m_seed % range));
}
