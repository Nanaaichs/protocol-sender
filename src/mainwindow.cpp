#include "mainwindow.h"

// ---------------- Qt 核心与基础工具 ----------------
#include <QCoreApplication> // Qt 核心应用支持，这份代码中实际上没有直接使用
#include <QDateTime>
#include <QDir>             // 路径、目录处理
#include <QFileDialog>      // 文件选择对话框
#include <QFormLayout>      // 表单布局：一行通常是“标签 + 输入控件”
#include <QHeaderView>      // 用于控制表格表头及列宽
#include <QIntValidator>    // 限制 QLineEdit 只能输入指定范围内的整数
#include <QLabel>           // 文本标签
#include <QLineEdit>        // 单行文本输入框
#include <QMessageBox>      // 消息提示框，例如 warning、information
#include <QProgressBar>     // 进度条
#include <QPushButton>      // 按钮
#include <QStandardPaths>   // 获取系统标准路径，例如应用程序数据目录
#include <QTableWidget>     // 表格控件
#include <QTextEdit>        // 多行文本显示/编辑控件
#include <QTextDocument>    // QTextEdit 内部对应的文本对象
#include <QVBoxLayout>      // 垂直布局

// ============================================================
// MainWindow 构造函数
// ============================================================
// 当 main.cpp 中执行：
//
//     MainWindow window;
//
// 时，就会进入这个构造函数。
//
// 主要完成四件事：
// 1. 初始化 MainWindow 本身以及成员变量
// 2. 创建整个界面
// 3. 建立 Controller 和 MainWindow 之间的信号槽连接
// 4. 默认加载示例协议并查询已有日志
// ============================================================
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)

      // 初始化传输日志数据库 repository
      //
      // QStandardPaths::AppDataLocation：
      // 获取当前操作系统为本程序分配的“应用数据目录”。
      //
      // 例如 Windows 上通常类似：
      // C:/Users/用户名/AppData/Roaming/组织名/程序名/
      //
      // QDir(...).absoluteFilePath("protocol_sender.db")
      // 则在该目录下生成数据库文件完整路径。
      //
      // 最终类似：
      // C:/Users/.../AppData/Roaming/.../protocol_sender.db
      //
      // "protocol_sender_main"：
      // 一般作为数据库连接名称，用于区分不同数据库连接。
      ,
      m_repository(
          QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation))
              .absoluteFilePath("protocol_sender.db"),
          "protocol_sender_main")

      // 初始化发送控制器。
      //
      // 这里把数据库对象 m_repository 的地址传给控制器，
      // 表示 controller 在发送数据时可以把发送记录写入数据库。
      //
      // this 作为 QObject 父对象：
      // MainWindow 被销毁时，Qt 会自动销毁 m_controller 所管理的相关资源。
      ,
      m_controller(&m_repository, this)

      // 状态标签初始设为空指针。
      //
      // 后面 buildUi() 中会：
      // m_statusLabel = new QLabel(this);
      //
      // 也就是说真正的 QLabel 对象稍后才创建。
      ,
      m_statusLabel(0),
      m_lastProgressSentCount(0)
{
    // 创建整个窗口界面。
    //
    // 包括：
    // - 协议路径输入框
    // - 开始/停止按钮
    // - IP、端口、频率输入框
    // - 进度条
    // - 发送内容预览框
    // - 日志查询区域
    // - 日志表格
    // - 状态栏文字
    buildUi();

    // --------------------------------------------------------
    // 建立 Controller -> MainWindow 的信号槽连接
    // --------------------------------------------------------

    // 当控制器发送：
    //
    // progressChanged(sentCount, totalCount)
    //
    // 信号时，调用 MainWindow::updateProgress()
    //
    // 用于更新发送进度条。
    connect(
        &m_controller,
        SIGNAL(progressChanged(int, int)),
        this,
        SLOT(updateProgress(int, int)));

    // 当控制器生成一条发送载荷时：
    //
    // messageGenerated(QString)
    //
    // 调用 appendPayload(QString)
    //
    // 将该条发送内容追加到界面中的文本框。
    connect(
        &m_controller,
        SIGNAL(messageGenerated(QString)),
        this,
        SLOT(appendPayload(QString)));

    // 当一次发送任务结束时：
    //
    // runFinished(QString)
    //
    // 调用 finishRun(QString)
    //
    // 用于：
    // - 更新状态文字
    // - 刷新发送日志
    connect(
        &m_controller,
        SIGNAL(runFinished(QString)),
        this,
        SLOT(finishRun(QString)));

    // --------------------------------------------------------
    // 默认加载示例协议
    // --------------------------------------------------------

    // ":/data/sample_protocol.xml"
    //
    // 注意这个不是普通磁盘路径。
    //
    // 以 ":/" 开头的是 Qt Resource System 资源路径。
    //
    // 也就是说 sample_protocol.xml 被编译进程序内部，
    // 一般在 .qrc 文件中定义。
    const QString samplePath = ":/data/sample_protocol.xml";

    // 将默认协议路径显示到输入框里。
    m_protocolPathEdit->setText(samplePath);

    // 真正读取并解析协议 XML。
    loadProtocol(samplePath);

    // 程序启动时查询一次数据库，
    // 将已有发送日志显示到表格。
    searchLogs();
}

