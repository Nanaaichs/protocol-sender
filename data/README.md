# 数据说明

- `sample_protocol.xml`：可编辑的示例协议文件；同一份示例也通过 Qt Resource 嵌入应用，程序首次启动不依赖外部 XML。
- `course_protocol_example_1.xml`：课程提供的 32 bit、2 字段协议样例的可复现副本。
- `course_protocol_example_2.xml`：课程提供的 96 bit、5 字段协议样例的可复现副本。
- 两个课程样例使用子元素描述字段并生成二进制报文；完整语义和 Packet Sender 验收方法见 `docs/COURSE_PROTOCOL_COMPATIBILITY.md`。
- SQLite 日志库不保存在本目录。程序把它写入 `QStandardPaths::AppDataLocation`，当前 Windows 环境通常为 `%APPDATA%\protocol_sender\protocol_sender.db`。
