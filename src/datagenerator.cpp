#include "datagenerator.h"

#include <QDateTime>    // 用于获取时间戳和初始化随机种子
#include <QHostAddress> // 用于IP地址解析
#include <QStringList>  // Qt字符串列表
#include <QtGlobal>     // Qt全局定义，包含qMax、qMin等

#include <cstring> // C字符串操作，用于memcpy
#include <limits>  // C++标准库，用于获取数值类型的最大最小值

// 匿名命名空间：内部工具函数，仅在datagenerator.cpp中可见
namespace
{
    /**
     * @brief 获取字段的规范化数据类型
     * @param field 协议字段
     * @return 大写的数据类型字符串
     *
     * 优先使用dataType，如果为空则使用type
     */
    QString normalizedType(const ProtocolField &field)
    {
        return field.dataType.isEmpty()
                   ? field.type.trimmed().toUpper()
                   : field.dataType.trimmed().toUpper();
    }

    /**
     * @brief 根据数据类型返回对应的进制基数
     * @param type 数据类型字符串
     * @return 进制基数（2、8、10或16）
     */
    int integerBase(const QString &type)
    {
        if (type == "BIN" || type == "FLAG")
        {
            return 2; // 二进制
        }
        if (type == "OCT")
        {
            return 8; // 八进制
        }
        if (type == "HEX")
        {
            return 16; // 十六进制
        }
        return 10; // 默认十进制
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
     * @brief 判断是否为紧凑位布局中的整数类型（包括有符号和无符号）
     * @param type 数据类型字符串
     * @return 是否为整数类型
     */
    bool isPackedIntegerType(const QString &type)
    {
        return isSignedIntegerType(type) || type == "UINT" || type == "UINT8" || type == "UINT16" || type == "UINT32" || type == "DEC" || type == "BIN" || type == "OCT" || type == "HEX" || type == "FLAG";
    }

    /**
     * @brief 解析有符号整数字符串
     * @param text 输入字符串
     * @param value 输出参数，存储解析结果
     * @return 解析是否成功
     */
    bool parseSignedText(const QString &text, qint64 *value)
    {
        bool ok = false;
        const qint64 parsed = text.trimmed().toLongLong(&ok, 10); // 按十进制解析
        if (ok && value)
        {
            *value = parsed;
        }
        return ok;
    }

    /**
     * @brief 解析无符号整数字符串
     * @param text 输入字符串
     * @param type 数据类型（决定进制）
     * @param value 输出参数，存储解析结果
     * @return 解析是否成功
     */
    bool parseUnsignedText(const QString &text, const QString &type, quint64 *value)
    {
        bool ok = false;
        const quint64 parsed = text.trimmed().toULongLong(&ok, integerBase(type)); // 按指定进制解析
        if (ok && value)
        {
            *value = parsed;
        }
        return ok;
    }

    /**
     * @brief 解析IPv4地址字符串
     * @param text IPv4地址字符串
     * @param value 输出参数，存储32位无符号整数
     * @return 解析是否成功
     */
    bool parseIPv4(const QString &text, quint32 *value)
    {
        if (text.isEmpty())
        {
            return false; // 空字符串无法解析
        }

        QHostAddress address;
        if (!address.setAddress(text))
        {
            return false; // 地址格式无效
        }

        const quint32 ipv4 = address.toIPv4Address();
        // 特殊处理：如果解析结果为0但输入不是"0.0.0.0"，说明解析失败
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

    /**
     * @brief 计算指定位数能表示的最大无符号整数
     * @param bits 位数
     * @return 最大无符号整数值
     */
    quint64 maxUnsignedForBits(int bits)
    {
        if (bits >= 64)
        {
            return std::numeric_limits<quint64>::max(); // 64位及以上返回最大值
        }
        return (Q_UINT64_C(1) << bits) - 1U; // 2^n - 1
    }

    /**
     * @brief 设置缓冲区中指定位置的位值
     * @param buffer 字节缓冲区指针
     * @param bitIndex 位索引（从0开始，大端序）
     * @param value 要设置的位值（true=1，false=0）
     *
     * 位布局采用大端序（MSB first）：
     * - 字节0的第0位是整个缓冲区的最高位
     * - 位索引计算：byteIndex = bitIndex / 8，bitOffset = 7 - (bitIndex % 8)
     */
    void setBit(QByteArray *buffer, int bitIndex, bool value)
    {
        if (!buffer || bitIndex < 0)
        {
            return; // 无效参数
        }

        const int byteIndex = bitIndex / 8;
        if (byteIndex < 0 || byteIndex >= buffer->size())
        {
            return; // 超出缓冲区范围
        }

        const int bitOffset = 7 - (bitIndex % 8); // 大端序位偏移
        uchar byteValue = static_cast<uchar>(buffer->at(byteIndex));
        if (value)
        {
            // 设置位为1：使用按位或
            byteValue = static_cast<uchar>(byteValue | (1U << bitOffset));
        }
        else
        {
            // 设置位为0：使用按位与和取反
            byteValue = static_cast<uchar>(byteValue & ~(1U << bitOffset));
        }
        (*buffer)[byteIndex] = static_cast<char>(byteValue);
    }

    /**
     * @brief 将无符号整数按指定位数写入缓冲区的指定位置
     * @param buffer 字节缓冲区指针
     * @param bitIndex 起始位索引
     * @param length 要写入的位数
     * @param value 要写入的无符号整数值
     *
     * 按大端序从高位到低位逐位写入
     */
    void writeUnsignedBits(QByteArray *buffer, int bitIndex, int length, quint64 value)
    {
        for (int offset = 0; offset < length; ++offset)
        {
            const int shift = length - offset - 1; // 从最高位开始
            const bool bitValue = shift >= 64
                                      ? false                          // 超过64位的位设为0
                                      : ((value >> shift) & 1U) != 0U; // 提取对应位
            setBit(buffer, bitIndex + offset, bitValue);
        }
    }

    /**
     * @brief 将字节数组按字节对齐方式写入缓冲区
     * @param buffer 字节缓冲区指针
     * @param bitIndex 起始位索引
     * @param bytes 要写入的字节数组
     *
     * 要求bitIndex必须是8的倍数（字节对齐）
     */
    void writeByteAlignedBytes(QByteArray *buffer, int bitIndex, const QByteArray &bytes)
    {
        for (int byteIndex = 0; byteIndex < bytes.size(); ++byteIndex)
        {
            const uchar byteValue = static_cast<uchar>(bytes.at(byteIndex));
            for (int bit = 0; bit < 8; ++bit)
            {
                // 按大端序提取字节中的每一位
                const bool bitValue = ((byteValue >> (7 - bit)) & 1U) != 0U;
                setBit(buffer, bitIndex + byteIndex * 8 + bit, bitValue);
            }
        }
    }

    /**
     * @brief 将字节数组转换为十六进制显示格式
     * @param datagram 输入字节数组
     * @return 十六进制字符串（大写，空格分隔）
     */
    QByteArray displayHex(const QByteArray &datagram)
    {
        return datagram.toHex(' ').toUpper(); // 转换为十六进制并大写
    }
}

/**
 * @brief DataGenerator构造函数
 *
 * 使用当前时间的毫秒数初始化随机数种子
 */
DataGenerator::DataGenerator()
    : m_seed(static_cast<quint32>(QDateTime::currentMSecsSinceEpoch() & 0xffffffff))
{
}

/**
 * @brief 生成协议数据报
 * @param definition 协议定义
 * @return 生成的数据报（包含显示文本和原始字节）
 *
 * 根据协议格式选择生成方式：
 * - 紧凑位布局：生成二进制数据报
 * - 传统格式：生成文本数据
 */
GeneratedPayload DataGenerator::generate(const ProtocolDefinition &definition)
{
    GeneratedPayload payload;

    // 非紧凑位布局：生成文本格式
    if (!definition.packedBitLayout)
    {
        payload.displayText = generateTextPayload(definition);
        payload.datagram = payload.displayText.toUtf8(); // 文本转UTF-8字节
        return payload;
    }

    // 计算总位数：找出所有字段中最大的结束位
    int totalBits = 0;
    for (int i = 0; i < definition.fields.size(); ++i)
    {
        const ProtocolField &field = definition.fields.at(i);
        totalBits = qMax(totalBits, qMax(field.endBit, field.bitIndex + field.length));
    }

    // 分配缓冲区：位转字节（向上取整）
    payload.datagram = QByteArray((totalBits + 7) / 8, '\0');

    // 逐个字段填充数据
    for (int i = 0; i < definition.fields.size(); ++i)
    {
        const ProtocolField &field = definition.fields.at(i);
        const QString type = normalizedType(field);

        // 判断是否为固定值字段（标识字段且有数据）
        const bool fixedKeyValue = field.isKey && !field.data.isEmpty();

        // === 字符串类型处理 ===
        if (type == "STRING")
        {
            const int byteLength = field.length / 8; // 位转字节
            QByteArray bytes = fixedKeyValue
                                   ? field.data.toUtf8()                // 使用固定值
                                   : randomString(byteLength).toUtf8(); // 生成随机字符串
            bytes = bytes.left(byteLength);                             // 截断到指定长度
            while (bytes.size() < byteLength)
            {
                bytes.append('\0'); // 不足部分用空字符填充
            }
            writeByteAlignedBytes(&payload.datagram, field.bitIndex, bytes);
            continue;
        }

        // === IP地址类型处理 ===
        if (type == "IP")
        {
            const QString ipText = fixedKeyValue
                                       ? field.data                              // 使用固定值
                                       : randomIp(field.minimum, field.maximum); // 生成随机IP
            quint32 ipv4 = 0;
            if (parseIPv4(ipText, &ipv4))
            {
                // 将32位IP地址转换为4字节大端序
                QByteArray bytes(4, '\0');
                bytes[0] = static_cast<char>((ipv4 >> 24) & 0xffU); // 最高字节
                bytes[1] = static_cast<char>((ipv4 >> 16) & 0xffU);
                bytes[2] = static_cast<char>((ipv4 >> 8) & 0xffU);
                bytes[3] = static_cast<char>(ipv4 & 0xffU); // 最低字节
                writeByteAlignedBytes(&payload.datagram, field.bitIndex, bytes);
            }
            continue;
        }

        // === 单精度浮点数处理 ===
        if (type == "FLT")
        {
            bool fixedOk = false;
            // 生成0-1之间的随机比例
            const double ratio = static_cast<double>(boundedUnsigned(0, 1000000)) / 1000000.0;
            const double value = fixedKeyValue
                                     ? field.data.toDouble(&fixedOk)                               // 使用固定值
                                     : field.minValue + (field.maxValue - field.minValue) * ratio; // 范围内随机
            if (!fixedKeyValue || fixedOk)
            {
                quint64 rawValue = 0;
                // 将double的二进制表示复制到64位整数中
                std::memcpy(&rawValue, &value, sizeof(rawValue));
                writeUnsignedBits(&payload.datagram, field.bitIndex, 64, rawValue);
            }
            continue;
        }

        // === 双精度浮点数处理 ===
        if (type == "DBL")
        {
            bool fixedOk = false;
            const double ratio = static_cast<double>(boundedUnsigned(0, 1000000)) / 1000000.0;
            const double value = fixedKeyValue
                                     ? field.data.toDouble(&fixedOk)
                                     : field.minValue + (field.maxValue - field.minValue) * ratio;
            if (!fixedKeyValue || fixedOk)
            {
                quint64 rawValue = 0;
                std::memcpy(&rawValue, &value, sizeof(rawValue)); // 复制二进制表示
                writeUnsignedBits(&payload.datagram, field.bitIndex, 64, rawValue);
            }
            continue;
        }

        // === 有符号整数类型处理 ===
        if (isSignedIntegerType(type))
        {
            qint64 signedValue = 0;
            if (fixedKeyValue)
            {
                parseSignedText(field.data, &signedValue); // 解析固定值
            }
            else
            {
                signedValue = boundedSigned(field.minValue, field.maxValue); // 生成随机值
            }

            // 转换为无符号表示（二进制补码）
            const quint64 rawValue = field.length >= 64
                                         ? static_cast<quint64>(signedValue)
                                         : (static_cast<quint64>(signedValue) & maxUnsignedForBits(field.length));
            writeUnsignedBits(&payload.datagram, field.bitIndex, field.length, rawValue);
            continue;
        }

        // === 无符号整数类型处理 ===
        if (isPackedIntegerType(type))
        {
            quint64 unsignedValue = 0;
            if (fixedKeyValue)
            {
                parseUnsignedText(field.data, type, &unsignedValue); // 解析固定值
            }
            else
            {
                unsignedValue = boundedUnsigned(field.minValue, field.maxValue); // 生成随机值
            }

            // 截断到指定位数
            if (field.length < 64)
            {
                unsignedValue &= maxUnsignedForBits(field.length);
            }
            writeUnsignedBits(&payload.datagram, field.bitIndex, field.length, unsignedValue);
        }
    }

    // 生成显示文本
    payload.displayText = QString("HEX: %1").arg(QString::fromLatin1(displayHex(payload.datagram)));
    return payload;
}

/**
 * @brief 生成协议数据文本
 * @param definition 协议定义
 * @return 生成的文本
 *
 * 对于紧凑位布局，返回generate()的显示文本
 * 对于传统格式，返回generateTextPayload()的结果
 */
QString DataGenerator::generatePayload(const ProtocolDefinition &definition)
{
    if (definition.packedBitLayout)
    {
        return generate(definition).displayText;
    }
    return generateTextPayload(definition);
}

/**
 * @brief 获取支持的数据类型列表
 * @return 支持的类型字符串列表
 */
QStringList DataGenerator::supportedTypes() const
{
    return supportedProtocolTypes(); // 调用ProtocolParser中的函数
}

/**
 * @brief 生成文本格式的数据（传统属性格式）
 * @param definition 协议定义
 * @return 生成的文本
 *
 * 格式：字段名=字段值;字段名=字段值 || 字段名=字段值
 * - 分号分隔同一组内的字段
 * - loopEnd字段标记组的结束
 * - || 分隔不同的组
 */
QString DataGenerator::generateTextPayload(const ProtocolDefinition &definition)
{
    QStringList groups;       // 存储所有组
    QStringList currentGroup; // 当前正在构建的组

    for (int i = 0; i < definition.fields.size(); ++i)
    {
        const ProtocolField &field = definition.fields.at(i);

        // 跳过未选中的字段
        if (!field.isSelected)
        {
            continue;
        }

        // 添加字段到当前组：字段名=字段值
        currentGroup.append(QString("%1=%2").arg(field.name, generateFieldValue(field)));

        // 如果遇到loopEnd字段，结束当前组
        if (field.loopEnd)
        {
            groups.append(currentGroup.join(";")); // 组内字段用分号连接
            currentGroup.clear();                  // 清空当前组
        }
    }

    // 处理最后一个未结束的组
    if (!currentGroup.isEmpty())
    {
        groups.append(currentGroup.join(";"));
    }

    return groups.join(" || "); // 组之间用||连接
}

/**
 * @brief 生成单个字段的值
 * @param field 协议字段
 * @return 生成的字段值字符串
 *
 * 根据字段类型生成对应的随机值或固定值
 */
QString DataGenerator::generateFieldValue(const ProtocolField &field)
{
    const QString type = normalizedType(field);
    QString baseValue;

    // === 根据类型生成基础值 ===
    if (type == "DEC")
    {
        // 十进制无符号整数
        baseValue = QString::number(boundedUnsigned(field.minValue, field.maxValue));
    }
    else if (type == "INT")
    {
        // 有符号整数
        baseValue = QString::number(boundedSigned(field.minValue, field.maxValue));
    }
    else if (type == "UINT")
    {
        // 无符号整数
        baseValue = QString::number(boundedUnsigned(field.minValue, field.maxValue));
    }
    else if (type == "INT8")
    {
        // 8位有符号整数，范围[-128, 127]
        baseValue = QString::number(boundedSigned(qMax(-128.0, qMin(127.0, field.minValue)),
                                                  qMax(-128.0, qMin(127.0, field.maxValue))));
    }
    else if (type == "UINT8")
    {
        // 8位无符号整数，范围[0, 255]
        baseValue = QString::number(boundedUnsigned(qMax(0.0, qMin(255.0, field.minValue)),
                                                    qMax(0.0, qMin(255.0, field.maxValue))));
    }
    else if (type == "INT16")
    {
        // 16位有符号整数，范围[-32768, 32767]
        baseValue = QString::number(boundedSigned(qMax(-32768.0, qMin(32767.0, field.minValue)),
                                                  qMax(-32768.0, qMin(32767.0, field.maxValue))));
    }
    else if (type == "UINT16")
    {
        // 16位无符号整数，范围[0, 65535]
        baseValue = QString::number(boundedUnsigned(qMax(0.0, qMin(65535.0, field.minValue)),
                                                    qMax(0.0, qMin(65535.0, field.maxValue))));
    }
    else if (type == "UINT32")
    {
        // 32位无符号整数，范围[0, 4294967295]
        baseValue = QString::number(static_cast<qulonglong>(
            boundedUnsigned(qMax(0.0, qMin(4294967295.0, field.minValue)),
                            qMax(0.0, qMin(4294967295.0, field.maxValue)))));
    }
    else if (type == "BIN")
    {
        // 二进制格式
        baseValue = QString::number(static_cast<qulonglong>(boundedUnsigned(field.minValue, field.maxValue)), 2);
    }
    else if (type == "OCT")
    {
        // 八进制格式
        baseValue = QString::number(static_cast<qulonglong>(boundedUnsigned(field.minValue, field.maxValue)), 8);
    }
    else if (type == "HEX")
    {
        // 十六进制格式（大写）
        baseValue = QString::number(static_cast<qulonglong>(boundedUnsigned(field.minValue, field.maxValue)), 16)
                        .toUpper();
    }
    else if (type == "FLT")
    {
        // 单精度浮点数
        const bool precisionOk = !field.precision.isEmpty();
        const int digits = precisionOk ? qMax(0, field.precision.toInt()) : 3; // 默认3位小数
        const double span = field.maxValue - field.minValue;
        const double ratio = static_cast<double>(boundedUnsigned(0, 10000)) / 10000.0;
        baseValue = QString::number(field.minValue + span * ratio, 'f', digits);
    }
    else if (type == "DBL")
    {
        // 双精度浮点数
        const bool precisionOk = !field.precision.isEmpty();
        const int digits = precisionOk ? qMax(0, field.precision.toInt()) : 6; // 默认6位小数
        const double span = field.maxValue - field.minValue;
        const double ratio = static_cast<double>(boundedUnsigned(0, 100000)) / 100000.0;
        baseValue = QString::number(field.minValue + span * ratio, 'f', digits);
    }
    else if (type == "STRING")
    {
        // 字符串：生成随机字符串
        baseValue = randomString(field.length > 0 ? field.length : 8);
    }
    else if (type == "FLAG")
    {
        // 标志位：0或1
        baseValue = QString::number(boundedUnsigned(0, 1));
    }
    else if (type == "IP")
    {
        // IP地址
        baseValue = randomIp(field.minimum, field.maximum);
    }
    else
    {
        // 不支持的类型
        baseValue = "UNSUPPORTED";
    }

    // 如果字段有固定数据且不包含模板变量，使用固定值
    const bool literalData = !field.data.isEmpty() && !field.data.contains("${");
    if (literalData)
    {
        baseValue = field.data;
    }

    // 位提取：如果bitIndex >= 0，从整数值中提取特定位
    if (field.bitIndex >= 0)
    {
        bool ok = false;
        int base = integerBase(type);
        const qulonglong value = baseValue.toULongLong(&ok, base);
        if (ok)
        {
            // 右移bitIndex位并取最低位
            baseValue = QString::number((value >> field.bitIndex) & 1ULL);
        }
    }

    // 应用模板或返回基础值
    return literalData ? baseValue : applyTemplate(field, baseValue);
}

/**
 * @brief 应用模板替换
 * @param field 协议字段
 * @param baseValue 基础值
 * @return 替换后的字符串
 *
 * 支持的占位符：
 * - ${value}：基础值
 * - ${name}：字段名
 * - ${type}：数据类型
 * - ${timestamp}：当前时间戳（毫秒）
 */
QString DataGenerator::applyTemplate(const ProtocolField &field, const QString &baseValue)
{
    if (field.data.isEmpty())
    {
        return baseValue; // 无模板，直接返回基础值
    }

    QString output = field.data;
    output.replace("${value}", baseValue);                                                // 替换基础值
    output.replace("${name}", field.name);                                                // 替换字段名
    output.replace("${type}", field.dataType.isEmpty() ? field.type : field.dataType);    // 替换类型
    output.replace("${timestamp}", QString::number(QDateTime::currentMSecsSinceEpoch())); // 替换时间戳
    return output;
}

/**
 * @brief 生成随机字符串
 * @param length 字符串长度
 * @return 随机字符串（大写字母和数字）
 */
QString DataGenerator::randomString(int length)
{
    const QString alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"; // 字符集
    QString result;
    for (int i = 0; i < length; ++i)
    {
        // 随机选择字符集中的字符
        result.append(alphabet.at(static_cast<int>(boundedUnsigned(0, alphabet.size() - 1))));
    }
    return result;
}

/**
 * @brief 生成指定范围内的随机IP地址
 * @param minimum 最小IP地址字符串
 * @param maximum 最大IP地址字符串
 * @return 随机IP地址字符串
 */
QString DataGenerator::randomIp(const QString &minimum, const QString &maximum)
{
    quint32 minValue = 0;
    quint32 maxValue = 0;
    const bool minOk = parseIPv4(minimum, &minValue);
    const bool maxOk = parseIPv4(maximum, &maxValue);

    // 如果解析失败或范围无效，使用默认回环地址范围
    if (!minOk || !maxOk || minValue > maxValue)
    {
        minValue = QHostAddress(QString("127.0.0.1")).toIPv4Address();
        maxValue = QHostAddress(QString("127.255.255.254")).toIPv4Address();
    }

    // 在范围内生成随机值
    const quint32 value = static_cast<quint32>(boundedUnsigned(minValue, maxValue));

    // 转换为点分十进制格式
    return QString("%1.%2.%3.%4")
        .arg((value >> 24) & 0xffU) // 第一段
        .arg((value >> 16) & 0xffU) // 第二段
        .arg((value >> 8) & 0xffU)  // 第三段
        .arg(value & 0xffU);        // 第四段
}

/**
 * @brief 生成指定范围内的有符号随机整数
 * @param minValue 最小值（double类型）
 * @param maxValue 最大值（double类型）
 * @return 范围内的有符号随机整数
 *
 * 使用线性同余生成器（LCG）：seed = seed * 1664525 + 1013904223
 * 这是经典的Numerical Recipes LCG参数
 */
qint64 DataGenerator::boundedSigned(double minValue, double maxValue)
{
    qint64 minInt = static_cast<qint64>(minValue);
    qint64 maxInt = static_cast<qint64>(maxValue);
    if (maxInt < minInt)
    {
        qSwap(minInt, maxInt); // 确保minInt <= maxInt
    }

    const quint64 range = static_cast<quint64>(maxInt - minInt + 1);
    // 更新随机种子
    m_seed = m_seed * 1664525U + 1013904223U;
    return minInt + static_cast<qint64>(m_seed % (range == 0 ? 1 : range));
}

/**
 * @brief 生成指定范围内的无符号随机整数
 * @param minValue 最小值（double类型）
 * @param maxValue 最大值（double类型）
 * @return 范围内的无符号随机整数
 *
 * 使用线性同余生成器（LCG）：seed = seed * 22695477 + 1
 * 这是经典的32位LCG参数
 */
quint64 DataGenerator::boundedUnsigned(double minValue, double maxValue)
{
    quint64 minInt = static_cast<quint64>(qMax(0.0, minValue)); // 确保非负
    quint64 maxInt = static_cast<quint64>(qMax(0.0, maxValue));
    if (maxInt < minInt)
    {
        qSwap(minInt, maxInt); // 确保minInt <= maxInt
    }

    const quint64 range = maxInt - minInt + 1;
    // 更新随机种子
    m_seed = m_seed * 22695477U + 1U;
    return minInt + (range == 0 ? 0 : (m_seed % range));
}