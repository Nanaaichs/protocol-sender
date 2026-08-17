#include "protocolparser.h"

#include <QDomDocument> // Qt的XML DOM解析器，将XML文件转换为DOM树结构
#include <QFile>        // 负责打开文件
#include <QFileInfo>    // 用于获取文件名信息
#include <QHostAddress> // Qt网络地址类，用于判断IP地址是否合法
#include <QPair>        // 用来检查字段之间是否发生位重叠

#include <limits> // C++标准库，此处是为了获取quint64能表示的最大值

// 匿名命名空间（anonymous namespace）
// 其中的函数并不是ProtocolParser类的成员函数，而是独立的工具函数
// 仅在protocolparser.cpp文件内部使用，外部无法访问
namespace
{
    /**
     * @brief 字符串转bool，支持多种表示方式（宽松解析）
     * @param value 传入XML里的字符串
     * @param defaultValue 默认值，如果字符串为空则返回此值
     * @return 解析后的布尔值
     *
     * 注意：const QString &value 使用const引用传递，避免复制整个字符串提高效率
     * 引用传递的是字符串的地址，而不是字符串本身
     */
    bool parseBool(const QString &value, bool defaultValue)
    {
        // 去掉字符串前后的空格并判断是否为空
        if (value.trimmed().isEmpty())
        {
            return defaultValue; // 空字符串返回默认值
        }
        // 将字符串转换为小写（局部变量存储转换后的结果）
        const QString normalized = value.trimmed().toLower();
        // 鲁棒性处理：支持"1"、"true"、"yes"等多种表示方式，都返回true
        return normalized == "1" || normalized == "true" || normalized == "yes";
    }

    /**
     * @brief 严格解析布尔值，不符合要求时返回false
     * @param value 输入字符串
     * @param defaultValue 输入为空时的默认值
     * @param result 输出参数，存储真正解析出的布尔值
     * @return 解析是否成功（true表示输入合法，false表示输入格式错误）
     *
     * 注意：*result和&value的区别：
     * - &value是函数参数中的引用传递，表示传入的字符串
     * - *result是指针解引用，用于向调用者返回解析结果
     */
    bool parseCourseBool(const QString &value, bool defaultValue, bool *result)
    {
        const QString normalized = value.trimmed().toLower();
        if (normalized.isEmpty())
        {
            if (result)
                *result = defaultValue; // 空值使用默认值
            return true;                // 空值是合法的
        }
        if (normalized == "true" || normalized == "false")
        {
            if (result)
                *result = normalized == "true"; // 解析为对应的布尔值
            return true;
        }
        return false; // 非"true"或"false"的非法输入
    }

    /**
     * @brief 查找父节点下指定标签名的直接子节点的文本内容
     * @param parent 父节点
     * @param tagName 要查找的子节点标签名
     * @return 子节点的文本内容（去除前后空格），如果不存在则返回空字符串
     */
    QString directChildText(const QDomElement &parent, const QString &tagName)
    {
        // 遍历父节点的所有直接子节点
        for (QDomNode node = parent.firstChild(); !node.isNull(); node = node.nextSibling())
        {
            const QDomElement element = node.toElement();
            // 检查节点是否为元素节点且标签名匹配
            if (!element.isNull() && element.tagName() == tagName)
            {
                return element.text().trimmed(); // 返回去除空格的文本
            }
        }
        return QString(); // 未找到返回空字符串
    }

    /**
     * @brief 判断父节点下是否存在指定标签名的直接子节点
     * @param parent 父节点
     * @param tagName 要查找的子节点标签名
     * @return 是否存在该子节点
     */
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

    /**
     * @brief 生成字段的显示标签
     * @param field 协议字段
     * @param index 字段索引（从0开始）
     * @return 字段标签字符串
     *
     * 如果字段有名称，返回"名称 (#序号)"格式
     * 如果字段无名称，返回"#序号"格式
     */
    QString fieldLabel(const ProtocolField &field, int index)
    {
        return field.name.isEmpty()
                   ? QString("#%1").arg(index + 1)
                   : QString("%1 (#%2)").arg(field.name).arg(index + 1);
    }

    /**
     * @brief 根据数据类型返回对应的进制基数
     * @param type 数据类型字符串
     * @return 进制基数（2、8、10或16）
     */
    int integerBase(const QString &type)
    {
        if (type == "BIN" || type == "FLAG")
            return 2; // 二进制
        if (type == "OCT")
            return 8; // 八进制
        if (type == "HEX")
            return 16; // 十六进制
        return 10;     // 默认十进制
    }

