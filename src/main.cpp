#include "mainwindow.h"
#include "protocolparser.h"
#include "transmissionrepository.h"

#include <QApplication>   // 负责：窗口管理 鼠标键盘事件 信号槽机制 绘制
#include <QDebug>         //输出日志
#include <QDir>           //处理：路径 文件夹 文件
#include <QStandardPaths> //获取系统标准路径

int main(int argc, char *argv[])
{
    // QApplicationc创建
    QApplication app(argc, argv);
    // 判断是否执行测试模式
    if (app.arguments().contains("--smoke-test"))
    {
        // 初始化协议解析器
        ProtocolParser parser;
        // 加载示例协议文件
        if (!parser.load(":/data/sample_protocol.xml"))
        {
            qCritical() << parser.lastError();
            return 2;
        }
        // 初始化传输日志数据库
        TransmissionRepository repository(
            QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation))
                .absoluteFilePath("protocol_sender.db"),
            "protocol_sender_smoke_test");
        // 检查数据库是否成功打开
        if (!repository.isOpen())
        {
            qCritical() << repository.lastError();
            return 3;
        }
        // 运行基线测试
        qInfo() << "CITEL-T-007 runtime initialized successfully";
        return 0;
    }
    // 创建主窗口并显示
    MainWindow window;
    window.show();
    // 进入应用程序的事件循环
    return app.exec();
}
