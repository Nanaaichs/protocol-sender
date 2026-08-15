#include "protocolparser.h"

#include <QDomDocument>
#include <QFile>
#include <QFileInfo>
#include <QHostAddress>
#include <QPair>

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

bool parseCourseBool(const QString &value, bool defaultValue, bool *result)
{
    const QString normalized = value.trimmed().toLower();
    if (normalized.isEmpty())
    {
        if (result) *result = defaultValue;
        return true;
    }
    if (normalized == "true" || normalized == "false")
    {
        if (result) *result = normalized == "true";
        return true;
    }
    return false;
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
    if (type == "BIN" || type == "FLAG") return 2;
    if (type == "OCT") return 8;
    if (type == "HEX") return 16;
    return 10;
}

bool isSignedIntegerType(const QString &type)
{
    return type == "INT" || type == "INT8" || type == "INT16";
}

bool isUnsignedIntegerType(const QString &type)
{
    return type == "DEC" || type == "UINT" || type == "UINT8" || type == "UINT16"
        || type == "UINT32" || type == "BIN" || type == "OCT" || type == "HEX"
        || type == "FLAG";
}

bool isIntegerType(const QString &type)
{
    return isSignedIntegerType(type) || isUnsignedIntegerType(type);
}

quint64 maximumForBits(int bitCount)
{
    if (bitCount >= 64) return std::numeric_limits<quint64>::max();
    return (Q_UINT64_C(1) << bitCount) - Q_UINT64_C(1);
}

bool parseIPv4(const QString &text, quint32 *value)
{
    QHostAddress address;
    if (text.trimmed().isEmpty() || !address.setAddress(text.trimmed())) return false;
    const quint32 ipv4 = address.toIPv4Address();
    if (ipv4 == 0 && text.trimmed() != "0.0.0.0") return false;
    if (value) *value = ipv4;
    return true;
}

bool parseOptionalPort(const QString &text, int *port)
{
    if (text.trimmed().isEmpty())
    {
        if (port) *port = 0;
        return true;
    }
    bool ok = false;
    const int parsed = text.trimmed().toInt(&ok);
    if (!ok || parsed < 1 || parsed > 65535) return false;
    if (port) *port = parsed;
    return true;
}

bool validateCourseLength(const ProtocolField &field, QString *error)
{
    const QString type = field.dataType;
    bool valid = field.length > 0;
    if (type == "DEC" || type == "INT" || type == "UINT" || type == "OCT"
        || type == "HEX" || type == "STRING")
    {
        valid = valid && field.length % 8 == 0;
    }
    else if (type == "FLAG") valid = field.length == 8;
    else if (type == "FLT" || type == "DBL") valid = field.length == 64;
    else if (type == "IP") valid = field.length == 32;
    else if (type == "INT8" || type == "UINT8") valid = field.length == 8;
    else if (type == "INT16" || type == "UINT16") valid = field.length == 16;
    else if (type == "UINT32") valid = field.length == 32;
    else if (type == "BIN") valid = field.length > 0;

    if (isIntegerType(type) && field.length > 64)
    {
        if (error) *error = QString::fromUtf8("整数类型 length 不能超过 64 bit");
        return false;
    }
    if (!valid)
    {
        if (error) *error = QString::fromUtf8("字段 length 不符合 datatype 规则");
        return false;
    }
    return true;
}