// ============================================================
// browseProtocol()
// ============================================================
// 当用户点击“选择协议”按钮时执行。
// 作用：弹出文件选择窗口，让用户选择一个 XML 协议文件。
// ============================================================
void MainWindow::browseProtocol()
{
    // 弹出文件选择对话框。
    //
    // 参数1：this
    // 表示该对话框属于 MainWindow。
    //
    // 参数2：
    // 对话框标题。
    //
    // 参数3：
    // 初始目录，这里 QString() 表示使用默认目录。
    //
    // 参数4：
    // 课程样例可能没有 .xml 扩展名，因此同时允许选择所有文件。
    const QString filePath =
        QFileDialog::getOpenFileName(
            this,
            QString::fromUtf8("选择协议 XML"),
            QString(),
            QString::fromUtf8("协议文件 (*.xml);;所有文件 (*)"));

    // 如果用户点击“取消”，filePath 会为空。
    //
    // 如果用户真正选择了文件，才继续执行。
    if (!filePath.isEmpty())
    {

        // 将选择的文件路径显示在路径输入框中。
        m_protocolPathEdit->setText(filePath);

        // 加载并解析新协议。
        if (loadProtocol(filePath))
        {
            // 课程协议在 XML 中提供默认目标地址和端口。
            // 仅在用户选择文件时应用；开始发送前的重载不覆盖用户后续手工修改。
            const ProtocolDefinition definition = m_parser.definition();
            if (!definition.destinationIp.isEmpty())
            {
                m_ipEdit->setText(definition.destinationIp);
            }
            if (definition.destinationPort > 0)
            {
                m_portEdit->setText(QString::number(definition.destinationPort));
            }
        }
    }
}

