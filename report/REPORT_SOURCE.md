# 项目报告底稿

## 1. 技术背景

协议发生器是典型的“配置驱动”工具：协议结构由外部描述文件定义，程序负责解析协议、生成随机字段、组装报文并通过网络发送。课程要求使用 XML、UDP 和 SQLite，因此本题既涉及解析器设计，也涉及网络发送控制和日志查询。

## 2. 需求与完成度

已完成项：

1. 读取自定义 XML 协议；
2. 支持 15 类字段类型；
3. 按字段规则随机生成值；
4. 配置发送频率、数量、IP、端口；
5. 数量留空时持续发送直到停止；
6. UDP 发送控制器；
7. SQLite 日志；
8. 按时间 / 协议 / IP 查询；
9. loopback 基线逻辑；
10. README、测试、部署说明、AI 使用说明和证据文档。

验收边界：

- 应用、测试、打包和本机 loopback 已实际运行；
- 仓库缺少老师提供的完整原始题面，因此自行补齐的字段类型仍需在提交前核对；
- 课程提交前建议再做一次人工 GUI 全流程演示并保存截图。

## 3. 15 类字段与完成度

题面显式给出或可直接归纳的基础类型包括：

- `DEC`
- `INT`
- `BIN`
- `OCT`
- `HEX`
- `FLT`
- `DBL`
- `STRING`
- `FLAG`
- `UINT`

在此基础上，合理扩展 5 类常见定长整型：

- `INT8`
- `UINT8`
- `INT16`
- `UINT16`
- `UINT32`

总计 15 类，均已在 `DataGenerator` 中实现。

## 4. 架构、数据流与模块

### 4.1 模块划分

- `ProtocolParser`
  - 解析 XML 协议文件；
- `DataGenerator`
  - 生成字段值与文本载荷；
- `TransmissionRepository`
  - 管理 SQLite 日志；
- `UdpSendController`
  - 控制发送节奏、持续发送和基线测试；
- `MainWindow`
  - 负责协议加载、参数输入、进度显示和日志查询。

### 4.2 数据流

1. 用户选择 XML 文件。
2. `ProtocolParser` 解析为 `ProtocolDefinition`。
3. `DataGenerator` 根据字段类型与取值范围随机生成 `key=value` 列表。
4. `UdpSendController` 调用 `QUdpSocket::writeDatagram` 发送。
5. 每次发送后写入 `TransmissionRepository`。
6. 用户按时间 / 协议 / IP 过滤并查看日志。

## 5. 存储接口

SQLite 表：`transmission_logs`

字段：

- `id`
- `created_at`
- `protocol_name`
- `ip`
- `port`
- `sequence_no`
- `payload`

查询接口：

- `search(const QString &timeLike, const QString &protocolLike, const QString &ipLike)`

## 6. 关键实现

1. 协议文件采用 `<protocol><field/></protocol>` 的简洁结构，兼容 `name`、`type`/`dataType`、`data`、`isSelected`、`isKey`、`length`、`min`/`max`、`bitIndex` 与 `loopEnd`；缺省属性使用保守默认值。
2. 载荷格式采用文本 `key=value;key=value`，便于课程验收、日志查询和本地调试。
3. 正常发送使用 `QTimer` 驱动，满足可配置频率与持续发送的需求。
4. 基线测试动态绑定 localhost 接收端，为每条报文增加测试序号，同时统计成功写入、唯一接收、异常报文、端到端耗时和丢包率。

## 7. 性能与最高频率分析

2026-08-08 在 Qt 5.9.7 `mingw53_32` / MinGW 5.3.0 32-bit 环境中完成了一次 2000 条 localhost 自动基准：

- 请求 2000 条；
- 成功写入 2000 条；
- 唯一接收 2000 条；
- 异常报文 0 条；
- 发送阶段 26.506 ms，约 75,455.47 Hz；
- 端到端 26.590 ms，约 75,216.81 Hz；
- 丢包率 0.00%。

正常 GUI 发送与性能基准是两条不同路径：正常发送限制为 1–1000 Hz，并逐条写入 SQLite；性能基准用于测量本机生成与 UDP loopback 的短时上界，不写业务日志。多次验证结果会随系统负载波动，上述数值只代表当前机器的一次干净构建运行，不能直接推广到其他机器或远端网络。

## 8. 测试证据与限制

测试覆盖协议解析与非法输入拒绝、15 类字段逐类生成、SQLite 查询、发送参数校验、有限和持续 UDP 发送、真实 localhost 接收、进度与日志一致性，以及 loopback 吞吐和丢包统计。

实际验证结果：

- 应用成功编译并链接为 `protocol_sender.exe`；
- 测试成功编译并链接为 `protocol_sender_tests.exe`；
- QtTest 实际运行：12 passed，0 failed；
- `windeployqt` 成功生成包含 Qt 平台插件和 SQLite 驱动的 `dist`；
- 打包程序无交互冒烟启动退出码为 0。

限制：

- 工程关闭了 Qt 5.9.7 的 `moc_predefs` 注入，以避免旧版 moc 在中文路径下误报无法打开头文件；
- GUI 尚需在课程提交前人工操作并保存最终截图；
- 性能结果是一次本机短时样本。

## 9. 部署

部署使用已安装的 Qt 5.9.7 `mingw53_32` 及配套 MinGW 5.3.0。项目提供统一入口：

- `powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build_and_test.ps1`
- 增加 `-Package` 可调用 `windeployqt` 生成 `dist`。

示例 XML 已嵌入 Qt Resource；SQLite 写入 `QStandardPaths::AppDataLocation`，避免依赖 exe 相对路径和安装目录写权限。

## 10. AI 使用与人工复核

### 10.1 AI 代表提示

- “实现一个能读取 XML 协议、随机生成字段并用 UDP 发送的桌面工具，同时写 SQLite 日志并支持条件查询。”

### 10.2 接受内容

- 协议模型初稿；
- 字段类型集合与生成器实现；
- UDP 控制器框架；
- QtTest 样例；
- 报告与文档底稿。

### 10.3 拒绝或修正内容

- 根据补充要求把字段类型从 14 类扩展为 15 类；
- 在旧工具链尚不可运行时删除任何暗示“已实测最高频率”的表述，避免虚构性能数据；
- 在获得真实 Qt 5.9.7 工具链后重新运行测试，并用实际 loopback 收包数据替换旧的“仅有方法论”结论。

### 10.4 人工复核

- 核对 15 类字段是否齐全；
- 审查性能分析是否区分“方法”和“实测”；
- 复核链接失败根因是否真实。

## 11. 收获、课程建议与结论

收获：

- 了解了配置驱动协议工具的基础架构；
- 理解了 GUI 定时发送和批量发送基线的差异；
- 认识到性能结论必须以真实运行数据为前提。

课程建议：

- 课程若涉及 Qt 桌面项目，建议配套统一构建镜像；
- 可进一步增加二进制协议打包与接收端校验作为拓展。

结论：

本项目已完成协议解析、15 类字段生成、参数校验、UDP 发送控制、SQLite 日志与查询、真实 loopback 基准、自动测试和 Windows 打包。当前代码交付已具备可构建和可运行证据；提交课程平台前剩余工作是核对老师原始 15 类字段清单，并完成一次人工 GUI 演示截图。