    /**
     * @brief 判断是否为有符号整数类型
     * @param type 数据类型字符串
     * @return 是否为有符号整数类型
     */
    bool isSignedIntegerType(const QString &type)
    {
        return type == "INT" || type == "INT8" || type == "INT16";
    }

    /**
     * @brief 判断是否为无符号整数类型
     * @param type 数据类型字符串
     * @return 是否为无符号整数类型
     */
    bool isUnsignedIntegerType(const QString &type)
    {
        return type == "DEC" || type == "UINT" || type == "UINT8" ||
               type == "UINT16" || type == "UINT32" || type == "BIN" ||
               type == "OCT" || type == "HEX" || type == "FLAG";
    }

    /**
     * @brief 判断是否为整数类型（有符号或无符号）
     * @param type 数据类型字符串
     * @return 是否为整数类型
     */
    bool isIntegerType(const QString &type)
    {
        return isSignedIntegerType(type) || isUnsignedIntegerType(type);
    }

    /**
     * @brief 计算指定位数能表示的最大值
     * @param bitCount 位数
     * @return 该位数能表示的最大无符号整数值
     */
    quint64 maximumForBits(int bitCount)
    {
        if (bitCount >= 64)
            return std::numeric_limits<quint64>::max();     // 64位及以上返回最大值
        return (Q_UINT64_C(1) << bitCount) - Q_UINT64_C(1); // 2^n - 1
    }

    /**
     * @brief 解析IPv4地址字符串
     * @param text IPv4地址字符串
     * @param value 输出参数，存储解析后的32位无符号整数
     * @return 解析是否成功
     */
    bool parseIPv4(const QString &text, quint32 *value)
    {
        QHostAddress address;
        // 空字符串或无法解析的地址返回false
        if (text.trimmed().isEmpty() || !address.setAddress(text.trimmed()))
            return false;
        const quint32 ipv4 = address.toIPv4Address();
        // 特殊处理：如果解析结果为0但输入不是"0.0.0.0"，说明解析失败
        if (ipv4 == 0 && text.trimmed() != "0.0.0.0")
            return false;
        if (value)
            *value = ipv4;
        return true;
    }

    /**
     * @brief 解析可选端口号
     * @param text 端口号字符串
     * @param port 输出参数，存储解析后的端口号
     * @return 解析是否成功
     *
     * 空字符串表示无端口（返回0），端口范围必须在1-65535之间
     */
    bool parseOptionalPort(const QString &text, int *port)
    {
        if (text.trimmed().isEmpty())
        {
            if (port)
                *port = 0; // 空值表示无端口
            return true;
        }
        bool ok = false;
        const int parsed = text.trimmed().toInt(&ok);
        if (!ok || parsed < 1 || parsed > 65535) // 端口范围验证
            return false;
        if (port)
            *port = parsed;
        return true;
    }

    /**
     * @brief 验证紧凑位布局格式下字段的长度是否符合数据类型规则
     * @param field 协议字段
     * @param error 输出参数，存储错误信息
     * @return 验证是否通过
     *
     * 不同数据类型有特定的长度要求：
     * - 十进制/整数类型：长度必须为8的倍数
     * - FLAG：必须为8位
     * - 浮点类型：必须为64位
     * - IP地址：必须为32位
     * - 固定位整数：有特定长度要求
     */
    bool validateCourseLength(const ProtocolField &field, QString *error)
    {
        const QString type = field.dataType;
        bool valid = field.length > 0;

        // 可变长度整数类型和字符串要求长度为8的倍数
        if (type == "DEC" || type == "INT" || type == "UINT" ||
            type == "OCT" || type == "HEX" || type == "STRING")
        {
            valid = valid && field.length % 8 == 0;
        }
        else if (type == "FLAG")
            valid = field.length == 8; // 标志位固定8位
        else if (type == "FLT" || type == "DBL")
            valid = field.length == 64; // 浮点数固定64位
        else if (type == "IP")
            valid = field.length == 32; // IP地址固定32位
        else if (type == "INT8" || type == "UINT8")
            valid = field.length == 8; // 8位整数
        else if (type == "INT16" || type == "UINT16")
            valid = field.length == 16; // 16位整数
        else if (type == "UINT32")
            valid = field.length == 32; // 32位整数
        else if (type == "BIN")
            valid = field.length > 0; // 二进制任意正长度

        // 整数类型长度不能超过64位
        if (isIntegerType(type) && field.length > 64)
        {
            if (error)
                *error = QString::fromUtf8("整数类型 length 不能超过 64 bit");
            return false;
        }
        if (!valid)
        {
            if (error)
                *error = QString::fromUtf8("字段 length 不符合 datatype 规则");
            return false;
        }
        return true;
    }

