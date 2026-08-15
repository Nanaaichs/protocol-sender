# CITEL-T-007 XML 协议 UDP 发生器

## 完整项目说明书

文档版本：1.1

适用系统：Windows 10/11

项目工具链：Qt 5.9.7、MinGW 5.3.0 32-bit、C++11、qmake

适用分支：`main`
编制日期：2026-08-16

> 本说明书面向没有 Qt、C++、UDP、SQLite 或 Git 经验的读者。只要按顺序完成“环境检查—构建—运行—验收—提交”，即可复现并交付当前项目。带有“必须核对”标记的内容不能跳过。

---

## 目录与阅读方法

如果只想尽快运行项目，阅读第 1、3、4、5、9 章。

如果需要向老师演示，继续阅读第 10、13 章。

如果需要理解或修改代码，阅读第 6、7、8、11 章。
如果需要规范提交 Git，阅读第 12 章。

---

# 1. 项目是什么

## 1.1 一句话说明

本项目是一个 Windows 桌面程序。它读取 XML 格式的通信协议说明，按照字段规则生成文本或二进制报文，以指定频率通过 UDP 发送；每次任务结束后，把配置数量、实际总数、频率、起止时间、目标地址和任务结果汇总到 SQLite，并提供条件检索和本机 loopback 性能测试。

## 1.2 外行人需要理解的五个名词

| 名词 | 通俗解释 |
| --- | --- |
| Qt | 用来开发 Windows 图形界面的 C++ 框架。本项目固定使用 Qt 5.9.7。 |
| XML | 一种有层次的文本格式。本项目用它描述报文中有哪些字段、字段类型和取值范围。 |
| UDP | 一种轻量网络传输方式。发送成功表示数据已写入本机网络接口，不天然保证远端已经收到。 |
| SQLite | 一个保存在单个文件中的小型数据库。本项目用它保存发送日志。 |
| loopback | 让本机同时充当发送端和接收端，用来证明 UDP 报文确实被收到并计算丢包率。 |

## 1.3 项目应实现的要求

| 编号 | 要求 | 当前实现 |
| --- | --- | --- |
| R1 | 读取自定义 XML 协议 | 已完成，且会拒绝结构或属性非法的 XML |
| R2 | 支持课程字段类型 | 当前保留 16 种兼容类型；最终 14 种类型映射按本轮约定暂不作为验收项 |
| R3 | 随机生成字段值 | 已完成，支持范围、长度、模板、固定值和位提取 |
| R4 | 配置频率、数量、IP、端口 | 已完成，常规频率范围为 1–1000 Hz |
| R5 | 数量为空时持续发送 | 已完成，用户点击“停止发送”结束 |
| R6 | 通过 UDP 发送 | 已完成，基于 `QUdpSocket` |
| R7 | 显示进度并写 SQLite 日志 | 已完成，预览区保留最近 200 条载荷，每次任务结束写 1 条汇总 |
| R8 | 按时间段、协议、IP 查询 | 已完成，时间使用起止范围，协议和 IP 支持模糊匹配 |
| R9 | 提供本机性能测试 | 已完成真实 localhost 接收、吞吐和丢包统计 |

## 1.4 当前已经验证的结果

- QtTest：17 passed，0 failed，0 skipped；
- loopback：请求 2000 条，写入 2000 条，接收 2000 条，异常 0 条，丢包率 0.00%；
- `windeployqt` 打包成功；
- 打包后的程序通过 `--smoke-test`，退出码为 0；
- 生成的 `dist` 包含 Qt 平台插件、SQLite 驱动和 MinGW 运行库。

性能数字会随机器负载变化。本轮多次样本约为 37,500–81,325 Hz，均为发送 2000、接收 2000、异常 0、丢包 0.00%。验收时首先关注“发送数、接收数、异常数和丢包率”，不要把一次吞吐结果宣传为所有机器都能达到的保证值。

---

# 2. 最终会得到什么

## 2.1 主要交付物

| 路径 | 作用 | 是否进入 Git |
| --- | --- | --- |
| `src` | 应用程序源码和 qmake 工程 | 是 |
| `tests` | QtTest 自动化测试 | 是 |
| `data/sample_protocol.xml` | 可修改的示例协议 | 是 |
| `scripts/build_and_test.ps1` | 一键构建、测试和打包脚本 | 是 |
| `docs` | 使用、部署、学习和本说明书 | 是 |
| `report/REPORT_SOURCE.md` | 项目报告底稿 | 是 |
| `build/app/release/protocol_sender.exe` | 开发构建程序 | 否，可重新生成 |
| `build/tests/release/protocol_sender_tests.exe` | 测试程序 | 否，可重新生成 |
| `dist/protocol_sender.exe` | 可交付运行程序 | 否，可重新生成 |

## 2.2 运行时数据位置

SQLite 日志不保存在项目目录或 `data` 目录，而是保存在当前 Windows 用户的应用数据目录。通常为：

```text
%APPDATA%\protocol_sender\protocol_sender.db
```

这样可以避免程序安装目录没有写权限，也可以在重新打包后继续保留历史日志。

## 2.3 当前已知边界

1. 本轮不以最终 14 种类型映射作为验收阻塞项；教师给出精确名单后可再收敛兼容集。
2. 普通发送的实际总数表示成功写入 UDP socket 的报文数，不等于远端设备已经收到；真实接收由 loopback 或 Packet Sender 验证。
3. 课程子元素格式中，`isKey=true` 必须提供固定 `data`；非标识字段根据值域随机生成。旧属性式格式继续保留原有模板兼容语义。
4. 基准是本机短时间测试，不代表远程网络和其他电脑的性能。
5. 课程提交平台、教师评分表和最终截图仍需人工处理。

