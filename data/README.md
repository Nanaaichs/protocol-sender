# 数据说明

- `sample_protocol.xml`：可编辑的示例协议文件；同一份示例也通过 Qt Resource 嵌入应用，程序首次启动不依赖外部 XML。
- SQLite 日志库不保存在本目录。程序把它写入 `QStandardPaths::AppDataLocation`，当前 Windows 环境通常为 `%APPDATA%\protocol_sender\protocol_sender.db`。