    /**
     * @brief 解析并验证紧凑位布局格式下字段的取值范围
     * @param field 协议字段指针
     * @param error 输出参数，存储错误信息
     * @return 解析是否成功
     *
     * 根据不同数据类型处理：
     * - STRING：验证标识字段的数据长度
     * - IP：解析IPv4地址范围
     * - 浮点数：解析数值范围
     * - 有符号整数：验证范围并检查是否超出位数限制
     * - 无符号整数：验证范围并检查是否超出位数限制
     */
    bool parseCourseRange(ProtocolField *field, QString *error)
    {
        if (!field)
            return false;
        const QString type = field->dataType;

        // 验证precision参数（精度）
        if (!field->precision.isEmpty())
        {
            bool precisionOk = false;
            const int precision = field->precision.toInt(&precisionOk);
            if (!precisionOk || precision < 0)
            {
                if (error)
                    *error = QString::fromUtf8("precision 必须是非负整数");
                return false;
            }
        }

        // 字符串类型处理
        if (type == "STRING")
        {
            // 标识字段的data不能超过length指定的字节数
            if (field->isKey && field->data.toUtf8().size() > field->length / 8)
            {
                if (error)
                    *error = QString::fromUtf8("标识 STRING 字段 data 超过 length");
                return false;
            }
            field->minValue = 0.0;
            field->maxValue = 0.0;
            return true;
        }

        // IP地址类型处理
        if (type == "IP")
        {
            // 设置默认IP范围
            if (field->minimum.isEmpty())
                field->minimum = "0.0.0.0";
            if (field->maximum.isEmpty())
                field->maximum = "255.255.255.255";
            quint32 minimum = 0;
            quint32 maximum = 0;
            // 解析并验证IP范围
            if (!parseIPv4(field->minimum, &minimum) ||
                !parseIPv4(field->maximum, &maximum) || minimum > maximum)
            {
                if (error)
                    *error = QString::fromUtf8("IP 字段 minimum/maximum 无效");
                return false;
            }
            // 标识字段的data必须在范围内
            if (field->isKey)
            {
                quint32 fixed = 0;
                if (!parseIPv4(field->data, &fixed) || fixed < minimum || fixed > maximum)
                {
                    if (error)
                        *error = QString::fromUtf8("标识 IP 字段 data 无效或超出范围");
                    return false;
                }
            }
            field->minValue = static_cast<double>(minimum);
            field->maxValue = static_cast<double>(maximum);
            return true;
        }

        // 浮点数类型处理
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
                if (error)
                    *error = QString::fromUtf8("浮点字段 minimum/maximum 无效");
                return false;
            }
            // 标识字段的data必须在范围内
            if (field->isKey)
            {
                bool dataOk = false;
                const double fixed = field->data.toDouble(&dataOk);
                if (!dataOk || fixed < field->minValue || fixed > field->maxValue)
                {
                    if (error)
                        *error = QString::fromUtf8("标识浮点字段 data 无效或超出范围");
                    return false;
                }
            }
            return true;
        }

        // 有符号整数类型处理
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
                if (error)
                    *error = QString::fromUtf8("有符号整数字段 minimum/maximum 无效");
                return false;
            }
            // 检查范围是否超出指定位数能表示的范围
            if (field->length < 64)
            {
                const qint64 lowest = -(Q_INT64_C(1) << (field->length - 1));
                const qint64 highest = (Q_INT64_C(1) << (field->length - 1)) - 1;
                if (minimum < lowest || maximum > highest)
                {
                    if (error)
                        *error = QString::fromUtf8("有符号整数范围超出 length");
                    return false;
                }
            }
            // 标识字段的data必须在范围内
            if (field->isKey)
            {
                bool dataOk = false;
                const qint64 fixed = field->data.toLongLong(&dataOk, 10);
                if (!dataOk || fixed < minimum || fixed > maximum)
                {
                    if (error)
                        *error = QString::fromUtf8("标识有符号整数字段 data 无效或超出范围");
                    return false;
                }
            }
            field->minValue = static_cast<double>(minimum);
            field->maxValue = static_cast<double>(maximum);
            return true;
        }

        // 无符号整数类型处理
        if (isUnsignedIntegerType(type))
        {
            const int base = integerBase(type);
            const QString minimumText = field->minimum.isEmpty() ? "0" : field->minimum;
            // 默认最大值为该位数能表示的最大值
            const QString maximumText = field->maximum.isEmpty()
                                            ? QString::number(maximumForBits(field->length), base)
                                            : field->maximum;
            bool minOk = false;
            bool maxOk = false;
            const quint64 minimum = minimumText.toULongLong(&minOk, base);
            const quint64 maximum = maximumText.toULongLong(&maxOk, base);
            // 验证范围并检查是否超出位数限制
            if (!minOk || !maxOk || minimum > maximum || maximum > maximumForBits(field->length))
            {
                if (error)
                    *error = QString::fromUtf8("无符号整数字段 minimum/maximum 无效或超出 length");
                return false;
            }
            // 标识字段的data必须在范围内
            if (field->isKey)
            {
                bool dataOk = false;
                const quint64 fixed = field->data.toULongLong(&dataOk, base);
                if (!dataOk || fixed < minimum || fixed > maximum)
                {
                    if (error)
                        *error = QString::fromUtf8("标识无符号整数字段 data 无效或超出范围");
                    return false;
                }
            }
            field->minValue = static_cast<double>(minimum);
            field->maxValue = static_cast<double>(maximum);
            return true;
        }

        if (error)
            *error = QString::fromUtf8("不支持的字段类型");
        return false;
    }

    /**
     * @brief 验证传统属性格式下的字段
     * @param field 协议字段指针
     * @param index 字段索引
     * @param error 输出参数，存储错误信息
     * @return 验证是否通过
     *
     * 检查内容包括：
     * - 字段名是否存在
     * - 数据类型是否支持
     * - 长度和位索引是否有效
     * - 最小/最大值是否合法
     * - bitIndex使用时类型是否支持
     */
    bool validateLegacyField(ProtocolField *field, int index, QString *error)
    {
        const QString label = fieldLabel(*field, index);

        // 字段名必须存在
        if (field->name.isEmpty())
        {
            if (error)
                *error = QString::fromUtf8("字段 %1 缺少 name").arg(label);
            return false;
        }

        // 数据类型必须受支持
        if (!supportedProtocolTypes().contains(field->dataType))
        {
            if (error)
                *error = QString::fromUtf8("字段 %1 使用了不支持的类型：%2")
                             .arg(label, field->dataType);
            return false;
        }

        // 长度和位索引范围验证
        if (field->length <= 0 || field->bitIndex < -1 || field->bitIndex > 63)
        {
            if (error)
                *error = QString::fromUtf8("字段 %1 的 length/bitIndex 无效").arg(label);
            return false;
        }

        // IP地址类型特殊处理
        if (field->dataType == "IP")
        {
            quint32 minimum = 0;
            quint32 maximum = 0;
            if (!parseIPv4(field->minimum, &minimum) ||
                !parseIPv4(field->maximum, &maximum) || minimum > maximum)
            {
                if (error)
                    *error = QString::fromUtf8("字段 %1 的 IP 范围无效").arg(label);
                return false;
            }
            field->minValue = static_cast<double>(minimum);
            field->maxValue = static_cast<double>(maximum);
        }
        else
        {
            // 其他类型：解析min/max为double
            bool minOk = false;
            bool maxOk = false;
            field->minValue = field->minimum.toDouble(&minOk);
            field->maxValue = field->maximum.toDouble(&maxOk);
            if (!minOk || !maxOk || field->minValue > field->maxValue)
            {
                if (error)
                    *error = QString::fromUtf8("字段 %1 的 min/max 无效").arg(label);
                return false;
            }
        }

        // bitIndex >= 0 表示该字段用于位提取
        if (field->bitIndex >= 0)
        {
            // 只有整数类型支持位提取
            if (!isIntegerType(field->dataType))
            {
                if (error)
                    *error = QString::fromUtf8("字段 %1 的类型不支持 bitIndex").arg(label);
                return false;
            }
            // data必须能解析为整数（除非包含变量占位符）
            if (!field->data.isEmpty() && !field->data.contains("${"))
            {
                bool dataOk = false;
                field->data.toULongLong(&dataOk, integerBase(field->dataType));
                if (!dataOk)
                {
                    if (error)
                        *error = QString::fromUtf8("字段 %1 的 data 不是可提取位的整数").arg(label);
                    return false;
                }
            }
        }
        return true;
    }
}
// namespace结束