---

# 3. 准备开发环境

## 3.1 必需条件

- Windows 10 或 Windows 11；
- Qt 5.9.7 的 `mingw53_32` 套件；
- Qt 配套 MinGW 5.3.0 32-bit；
- PowerShell 5 或更高版本；
- Git；
- 至少约 1 GB 可用磁盘空间，用于构建和打包文件。

## 3.2 Qt 安装时必须选择的组件

如果 Qt 已经安装，可以直接进入 3.3。重新安装时必须保证同时存在：

1. `Qt 5.9.7 > MinGW 5.3.0 32 bit`；
2. `Tools > MinGW 5.3.0 32 bit`。

不要只安装 MSVC 套件，也不要把 Anaconda 自带的 Qt 库和 MinGW 混用。MSVC 与 MinGW 生成的库格式不同，混用常表现为链接时找不到符号或提示 `.lib` 不兼容。

## 3.3 检查 Qt 是否安装正确

打开 PowerShell，逐条复制：

```powershell
Test-Path 'C:\Qt\Qt5.9.7\5.9.7\mingw53_32\bin\qmake.exe'
Test-Path 'C:\Qt\Qt5.9.7\Tools\mingw530_32\bin\mingw32-make.exe'
& 'C:\Qt\Qt5.9.7\5.9.7\mingw53_32\bin\qmake.exe' -v
& 'C:\Qt\Qt5.9.7\Tools\mingw530_32\bin\mingw32-make.exe' --version
```

前两条应输出 `True`。`qmake -v` 应包含 Qt 5.9.7，`mingw32-make --version` 应显示 GNU Make。若路径不同，不必移动安装目录，后续通过 `-QtRoot` 参数告诉脚本实际位置。

## 3.4 检查 Git 和 PowerShell

```powershell
git --version
$PSVersionTable.PSVersion
```

Git 应输出版本号；PowerShell 主版本建议不低于 5。构建命令使用进程级 `-ExecutionPolicy Bypass`，不会永久修改系统执行策略。

---

# 4. 获取代码并确认 Git 状态

## 4.1 当前电脑已有项目时

打开项目目录：

```powershell
cd 'C:\Users\LHX\Desktop\上课\高级软件研发实践\projects\CITEL-T-007'
git switch main
git branch --show-current
git status --short -- .
```

分支名应为 `main`。最后一条命令没有输出，表示本项目目录没有未提交文件。

上层课程仓库可能还有其他练习文件的改动，所以不要用不带路径限制的状态作为本项目是否干净的唯一判断，也不要随意执行 `git add -A`。

## 4.2 从远程仓库获取时

当前仓库已经配置 `origin`。如果要把它改成自己的远程仓库，先查看现有地址，再替换并推送：

```powershell
$repoUrl = Read-Host '请输入远程仓库地址，例如 https://github.com/your-name/course-repository.git'
git remote -v
git remote set-url origin $repoUrl
git push -u origin main
```

只有在 `git remote -v` 没有显示 `origin` 时，才使用 `git remote add origin $repoUrl`，不要对已经存在的 `origin` 重复执行 `add`。

其他人随后可以执行：

```powershell
$repoUrl = Read-Host '请输入远程仓库地址'
git clone $repoUrl '.\course-repository'
cd '.\course-repository\projects\CITEL-T-007'
git switch main
```

如果没有远程仓库，也可以复制整个课程仓库目录。只复制 `CITEL-T-007` 可以构建和运行，但不会包含完整 Git 历史。

---

# 5. 一键构建、测试和打包

## 5.1 推荐命令

在项目根目录执行：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build_and_test.ps1 -Package
```

如果 Qt 安装在其他根目录，例如 `D:\Qt\Qt5.9.7`：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build_and_test.ps1 -QtRoot D:\Qt\Qt5.9.7 -Package
```

## 5.2 脚本在做什么

脚本依次完成：

1. 检查 `qmake.exe` 和 `mingw32-make.exe` 是否存在；
2. 将正确 Qt 与 MinGW 目录临时放到当前进程 `PATH` 前面；
3. 在 `build/app` 生成 Makefile 并构建应用；
4. 在 `build/tests` 生成 Makefile并构建测试程序；
5. 实际运行 QtTest；
6. 使用 `windeployqt` 收集运行依赖；
7. 检查关键 DLL、平台插件和 SQLite 驱动是否齐全；
8. 将结果复制到 `dist`；若旧程序正在运行并占用 DLL，则改写到 `dist-candidate`；
9. 对实际写入的包运行 `protocol_sender.exe --smoke-test`。

Qt 5.9 的 `windeployqt` 对中文路径处理不稳定，因此脚本先在纯英文临时目录完成部署，再复制回项目。不要为了“简化”而删除这一阶段。

## 5.3 怎样判断构建成功

输出中应出现：

```text
Totals: 17 passed, 0 failed, 0 skipped
Package ready: ...\CITEL-T-007\dist
Build and tests completed successfully.
```

如果旧的 `dist\protocol_sender.exe` 正在运行，输出目录会是 `dist-candidate`。这仍表示新包已经通过冒烟测试；关闭旧程序后再执行一次完整命令，即可更新正式 `dist`。

loopback 测试还应出现类似：

```text
请求 2000 条，成功写入 2000 条，接收 2000 条，异常报文 0 条，丢包率 0.00%
```

