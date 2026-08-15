# 检验、学习与复现指南

这份指南面向第一次接触本项目的人。推荐顺序是：先用一条命令证明环境和交付包可用，再沿数据流阅读代码，最后做一次人工验收。

## 1. 五分钟自动检验

在项目根目录打开 PowerShell，先确认当前分支和工作区：

```powershell
git branch --show-current
git status --short -- .
```

当前交付位于 `main` 分支。执行 `git status --short` 时应没有未提交的项目文件。

执行完整构建、测试、部署和冒烟检查：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build_and_test.ps1 -Package
```

脚本使用绝对工具路径，不依赖系统 `PATH`。默认工具链是：

```text
C:\Qt\Qt5.9.7\5.9.7\mingw53_32\bin\qmake.exe
C:\Qt\Qt5.9.7\Tools\mingw530_32\bin\mingw32-make.exe
```

成功标准：

- QtTest 末尾显示 `18 passed, 0 failed`；
- 2000 条 loopback 测试中，发送数和接收数均为 2000，异常报文为 0、丢包率为 0.00%；
- 输出 `Package ready:` 和 `Build and tests completed successfully.`；
- 打包程序的 `--smoke-test` 返回退出码 0。

如果正式 `dist` 中的程序仍在运行，脚本会输出警告并把新包放入 `dist-candidate`，随后仍会在该目录完成冒烟测试。关闭旧程序后重新执行命令即可刷新 `dist`。

生成物：

| 路径 | 用途 |
| --- | --- |
| `build\app\release\protocol_sender.exe` | 开发构建 |
| `build\tests\release\protocol_sender_tests.exe` | QtTest 测试程序 |
| `dist\protocol_sender.exe` | 包含 Qt/MinGW 运行库的交付版本 |
| `dist-candidate\protocol_sender.exe` | 仅在正式 `dist` 被运行中的程序占用时生成的已验证候选包 |
| `%APPDATA%\protocol_sender\protocol_sender.db` | 运行后产生的 SQLite 日志库 |

`build` 和 `dist` 都是可再生成目录，已经被 Git 忽略。

## 2. 人工验收

启动交付版本：

```powershell
.\dist\protocol_sender.exe
```

按下面的顺序操作并保留截图：

1. 确认窗口启动后自动加载 `LoopbackDemo`；也可点“选择协议”加载 `data\sample_protocol.xml`。
2. 点击“loopback 基线”，确认弹窗中的请求、写入和接收数量均为 2000，异常为 0，丢包率为 0.00%。这是端到端 UDP 接收检验。
3. 保持默认参数 `10 Hz / 20 条 / 127.0.0.1 / 39001`，点击“开始发送”，确认进度到 20、预览区出现载荷、日志表新增 1 条汇总记录，并显示“配置数量 20、实际总数 20、状态 已完成”。
4. 将“数量”留空，开始后观察持续发送，再点击“停止发送”，确认进度条停止滚动并显示实际发送数量，状态栏报告任务已停止。
5. 用“开始时间”和“结束时间”限定时间段，再分别按协议 `LoopbackDemo`、IP `127.0.0.1` 或组合条件查询日志。
6. 输入非法 IP、端口或频率，确认程序拒绝启动并说明原因。

注意：普通“开始发送”能证明报文已成功写入本机 UDP socket 并写入 SQLite，但 UDP 本身不保证远端收到。项目中的“loopback 基线”和自动化测试才负责验证真实接收与丢包统计。

## 3. 推荐学习路线

先理解输入和目标，再顺着报文的数据流读代码：

1. `requirements\original-statement.md`：了解题目目标及仍需向老师核对的原始要求。
2. `data\sample_protocol.xml`：观察协议名、字段类型、范围、固定值、模板、位提取和分组属性。
3. `src\protocolparser.h/.cpp`：XML 如何变成 `ProtocolDefinition`，以及非法结构怎样被拒绝。
4. `src\datagenerator.h/.cpp`：现有兼容类型怎样生成，旧格式如何替换模板，课程格式如何按 bit 布局打包。
5. `src\transmissionrepository.h/.cpp`：SQLite 表初始化、写入和组合查询。
6. `src\udpsendcontroller.h/.cpp`：参数校验、定时调度、UDP 发送、失败停止、loopback 接收统计。
7. `src\mainwindow.h/.cpp`：界面事件如何调用上述模块，以及进度、预览、检索和错误提示如何呈现。
8. `tests\test_protocol_sender.cpp`：把每条需求和相应测试对应起来。
9. `scripts\build_and_test.ps1`：理解从源码到可交付目录的完整流水线。

核心数据流如下：

```text
XML -> ProtocolParser -> ProtocolDefinition -> DataGenerator -> UDP socket
                                                    |
                                                    +-> 任务结束时写 SQLite 汇总日志

loopback 基线: DataGenerator -> 本机 UDP 发送端 -> 本机 UDP 接收端 -> 丢包/吞吐统计
```

建议边读边做三个小实验：

- 修改示例 XML 的 `min`/`max`、`data` 或 `bitIndex`，观察载荷变化；
- 在测试中故意加入未知类型或非法范围，观察解析器给出的错误；
- 重复运行 loopback 测试，记录吞吐波动，并解释为什么它不等于 GUI 的 1–1000 Hz 常规发送频率。

## 4. 从新副本复现

在一台安装了相同 Qt 套件的 Windows 机器上：

```powershell
git clone <课程仓库地址>
cd <课程仓库>\projects\CITEL-T-007
git switch main
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build_and_test.ps1 -Package
.\dist\protocol_sender.exe
```

如果 Qt 不在默认位置：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build_and_test.ps1 -QtRoot D:\Qt\Qt5.9.7 -Package
```

当前仓库没有配置远程地址，因此在另一台机器复现前，需要先把分支推送到你自己的课程仓库，或连同 `.git` 历史复制整个仓库。只复制项目源码也能构建，但无法复现提交历史。

## 5. 常见问题

- `qmake` 指向 Anaconda 或其他 Qt：不要手工调用环境中的 `qmake`；直接运行项目脚本。
- 出现 `.lib` 与 MinGW 不兼容：混用了 MSVC Qt 和 MinGW，必须使用 `mingw53_32` 套件。
- 中文路径下 `windeployqt` 异常：脚本已通过纯英文临时目录规避，不要删掉该部署步骤。
- 提示找不到 `qwindows.dll` 或 `qsqlite.dll`：重新执行带 `-Package` 的脚本；脚本会逐项校验运行库。
- 性能数字与报告不完全一致：吞吐受 CPU 调度和机器负载影响，应关注发送/接收数量、异常数和丢包率，并把吞吐视为本机样本。

## 6. 提交前最终清单

- QtTest 为 18 passed、0 failed；
- 打包后冒烟检查退出码为 0；
- 人工完成有限发送、持续发送、日志查询和非法输入检查；
- 保存关键界面截图；
- 若老师提供最终名单，再核对并收敛 14 种字段类型；本轮不以此作为验收阻塞项；
- 在项目根目录执行 `git status --short -- .`，没有遗漏的项目文件；
- 阅读 `TEST_REPORT.md` 中的剩余风险，不把单机性能样本表述为跨机器保证。