// ============================================================
// startSending()
// ============================================================
// 用户点击“开始发送”后执行。
// 这是整个发送流程的重要入口。
// ============================================================
void MainWindow::startSending()
{
    // --------------------------------------------------------
    // 第一步：重新加载协议
    // --------------------------------------------------------
    //
    // 即使之前已经加载过，
    // 开始发送前仍重新读取当前输入框中的协议路径。
    //
    // trim() / trimmed()
    // 用于去掉字符串前后的空格。
    //
    // 如果协议加载失败，则直接退出 startSending()。
    if (!loadProtocol(m_protocolPathEdit->text().trimmed()))
    {
        return;
    }

    // --------------------------------------------------------
    // 第二步：读取“发送数量”
    // --------------------------------------------------------

    const QString countText = m_countEdit->text().trimmed();

    // 如果数量输入框为空，
    // 认为这是合法情况。
    //
    // 因为你的界面定义：
    //
    // “数量(留空持续发送)”
    //
    // 所以留空本身就是一种合法输入。
    bool countOk = countText.isEmpty();

    // frequencyOk：
    // 后面用于判断频率字符串是否成功转换成整数。
    bool frequencyOk = false;

    // portOk：
    // 后面用于判断端口字符串是否成功转换成整数。
    bool portOk = false;

    // 如果 countText 为空：
    // count = 0
    //
    // 你的程序约定：
    // count <= 0 表示持续发送。
    //
    // 如果不为空：
    // 使用 toInt() 转换，并将转换是否成功写入 countOk。
    const int count =
        countText.isEmpty()
            ? 0
            : countText.toInt(&countOk);

    // 将频率文本转换为 int。
    //
    // 如果转换成功：
    // frequencyOk = true
    //
    // 否则：
    // frequencyOk = false
    const int frequency =
        m_frequencyEdit->text().toInt(&frequencyOk);

    // 将端口转换成无符号整数。
    const uint port =
        m_portEdit->text().toUInt(&portOk);

    // --------------------------------------------------------
    // 第三步：检查参数是否合法
    // --------------------------------------------------------
    //
    // 以下任意一种情况都算非法：
    //
    // 1. count 转换失败
    // 2. frequency 转换失败
    // 3. port 转换失败
    // 4. port == 0
    // 5. port > 65535
    //
    // UDP/TCP 端口合法范围：
    // 1 ~ 65535
    if (!countOk || !frequencyOk || !portOk || port == 0 || port > 65535)
    {
        // 弹出警告对话框。
        QMessageBox::warning(
            this,
            QString::fromUtf8("参数无效"),
            QString::fromUtf8(
                "请检查频率、发送数量和端口；"
                "数量可以留空，但不能填写非数字或负数。"));

        return;
    }

    // --------------------------------------------------------
    // 第四步：启动控制器
    // --------------------------------------------------------
    //
    // 将以下信息交给 Controller：
    //
    // IP
    // 端口
    // 发送频率
    // 发送数量
    //
    // controller.start() 返回 bool：
    //
    // true  -> 启动成功
    // false -> 启动失败
    if (!m_controller.start(
            m_ipEdit->text().trimmed(),
            static_cast<quint16>(port),
            frequency,
            count))
    {
        // 如果 Controller 启动失败，
        // 显示它提供的错误信息。
        QMessageBox::warning(
            this,
            QString::fromUtf8("启动失败"),
            m_controller.lastError());

        return;
    }

    // --------------------------------------------------------
    // 第五步：设置进度条
    // --------------------------------------------------------
    //
    // 如果 count <= 0：
    //
    // setMaximum(0)
    //
    // Qt 会将 QProgressBar 变成“不确定进度模式”，
    // 也就是类似来回滚动的 busy indicator。
    //
    // 因为持续发送时没有总数量，
    // 所以不能计算百分比。
    //
    // 如果 count > 0：
    // 最大值就是总发送数量。
    m_progressBar->setMaximum(
        count <= 0 ? 0 : count);

    // 发送任务开始时进度清零。
    m_lastProgressSentCount = 0;
    m_progressBar->setFormat("%p%");
    m_progressBar->setValue(0);

    // 状态文本更新为“发送中...”。
    m_statusLabel->setText(
        QString::fromUtf8("发送中..."));
}

// ============================================================
// stopSending()
// ============================================================
// 用户点击“停止发送”后调用。
// ============================================================
void MainWindow::stopSending()
{
    // MainWindow 本身并不直接停止定时器、socket 等。
    //
    // 它只告诉 Controller：
    //
    // “停止当前发送任务。”
    //
    // 真正的停止逻辑封装在 m_controller 中。
    m_controller.stop();
}

// ============================================================
// searchLogs()
// ============================================================
// 根据界面输入的查询条件搜索历史发送日志。
// ============================================================
void MainWindow::searchLogs()
{
    const QString fromText = m_fromTimeFilterEdit->text().trimmed();
    const QString toText = m_toTimeFilterEdit->text().trimmed();
    const QString timeFormat = "yyyy-MM-dd HH:mm:ss";
    const QDateTime fromDateTime = QDateTime::fromString(fromText, timeFormat);
    const QDateTime toDateTime = QDateTime::fromString(toText, timeFormat);
    if ((!fromText.isEmpty() && !fromDateTime.isValid())
        || (!toText.isEmpty() && !toDateTime.isValid())
        || (fromDateTime.isValid() && toDateTime.isValid() && fromDateTime > toDateTime))
    {
        QMessageBox::warning(this,
                             QString::fromUtf8("时间范围无效"),
                             QString::fromUtf8("请使用 yyyy-MM-dd HH:mm:ss；开始时间不能晚于结束时间。"));
        return;
    }

    const QString normalizedFrom = fromDateTime.isValid()
        ? fromDateTime.toString(timeFormat) + ".000"
        : QString();
    const QString normalizedTo = toDateTime.isValid()
        ? toDateTime.toString(timeFormat) + ".999"
        : QString();

    // 查询与指定时间段相交的发送任务，协议和 IP 使用模糊匹配。
    //
    // 查询条件：
    //
    // 1. 时间
    // 2. 协议名称
    // 3. IP
    //
    // search() 返回：
    //
    // QList<TransmissionRunEntry>
    //
    // 然后直接交给 renderLogs()，
    // 将查询结果显示到表格。
    renderLogs(
        m_repository.searchRuns(
            normalizedFrom,
            normalizedTo,
            m_protocolFilterEdit->text().trimmed(),
            m_ipFilterEdit->text().trimmed()));
}