吞吐数字可能不同，只要测试总数、收发数量、异常数、丢包率和退出码符合要求即可。

## 5.4 构建产物

```text
build\app\release\protocol_sender.exe
build\tests\release\protocol_sender_tests.exe
dist\protocol_sender.exe
dist-candidate\protocol_sender.exe    # 仅在 dist 被运行中的程序占用时出现
```

`build` 和 `dist` 都被 `.gitignore` 忽略，不应提交到 Git。需要重新生成时再次运行脚本即可。

## 5.5 只构建和测试，不打包

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build_and_test.ps1
```

这种方式不会生成完整 `dist`，适合日常开发；课程最终验收应使用带 `-Package` 的完整命令。

---

# 6. 第一次运行程序

## 6.1 启动

```powershell
.\dist\protocol_sender.exe
```

也可以在资源管理器中双击 `dist\protocol_sender.exe`。正常情况下会出现标题为“T007 XML 协议 UDP 发生器”的窗口，并自动加载内嵌示例协议 `LoopbackDemo`。

## 6.2 界面控件说明

| 界面文字 | 作用 | 初次建议值 |
| --- | --- | --- |
| 协议路径 | 当前 XML 文件路径；启动时可能显示内嵌资源路径 | 保持默认或选择示例 XML |
| 选择协议 | 打开本地 XML 文件 | `data\sample_protocol.xml` |
| 频率(Hz) | 每秒计划发送的报文数 | `10` |
| 数量(留空持续发送) | 有限发送总数；留空表示持续发送 | `20` |
| 目标 IP | 接收端 IPv4/IPv6 地址 | `127.0.0.1` |
| 目标端口 | 接收端 UDP 端口 | `39001` |
| 开始发送 | 校验参数后开始常规发送和日志记录 | - |
| 停止发送 | 停止持续发送任务 | - |
| loopback 基线 | 自动建立本机接收端并发送 2000 条测试报文 | - |
| 开始时间 | 限定任务时间范围的下界；留空表示不限 | `2026-08-15 09:00:00` |
| 结束时间 | 限定任务时间范围的上界；留空表示不限 | `2026-08-15 18:00:00` |
| 协议检索 | 按协议名模糊匹配日志 | `LoopbackDemo` |
| IP 检索 | 按目标 IP 模糊匹配日志 | `127.0.0.1` |
| 查询日志 | 按四个输入框的组合条件查询 | - |

## 6.3 第一次操作

1. 确认状态栏显示已加载 `LoopbackDemo`。
2. 保持默认值 `10 Hz、20 条、127.0.0.1、39001`。
3. 点击“开始发送”。
4. 等待进度条到 20。
5. 检查预览区是否出现类似 `device_id=...;flags=...` 的文本。
6. 检查日志表是否新增 1 条记录，且“配置数量”和“实际总数”都是 20，状态为“已完成”。

如果端口上没有其他接收程序，普通发送仍可能显示成功，因为 UDP 写入 socket 并不要求对方回执。要验证真正接收，请继续执行 loopback 基线。

## 6.4 持续发送

1. 清空“数量”输入框，不要输入 0 或空格以外的字符。
2. 点击“开始发送”。
3. 观察预览区和进度条持续更新。
4. 数秒后点击“停止发送”。
5. 状态栏应显示“发送已停止，共发送 N 条”。

## 6.5 日志查询

四个条件可以单独使用，也可以组合使用：

- 开始时间/结束时间：格式为 `yyyy-MM-dd HH:mm:ss`，用于查询与该时间段相交的任务；任一项可留空；
- 协议：输入 `Loopback` 可匹配 `LoopbackDemo`；
- IP：输入 `127.0.0.1` 可匹配本机发送记录；
- 全部留空：返回所有任务汇总，按开始时间从新到旧排列。

关闭程序再重新打开并查询，历史记录仍应存在。这一步可以验证 SQLite 持久化。

---

# 7. XML 协议文件怎么写

本章同时说明两种输入格式。7.1–7.7 介绍项目原有的“属性式兼容格式”，便于理解和修改内置示例；老师提供的协议文件属于“课程子元素格式”，其严格规则以 7.8 和 `docs/COURSE_PROTOCOL_COMPATIBILITY.md` 为准。两种格式不要在同一个字段中混写。

## 7.1 最小可用示例

```xml
<?xml version="1.0" encoding="UTF-8"?>
<protocol name="MyDemo">
  <field name="device_id"
         type="UINT16"
         dataType="UINT16"
         min="1"
         max="100"
         data="42"
         isSelected="true"
         isKey="true"
         bitIndex="-1"
         loopEnd="false" />
