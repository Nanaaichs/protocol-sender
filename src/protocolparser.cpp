#include "protocolparser.h"

#include <QDomDocument>
#include <QFile>
#include <QFileInfo>
#include <QHostAddress>

#include <limits>

namespace
{
bool parseBool(const QString &value, bool defaultValue)
{
    if (value.trimmed().isEmpty())
    {
        return defaultValue;
    }
    const QString normalized = value.trimmed().toLower();
    return normalized == "1" || normalized == "true" || normalized == "yes";
}

QString directChildText(const QDomElement &parent, const QString &tagName)
{
    for (QDomNode node = parent.firstChild(); !node.isNull(); node = node.nextSibling())
    {
        const QDomElement element = node.toElement();
        if (!element.isNull() && element.tagName() == tagName)
        {
            return element.text().trimmed();
        }
    }
    return QString();
}

bool hasDirectChild(const QDomElement &parent, const QString &tagName)
{
    for (QDomNode node = parent.firstChild(); !node.isNull(); node = node.nextSibling())
    {
        const QDomElement element = node.toElement();
        if (!element.isNull() && element.tagName() == tagName)
        {
            return true;
        }
    }
    return false;
}

QString fieldLabel(const ProtocolField &field, int index)
{
    return field.name.isEmpty()
        ? QString("#%1").arg(index + 1)
        : QString("%1 (#%2)").arg(field.name).arg(index + 1);
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
    return type == "DEC" || type == "INT" || type == "INT8" || type == "INT16";
}

bool isUnsignedIntegerType(const QString &type)
{
    return type == "UINT" || type == "UINT8" || type == "UINT16" || type == "UINT32"
        || type == "BIN" || type == "OCT" || type == "HEX" || type == "FLAG";
}

quint64 maximumForBits(int bitCount)
{
    if (bitCount >= 64)
    {
        return std::numeric_limits<quint64>::max();
    }
    return (Q_UINT64_C(1) << bitCount) - Q_UINT64_C(1);
}

bool parseIPv4(const QString &text, quint32 *value)
{
    QHostAddress address;
    if (text.trimmed().isEmpty() || !address.setAddress(text.trimmed()))
    {
        return false;
    }
    const quint32 ipv4 = address.toIPv4Address();
    if (ipv4 == 0 && text.trimmed() != "0.0.0.0")
    {
        return false;
    }
    if (value)
    {
        *value = ipv4;
    }
    return true;
}

bool parseOptionalPort(const QString &text, int *port)
{
    if (text.isEmpty())
    {
        if (port)
        {
            *port = 0;
        }
        return true;
    }
    bool ok = false;
    const int parsed = text.toInt(&ok);
    if (!ok || parsed < 1 || parsed > 65535)
    {
        return false;
    }
    if (port)
    {
        *port = parsed;
    }
    return true;
}

bool parseCourseRange(ProtocolField *field, QString *error)
{
    if (!field)
    {
        return false;
    }

    const QString type = field->dataType;
    if (type == "STRING")
    {
        field->minValue = 0.0;
        field->maxValue = 0.0;
        return true;
    }

    if (type == "IP")
    {
        if (field->length != 32)
        {
            if (error) *error = QString::fromUtf8("IP 字段 length 必须为 32 bit");
            return false;
        }
        if (field->minimum.isEmpty()) field->minimum = "0.0.0.0";
        if (field->maximum.isEmpty()) field->maximum = "255.255.255.255";
        quint32 minimum = 0;
        quint32 maximum = 0;
        if (!parseIPv4(field->minimum, &minimum) || !parseIPv4(field->maximum, &maximum)
            || minimum > maximum)
        {
            if (error) *error = QString::fromUtf8("IP 字段 minimum/maximum 无效");
            return false;
        }
        if (!field->data.isEmpty())
        {
            quint32 fixed = 0;
            if (!parseIPv4(field->data, &fixed) || fixed < minimum || fixed > maximum)
            {
                if (error) *error = QString::fromUtf8("IP 字段 data 无效或超出范围");
                return false;
            }
        }
        field->minValue = static_cast<double>(minimum);
        field->maxValue = static_cast<double>(maximum);
        return true;
    }

    if (type == "FLT" || type == "DBL")
    {
        bool minOk = false;
        bool maxOk = false;
        const QString minimumText = field->minimum.isEmpty() ? "0" : field->minimum;
        const QString maximumText = field->maximum.isEmpty() ? "100" : field->maximum;
        field->minValue = minimumText.toDouble(&minOk);
        field->maxValue = maximumText.toDouble(&maxOk);
        if (!minOk || !maxOk || field->minValue > field->maxValue)
        {
            if (error) *error = QString::fromUtf8("浮点字段 minimum/maximum 无效");
            return false;
        }
        if (!field->data.isEmpty())
        {
            bool dataOk = false;
            const double fixed = field->data.toDouble(&dataOk);
            if (!dataOk || fixed < field->minValue || fixed > field->maxValue)
            {
                if (error) *error = QString::fromUtf8("浮点字段 data 无效或超出范围");
                return false;
            }
        }
        return true;
    }

    if (isSignedIntegerType(type))
    {
        const QString minimumText = field->minimum.isEmpty() ? "0" : field->minimum;
        const QString maximumText = field->maximum.isEmpty() ? "100" : field->maximum;
        bool minOk = false;
        bool maxOk = false;
        const qint64 minimum = minimumText.toLongLong(&minOk, 10);
        const qint64 maximum = maximumText.toLongLong(&maxOk, 10);
        if (!minOk || !maxOk || minimum > maximum)
        {
            if (error) *error = QString::fromUtf8("有符号字段 minimum/maximum 无效");
            return false;
        }
        if (field->length < 64)
        {
            const qint64 lowest = -(Q_INT64_C(1) << (field->length - 1));
            const qint64 highest = (Q_INT64_C(1) << (field->length - 1)) - 1;
            if (minimum < lowest || maximum > highest)
            {
                if (error) *error = QString::fromUtf8("有符号字段范围超出 length 可表示范围");
                return false;
            }
        }
        if (!field->data.isEmpty())
        {
            bool dataOk = false;
            const qint64 fixed = field->data.toLongLong(&dataOk, 10);
            if (!dataOk || fixed < minimum || fixed > maximum)
            {
                if (error) *error = QString::fromUtf8("有符号字段 data 无效或超出范围");
                return false;
            }
        }
        field->minValue = static_cast<double>(minimum);
        field->maxValue = static_cast<double>(maximum);
        return true;
    }

    if (isUnsignedIntegerType(type))
    {
        const int base = integerBase(type);
        const QString minimumText = field->minimum.isEmpty() ? "0" : field->minimum;
        QString maximumText = field->maximum;
        if (maximumText.isEmpty())
        {
            maximumText = QString::number(maximumForBits(field->length), base);
        }
        bool minOk = false;
        bool maxOk = false;
        const quint64 minimum = minimumText.toULongLong(&minOk, base);
        const quint64 maximum = maximumText.toULongLong(&maxOk, base);
        if (!minOk || !maxOk || minimum > maximum || maximum > maximumForBits(field->length))
        {
            if (error) *error = QString::fromUtf8("无符号/进制字段 minimum/maximum 无效或超出 length");
            return false;
        }
        if (!field->data.isEmpty())
        {
            bool dataOk = false;
            const quint64 fixed = field->data.toULongLong(&dataOk, base);
            if (!dataOk || fixed < minimum || fixed > maximum || fixed > maximumForBits(field->length))
            {
                if (error) *error = QString::fromUtf8("无符号/进制字段 data 无效或超出范围");
                return false;
            }
        }
        field->minValue = static_cast<double>(minimum);
        field->maxValue = static_cast<double>(maximum);
        return true;
    }

    if (error) *error = QString::fromUtf8("未知字段类型");
    return false;
}

bool validateLegacyField(ProtocolField *field, int index, QString *error)
{
    const QString label = fieldLabel(*field, index);
    if (field->name.isEmpty())
    {
        if (error) *error = QString::fromUtf8("字段 %1 缺少 name").arg(label);
        return false;
    }
    if (!supportedProtocolTypes().contains(field->dataType))
    {
        if (error) *error = QString::fromUtf8("字段 %1 使用了不支持的类型：%2")
            .arg(label, field->dataType);
        return false;
    }
    if (field->dataType == "IP")
    {
        quint32 minimum = 0;
        quint32 maximum = 0;
        if (!parseIPv4(field->minimum, &minimum) || !parseIPv4(field->maximum, &maximum)
            || minimum > maximum)
        {
            if (error) *error = QString::fromUtf8("字段 %1 的 min/max IP 无效").arg(label);
            return false;
        }
        field->minValue = static_cast<double>(minimum);
        field->maxValue = static_cast<double>(maximum);
    }
    else
    {
        bool minOk = false;
        bool maxOk = false;
        field->minValue = field->minimum.toDouble(&minOk);
        field->maxValue = field->maximum.toDouble(&maxOk);
        if (!minOk || !maxOk || field->minValue > field->maxValue)
        {
            if (error) *error = QString::fromUtf8("字段 %1 的 min/max 无效").arg(label);
            return false;
        }
    }
    if (field->length <= 0)
    {
        if (error) *error = QString::fromUtf8("字段 %1 的 length 必须为正整数").arg(label);
        return false;
    }
    if (field->bitIndex < -1 || field->bitIndex > 63)
    {
        if (error) *error = QString::fromUtf8("字段 %1 的 bitIndex 必须在 -1 到 63 之间").arg(label);
        return false;
    }
    if (field->bitIndex >= 0)
    {
        const bool supportsBitExtraction = isSignedIntegerType(field->dataType)
            || isUnsignedIntegerType(field->dataType);
        if (!supportsBitExtraction)
        {
            if (error) *error = QString::fromUtf8("字段 %1 的类型不支持 bitIndex").arg(label);
            return false;
        }
        if (!field->data.isEmpty() && !field->data.contains("${"))
        {
            bool ok = false;
            field->data.toULongLong(&ok, integerBase(field->dataType));
            if (!ok)
            {
                if (error) *error = QString::fromUtf8("字段 %1 的 data 不是可提取位的整数").arg(label);
                return false;
            }
        }
    }
    return true;
}
}

