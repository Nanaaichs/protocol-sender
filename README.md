# CITEL-T-007 XML 协议 UDP 发生器

本项目基于 Qt 5.9.7、C++11 和 qmake，实现一个桌面协议发生器：读取 XML 协议定义，随机生成字段值，通过 UDP 按指定频率发送，并在每次发送任务结束时把配置、实际数量、起止时间和结果汇总到 SQLite。

## 已实现功能

- 加载并校验旧属性式 XML 与课程子元素式 XML；
- 解析课程协议的源/目地址、端口、系统和字段位布局；
- 随机生成题面字段，并额外兼容课程样例中的 `IP`；
- 课程格式按 bit 布局生成真正二进制 UDP 报文；
- 配置 1–1000 Hz 常规发送频率、发送数量、目标 IP 和端口；
- 发送数量留空时持续发送，直到用户停止；
- UDP 写入失败时停止任务并显示原因；任务无论完成、停止还是失败都会写入一条 SQLite 汇总日志；
- 展示成功发送进度和最近 200 条载荷；
- 按起止时间段、协议和目标 IP 组合查询 SQLite 汇总日志，其中协议和 IP 支持模糊匹配；
- 使用真实 localhost 接收端执行 loopback 基准，报告发送、接收、异常报文、吞吐和丢包率。

## 支持的数据类型（兼容集）

`DEC`、`INT`、`UINT`、`BIN`、`OCT`、`HEX`、`FLT`、`DBL`、`STRING`、`FLAG`、`INT8`、`UINT8`、`INT16`、`UINT16`、`UINT32`、`IP`。

为不破坏旧文件，当前实现保留 16 种兼容类型。课程最终要求中的 14 种类型映射不作为本轮修改的验收项；字段结构、取值约束、UDP 发送和任务汇总日志已经按课程规则实现。详见 [`docs/COURSE_PROTOCOL_COMPATIBILITY.md`](docs/COURSE_PROTOCOL_COMPATIBILITY.md)。

## 一键构建、测试和打包

默认安装位置应为 `C:\Qt\Qt5.9.7`，并包含 `mingw53_32` 与 `Tools\mingw530_32`：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build_and_test.ps1 -Package
```

脚本会：

1. 构建 `build\app\release\protocol_sender.exe`；
2. 构建并运行 `build\tests\release\protocol_sender_tests.exe`；
3. 使用 `windeployqt` 生成可独立运行的 `dist` 目录；
4. 自动运行打包后的 `protocol_sender.exe --smoke-test`，检查平台插件、嵌入协议和 SQLite 驱动。

如果旧的 `dist\protocol_sender.exe` 正在运行，Windows 会锁定其中的 DLL。脚本不会强行结束程序，而会把已验证的新包写入 `dist-candidate`；关闭旧程序后再运行一次同一命令，即可更新正式 `dist`。

Qt 安装在其他位置时：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build_and_test.ps1 -QtRoot D:\Qt\Qt5.9.7 -Package
```

## 运行

```powershell
.\dist\protocol_sender.exe
```

若刚才输出的是 `Package ready: ...\dist-candidate`，可以先运行其中的候选程序，也可以关闭旧版后重新打包，再运行正式 `dist`。

示例协议已嵌入应用；可以加载 [`data/sample_protocol.xml`](data/sample_protocol.xml)，也可以直接选择两个课程协议样例。SQLite 数据库保存在当前用户的标准应用数据目录，而不是 exe 相邻目录。

## 当前验证结果

- 应用构建成功；
- 测试构建并实际运行成功：18 passed，0 failed；
- 2000 条 localhost 基准：写入 2000、接收 2000、异常 0、丢包率 0.00%；
- 本轮多次构建的端到端样本约 37,500–81,325 Hz，均为 2000/2000、丢包 0.00%；吞吐会随机器负载波动，不是跨机器保证值；
- `dist` 已确认包含 Qt 平台插件、SQLite 驱动和 MinGW 运行库；
- 打包程序无交互冒烟启动成功，退出码 0。

第一次接触本项目时，建议直接阅读完整项目说明书（[`Markdown 版`](docs/CITEL-T-007_完整项目说明书.md) / [`Word 最终版`](docs/CITEL-T-007_完整项目说明书_最终版.docx)）。说明书从术语、环境安装、构建运行、XML 编写、人工验收到 Git 交付逐步讲解；快速检验路径见 [`docs/LEARNING_AND_REPRODUCTION.md`](docs/LEARNING_AND_REPRODUCTION.md)，完整测试条件和限制见 [`TEST_REPORT.md`](TEST_REPORT.md)，部署细节见 [`docs/DEPLOYMENT.md`](docs/DEPLOYMENT.md)。
