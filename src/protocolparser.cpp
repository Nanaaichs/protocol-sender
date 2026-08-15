// 典型的配置驱动型协议软件架构
// 读取XML协议描述文件->解析字段定义->校验协议合法性->保存成程序内部结构
#include "protocolparser.h"

#include <QFile>        //负责打开XML文件
#include <QDomDocument> // 负责解析XML文件
#include <QStringList>  // 负责处理字符串列表

// 匿名命名空间用于定义仅在当前文件中可见的辅助函数，防止命名冲突
namespace
{
    bool parseBool(const QString &value, bool defaultValue)
    {
        if (value.isEmpty())
        {
            return defaultValue;
        }
        const QString normalized = value.trimmed().toLower();
        return normalized == "1" || normalized == "true" || normalized == "yes";
    }

    QString fieldLabel(const ProtocolField &field, int index)
    {
        return field.name.isEmpty()
                   ? QString("#%1").arg(index + 1)
                   : QString("%1 (#%2)").arg(field.name).arg(index + 1);
    }

    int integerBase(const QString &type)
    {
        if (type == "BIN")
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
}

QStringList supportedProtocolTypes()
{
    return QStringList()
           << "DEC" << "INT" << "UINT" << "BIN" << "OCT" << "HEX" << "FLT" << "DBL"
           << "STRING" << "FLAG" << "INT8" << "UINT8" << "INT16" << "UINT16" << "UINT32";
}
// 核心函数：加载协议文件并解析成内部结构
bool ProtocolParser::load(const QString &filePath)
{
    // 1.打开XML文件
    QFile file(filePath); // 创建QFile对象用于操作文件
    // 2.检查文件是否成功打开
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        m_lastError = QString("无法打开 XML：%1").arg(file.errorString());
        return false;
    }
    // 3.解析XML内容
    QDomDocument document;
    // 4.检查XML解析是否成功
    QString errorMessage;

    int errorLine = 0;
    int errorColumn = 0;
    if (!document.setContent(&file, &errorMessage, &errorLine, &errorColumn))
    {
        m_lastError = QString("XML 解析失败：%1 (%2:%3)").arg(errorMessage).arg(errorLine).arg(errorColumn);
        return false;
    }
    // 5.检查根节点是否为 protocol 确认这个 XML 文件是不是一个合法的协议描述文件
    const QDomElement root = document.documentElement();
    if (root.tagName() != "protocol")
    {
        m_lastError = "根节点必须是 protocol";
        return false;
    }
    // 6.解析协议定义
    ProtocolDefinition definition;
    definition.name = root.attribute("name", "UnnamedProtocol");
    const QStringList supportedTypes = supportedProtocolTypes();
    QDomNodeList fieldNodes = root.elementsByTagName("field");
    for (int i = 0; i < fieldNodes.size(); ++i)
    {
        const QDomElement element = fieldNodes.at(i).toElement();
        ProtocolField field;
        field.name = element.attribute("name").trimmed();
        field.type = element.attribute("type").trimmed().toUpper();
        field.dataType = element.attribute("dataType", field.type).trimmed().toUpper();
        if (field.type.isEmpty())
        {
            field.type = field.dataType;
        }
        field.data = element.attribute("data");

        bool minOk = false;
        bool maxOk = false;
        bool lengthOk = false;
        bool bitIndexOk = false;
        field.minValue = element.attribute("min", "0").toDouble(&minOk);
        field.maxValue = element.attribute("max", "100").toDouble(&maxOk);
        field.length = element.attribute("length", "8").toInt(&lengthOk);
        field.isSelected = parseBool(element.attribute("isSelected"), true);
        field.isKey = parseBool(element.attribute("isKey"), false);
        field.bitIndex = element.attribute("bitIndex", "-1").toInt(&bitIndexOk);
        field.loopEnd = parseBool(element.attribute("loopEnd"), false);

        const QString label = fieldLabel(field, i);
        if (field.name.isEmpty())
        {
            m_lastError = QString("字段 %1 缺少 name").arg(label);
            return false;
        }
        if (!supportedTypes.contains(field.dataType))
        {
            m_lastError = QString("字段 %1 使用了不支持的类型：%2").arg(label, field.dataType);
            return false;
        }
        if (!minOk || !maxOk || field.minValue > field.maxValue)
        {
            m_lastError = QString("字段 %1 的 min/max 无效").arg(label);
            return false;
        }
        if (!lengthOk || field.length <= 0)
        {
            m_lastError = QString("字段 %1 的 length 必须为正整数").arg(label);
            return false;
        }
        if (!bitIndexOk || field.bitIndex < -1 || field.bitIndex > 63)
        {
            m_lastError = QString("字段 %1 的 bitIndex 必须在 -1 到 63 之间").arg(label);
            return false;
        }
        if (field.bitIndex >= 0)
        {
            const QStringList bitTypes = QStringList()
                                         << "DEC" << "INT" << "UINT" << "BIN" << "OCT" << "HEX" << "FLAG"
                                         << "INT8" << "UINT8" << "INT16" << "UINT16" << "UINT32";
            if (!bitTypes.contains(field.dataType))
            {
                m_lastError = QString("字段 %1 的类型不支持 bitIndex").arg(label);
                return false;
            }
            if (!field.data.isEmpty() && !field.data.contains("${"))
            {
                bool dataOk = false;
                field.data.toULongLong(&dataOk, integerBase(field.dataType));
                if (!dataOk)
                {
                    m_lastError = QString("字段 %1 的 data 不是可提取位的整数").arg(label);
                    return false;
                }
            }
        }
        definition.fields.append(field);
    }

    if (definition.fields.isEmpty())
    {
        m_lastError = "协议中至少需要一个 field";
        return false;
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