</protocol>
```

根节点必须叫 `protocol`，并且至少有一个 `field`。字段名不能为空。

## 7.2 protocol 属性

| 属性 | 是否必需 | 说明 |
| --- | --- | --- |
| `name` | 建议填写 | 协议名称；缺省时使用 `UnnamedProtocol`，也会写入日志 |

## 7.3 field 属性

| 属性 | 默认值 | 说明 |
| --- | --- | --- |
| `name` | 无 | 必填，作为载荷中的键名 |
| `type` | 参考 `dataType` | 字段声明类型，程序会转换为大写 |
| `dataType` | `type` | 实际生成类型，必须属于支持的兼容集 |
| `min` | `0` | 数值下界，必须是数字且不大于 `max` |
| `max` | `100` | 数值上界，必须是数字 |
| `length` | `8` | 字符串长度，必须为正整数 |
| `data` | 空 | 固定值或模板，具体规则见 7.5 |
| `isSelected` | `true` | 旧属性式格式中为 false 时不输出该字段；课程格式中只代表“不关注”，字段仍占据报文位区间 |
| `isKey` | `false` | 课程格式中为 true 时必须提供固定 `data`；为 false 时根据值域随机生成 |
| `bitIndex` | `-1` | `-1` 表示不提取位；0–63 表示提取相应二进制位 |
| `loopEnd` | `false` | 为 true 时结束当前字段分组，组之间使用 ` || ` |

旧属性式兼容格式允许用 `1`、`true`、`yes` 表示真；课程子元素格式只接受 `true` 或 `false`。为了避免同一文件在两种规则下含义不同，统一写 `true` 或 `false`。

## 7.4 支持的数据类型

| 类型 | 输出形式 | 说明 |
| --- | --- | --- |
| `DEC` | 无符号十进制整数 | 按 `min`/`max` 生成非负整数 |
| `INT` | 十进制整数 | 按 `min`/`max` 生成有符号整数 |
| `UINT` | 非负十进制整数 | 负下界会按 0 处理 |
| `BIN` | 二进制字符串 | 例如 `1101` |
| `OCT` | 八进制字符串 | 例如 `15` |
| `HEX` | 大写十六进制字符串 | 例如 `FF` |
| `FLT` | 3 位小数 | 在 `min`/`max` 之间生成 |
| `DBL` | 6 位小数 | 在 `min`/`max` 之间生成 |
| `STRING` | 大写字母和数字 | 长度由 `length` 决定 |
| `FLAG` | `0` 或 `1` | 不使用普通数值范围 |
| `INT8` | -128 到 127 | 先把配置范围限制在类型范围内 |
| `UINT8` | 0 到 255 | 先把配置范围限制在类型范围内 |
| `INT16` | -32768 到 32767 | 先把配置范围限制在类型范围内 |
| `UINT16` | 0 到 65535 | 先把配置范围限制在类型范围内 |
| `UINT32` | 0 到 4294967295 | 先把配置范围限制在类型范围内 |
| `IP` | IPv4 地址 | 课程样例要求；按 IPv4 数值范围随机生成 |

为了兼容既有文件和课程样例，当前可接受的兼容集共 16 种。本轮不把最终 14 种类型映射作为验收阻塞项；后续拿到教师精确清单后再收敛。

## 7.5 data：固定值与模板

`data` 为空时，程序直接使用随机生成值。

`data` 非空且不含 `${...}` 时，被视为固定值。例如：

```xml
<field name="status" type="UINT8" data="13" />
```

输出始终以 13 为基础。如果同时设置 `bitIndex="2"`，程序会先读取固定整数 13，再提取第 2 位，结果为 1。

`data` 含占位符时，被视为模板。支持：

- `${value}`：生成的基础值；
- `${name}`：字段名；
- `${type}`：数据类型；
- `${timestamp}`：当前毫秒时间戳。

示例：

```xml
<field name="mode" type="STRING" length="4" data="MODE-${value}" />
```

可能输出：

```text
mode=MODE-A7Q2
```

## 7.6 分组与选择

`isSelected="false"` 的字段不会出现在载荷中。`loopEnd="true"` 会结束当前组。例如三个被选择字段在第一个字段后分组，输出可能是：

```text
first=1 || second=MODE-A7Q2;checksum=FF03
```

组内字段用分号 `;` 分隔，组之间用 ` || ` 分隔。

## 7.7 常见 XML 错误

- 根节点不是 `protocol`；
- 没有任何 `field`；
- 字段没有 `name`；
- `dataType` 不在支持列表；
- `min` 或 `max` 不是数字；
- `min` 大于 `max`；
- `length` 小于等于 0；
- `bitIndex` 不在 -1 到 63；
- 对 `STRING`、`FLT` 或 `DBL` 使用位提取；
- 固定 `data` 不是相应进制的整数，却要求位提取；
- XML 标签没有闭合或引号不匹配。

程序会在加载阶段拒绝这些协议并显示具体原因。

## 7.8 课程子元素格式

老师提供的两个协议样例不把字段数据写成 XML 属性，而是使用 `fieldName`、`datatype`、`minimum`、`maximum` 等子元素。该格式中 `bitIndex`、`length`、`loopEnd` 分别表示起始位、位长和结束位，且必须满足 `loopEnd = bitIndex + length`；字段不得重叠，程序按网络字节序/MSB-first 生成二进制 UDP 报文。

课程格式还有以下强制规则：`isSelected` 和 `isKey` 只能写 `true` 或 `false`；标识字段必须提供 `data`；非标识字段根据 `minimum`、`maximum`、`precision` 随机生成；`isSelected=false` 不会删除线缆报文中的位区间；字段长度必须符合各类型约束，例如 `FLAG=8 bit`、`FLT/DBL=64 bit`、`IP=32 bit`。

完整字段映射、字节序假设、两个样例的预期字节数以及 Packet Sender 验收步骤见 `docs/COURSE_PROTOCOL_COMPATIBILITY.md`。

---

# 8. 程序内部怎样工作

## 8.1 总体数据流

```text
XML 文件
  -> ProtocolParser 解析和校验
  -> ProtocolDefinition 内存对象
  -> DataGenerator 生成字段值和文本/二进制载荷
  -> UdpSendController 按时间调度并写入 UDP socket
  -> 任务结束时由 TransmissionRepository 写入 SQLite 汇总
  -> MainWindow 显示进度、预览、状态和查询结果