// ============================================================
// runBenchmark()
// ============================================================
// 执行 loopback 基线性能测试。
// ============================================================
void MainWindow::runBenchmark()
{
    // benchmark 前先确保协议能够正确加载。
    if (!loadProtocol(m_protocolPathEdit->text().trimmed()))
    {
        return;
    }

    // 执行 loopback benchmark。
    //
    // m_parser.definition()
    //
    // 返回当前已经解析完成的协议定义。
    //
    // 2000：
    //
    // 表示测试次数、消息数量或者测试规模，
    // 具体含义取决于 runLoopbackBenchmark() 的实现。
    const LoopbackBenchmarkResult result =
        m_controller.runLoopbackBenchmark(
            m_parser.definition(),
            2000);

    // 获取 benchmark 的文字摘要。
    const QString analysis = result.summary();

    // 将结果显示在主界面状态标签。
    m_statusLabel->setText(analysis);

    // 判断 benchmark 结果是否有效。
    if (result.isValid())
    {

        // 测试通过：
        // 弹普通信息框。
        QMessageBox::information(
            this,
            QString::fromUtf8("基线测试"),
            analysis);
    }
    else
    {

        // 测试失败：
        // 弹警告框。
        QMessageBox::warning(
            this,
            QString::fromUtf8("基线测试"),
            analysis);
    }
}

// ============================================================
// updateProgress()
// ============================================================
// Controller 发出 progressChanged 信号后调用。
//
// sentCount：已经发送的数量
// totalCount：计划发送的总数量
// ============================================================
void MainWindow::updateProgress(
    int sentCount,
    int totalCount)
{
    m_lastProgressSentCount = sentCount;

    // 如果 totalCount > 0，
    // 表示是“有限数量发送”。
    if (totalCount > 0)
    {

        // 设置进度条最大值。
        m_progressBar->setMaximum(totalCount);

        // 当前进度 = 已发送数量。
        m_progressBar->setValue(sentCount);
    }
    else
    {

        // totalCount <= 0：
        // 表示持续发送。
        //
        // 最大值设为 0，
        // 让进度条进入 busy 模式。
        m_progressBar->setMaximum(0);
    }
}

// ============================================================
// appendPayload()
// ============================================================
// Controller 每生成一条发送数据时调用。
// ============================================================
void MainWindow::appendPayload(
    const QString &payload)
{
    // 将 payload 追加到 QTextEdit。
    //
    // append() 不会覆盖原来的内容，
    // 而是在后面追加一行。
    m_previewText->append(payload);
}

// ============================================================
// finishRun()
// ============================================================
// 一次发送任务结束后执行。
// ============================================================
void MainWindow::finishRun(
    const QString &message)
{
    // 持续发送时 maximum == 0 代表 Qt 的 busy 动画。控制器停止后必须
    // 退出该模式，否则即使 UDP 定时器已经停止，进度条仍会继续滚动。
    if (m_progressBar->maximum() == 0)
    {
        m_progressBar->setRange(0, 1);
        m_progressBar->setValue(1);
        m_progressBar->setFormat(
            QString::fromUtf8("已发送 %1 条").arg(m_lastProgressSentCount));
    }

    // 显示 Controller 给出的结束信息。
    //
    // 例如：
    // “发送完成”
    // “已停止”
    // “发送失败”
    // 等。
    m_statusLabel->setText(message);

    // 由于发送过程中产生了新的数据库日志，
    // 所以发送结束后重新查询数据库，
    // 更新下面的日志表格。
    searchLogs();
}

