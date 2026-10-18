# CodexUsage 开发说明

## 环境与依赖

- Windows，CMake 3.21 或更新版本，支持 C++20 的编译器，以及 Windows 资源编译器。
- 可使用 Visual Studio C++ 工具链和 Windows SDK，或 MinGW-w64 与 Ninja。
- 需要 TrafficMonitor 的 `PluginInterface.h`。CMake 自动查找项目内或相邻 `TrafficMonitor/include`；其他位置通过 `-DTRAFFICMONITOR_INCLUDE_DIR=路径` 指定。
- 系统库：WinHTTP、Comdlg32、Shlwapi；测试额外使用 Winsock2。不需要第三方网络库。

## 构建与测试

在 `CodexUsage` 目录运行：

```powershell
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

Visual Studio 多配置生成器产物位于 `build/Release`；Ninja 产物位于 `build`。DLL 架构应与宿主一致。CI 分别构建 x86、x64、ARM64，ARM64 不在 x64 runner 上运行测试。版本由 `CODEX_USAGE_VERSION` 控制，默认 `0.1.4`。

测试覆盖用量解析、显示格式、中文路径配置往返、默认路径、三种代理模式、无效地址、写入失败和本地 HTTP CONNECT 代理路由。测试仅写临时配置及合成令牌文件，不读取真实令牌或访问真实用量接口。系统代理的实际 PAC/WPAD 行为以及 TrafficMonitor 内的界面操作，需要在对应 Windows 环境手动验证。

## 配置与网络实现

- `src/CodexUsageConfig.*`：解析默认令牌路径，校验 HTTP 代理地址，读写 DLL 同目录的 UTF-16 LE INI 文件。先写临时文件再替换，失败时保留原文件。
- `src/CodexUsageOptions.*`、`src/CodexUsage.rc`：原生中文模态设置界面、JSON 文件选择、错误提示。
- `src/CodexUsageFetcher.*`：每次请求使用一份配置快照；系统代理使用 `WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY`，直连使用 `WINHTTP_ACCESS_TYPE_NO_PROXY`，手动代理使用 `WINHTTP_ACCESS_TYPE_NAMED_PROXY`。[Microsoft WinHttpOpen 文档](https://learn.microsoft.com/en-us/windows/win32/api/winhttp/nf-winhttp-winhttpopen) 说明自动模式读取系统与当前用户代理设置，要求 Windows 8.1 或更新版本。
- `src/CodexUsagePlugin.cpp`：保存成功后清空旧显示，触发刷新；配置版本标记防止在途旧请求覆盖新配置结果。

INI 节为 `[CodexUsage]`，键为 `auth_path`、`proxy_mode`（`system` / `direct` / `manual`）、`proxy_server`。没有配置文件时使用默认路径并直连；缺少或无法识别 `proxy_mode` 时也默认直连，已保存的有效代理模式会保留。手工编辑后需重启 TrafficMonitor。

## 排错

- 配置保存失败：检查 DLL 所在目录是否可写，以及 `CodexUsage.ini` 是否被锁定。
- 令牌读取失败：检查提示中的文件路径、读取权限和 `tokens.access_token` 字段；自定义文件读取失败不会回退到其他账号的默认令牌。
- HTTP 401：先通过 Codex 重新登录以更新令牌。
- 网络请求失败：核对系统代理或手动代理端口，确认代理支持 HTTP CONNECT；可切换直连进行比较。
- 编译时找不到 `PluginInterface.h`：通过上述 CMake 参数指定包含该文件的目录；更换编译器或生成器时使用新的构建目录。