```

性能基准使用另一条受控路径：

```text
DataGenerator
  -> 本机 UDP 发送端
  -> 动态端口上的本机 UDP 接收端
  -> 序号去重、异常检查、耗时、吞吐和丢包率
```

## 8.2 模块职责

| 模块 | 主要文件 | 责任 |
| --- | --- | --- |
| 程序入口 | `src/main.cpp` | 建立 Qt 应用；支持正常 GUI 和 `--smoke-test` |
| 协议解析 | `src/protocolparser.*` | 读取 XML、设置默认值、校验属性、形成协议对象 |
| 数据生成 | `src/datagenerator.*` | 实现 16 类兼容集、旧文本载荷与课程二进制位打包 |
| 日志仓储 | `src/transmissionrepository.*` | 建立任务汇总表、插入、按时间段/协议/IP 查询 |
| 发送控制 | `src/udpsendcontroller.*` | 参数校验、定时发送、错误停止、loopback 基准 |
| 图形界面 | `src/mainwindow.*` | 收集输入、调用模块、显示反馈 |
| 应用工程 | `src/protocol_sender.pro` | 声明 Qt 模块、源码和资源 |
| 自动测试 | `tests/test_protocol_sender.cpp` | 覆盖核心行为与真实 localhost 接收 |

## 8.3 常规发送的时序

1. 用户点击“开始发送”。
2. 界面重新加载并校验 XML。
3. 控制器校验 IP、端口、频率、数量和 SQLite 可用性。
4. 第一个单次定时器立即触发。
5. 生成载荷并调用 `writeDatagram`。
6. 如果写入字节数不等于载荷长度，停止并报告 UDP 错误。
7. UDP 写入成功后序号加一，发出预览和进度信号。
8. 有限任务达到数量后结束；否则根据累计目标时间安排下一次发送。
9. 任务完成、用户停止或发生错误时，统一记录结束时间、实际总数和状态，并只写入一条 SQLite 汇总日志。
10. 如果汇总日志写入失败，界面明确提示数据库错误，但不会伪造已经入库的记录。

调度使用累计目标时间，而不是简单地每次等待固定间隔，可以减小单次处理时间逐步累积造成的漂移。

## 8.4 SQLite 表结构

表名：`transmission_runs`

| 列名 | 含义 |
| --- | --- |
| `id` | 自动增长主键 |
| `protocol_name` | 协议名 |
| `requested_count` | 用户配置数量；0 表示持续发送 |
| `total_count` | 本次实际成功写入 UDP socket 的总数 |
| `frequency_hz` | 本次配置频率 |
| `start_time` | ISO 格式开始时间 |
| `end_time` | ISO 格式结束时间 |
| `target_ip` | 目标 IP |
| `target_port` | 目标端口 |
| `status` | `completed`、`stopped` 或 `failed` |
| `error_message` | 失败原因；成功或用户停止时为空 |

查询使用参数绑定而不是字符串拼接，可以避免输入内容破坏 SQL 结构。时间条件采用“任务区间与查询区间相交”的语义；协议和 IP 使用模糊匹配。旧版本的 `transmission_logs` 表若已存在会原样保留，因此升级不会删除历史数据。

---

# 9. 自动测试和验收

## 9.1 自动测试覆盖什么

| 测试 | 证明内容 |
| --- | --- |
| `parseProtocol` | 合法 XML 和主要属性能被正确解析 |
| `parseCourseProtocolExamples` | 两个课程样例的元数据、字段和位布局正确 |
| `rejectInvalidPackedLayout` | 错误结束位、字段重叠和过大报文被拒绝 |
| `enforceCourseFieldContract` | 标识字段固定值、长度规则及不关注字段占位符合课程约束 |
| `rejectInvalidProtocol` | 未知数据类型会被拒绝 |
| `generateAllSupportedTypes` | 16 类兼容集都能生成且格式可解析 |
| `generatePayloadGroups` | 选择、模板和 `loopEnd` 分组正确 |
| `literalDataSupportsBitExtraction` | 固定整数数据可以先固定再提取位 |
| `repositoryQuery` | SQLite 任务汇总及时间段、协议、IP 组合查询可用 |
| `controllerRejectsInvalidParameters` | 空协议、非法 IP、端口、频率和负数量被拒绝 |
| `controllerSendsAndLogs` | 有限发送、真实接收、进度及单条完成汇总一致 |
| `controllerSendsPackedCourseDatagram` | 课程二进制报文经真实 UDP 收发且写入单条汇总 |
| `controllerStopsContinuousRun` | 数量为空的持续发送可停止，并写入单条停止汇总 |
| `loopbackBenchmarkDeliversDatagrams` | 2000 条真实接收、序号、异常和丢包统计正确 |

QtTest 还会统计初始化与清理阶段，因此当前最终总数显示为 17 passed。

## 9.2 完整自动验收命令

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build_and_test.ps1 -Package
```

通过条件：

- 17 passed，0 failed；
- 2000 条全部写入并全部接收；
- 异常 0；
- 丢包率 0.00%；
- 打包成功；
- 冒烟测试退出码 0。

## 9.3 人工 GUI 验收步骤

1. 启动 `dist\protocol_sender.exe` 并截图主界面。
2. 确认自动加载 `LoopbackDemo`。
3. 点击“loopback 基线”，截图 2000/2000、异常 0、丢包 0.00%。
4. 默认参数发送 20 条，截图进度、预览和日志表。
5. 清空数量，持续发送数秒并停止，截图停止状态。
6. 输入开始/结束时间段，再分别按协议和 IP 查询，截图组合检索结果。
7. 输入非法 IP，例如 `not-an-ip`，确认拒绝。
8. 输入端口 0 或频率超过 1000，确认拒绝。
9. 关闭后重新打开程序，确认历史日志仍能查到。