bool parseCourseRange(ProtocolField *field, QString *error)
{
    if (!field) return false;
    const QString type = field->dataType;

    if (!field->precision.isEmpty())
    {
        bool precisionOk = false;
        const int precision = field->precision.toInt(&precisionOk);
        if (!precisionOk || precision < 0)
        {
            if (error) *error = QString::fromUtf8("precision 必须是非负整数");
            return false;
        }
    }

    if (type == "STRING")
    {
        if (field->isKey && field->data.toUtf8().size() > field->length / 8)
        {
            if (error) *error = QString::fromUtf8("标识 STRING 字段 data 超过 length");
            return false;
        }
        field->minValue = 0.0;
        field->maxValue = 0.0;
        return true;
    }

    if (type == "IP")
    {
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
        if (field->isKey)
        {
            quint32 fixed = 0;
            if (!parseIPv4(field->data, &fixed) || fixed < minimum || fixed > maximum)
            {
                if (error) *error = QString::fromUtf8("标识 IP 字段 data 无效或超出范围");
                return false;
            }
        }
        field->minValue = static_cast<double>(minimum);
        field->maxValue = static_cast<double>(maximum);
        return true;
    }

    if (type == "FLT" || type == "DBL")
    {
        const QString minimumText = field->minimum.isEmpty() ? "0" : field->minimum;
        const QString maximumText = field->maximum.isEmpty() ? "100" : field->maximum;
        bool minOk = false;
        bool maxOk = false;
        field->minValue = minimumText.toDouble(&minOk);
        field->maxValue = maximumText.toDouble(&maxOk);
        if (!minOk || !maxOk || field->minValue > field->maxValue)
        {
            if (error) *error = QString::fromUtf8("浮点字段 minimum/maximum 无效");
            return false;
        }
        if (field->isKey)
        {
            bool dataOk = false;
            const double fixed = field->data.toDouble(&dataOk);
            if (!dataOk || fixed < field->minValue || fixed > field->maxValue)
            {
                if (error) *error = QString::fromUtf8("标识浮点字段 data 无效或超出范围");
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
            if (error) *error = QString::fromUtf8("有符号整数字段 minimum/maximum 无效");
            return false;
        }
        if (field->length < 64)
        {
            const qint64 lowest = -(Q_INT64_C(1) << (field->length - 1));
            const qint64 highest = (Q_INT64_C(1) << (field->length - 1)) - 1;
            if (minimum < lowest || maximum > highest)
            {
                if (error) *error = QString::fromUtf8("有符号整数范围超出 length");
                return false;
            }
        }
        if (field->isKey)
        {
            bool dataOk = false;
            const qint64 fixed = field->data.toLongLong(&dataOk, 10);
            if (!dataOk || fixed < minimum || fixed > maximum)
            {
                if (error) *error = QString::fromUtf8("标识有符号整数字段 data 无效或超出范围");
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
        const QString maximumText = field->maximum.isEmpty()
            ? QString::number(maximumForBits(field->length), base)
            : field->maximum;
        bool minOk = false;
        bool maxOk = false;
        const quint64 minimum = minimumText.toULongLong(&minOk, base);
        const quint64 maximum = maximumText.toULongLong(&maxOk, base);
        if (!minOk || !maxOk || minimum > maximum || maximum > maximumForBits(field->length))
        {
            if (error) *error = QString::fromUtf8("无符号整数字段 minimum/maximum 无效或超出 length");
            return false;
        }
        if (field->isKey)
        {
            bool dataOk = false;
            const quint64 fixed = field->data.toULongLong(&dataOk, base);
            if (!dataOk || fixed < minimum || fixed > maximum)
            {
                if (error) *error = QString::fromUtf8("标识无符号整数字段 data 无效或超出范围");
                return false;
            }
        }
        field->minValue = static_cast<double>(minimum);
        field->maxValue = static_cast<double>(maximum);
        return true;
    }

    if (error) *error = QString::fromUtf8("不支持的字段类型");
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
    if (field->length <= 0 || field->bitIndex < -1 || field->bitIndex > 63)
    {
        if (error) *error = QString::fromUtf8("字段 %1 的 length/bitIndex 无效").arg(label);
        return false;
    }

    if (field->dataType == "IP")
    {
        quint32 minimum = 0;
        quint32 maximum = 0;
        if (!parseIPv4(field->minimum, &minimum) || !parseIPv4(field->maximum, &maximum)
            || minimum > maximum)
        {
            if (error) *error = QString::fromUtf8("字段 %1 的 IP 范围无效").arg(label);
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

    if (field->bitIndex >= 0)
    {
        if (!isIntegerType(field->dataType))
        {
            if (error) *error = QString::fromUtf8("字段 %1 的类型不支持 bitIndex").arg(label);
            return false;
        }
        if (!field->data.isEmpty() && !field->data.contains("${"))
        {
            bool dataOk = false;
            field->data.toULongLong(&dataOk, integerBase(field->dataType));
            if (!dataOk)
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
    m_definition = ProtocolDefinition();
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
    definition.packedBitLayout = hasDirectChild(fieldNodes.at(0).toElement(), "fieldName");
    definition.name = root.attribute("name").trimmed();
    if (definition.name.isEmpty())
    {
        definition.name = definition.packedBitLayout
            ? QFileInfo(filePath).completeBaseName()
            : QString("UnnamedProtocol");
    }

    if (definition.packedBitLayout)
    {
        definition.sourceIp = directChildText(root, "sourceIP");
        definition.destinationIp = directChildText(root, "destIP");
        definition.system = directChildText(root, "system");
        definition.metadata.insert("protoHead", directChildText(root, "protoHead"));

        quint32 parsedIp = 0;
        if ((!definition.sourceIp.isEmpty() && !parseIPv4(definition.sourceIp, &parsedIp))
            || (!definition.destinationIp.isEmpty() && !parseIPv4(definition.destinationIp, &parsedIp)))
        {
            m_lastError = QString::fromUtf8("sourceIP/destIP 无效");
            return false;
        }
        if (!parseOptionalPort(directChildText(root, "sourcePort"), &definition.sourcePort)
            || !parseOptionalPort(directChildText(root, "destPort"), &definition.destinationPort))
        {
            m_lastError = QString::fromUtf8("sourcePort/destPort 必须在 1 到 65535 之间");
            return false;
        }
        const QString typeText = directChildText(root, "nType");
        if (!typeText.isEmpty())
        {
            bool typeOk = false;
            definition.messageType = typeText.toInt(&typeOk);
            if (!typeOk)
            {
                m_lastError = QString::fromUtf8("nType 必须是整数");
                return false;
            }
        }
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

            if (!parseCourseBool(directChildText(element, "isSelected"), true, &field.isSelected)
                || !parseCourseBool(directChildText(element, "isKey"), false, &field.isKey))
            {
                m_lastError = QString::fromUtf8("字段 %1 的 isSelected/isKey 只能是 true 或 false")
                    .arg(fieldLabel(field, i));
                return false;
            }

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
                m_lastError = QString::fromUtf8("字段 %1 超过 UDP 最大载荷").arg(label);
                return false;
            }
            QString validationError;
            if (!validateCourseLength(field, &validationError))
            {
                m_lastError = QString::fromUtf8("字段 %1：%2").arg(label, validationError);
                return false;
            }
            if (field.isKey && field.data.isEmpty())
            {
                m_lastError = QString::fromUtf8("字段 %1 的 isKey=true 时 data 必须有值").arg(label);
                return false;
            }
            if (!parseCourseRange(&field, &validationError))
            {
                m_lastError = QString::fromUtf8("字段 %1：%2").arg(label, validationError);
                return false;
            }
            for (int rangeIndex = 0; rangeIndex < occupiedRanges.size(); ++rangeIndex)
            {
                const QPair<int, int> range = occupiedRanges.at(rangeIndex);
                if (field.bitIndex < range.second && field.endBit > range.first)
                {
                    m_lastError = QString::fromUtf8("字段 %1 的位区间与其他字段重叠").arg(label);
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
            field.bitIndex = element.attribute("bitIndex", "-1").toInt(&bitIndexOk);
            field.isSelected = parseBool(element.attribute("isSelected"), true);
            field.isKey = parseBool(element.attribute("isKey"), false);
            field.loopEnd = parseBool(element.attribute("loopEnd"), false);
            if (!lengthOk || !bitIndexOk || !validateLegacyField(&field, i, &m_lastError))
            {
                if (m_lastError.isEmpty())
                    m_lastError = QString::fromUtf8("字段 %1 的 length/bitIndex 无效").arg(fieldLabel(field, i));
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
