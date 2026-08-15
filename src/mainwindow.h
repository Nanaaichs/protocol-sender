#ifndef MAINWINDOW_H
#define MAINWINDOW_H

// ============================================================
// 头文件保护（Header Guard）
//
// 作用：防止 mainwindow.h 被重复包含，导致类、函数等重复定义。
//
// 第一次包含本文件时：
//     MAINWINDOW_H 尚未定义，因此进入下面内容。
//     然后通过 #define MAINWINDOW_H 将其定义。
//
// 第二次再包含时：
//     MAINWINDOW_H 已经定义，#ifndef 条件不成立，
//     编译器就直接跳过整个文件。
// ============================================================


// ============================================================
// 引入本项目自己的三个模块
// ============================================================

#include "protocolparser.h"

// ProtocolParser：协议解析器。
// 负责读取 XML 协议文件，并把 XML 文本转换为程序内部可以使用的C++ 数据结构，例如 ProtocolDefinition。
//
// MainWindow 中有一个ProtocolParser m_parser;因此这里需要知道 ProtocolParser 类的完整定义。


#include "transmissionrepository.h"

// TransmissionRepository：发送日志/数据持久化模块。
// 项目中主要负责与 SQLite 数据库交互，例如：
//     1. 保存发送记录
//     2. 查询历史发送日志
//     3. 根据时间、协议、IP 等条件筛选记录
//
// MainWindow 中有一个：
//     TransmissionRepository m_repository;
// 所以需要包含它的头文件。


#include "udpsendcontroller.h"

// UdpSendController：UDP 发送控制器。
// 它通常位于 GUI 和底层发送逻辑之间，负责：
//     1. 根据协议定义生成数据
//     2. 按指定频率发送 UDP 数据
//     3. 控制发送次数
//     4. 停止发送
//     5. 汇报发送进度
//     6. 通知 MainWindow 当前生成的数据和最终状态
//
// MainWindow 中有：
//     UdpSendController m_controller;


// ============================================================
// Qt 主窗口基类
// ============================================================

#include <QMainWindow>

// QMainWindow 是 Qt 提供的标准“主窗口”类。
// 我们自己的 MainWindow 会继承 QMainWindow，
// 从而拥有窗口、标题栏、菜单栏、状态栏、centralWidget 等能力。


// ============================================================
// 前向声明（Forward Declaration）
// ============================================================
//
// 这里没有直接写：
//     #include <QLabel>
//     #include <QLineEdit>
//     ...
//
// 而只是告诉编译器：
//     “后面会存在一个叫 QLabel 的类。”
//
// 因为 MainWindow.h 中只保存这些类的“指针”：
//     QLabel *m_statusLabel;
//
// 编译器只需要知道 QLabel 是一个类型，
// 暂时不需要知道 QLabel 内部具体是什么结构。
//
// 这样可以减少头文件依赖，缩短编译时间。
// 真正使用 QLabel 的地方，一般在 mainwindow.cpp 中再
//     #include <QLabel>
// ============================================================

class QLabel;
class QLineEdit;
class QProgressBar;
class QTableWidget;
class QTextEdit;


// ============================================================
// MainWindow 主窗口类
// ============================================================

class MainWindow : public QMainWindow
{
    // --------------------------------------------------------
    // Q_OBJECT 是 Qt 的宏。
    //
    // 只要一个 QObject 派生类需要使用：
    //     signals
    //     slots
    //     Qt 元对象系统
    //
    // 一般就需要写 Q_OBJECT。
    //
    // Qt 的 moc（Meta-Object Compiler）会扫描这个宏，
    // 自动生成额外代码，让信号槽机制能够正常工作。
    // --------------------------------------------------------

    Q_OBJECT


public:

    // --------------------------------------------------------
    // MainWindow 构造函数
    // --------------------------------------------------------
    //
    // explicit：
    //     防止编译器进行不期望的隐式类型转换。
    //
    // QWidget *parent = 0：
    //     parent 表示这个窗口的父对象。
    //
    //     如果 parent == 0，
    //     说明 MainWindow 是一个顶层窗口。
    //
    // Qt5 旧代码经常使用 0，
    // 现代 C++ 更常写：
    //
    //     QWidget *parent = nullptr
    //
    // --------------------------------------------------------

    explicit MainWindow(QWidget *parent = 0);


private slots:

    // ========================================================
    // private slots：
    //
    // 这些函数本质上仍然是成员函数，
    // 但同时可以作为 Qt 信号槽机制中的“槽函数”。
    //
    // private：
    //     表示原则上只由 MainWindow 内部使用。
    //
    // 例如：
    //
    // QPushButton::clicked
    //          ↓
    // MainWindow::startSending()
    //
    // ========================================================


    void browseProtocol();

    // “浏览协议文件”按钮对应的槽函数。
    //
    // 用户点击“选择协议/XML”按钮之后，
    // 一般会调用 QFileDialog 打开文件选择窗口，
    // 让用户选择一个 XML 文件。
    //
    // 选择成功后通常会：
    //     1. 把路径写入 m_protocolPathEdit
    //     2. 调用 loadProtocol(path)
    //     3. 解析 XML