## 9.4 最好保存的证据

- PowerShell 中完整测试结果；
- loopback 基线弹窗；
- 有限发送完成界面；
- 持续发送停止界面；
- 日志查询界面；
- 非法输入提示；
- `dist` 中关键 DLL 目录；
- `git log --oneline --decorate`；
- `git status --short -- .` 无输出。

## 9.5 最终判定规则

以下任一情况都不应判为“完全验收”：

- 测试存在 failed；
- loopback 接收数少于发送数且无法解释；
- `qwindows.dll` 或 `qsqlite.dll` 缺失；
- 一次发送没有产生汇总日志，或“实际总数”与成功进度不一致；
- 非法参数导致程序崩溃；
- Git 中遗漏源码或误提交 `build`、`dist`；
- 课程字段位置、长度、标识值或随机值域规则与协议文件不一致。

---

# 10. 打包和在另一台电脑运行

## 10.1 本机生成交付包

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build_and_test.ps1 -Package
```

交付目录为整个 `dist`，不能只复制其中的 exe。

## 10.2 dist 至少应包含

- `protocol_sender.exe`；
- `Qt5Core.dll`、`Qt5Gui.dll`、`Qt5Widgets.dll`；
- `Qt5Network.dll`、`Qt5Sql.dll`、`Qt5Xml.dll`；
- `platforms\qwindows.dll`；
- `sqldrivers\qsqlite.dll`；
- `libgcc_s_dw2-1.dll`、`libstdc++-6.dll`、`libwinpthread-1.dll`。

脚本还可能复制图像格式、SVG、网络 bearer 等插件，这是正常现象。

## 10.3 另一台电脑上的推荐验证

1. 把整个 `dist` 复制到另一台 Windows 电脑。
2. 最好选择没有安装 Qt 的机器，以验证依赖是否齐全。
3. 双击 `protocol_sender.exe`。
4. 运行一次 loopback 基线。
5. 进行一次 20 条有限发送。
6. 关闭并重开，检查日志持久化。

程序首次启动不依赖外部 `sample_protocol.xml`，因为示例协议已通过 Qt Resource 嵌入 exe。需要测试自定义协议时，再单独携带 XML 文件。

## 10.4 冒烟测试

不想显示窗口时可运行：

```powershell
.\dist\protocol_sender.exe --smoke-test
Write-Output $LASTEXITCODE
```

退出码含义：

| 退出码 | 含义 |
| --- | --- |
| 0 | Qt 平台、嵌入协议和 SQLite 均初始化成功 |
| 2 | 嵌入 XML 加载失败 |
| 3 | SQLite 数据库初始化失败 |

---

# 11. 外行人如何学习和修改

## 11.1 推荐学习顺序

1. 阅读 `requirements/original-statement.md`，知道要解决什么问题。
2. 修改 `data/sample_protocol.xml`，观察输入规则。
3. 阅读 `protocolparser.h/.cpp`，理解 XML 如何变成内存对象。
4. 阅读 `datagenerator.h/.cpp`，理解每种数据如何生成。
5. 阅读 `transmissionrepository.h/.cpp`，理解日志存储。
6. 阅读 `udpsendcontroller.h/.cpp`，理解发送与性能测试。
7. 阅读 `mainwindow.h/.cpp`，理解界面如何串联模块。
8. 阅读 `tests/test_protocol_sender.cpp`，把功能和测试一一对应。
9. 阅读 `scripts/build_and_test.ps1`，理解交付流水线。

## 11.2 最安全的入门实验

复制示例 XML 为新文件，不修改 C++：

1. 把 `temperature` 的范围改为 0–10；
2. 把 `mode` 的模板改为 `STATE-${value}`；
3. 把某个字段设为 `isSelected="false"`；
4. 给某个字段设置 `loopEnd="true"`；
5. 在界面加载新 XML，观察载荷变化。

这些实验只改变输入数据，风险最低。

## 11.3 修改代码前的原则

- 一次只改一个主题；
- 修改前先运行测试，确认基线为绿色；
- 修改后补充或调整测试；
- 再运行完整 `-Package` 验证；
- 不直接编辑 `build` 和 `dist` 中的文件；
- 不引入新依赖，除非任务明确要求；
- 不把性能样本写成普遍保证；
- 不使用 `git add -A` 混入上层仓库其他文件。

## 11.4 增加一种字段类型时

必须同步修改：

1. `supportedProtocolTypes()`：加入类型名；
2. `DataGenerator::generateFieldValue()`：实现生成规则；
3. 若支持 `bitIndex`，同步更新解析器中的整数类型列表；
4. `generateAllSupportedTypes` 或新增测试：验证输出范围与格式；
5. 示例 XML、README、测试报告和本说明书：更新口径；
6. 运行完整构建、测试、打包；
7. 用独立 Git 提交记录为什么增加以及如何验证。

## 11.5 修改界面时

Qt 5.9.7 项目采用传统 `SIGNAL`/`SLOT` 写法以兼容旧工具链。增加控件时需要：

1. 在 `mainwindow.h` 声明控件成员或槽函数；
2. 在 `buildUi()` 创建控件并加入布局；
3. 使用 `connect` 连接信号和槽；
4. 把业务逻辑放在解析器、生成器、仓储或控制器中，界面主要负责输入和显示；
5. 重新运行 qmake 和测试。项目脚本会自动完成这些步骤。

---

# 12. Git 提交规范

## 12.1 当前分支

```text
main
```

## 12.2 查看当前项目提交链

提交哈希会随新增提交变化，不应把旧哈希抄死在说明书里。以仓库实际结果为准：

```powershell
git log --oneline --decorate -10
```

本轮建议拆成三次：协议解析与生成、任务汇总日志与界面、构建证据与说明文档。每次提交都应能说明“为什么改、改动边界和如何验证”。

## 12.3 每次提交的操作顺序

在项目根目录执行：

```powershell
git status --short -- .
git diff -- .
git add -- README.md
git add -- docs/CITEL-T-007_完整项目说明书.md
git add -- docs/CITEL-T-007_完整项目说明书_最终版.docx
git diff --cached --check
git diff --cached --stat
git commit
```

只添加本次负责的文件。确认暂存差异后再提交。

## 12.4 Lore 提交格式

```text
<第一行：为什么要做，而不是简单复述改了什么>

