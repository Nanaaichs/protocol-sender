#ifndef DATAGENERATOR_H
#define DATAGENERATOR_H

// ============================================================
// 头文件保护
//
// 防止 datagenerator.h 被重复包含。
// ============================================================



#include "protocolparser.h"

// DataGenerator 需要使用：
//
//     ProtocolDefinition
//     ProtocolField
//
// 这两个类型都定义在 protocolparser.h 中。
//
// 后面你会看到：
//
//     generatePayload(const ProtocolDefinition &definition)
//
//     generateFieldValue(const ProtocolField &field)
//
// 所以这里必须让编译器知道
// ProtocolDefinition 和 ProtocolField 是什么。

#include <QString>

// Qt 字符串类型。
//
// DataGenerator 的核心输出就是 QString：
//
//     QString generatePayload(...);
//
// 字段生成结果也都是 QString。



// ============================================================
// DataGenerator 类
// ============================================================

class DataGenerator
{
public:

    // ========================================================
    // 构造函数
    // ========================================================

    DataGenerator();

    // 创建 DataGenerator 对象时自动调用。
    //
    // 主要可能用于初始化：
    //
    //     m_seed
    //
    // 即随机数种子。
    //
    // 例如：
    //
    //     DataGenerator generator;
    //
    // 创建 generator 时就会自动调用：
    //
    //     DataGenerator::DataGenerator()



    // ========================================================
    // 生成完整 Payload
    // ========================================================

    QString generatePayload(
        const ProtocolDefinition &definition
    );

    // 这是 DataGenerator 最核心的 public 函数。
    //
    // 输入：
    //
    //     const ProtocolDefinition &definition
    //
    // 即已经由 ProtocolParser 解析好的整个协议定义。
    //
    //
    // 输出：
    //
    //     QString
    //
    // 即按照协议生成好的完整 Payload。
    //
    //
    // 例如：
    //
    // definition.fields 里面有：
    //
    //     field_1 → HEX
    //     field_2 → BIN
    //     field_3 → STRING
    //
    // DataGenerator 可能逐个处理：
    //
    // field_1
    //    ↓
    // "1234"
    //
    // field_2
    //    ↓
    // "00101101"
    //
    // field_3
    //    ↓
    // "ABCD"
    //
    // 最终组合：
    //
    //     "123400101101ABCD"
    //
    // 返回这个 QString。
    //
    //
    // const ProtocolDefinition & 的含义：
    //
    //     &     → 引用传递，避免复制整个协议定义
    //
    //     const → DataGenerator 不允许修改传入的 definition



    // ========================================================
    // 返回 DataGenerator 支持的数据类型
    // ========================================================

    QStringList supportedTypes() const;

    // 输入：无
    //
    // 输出：
    //
    //     QStringList
    //
    // 即一组 DataGenerator 支持的类型名称。
    //
    // 例如可能是：
    //
    //     ["HEX", "BIN", "STRING", "IP", "FLAG"]
    //
    //
    // 最后的 const：
    //
    //     supportedTypes() const
    //
    // 表示调用这个函数不会修改 DataGenerator 对象内部状态。
    //
    //
    // 注意：
    // 当前文件没有直接：
    //
    //     #include <QStringList>
    //
    // 但 protocolparser.h 本身包含了 QStringList，
    // 所以它可能仍然能编译。
    //
    // 更规范的写法是：
    //
    //     #include <QStringList>
    //
    // 因为 datagenerator.h 自己直接使用了 QStringList。



private:

    // ========================================================
    // private
    //
    // 以下函数只供 DataGenerator 内部使用。
    //
    // 外部不能：
    //
    //     generator.generateFieldValue(...)
    //
    // 外部只需要：
    //
    //     generatePayload(...)
    //
    // 至于每个字段具体怎么生成，
    // 被封装在 DataGenerator 内部。
    // ========================================================



    // ========================================================
    // 生成“单个字段”的值
    // ========================================================

    QString generateFieldValue(
        const ProtocolField &field
    );