    void startSending();

    // 开始 UDP 发送。
    //
    // 通常负责从 GUI 中读取：
    //     m_protocolPathEdit → 协议路径
    //     m_frequencyEdit    → 发送频率
    //     m_countEdit        → 发送次数
    //     m_ipEdit           → 目标 IP
    //     m_portEdit         → 目标端口
    //
    // 然后检查输入是否合法，
    // 最终调用 m_controller 开始发送。


    void stopSending();

    // 停止当前 UDP 发送任务。
    //
    // 通常会调用类似：
    //
    //     m_controller.stop();
    //
    // 具体函数名取决于 UdpSendController 的实现。


    void searchLogs();

    // 查询历史发送日志。
    //
    // 通常会读取下面这些过滤条件：
    //     m_fromTimeFilterEdit / m_toTimeFilterEdit
    //     m_protocolFilterEdit
    //     m_ipFilterEdit
    //
    // 然后调用：
    //     m_repository
    //
    // 从 SQLite 查询日志，
    // 再调用 renderLogs() 显示到表格。


    void runBenchmark();

    // 执行性能基准测试 Benchmark。
    //
    // 用来测试程序自身 UDP 数据生成和发送能力，
    // 例如：
    //     吞吐率
    //     发送速率
    //     丢包率
    //     异常包
    //     时间消耗
    //
    // 它和正常“给 Packet Sender 发数据”不是完全同一件事情。
    // Benchmark 主要是程序自己的性能自检。


    void updateProgress(int sentCount, int totalCount);

    // 更新发送进度。
    //
    // 参数：
    //
    //     sentCount
    //         已经发送的数据包数量
    //
    //     totalCount
    //         总共需要发送的数据包数量
    //
    // 例如：
    //
    //     sentCount = 30
    //     totalCount = 100
    //
    // 可以在进度条中显示 30%。
    //
    // 这个槽通常由 UdpSendController 的信号触发：
    //
    //     progressChanged(int, int)
    //
    //          ↓
    //
    //     updateProgress(int, int)


    void appendPayload(const QString &payload);

    // 将当前生成/发送的数据追加到预览窗口。
    //
    // payload：
    //     当前生成的数据内容，例如 HEX 字符串、协议数据等。
    //
    // 最终通常显示到：
    //
    //     m_previewText
    //
    // 也就是 QTextEdit 控件中。
    //
    // 常见连接关系：
    //
    // UdpSendController
    //     messageGenerated(QString)
    //             ↓
    // MainWindow
    //     appendPayload(QString)


    void finishRun(const QString &message);

    // 一次发送任务结束后的处理函数。
    //
    // message 可能是：
    //     "发送完成"
    //     "用户停止发送"
    //     "发送失败：..."
    //
    // 通常用于：
    //     1. 更新状态栏
    //     2. 修改 m_statusLabel
    //     3. 恢复按钮状态
    //     4. 刷新日志
    //
    // 一般由 UdpSendController 的：
    //
    //     runFinished(QString)
    //
    // 信号触发。



private:

    // ========================================================
    // private：
    //
    // 以下成员只能由 MainWindow 自己直接访问。
    //
    // 主要分成两类：
    //
    //     ① 私有辅助函数
    //     ② MainWindow 持有的对象和 GUI 控件
    //
    // ========================================================


    void buildUi();

    // 创建和组织整个 GUI。
    //
    // 由于当前项目没有完全依赖 Qt Designer 的 .ui 文件，
    // 所以很多界面控件是直接用 C++ 创建的。
    //
    // 例如：
    //
    //     m_statusLabel = new QLabel(...);
    //     m_ipEdit = new QLineEdit(...);
    //     QPushButton *button = new QPushButton(...);
    //
    // 然后通过：
    //
    //     QVBoxLayout
    //     QFormLayout
    //     QHBoxLayout
    //
    // 等布局管理器组织控件。
    //
    // 同时 connect() 往往也在这里完成：
    //
    //     点击按钮
    //          ↓
    //     对应槽函数


    bool loadProtocol(const QString &path);

    // 加载并解析一个协议 XML 文件。
    //
    // 参数：
    //
    //     path
    //         XML 文件路径
    //
    // 返回值：
    //
    //     true
    //         XML 加载和解析成功
    //
    //     false
    //         XML 文件不存在、格式错误、协议不合法等
    //
    // 内部通常调用：
    //
    //     m_parser
    //
    // 即 ProtocolParser。


    void renderLogs(const QList<TransmissionRunEntry> &entries);

    // 把数据库查询得到的日志显示在 GUI 表格中。
    //
    // entries：
    //
    //     QList<TransmissionRunEntry>
    //
    // 可以理解成：
    //
    //     一组发送日志记录。
    //
    // 例如：
    //
    // entries
    // ├── 第1条日志
    // ├── 第2条日志
    // ├── 第3条日志
    // └── ...
    //
    // renderLogs() 会把它们逐行写进：
    //
    //     m_logTable
    //
    // 即 QTableWidget。



    // ========================================================
    // 第一组：核心业务对象
    // ========================================================