<背景、约束和方案说明>

Constraint: <外部约束>
Rejected: <拒绝的方案> | <拒绝原因>
Confidence: high
Scope-risk: narrow
Reversibility: clean
Directive: <未来修改者需要遵守的事项>
Tested: <实际运行过的检查>
Not-tested: <还没有验证的内容>
```

## 12.5 推荐的后续提交拆分

| 场景 | 建议提交内容 | 不应混入 |
| --- | --- | --- |
| 修改 XML 协议 | 示例 XML、解析规则测试、协议说明 | GUI 美化 |
| 新增数据类型 | 解析器、生成器、对应测试、类型文档 | 打包脚本重构 |
| 修改发送逻辑 | 控制器、发送测试、性能解释 | 无关报告排版 |
| 修改数据库 | 仓储、迁移/查询测试、数据位置说明 | 协议生成逻辑 |
| 修改 UI | MainWindow、必要资源、人工截图说明 | 未相关核心算法 |
| 修改部署 | `.pro`、构建脚本、部署文档、冒烟证据 | 功能需求扩张 |
| 修改说明书 | Markdown、DOCX、README 入口 | 功能代码 |

## 12.6 推送前检查

```powershell
git status --short -- .
git log --oneline --decorate -10
git remote -v
```

如果还没有远程：

```powershell
$repoUrl = Read-Host '请输入远程仓库地址，例如 https://github.com/your-name/course-repository.git'
git remote add origin $repoUrl
git push -u origin main
```

---

# 13. 五分钟课程验收演示脚本

## 第 1 分钟：说明目标

“这是一个基于 Qt 5.9.7 和 C++11 的 XML 协议 UDP 发生器。XML 决定字段结构，程序随机生成载荷，通过 UDP 发送，并在每次任务结束时把配置和结果汇总到 SQLite，还可以查询日志和执行真实本机收包测试。”

展示 `data/sample_protocol.xml` 和主界面。

## 第 2 分钟：展示常规发送

使用默认 `10 Hz、20 条、127.0.0.1、39001`，点击开始。说明进度只统计 UDP 写入成功的报文；任务完成后日志表新增一条汇总，其中配置数量和实际总数均为 20。

## 第 3 分钟：展示持续发送和查询

清空数量，开始后停止。随后使用协议名和 IP 查询日志，说明数据库位于用户应用数据目录，关闭程序后仍保留。

## 第 4 分钟：展示真实接收和性能

点击“loopback 基线”，说明程序动态创建本机接收端，为 2000 条报文增加序号，统计唯一接收数、异常数、吞吐和丢包率。强调普通 UDP 写入与真正收到是两种证据。

## 第 5 分钟：展示质量证据

展示 PowerShell 中的 `17 passed, 0 failed`、`dist` 打包结果和 Git 提交历史。最后主动说明性能是本机样本；课程字段规则已经实现，最终 14 种类型映射按本轮约定暂不作为验收项。

---

# 14. 故障排查

## 14.1 Required Qt tool was not found

原因：脚本指定的 Qt 根目录不正确，或没有安装 MinGW 套件。

处理：先用 `Test-Path` 检查 3.3 中两个文件。Qt 在其他目录时传入：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build_and_test.ps1 -QtRoot 'D:\Qt\Qt5.9.7' -Package
```

## 14.2 qmake 版本不对或指向 Anaconda

不要直接运行环境中的 `qmake`。项目脚本使用绝对路径，可以避开 Anaconda 或其他 Qt。若手工排查，使用完整路径调用。

## 14.3 链接错误或出现 MSVC `.lib`

原因通常是混用了 MSVC Qt 和 MinGW。确认 qmake 来自 `mingw53_32`，make 来自 `Tools\mingw530_32`，然后重新运行项目脚本。

## 14.4 中文路径导致 windeployqt 异常

不要直接对中文路径下的 exe 手工运行旧版 `windeployqt`。项目脚本会创建纯英文临时目录并验证 DLL，使用脚本即可。

## 14.5 This application failed to start because no Qt platform plugin could be initialized

检查 `dist\platforms\qwindows.dll`。重新执行带 `-Package` 的完整脚本，不要只复制 exe。若脚本提示 `dist is currently in use`，先关闭正在运行的旧程序；脚本同时会在 `dist-candidate` 留下一份已通过冒烟测试的新包。

## 14.6 SQLite driver not loaded

检查 `dist\sqldrivers\qsqlite.dll` 和 `Qt5Sql.dll`。重新打包。若数据库目录没有写权限，确认 `%APPDATA%` 可写。