// ============================================================
// buildUi()
// ============================================================
// 创建 MainWindow 中所有界面控件。
//
// 这是整个 UI 构造的核心函数。
// ============================================================
void MainWindow::buildUi()
{
    // --------------------------------------------------------
    // 1. 创建中央控件
    // --------------------------------------------------------
    //
    // QMainWindow 和普通 QWidget 不一样。
    //
    // QMainWindow 内部有固定区域：
    //
    // menu bar
    // toolbar
    // central widget
    // dock widget
    // status bar
    //
    // 普通控件一般不能直接加在 QMainWindow 上，
    // 而是先创建一个 central widget。
    QWidget *central = new QWidget(this);

    // 给中央控件创建垂直布局。
    //
    // 后续所有控件从上到下排列。
    QVBoxLayout *layout =
        new QVBoxLayout(central);

    // ========================================================
    // 第一部分：协议及发送参数区域
    // ========================================================

    // QFormLayout 特别适合：
    //
    // 标签      输入框
    // 标签      输入框
    // 标签      输入框
    //
    // 这种表单型布局。
    QFormLayout *form =
        new QFormLayout();

    // 创建协议路径输入框。
    m_protocolPathEdit =
        new QLineEdit(this);

    // 创建“选择协议”按钮。
    QPushButton *browseButton =
        new QPushButton(
            QString::fromUtf8("选择协议"),
            this);

    // 点击按钮时调用 browseProtocol()。
    connect(
        browseButton,
        SIGNAL(clicked()),
        this,
        SLOT(browseProtocol()));

    // 第一行：
    //
    // 协议路径   [____________]
    form->addRow(
        QString::fromUtf8("协议路径"),
        m_protocolPathEdit);

    // 第二行只有按钮。
    form->addRow(
        "",
        browseButton);

    // --------------------------------------------------------
    // 创建发送参数输入框
    // --------------------------------------------------------

    // 默认发送频率：10 Hz
    m_frequencyEdit =
        new QLineEdit("10", this);

    // 默认发送数量：20
    m_countEdit =
        new QLineEdit("20", this);

    // 默认目标 IP：
    // 127.0.0.1 表示本机回环地址。
    m_ipEdit =
        new QLineEdit(
            "127.0.0.1",
            this);

    // 默认目标 UDP 端口。
    m_portEdit =
        new QLineEdit(
            "39001",
            this);

    // --------------------------------------------------------
    // 对输入框设置数字限制
    // --------------------------------------------------------

    // 频率只能输入：
    // 1 ~ 1000
    //
    // 第三个参数 m_frequencyEdit：
    // 让 validator 归属于输入框，
    // 输入框销毁时 validator 自动销毁。
    m_frequencyEdit->setValidator(
        new QIntValidator(
            1,
            1000,
            m_frequencyEdit));

    // 数量只能输入：
    // 1 ~ INT_MAX
    //
    // 注意：
    // QLineEdit 本身仍然可以完全为空，
    // 所以“留空持续发送”仍然成立。
    m_countEdit->setValidator(
        new QIntValidator(
            1,
            2147483647,
            m_countEdit));

    // 端口限制：
    // 1 ~ 65535
    m_portEdit->setValidator(
        new QIntValidator(
            1,
            65535,
            m_portEdit));

    // 加入表单。
    form->addRow(
        QString::fromUtf8("频率(Hz)"),
        m_frequencyEdit);

    form->addRow(
        QString::fromUtf8("数量(留空持续发送)"),
        m_countEdit);

    form->addRow(
        QString::fromUtf8("目标 IP"),
        m_ipEdit);

    form->addRow(
        QString::fromUtf8("目标端口"),
        m_portEdit);

    // 将整个 form 加入主垂直布局。
    layout->addLayout(form);

    // ========================================================
    // 第二部分：控制按钮
    // ========================================================

    QPushButton *startButton =
        new QPushButton(
            QString::fromUtf8("开始发送"),
            this);

    QPushButton *stopButton =
        new QPushButton(
            QString::fromUtf8("停止发送"),
            this);

    QPushButton *benchmarkButton =
        new QPushButton(
            QString::fromUtf8("loopback 基线"),
            this);

    // 开始发送按钮：
    //
    // clicked()
    //     ↓
    // startSending()
    connect(
        startButton,
        SIGNAL(clicked()),
        this,
        SLOT(startSending()));

    // 停止按钮。
    connect(
        stopButton,
        SIGNAL(clicked()),
        this,
        SLOT(stopSending()));

    // benchmark 按钮。
    connect(
        benchmarkButton,
        SIGNAL(clicked()),
        this,
        SLOT(runBenchmark()));

    // 依次加入垂直布局。
    layout->addWidget(startButton);
    layout->addWidget(stopButton);
    layout->addWidget(benchmarkButton);

    // ========================================================
    // 第三部分：发送进度条
    // ========================================================

    m_progressBar =
        new QProgressBar(this);

    layout->addWidget(
        m_progressBar);

    // ========================================================
    // 第四部分：发送 payload 预览
    // ========================================================

    m_previewText =
        new QTextEdit(this);

    // 设置为只读。
    //
    // 用户可以看，但是不能修改。
    m_previewText->setReadOnly(true);

    // QTextEdit 内部维护 QTextDocument。
    //
    // setMaximumBlockCount(200)
    //
    // 只保留最近 200 个文本块。
    //
    // 这里通常可以理解为最多保留大约 200 行发送记录。
    //
    // 防止持续发送时文本无限增长，
    // 最终占用大量内存。
    m_previewText
        ->document()
        ->setMaximumBlockCount(200);

    layout->addWidget(
        m_previewText);

    // ========================================================
    // 第五部分：日志查询条件
    // ========================================================

    QFormLayout *queryForm =
        new QFormLayout();

    // 时间格式采用可排序的 yyyy-MM-dd HH:mm:ss；留空表示不限制。
    m_fromTimeFilterEdit =
        new QLineEdit(this);
    m_fromTimeFilterEdit->setPlaceholderText("2026-08-15 09:00:00");

    m_toTimeFilterEdit =
        new QLineEdit(this);
    m_toTimeFilterEdit->setPlaceholderText("2026-08-15 18:00:00");

    // 协议搜索输入框。
    m_protocolFilterEdit =
        new QLineEdit(this);

    // IP 搜索输入框。
    m_ipFilterEdit =
        new QLineEdit(this);

    queryForm->addRow(
        QString::fromUtf8("开始时间(可空)"),
        m_fromTimeFilterEdit);

    queryForm->addRow(
        QString::fromUtf8("结束时间(可空)"),
        m_toTimeFilterEdit);

    queryForm->addRow(
        QString::fromUtf8("协议检索"),
        m_protocolFilterEdit);

    queryForm->addRow(
        QString::fromUtf8("IP 检索"),
        m_ipFilterEdit);

    layout->addLayout(queryForm);

    // 创建查询按钮。
    QPushButton *searchButton =
        new QPushButton(
            QString::fromUtf8("查询日志"),
            this);

    // 点击：
    //
    // searchLogs()
    connect(
        searchButton,
        SIGNAL(clicked()),
        this,
        SLOT(searchLogs()));

    layout->addWidget(
        searchButton);

    // ========================================================
    // 第六部分：日志表格
    // ========================================================

    m_logTable =
        new QTableWidget(this);

    // 一行对应一次发送任务汇总。
    m_logTable->setColumnCount(10);

    // 设置表头。
    //
    // QStringList()
    //      << ...
    //      << ...
    //
    // 是 Qt 中很常见的 QStringList 构造方式。
    m_logTable->setHorizontalHeaderLabels(
        QStringList()
        << QString::fromUtf8("开始时间")
        << QString::fromUtf8("结束时间")
        << QString::fromUtf8("协议")
        << QString::fromUtf8("配置数量")
        << QString::fromUtf8("实际总数")
        << QString::fromUtf8("频率(Hz)")
        << "IP"
        << QString::fromUtf8("端口")
        << QString::fromUtf8("状态")
        << QString::fromUtf8("错误信息"));

    // Stretch：
    //
    // 让所有列自动拉伸，
    // 尽量填满整个表格宽度。
    m_logTable
        ->horizontalHeader()
        ->setSectionResizeMode(
            QHeaderView::Stretch);

    // 禁止用户直接双击修改表格内容。
    //
    // 因为表格只是日志显示，
    // 不是日志编辑器。
    m_logTable->setEditTriggers(
        QAbstractItemView::NoEditTriggers);

    layout->addWidget(
        m_logTable);

    // ========================================================
    // 第七部分：状态文字
    // ========================================================

    m_statusLabel =
        new QLabel(this);

    layout->addWidget(
        m_statusLabel);

    // ========================================================
    // 第八部分：设置 MainWindow
    // ========================================================

    // 把刚才创建的 central QWidget
    // 设置为 QMainWindow 的中央控件。
    setCentralWidget(central);

    // 设置窗口标题。
    setWindowTitle(
        QString::fromUtf8(
            "T007 XML 协议 UDP 发生器"));

    // 设置窗口初始大小：
    //
    // 宽 980
    // 高 720
    //
    // 用户之后仍然可以拖动改变窗口大小。
    resize(980, 720);
}