    // 输入：
    //
    //     一个 ProtocolField
    //
    // 输出：
    //
    //     这个字段对应的 QString 数据。
    //
    //
    // 例如：
    //
    // field.dataType = "HEX"
    // field.minimum  = "0000"
    // field.maximum  = "1FFF"
    //
    // 那么：
    //
    // generateFieldValue(field)
    //
    // 可能生成：
    //
    //     "0A3F"
    //
    //
    // 又比如：
    //
    // field.dataType = "STRING"
    //
    // 可能返回：
    //
    //     "ABCD"
    //
    //
    // 所以关系是：
    //
    // ProtocolField
    //      ↓
    // generateFieldValue()
    //      ↓
    // 一个字段的数据



    // ========================================================
    // 应用字段模板
    // ========================================================

    QString applyTemplate(
        const ProtocolField &field,
        const QString &baseValue
    );

    // 这个函数用于处理类似：
    //
    //     MODE-${value}
    //
    //     TRACE-${timestamp}
    //
    // 这样的模板字符串。
    //
    //
    // 输入1：
    //
    //     field
    //
    // 当前字段的定义。
    //
    //
    // 输入2：
    //
    //     baseValue
    //
    // 已经生成的基础值。
    //
    //
    // 例如：
    //
    // field.data = "MODE-${value}"
    //
    // baseValue = "3"
    //
    // 那么：
    //
    // applyTemplate(...)
    //
    // 可能返回：
    //
    //     "MODE-3"
    //
    //
    // 如果模板中有：
    //
    //     ${timestamp}
    //
    // 可能替换成当前时间戳。
    //
    //
    // 这个函数明显是为了你之前那份
    // LoopbackDemo XML 中的模板设计服务的。
    //
    // 但要注意：
    // 你后来给出的课程标准 XML 中并没有明显出现
    // ${value}、${timestamp} 这种模板规则。
    //
    // 所以这个功能是否符合题目要求，
    // 需要重新核查。



    // ========================================================
    // 生成随机字符串
    // ========================================================

    QString randomString(int length);

    // 输入：
    //
    //     length
    //
    // 表示要生成多少长度的字符串。
    //
    // 例如：
    //
    //     randomString(4)
    //
    // 可能得到：
    //
    //     "ABCD"
    //
    // 或：
    //
    //     "X7K2"
    //
    //
    // 主要供 STRING 类型字段使用。



    // ========================================================
    // 生成有符号随机整数
    // ========================================================

    qint64 boundedSigned(
        double minValue,
        double maxValue
    );

    // 输入：
    //
    //     minValue
    //     maxValue
    //
    // 输出：
    //
    //     qint64
    //
    // 即 Qt 定义的“64位有符号整数”。
    //
    //
    // qint64 的概念大致就是：
    //
    //     signed 64-bit integer
    //
    // 范围大约：
    //
    // -9.22×10^18
    // 到
    // +9.22×10^18
    //
    //
    // boundedSigned 的意思可以理解为：
    //
    //     在给定范围内产生一个有符号整数。
    //
    // 例如：
    //
    //     boundedSigned(-100, 100)
    //
    // 可能返回：
    //
    //     -27
    //
    //     46
    //
    //     0
    //
    // 等。



    // ========================================================
    // 生成无符号随机整数
    // ========================================================

    quint64 boundedUnsigned(
        double minValue,
        double maxValue
    );

    // 和 boundedSigned 类似，
    // 但是返回的是：
    //
    //     quint64
    //
    // 即“64位无符号整数”。
    //
    // 无符号表示没有负数。
    //
    // 例如：
    //
    //     boundedUnsigned(0, 65535)
    //
    // 可能返回：
    //
    //     42315
    //
    //
    // 一般可能用于：
    //
    //     UINT8
    //     UINT16
    //     UINT32
    //     HEX
    //
    // 等非负数据类型。



    // ========================================================
    // 随机数种子
    // ========================================================

    quint32 m_seed;

    // quint32：
    //
    //     Qt 定义的32位无符号整数。
    //
    // m_seed：
    //
    //     保存随机数生成所需要的种子。
    //
    // 随机数生成器通常需要一个“初始状态”。
    //
    // 例如：
    //
    //     seed = 12345
    //
    // 然后根据这个 seed
    // 产生一系列伪随机数。
    //
    //
    // 它不是“真正完全随机”，
    // 而是根据 seed 算出一串看起来随机的结果。
};

#endif