## 14.7 协议加载失败

检查根节点、字段名、数据类型、数值范围、字符串长度、位索引和 XML 标签。先从可运行的 `sample_protocol.xml` 复制，再逐项修改，最容易定位问题。

## 14.8 点击发送但接收端没有数据

依次检查：

1. 目标 IP 是否正确；
2. 接收程序是否监听 UDP 而不是 TCP；
3. 端口是否一致；
4. Windows 防火墙是否拦截；
5. 接收程序是否绑定正确网卡；
6. 先运行本项目 loopback，判断是本项目问题还是外部网络问题。

## 14.9 loopback 吞吐与报告不同

这是正常波动。关闭高负载程序并重复 3–5 次；记录均值、最低值、最高值。不要为了追求固定数字而修改测试断言，测试只要求吞吐为正且收发统计正确。

## 14.10 Git 状态显示其他练习有改动

项目位于更大的课程仓库中。请在项目根目录使用：

```powershell
git status --short -- .
```

提交时列出具体文件，避免 `git add -A`。不要删除或回滚自己不确定来源的上层文件。

## 14.11 日志过多需要清理

先关闭程序，备份 `%APPDATA%\protocol_sender\protocol_sender.db`。确认备份可用后，才删除原数据库；下次启动会自动重建空表。该操作会永久清除当前日志，课程验收前不要贸然执行。

---

# 15. 最终交付清单

## 15.1 自动化

- [ ] 使用指定 Qt 5.9.7 MinGW 套件；
- [ ] 完整脚本退出码为 0；
- [ ] 17 passed，0 failed；
- [ ] loopback 发送 2000、接收 2000；
- [ ] 异常 0、丢包率 0.00%；
- [ ] `dist` 打包完成；
- [ ] `--smoke-test` 退出码 0。

## 15.2 人工功能

- [ ] 自动或手工加载 XML 成功；
- [ ] 默认 20 条发送完成；
- [ ] 数量留空可持续发送；
- [ ] 停止按钮有效；
- [ ] 预览、进度、汇总日志中的实际总数符合预期；
- [ ] 开始/结束时间段、协议、IP 查询有效；
- [ ] 重启后日志仍存在；
- [ ] 非法 XML、IP、端口、频率会被拒绝；
- [ ] 保存必要截图。

## 15.3 文档和诚实性

- [ ] README、测试报告、部署说明、本说明书齐全；
- [ ] 性能数字标注为本机样本；
- [ ] 不把 UDP socket 写入描述成远端必然收到；
- [ ] 已披露 AI 辅助内容；
- [ ] 已注明最终 14 种类型映射不是本轮验收阻塞项；
- [ ] 已填写课程平台要求的姓名、学号或其他元数据。

## 15.4 Git

- [ ] 当前分支正确；
- [ ] `git status --short -- .` 无遗漏；
- [ ] `build`、`dist` 和数据库未进入 Git；
- [ ] 每个提交主题单一并符合 Lore 格式；
- [ ] 已配置远程并推送当前分支；
- [ ] 在远程网页上确认最新提交和文件均存在。

---

# 附录 A：命令速查

## 环境检查

```powershell
Test-Path 'C:\Qt\Qt5.9.7\5.9.7\mingw53_32\bin\qmake.exe'
Test-Path 'C:\Qt\Qt5.9.7\Tools\mingw530_32\bin\mingw32-make.exe'
git --version
```

## 完整构建、测试和打包

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build_and_test.ps1 -Package
```

## 启动程序

```powershell
.\dist\protocol_sender.exe
```

## 无界面冒烟检查

```powershell
.\dist\protocol_sender.exe --smoke-test
Write-Output $LASTEXITCODE
```

## 项目范围 Git 检查

```powershell
git branch --show-current
git status --short -- .
git log --oneline --decorate -- .
```

## 数据库位置

```powershell
Join-Path $env:APPDATA 'protocol_sender\protocol_sender.db'
```

---

# 附录 B：验收记录模板

项目版本/提交：____________________________

验收日期：____________________________

验收电脑：____________________________

Qt 版本：____________________________

MinGW 版本：____________________________

自动测试：通过 / 不通过

通过数量：________ 失败数量：________

loopback 请求：________ 写入：________ 接收：________

异常报文：________ 丢包率：________

端到端吞吐样本：________ Hz

有限发送：通过 / 不通过

持续发送和停止：通过 / 不通过

SQLite 持久化：通过 / 不通过

日志组合查询：通过 / 不通过

非法输入拒绝：通过 / 不通过

独立电脑运行：通过 / 未测试 / 不通过

最终 14 种字段类型已核对：是 / 暂不验收

GUI 截图已保存：是 / 否

远程 Git 已推送：是 / 否

问题与备注：

__________________________________________________________________

__________________________________________________________________

验收人签字：____________________________

---

# 附录 C：完成状态结论

当前代码、自动测试、loopback 接收、SQLite、Windows 打包和无界面冒烟检查已经完成并实际验证。一个完全不了解项目的读者，可以按照本说明书安装或核对环境、获取分支、运行一键脚本、操作界面、编辑 XML、执行验收、理解源码结构并准备 Git 交付。

正式提交前仍需项目负责人完成两项外部工作：

1. 人工执行 GUI 全流程并保存课程要求的截图；
2. 推送最新提交，并在课程平台完成最终提交。

最终 14 种字段类型的精确映射按本轮约定暂不影响上述验收，可在教师给出完整清单后继续收敛。
