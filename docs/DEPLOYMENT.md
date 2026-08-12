# 构建与部署说明

## 环境要求

- Windows；
- Qt 5.9.7 `mingw53_32`；
- Qt 配套的 MinGW 5.3.0 32-bit；
- PowerShell 5 或更高版本。

默认安装布局：

- `C:\Qt\Qt5.9.7\5.9.7\mingw53_32\bin\qmake.exe`
- `C:\Qt\Qt5.9.7\Tools\mingw530_32\bin\mingw32-make.exe`

不要混用 Anaconda 的 MSVC Qt `.lib` 与 MinGW 编译器。

## 推荐流程

在项目根目录执行：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build_and_test.ps1 -Package
```

Qt 安装在其他位置时传入 `-QtRoot`：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build_and_test.ps1 -QtRoot D:\Qt\Qt5.9.7 -Package
```

成功后会生成：

- `build\app\release\protocol_sender.exe`：开发构建；
- `build\tests\release\protocol_sender_tests.exe`：测试程序；
- `dist\protocol_sender.exe`：附带运行库的交付版本。

打包完成后，脚本会自动运行 `dist\protocol_sender.exe --smoke-test`。该模式不显示窗口，验证 Qt 平台初始化、嵌入 XML 和 SQLite 驱动，任一环节失败都会返回非零退出码。

由于 Qt 5.9 的 `windeployqt` 不能可靠处理中文路径，脚本会先在纯英文临时目录部署并核对必要 DLL，再复制到项目的 `dist`。

`dist` 中应至少包含：

- `Qt5Core.dll`、`Qt5Widgets.dll`、`Qt5Network.dll`、`Qt5Sql.dll`、`Qt5Xml.dll`；
- `platforms\qwindows.dll`；
- `sqldrivers\qsqlite.dll`；
- `libgcc_s_dw2-1.dll`、`libstdc++-6.dll`、`libwinpthread-1.dll`。

## 运行和数据位置

```powershell
.\dist\protocol_sender.exe
```

示例 XML 以 Qt Resource 形式嵌入应用。SQLite 数据库写入 `QStandardPaths::AppDataLocation`，在当前 Windows 环境中通常为：

```text
%APPDATA%\protocol_sender\protocol_sender.db
```

`build` 和 `dist` 都是可再生成目录，不进入 Git。