/**
 * @brief 返回所有支持的协议字段类型列表
 * @return 支持的类型字符串列表
 */
QStringList supportedProtocolTypes()
{
    return QStringList()
           << "DEC" << "INT" << "UINT" << "BIN" << "OCT" << "HEX" << "FLT" << "DBL"
           << "STRING" << "FLAG" << "INT8" << "UINT8" << "INT16" << "UINT16" << "UINT32"
           << "IP";
}

/**
 * @brief 从XML文件加载协议定义
 * @param filePath XML文件路径
 * @return 加载是否成功
 *
 * 加载流程：
 * 1. 打开XML文件
 * 2. 解析XML文档
 * 3. 验证根节点
 * 4. 检测XML格式（紧凑位布局或传统属性格式）
 * 5. 解析协议级信息（IP、端口等）
 * 6. 逐个解析字段
 * 7. 验证字段合法性
 */
bool ProtocolParser::load(const QString &filePath)
{
    // 重置协议定义
    m_definition = ProtocolDefinition();

    // 打开文件
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        m_lastError = QString::fromUtf8("无法打开 XML：%1").arg(file.errorString());
        return false;
    }

    // 解析XML文档
    QDomDocument document;
    QString errorMessage;
    int errorLine = 0;
    int errorColumn = 0;
    if (!document.setContent(&file, &errorMessage, &errorLine, &errorColumn))
    {
        m_lastError = QString::fromUtf8("XML 解析失败：%1 (%2:%3)")
                          .arg(errorMessage)
                          .arg(errorLine)
                          .arg(errorColumn);
        return false;
    }

    // 验证根节点必须是<protocol>
    const QDomElement root = document.documentElement();
    if (root.tagName() != "protocol")
    {
        m_lastError = QString::fromUtf8("根节点必须是 protocol");
        return false;
    }

    // 至少需要一个field节点
    const QDomNodeList fieldNodes = root.elementsByTagName("field");
    if (fieldNodes.isEmpty())
    {
        m_lastError = QString::fromUtf8("协议中至少需要一个 field");
        return false;
    }

    ProtocolDefinition definition;

    // 检测XML格式：如果第一个field节点包含fieldName子节点，则为紧凑位布局格式
    definition.packedBitLayout = hasDirectChild(fieldNodes.at(0).toElement(), "fieldName");

    // 获取协议名称
    definition.name = root.attribute("name").trimmed();
    if (definition.name.isEmpty())
    {
        definition.name = definition.packedBitLayout
                              ? QFileInfo(filePath).completeBaseName() // 使用文件名
                              : QString("UnnamedProtocol");            // 默认名称
    }

    // 紧凑位布局格式的协议级信息解析
    if (definition.packedBitLayout)
    {
        // 解析源/目标IP、系统、协议头等
        definition.sourceIp = directChildText(root, "sourceIP");
        definition.destinationIp = directChildText(root, "destIP");
        definition.system = directChildText(root, "system");
        definition.metadata.insert("protoHead", directChildText(root, "protoHead"));

        // 验证IP地址
        quint32 parsedIp = 0;
        if ((!definition.sourceIp.isEmpty() && !parseIPv4(definition.sourceIp, &parsedIp)) ||
            (!definition.destinationIp.isEmpty() && !parseIPv4(definition.destinationIp, &parsedIp)))
        {
            m_lastError = QString::fromUtf8("sourceIP/destIP 无效");
            return false;
        }

        // 验证端口号
        if (!parseOptionalPort(directChildText(root, "sourcePort"), &definition.sourcePort) ||
            !parseOptionalPort(directChildText(root, "destPort"), &definition.destinationPort))
        {
            m_lastError = QString::fromUtf8("sourcePort/destPort 必须在 1 到 65535 之间");
            return false;
        }

        // 解析消息类型
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

    // 用于检测字段位区间重叠的列表
    QList<QPair<int, int>> occupiedRanges;

    // 遍历所有field节点
    for (int i = 0; i < fieldNodes.size(); ++i)
    {
        const QDomElement element = fieldNodes.at(i).toElement();
        ProtocolField field;

        if (definition.packedBitLayout)
        {
            // === 紧凑位布局格式解析 ===
            field.name = directChildText(element, "fieldName");
            field.type = directChildText(element, "datatype").toUpper();
            field.dataType = field.type;
            field.data = directChildText(element, "data");
            field.minimum = directChildText(element, "minimum");
            field.maximum = directChildText(element, "maximum");
            field.precision = directChildText(element, "precision");
            field.comment = directChildText(element, "comment");

            // 严格解析布尔值
            if (!parseCourseBool(directChildText(element, "isSelected"), true, &field.isSelected) ||
                !parseCourseBool(directChildText(element, "isKey"), false, &field.isKey))
            {
                m_lastError = QString::fromUtf8("字段 %1 的 isSelected/isKey 只能是 true 或 false")
                                  .arg(fieldLabel(field, i));
                return false;
            }

            // 解析位索引、长度、结束位
            bool bitIndexOk = false;
            bool lengthOk = false;
            bool endBitOk = false;
            field.bitIndex = directChildText(element, "bitIndex").toInt(&bitIndexOk);
            field.length = directChildText(element, "length").toInt(&lengthOk);
            field.endBit = directChildText(element, "loopEnd").toInt(&endBitOk);
            const QString label = fieldLabel(field, i);

            // 字段名不能为空
            if (field.name.isEmpty())
            {
                m_lastError = QString::fromUtf8("字段 %1 缺少 fieldName").arg(label);
                return false;
            }

            // 数据类型验证
            if (!supportedProtocolTypes().contains(field.dataType))
            {
                m_lastError = QString::fromUtf8("字段 %1 使用了不支持的类型：%2")
                                  .arg(label, field.dataType);
                return false;
            }

            // 位区间验证：loopEnd必须等于bitIndex + length
            if (!bitIndexOk || field.bitIndex < 0 || !lengthOk ||
                field.length <= 0 || !endBitOk || field.endBit != field.bitIndex + field.length)
            {
                m_lastError = QString::fromUtf8("字段 %1 必须满足 loopEnd = bitIndex + length").arg(label);
                return false;
            }

            // UDP最大载荷检查
            if (field.endBit > 65507 * 8)
            {
                m_lastError = QString::fromUtf8("字段 %1 超过 UDP 最大载荷").arg(label);
                return false;
            }

            // 长度规则验证
            QString validationError;
            if (!validateCourseLength(field, &validationError))
            {
                m_lastError = QString::fromUtf8("字段 %1：%2").arg(label, validationError);
                return false;
            }

            // 标识字段必须有data
            if (field.isKey && field.data.isEmpty())
            {
                m_lastError = QString::fromUtf8("字段 %1 的 isKey=true 时 data 必须有值").arg(label);
                return false;
            }

            // 范围验证
            if (!parseCourseRange(&field, &validationError))
            {
                m_lastError = QString::fromUtf8("字段 %1：%2").arg(label, validationError);
                return false;
            }

            // 位区间重叠检查
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
            // === 传统属性格式解析 ===
            field.name = element.attribute("name").trimmed();
            field.type = element.attribute("type").trimmed().toUpper();
            field.dataType = element.attribute("dataType", field.type).trimmed().toUpper();
            if (field.type.isEmpty())
                field.type = field.dataType;
            field.data = element.attribute("data");
            // IP类型默认范围为全地址空间
            field.minimum = element.attribute("min", field.dataType == "IP" ? "0.0.0.0" : "0");
            field.maximum = element.attribute("max", field.dataType == "IP" ? "255.255.255.255" : "100");
            field.precision = element.attribute("precision");

            // 解析长度和位索引
            bool lengthOk = false;
            bool bitIndexOk = false;
            field.length = element.attribute("length", "8").toInt(&lengthOk);
            field.bitIndex = element.attribute("bitIndex", "-1").toInt(&bitIndexOk);

            // 宽松解析布尔值
            field.isSelected = parseBool(element.attribute("isSelected"), true);
            field.isKey = parseBool(element.attribute("isKey"), false);
            field.loopEnd = parseBool(element.attribute("loopEnd"), false);

            // 验证字段
            if (!lengthOk || !bitIndexOk || !validateLegacyField(&field, i, &m_lastError))
            {
                if (m_lastError.isEmpty())
                    m_lastError = QString::fromUtf8("字段 %1 的 length/bitIndex 无效").arg(fieldLabel(field, i));
                return false;
            }
        }
        definition.fields.append(field);
    }

    // 保存解析结果
    m_definition = definition;
    m_lastError.clear();
    return true;
}

/**
 * @brief 获取最后一次错误信息
 * @return 错误信息字符串
 */
QString ProtocolParser::lastError() const
{
    return m_lastError;
}

/**
 * @brief 获取解析后的协议定义
 * @return 协议定义对象
 */
ProtocolDefinition ProtocolParser::definition() const
{
    return m_definition;
}