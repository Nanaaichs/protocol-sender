# 验证证据索引

验证日期：2026-08-08

## 可复现命令

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build_and_test.ps1 -Package
```

## 已确认结果

- 应用和测试均使用 Qt 5.9.7 `mingw53_32` 成功链接；
- QtTest：12 passed，0 failed；
- loopback：2000 写入、2000 接收、0 异常、0.00% 丢包；
- 最终干净构建端到端样本：26.590 ms，约 75,216.81 Hz；
- `dist` 包含 `platforms\qwindows.dll` 和 `sqldrivers\qsqlite.dll`；
- `dist\protocol_sender.exe --smoke-test` 退出码为 0；
- SQLite 数据库成功创建于 `%APPDATA%\protocol_sender\protocol_sender.db`。

## 证据边界

- `build` 与 `dist` 为可再生成目录，按约定不提交 Git；
- 吞吐值依赖当前机器、运行负载和样本规模；
- 仓库仍需老师的完整原始题面来最终确认 15 类字段清单。