// ============================================================
// loadProtocol()
// ============================================================
// 加载 XML 协议文件。
//
// 返回：
// true  -> 加载成功
// false -> 加载失败
// ============================================================
bool MainWindow::loadProtocol(
    const QString &path)
{
    // 调用 ProtocolParser::load()
    //
    // parser 负责真正读取 XML 并解析协议定义。
    if (!m_parser.load(path))
    {

        // 如果解析失败，
        // 显示 parser 中记录的错误信息。
        QMessageBox::warning(
            this,
            QString::fromUtf8("协议加载失败"),
            m_parser.lastError());

        return false;
    }

    // 如果协议解析成功，
    // 把协议定义交给发送控制器。
    //
    // 之后 controller 发送数据时，
    // 就知道：
    //
    // - 协议叫什么
    // - 有哪些字段
    // - 每个字段什么类型
    // - 数据如何生成
    // 等等。
    m_controller.setProtocol(
        m_parser.definition());

    // 更新状态栏。
    //
    // %1 会被协议名称替换。
    //
    // 例如：
    //
    // 已加载协议：TelemetryProtocol
    m_statusLabel->setText(
        QString::fromUtf8(
            "已加载协议：%1")
            .arg(
                m_parser.definition().name));

    return true;
}

// ============================================================
// renderLogs()
// ============================================================
// 把数据库查询结果显示到 QTableWidget。
//
// entries：数据库查询得到的一组日志记录。
// ============================================================
void MainWindow::renderLogs(
    const QList<TransmissionRunEntry> &entries)
{
    // 设置表格行数。
    //
    // 有多少条日志，
    // 就创建多少行。
    m_logTable->setRowCount(
        entries.size());

    // 遍历每一条日志。
    for (int row = 0;
         row < entries.size();
         ++row)
    {
        // 获取当前行对应的一条日志记录。
        //
        // 使用 const 引用可以避免不必要的数据复制。
        const TransmissionRunEntry &entry =
            entries.at(row);

        // 第 0 列：发送时间。
        m_logTable->setItem(
            row,
            0,
            new QTableWidgetItem(
                entry.startTime));

        m_logTable->setItem(
            row,
            1,
            new QTableWidgetItem(entry.endTime));

        // 第 2 列：协议名称。
        m_logTable->setItem(
            row,
            2,
            new QTableWidgetItem(
                entry.protocolName));

        m_logTable->setItem(row, 3, new QTableWidgetItem(
            entry.requestedCount == 0 ? QString::fromUtf8("持续") : QString::number(entry.requestedCount)));
        m_logTable->setItem(row, 4, new QTableWidgetItem(QString::number(entry.totalCount)));
        m_logTable->setItem(row, 5, new QTableWidgetItem(QString::number(entry.frequencyHz)));

        // 第 6 列：目标 IP。
        m_logTable->setItem(
            row,
            6,
            new QTableWidgetItem(
                entry.targetIp));

        // 第 7 列：目标端口。
        //
        // entry.port 是整数，
        // QTableWidgetItem 这里需要字符串，
        // 所以使用 QString::number() 转换。
        m_logTable->setItem(
            row,
            7,
            new QTableWidgetItem(
                QString::number(
                    entry.targetPort)));

        QString statusText = entry.status;
        if (entry.status == "completed") statusText = QString::fromUtf8("完成");
        else if (entry.status == "stopped") statusText = QString::fromUtf8("已停止");
        else if (entry.status == "failed") statusText = QString::fromUtf8("失败");
        m_logTable->setItem(row, 8, new QTableWidgetItem(statusText));
        m_logTable->setItem(row, 9, new QTableWidgetItem(entry.errorMessage));
    }
}