    ProtocolParser m_parser;

    // 协议解析器对象。
    //
    // 负责：
    //
    //     XML
    //      ↓
    //     ProtocolDefinition
    //
    // 它不是 GUI 控件，而是业务逻辑对象。


    TransmissionRepository m_repository;

    // 数据仓库/数据库访问对象。
    //
    // Repository 是一种常见的软件设计思想：
    //
    //     MainWindow
    //         不直接写 SQL
    //
    //         ↓
    //
    //     TransmissionRepository
    //
    //         ↓
    //
    //     SQLite
    //
    // MainWindow 只告诉 repository：
    //
    //     “帮我保存这条发送记录”
    //     “帮我查一下这些日志”
    //
    // 至于 SQL 怎么写，由 Repository 自己处理。


    UdpSendController m_controller;

    // UDP 发送控制器。
    //
    // Controller 可以理解为：
    //
    //     GUI 和底层业务逻辑之间的协调者。
    //
    // MainWindow：
    //     管“用户想做什么”
    //
    // Controller：
    //     管“发送过程怎么执行”
    //
    // 典型关系：
    //
    //     用户点击开始
    //          ↓
    //     MainWindow::startSending()
    //          ↓
    //     m_controller
    //          ↓
    //     数据生成
    //          ↓
    //     UDP发送



    // ========================================================
    // 第二组：状态显示控件
    // ========================================================

    QLabel *m_statusLabel;

    // 状态标签。
    //
    // 可能显示：
    //
    //     “就绪”
    //     “正在发送”
    //     “发送完成”
    //     “协议加载失败”
    //
    // QLabel 是显示简单文本最常用的 Qt 控件。



    // ========================================================
    // 第三组：发送参数输入区域
    // ========================================================

    QLineEdit *m_protocolPathEdit;

    // XML 协议文件路径输入框。
    //
    // 示例：
    //
    // C:/protocol/test.xml
    //
    // 或 Qt Resource：
    //
    // :/data/sample_protocol.xml


    QLineEdit *m_frequencyEdit;

    // 发送频率输入框。
    //
    // 例如：
    //
    //     10
    //     100
    //     1000
    //
    // 至于单位是 Hz、包/秒还是其他，
    // 要看 UdpSendController 中具体定义。


    QLineEdit *m_countEdit;

    // 总发送次数。
    //
    // 例如：
    //
    //     100
    //
    // 表示发送100个数据包。
    //
    // 某些项目也可能约定：
    //
    //     空字符串 / 0
    //
    // 表示持续发送。


    QLineEdit *m_ipEdit;

    // 目标 IP 地址输入框。
    //
    // 例如本机测试：
    //
    //     127.0.0.1
    //
    // 或局域网其他计算机：
    //
    //     192.168.1.100


    QLineEdit *m_portEdit;

    // UDP 目标端口输入框。
    //
    // 例如：
    //
    //     5000
    //     8080
    //     12345
    //
    // UDP/TCP 端口有效范围通常是：
    //
    //     1 ~ 65535



    // ========================================================
    // 第四组：发送过程显示
    // ========================================================

    QProgressBar *m_progressBar;

    // Qt 进度条。
    //
    // 用于显示：
    //
    //     已发送数量 / 总数量
    //
    // 例如：
    //
    //     53 / 100
    //
    // GUI 上可以表现为：
    //
    //     [==========          ] 53%


    QTextEdit *m_previewText;

    // 多行文本框。
    //
    // 用于显示已经生成或已经发送的数据内容。
    //
    // 与 QLineEdit 的区别：
    //
    //     QLineEdit
    //         单行文本输入
    //
    //     QTextEdit
    //         多行文本显示/编辑
    //
    // 所以它适合做“数据包预览区”。



    // ========================================================
    // 第五组：日志筛选条件
    // ========================================================

    QLineEdit *m_fromTimeFilterEdit;
    QLineEdit *m_toTimeFilterEdit;

    // 按时间筛选发送日志的输入框。


    QLineEdit *m_protocolFilterEdit;

    // 按协议名称筛选日志。
    //
    // 例如：
    //
    //     ProtocolA
    //     telemetry
    //     xxx


    QLineEdit *m_ipFilterEdit;

    // 按目标 IP 地址筛选历史发送记录。



    // ========================================================
    // 第六组：日志显示表格
    // ========================================================

    QTableWidget *m_logTable;

    // 表格控件，用于展示发送历史。
    //
    // 例如可能显示：
    //
    // 时间      协议      IP       端口      数量     状态
    // -----------------------------------------------------
    // 10:21    P1    127.0.0.1    5000      100     完成
    // 10:25    P2    192.168...   8000       50     完成
    //
    // 数据通常由：
    //
    //     m_repository
    //          ↓
    //     searchLogs()
    //          ↓
    //     renderLogs()
    //          ↓
    //     m_logTable
    //
    // 显示出来。
};


// ============================================================
// 与文件开头：
//
//     #ifndef MAINWINDOW_H
//     #define MAINWINDOW_H
//
// 配套。
// ============================================================

#endif