QStringList supportedProtocolTypes()
{
    return QStringList()
        << "DEC" << "INT" << "UINT" << "BIN" << "OCT" << "HEX" << "FLT" << "DBL"
        << "STRING" << "FLAG" << "INT8" << "UINT8" << "INT16" << "UINT16" << "UINT32"
        << "IP";
}

bool ProtocolParser::load(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        m_lastError = QString::fromUtf8("无法打开 XML：%1").arg(file.errorString());
        return false;
    }

    QDomDocument document;
    QString errorMessage;
    int errorLine = 0;
    int errorColumn = 0;
    if (!document.setContent(&file, &errorMessage, &errorLine, &errorColumn))
    {
        m_lastError = QString::fromUtf8("XML 解析失败：%1 (%2:%3)")
            .arg(errorMessage).arg(errorLine).arg(errorColumn);
        return false;
    }

    const QDomElement root = document.documentElement();
    if (root.tagName() != "protocol")
    {
        m_lastError = QString::fromUtf8("根节点必须是 protocol");
        return false;
    }

    const QDomNodeList fieldNodes = root.elementsByTagName("field");
    if (fieldNodes.isEmpty())
    {
        m_lastError = QString::fromUtf8("协议中至少需要一个 field");
        return false;
    }

    ProtocolDefinition definition;
    const QDomElement firstField = fieldNodes.at(0).toElement();
    definition.packedBitLayout = hasDirectChild(firstField, "fieldName");

    if (definition.packedBitLayout)
    {
        definition.sourceIp = directChildText(root, "sourceIP");
        definition.destinationIp = directChildText(root, "destIP");
        definition.system = directChildText(root, "system");
        definition.metadata.insert("protoHead", directChildText(root, "protoHead"));
        definition.metadata.insert("sourceIP", definition.sourceIp);
        definition.metadata.insert("destIP", definition.destinationIp);

        if (!definition.sourceIp.isEmpty())
        {
            quint32 ignored = 0;
            if (!parseIPv4(definition.sourceIp, &ignored))
            {
                m_lastError = QString::fromUtf8("sourceIP 无效：%1").arg(definition.sourceIp);
                return false;
            }
        }
        if (!definition.destinationIp.isEmpty())
        {
            quint32 ignored = 0;
            if (!parseIPv4(definition.destinationIp, &ignored))
            {
                m_lastError = QString::fromUtf8("destIP 无效：%1").arg(definition.destinationIp);
                return false;
            }
        }
        if (!parseOptionalPort(directChildText(root, "sourcePort"), &definition.sourcePort))
        {
            m_lastError = QString::fromUtf8("sourcePort 必须在 1 到 65535 之间");
            return false;
        }
        if (!parseOptionalPort(directChildText(root, "destPort"), &definition.destinationPort))
        {
            m_lastError = QString::fromUtf8("destPort 必须在 1 到 65535 之间");
            return false;
        }
        const QString messageTypeText = directChildText(root, "nType");
        if (!messageTypeText.isEmpty())
        {
            bool ok = false;
            definition.messageType = messageTypeText.toInt(&ok);
            if (!ok || (definition.messageType != 0 && definition.messageType != 1))
            {
                m_lastError = QString::fromUtf8("nType 必须为 0（正常）或 1（报文头）");
                return false;
            }
        }
        definition.metadata.insert("nType", QString::number(definition.messageType));
        definition.metadata.insert("system", definition.system);
    }

    definition.name = root.attribute("name").trimmed();
    if (definition.name.isEmpty())
    {
        definition.name = definition.packedBitLayout
            ? QFileInfo(filePath).completeBaseName()
            : QString("UnnamedProtocol");
    }
    if (definition.name.isEmpty())
    {
        definition.name = definition.system.isEmpty() ? QString("UnnamedProtocol") : definition.system;
    }

    QList<QPair<int, int> > occupiedRanges;
    for (int i = 0; i < fieldNodes.size(); ++i)
    {
        const QDomElement element = fieldNodes.at(i).toElement();
        ProtocolField field;

        if (definition.packedBitLayout)
        {
            field.name = directChildText(element, "fieldName");
            field.type = directChildText(element, "datatype").toUpper();
            field.dataType = field.type;
            field.data = directChildText(element, "data");
            field.minimum = directChildText(element, "minimum");
            field.maximum = directChildText(element, "maximum");
            field.precision = directChildText(element, "precision");
            field.comment = directChildText(element, "comment");
            field.isSelected = parseBool(directChildText(element, "isSelected"), true);
            field.isKey = parseBool(directChildText(element, "isKey"), false);

            bool bitIndexOk = false;
            bool lengthOk = false;
            bool endBitOk = false;
            field.bitIndex = directChildText(element, "bitIndex").toInt(&bitIndexOk);
            field.length = directChildText(element, "length").toInt(&lengthOk);
            field.endBit = directChildText(element, "loopEnd").toInt(&endBitOk);

            const QString label = fieldLabel(field, i);
            if (field.name.isEmpty())
            {
                m_lastError = QString::fromUtf8("字段 %1 缺少 fieldName").arg(label);
                return false;
            }
            if (!supportedProtocolTypes().contains(field.dataType))
            {
                m_lastError = QString::fromUtf8("字段 %1 使用了不支持的类型：%2")
                    .arg(label, field.dataType);
                return false;
            }
            if (!bitIndexOk || field.bitIndex < 0 || !lengthOk || field.length <= 0
                || !endBitOk || field.endBit != field.bitIndex + field.length)
            {
                m_lastError = QString::fromUtf8("字段 %1 必须满足 loopEnd = bitIndex + length").arg(label);
                return false;
            }
            if (field.endBit > 65507 * 8)
            {
                m_lastError = QString::fromUtf8("字段 %1 使报文超出 UDP 载荷上限").arg(label);
                return false;
            }
            if ((isSignedIntegerType(field.dataType) || isUnsignedIntegerType(field.dataType))
                && field.length > 64)
            {
                m_lastError = QString::fromUtf8("字段 %1 的整数位长不能超过 64 bit").arg(label);
                return false;
            }
            if ((field.dataType == "STRING" && field.length % 8 != 0)
                || (field.dataType == "FLT" && field.length != 32)
                || (field.dataType == "DBL" && field.length != 64))
            {
                m_lastError = QString::fromUtf8("字段 %1 的 length 与类型不匹配").arg(label);
                return false;
            }
            bool precisionOk = false;
            const int precision = field.precision.isEmpty() ? 0 : field.precision.toInt(&precisionOk);
            if ((!field.precision.isEmpty() && !precisionOk) || precision < 0)
            {
                m_lastError = QString::fromUtf8("字段 %1 的 precision 必须为非负整数").arg(label);
                return false;
            }

            QString rangeError;
            if (!parseCourseRange(&field, &rangeError))
            {
                m_lastError = QString::fromUtf8("字段 %1：%2").arg(label, rangeError);
                return false;
            }
            for (int rangeIndex = 0; rangeIndex < occupiedRanges.size(); ++rangeIndex)
            {
                const QPair<int, int> range = occupiedRanges.at(rangeIndex);
                if (field.bitIndex < range.second && field.endBit > range.first)
                {
                    m_lastError = QString::fromUtf8("字段 %1 与已有字段位区间重叠").arg(label);
                    return false;
                }
            }
            occupiedRanges.append(qMakePair(field.bitIndex, field.endBit));
        }
        else
        {
            field.name = element.attribute("name").trimmed();
            field.type = element.attribute("type").trimmed().toUpper();
            field.dataType = element.attribute("dataType", field.type).trimmed().toUpper();
            if (field.type.isEmpty()) field.type = field.dataType;
            field.data = element.attribute("data");
            field.minimum = element.attribute("min", field.dataType == "IP" ? "0.0.0.0" : "0");
            field.maximum = element.attribute("max", field.dataType == "IP" ? "255.255.255.255" : "100");
            field.precision = element.attribute("precision");

            bool lengthOk = false;
            bool bitIndexOk = false;
            field.length = element.attribute("length", "8").toInt(&lengthOk);
            field.isSelected = parseBool(element.attribute("isSelected"), true);
            field.isKey = parseBool(element.attribute("isKey"), false);
            field.bitIndex = element.attribute("bitIndex", "-1").toInt(&bitIndexOk);
            field.loopEnd = parseBool(element.attribute("loopEnd"), false);
            if (!lengthOk || !bitIndexOk)
            {
                m_lastError = QString::fromUtf8("字段 %1 的 length/bitIndex 必须为整数")
                    .arg(fieldLabel(field, i));
                return false;
            }
            if (!validateLegacyField(&field, i, &m_lastError))
            {
                return false;
            }
        }

        definition.fields.append(field);
    }

    m_definition = definition;
    m_lastError.clear();
    return true;
}

QString ProtocolParser::lastError() const
{
    return m_lastError;
}

ProtocolDefinition ProtocolParser::definition() const
{
    return m_definition;
}
