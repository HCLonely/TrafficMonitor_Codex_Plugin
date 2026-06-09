# CodexUsage TrafficMonitor 插件

这是一个 TrafficMonitor 插件，用于从 ChatGPT 的用量接口获取并显示 Codex 剩余用量。

## 显示项目

- `Codex 5h Remaining`：5 小时用量窗口的剩余百分比。
- `Codex Weekly Remaining`：周用量窗口的剩余百分比。

鼠标提示会显示剩余百分比、重置倒计时，或最近一次获取失败的错误信息。

## 数据来源

插件会按以下顺序读取 Codex 访问令牌：

1. `%CODEX_HOME%\auth.json`
2. `%USERPROFILE%\.codex\auth.json`
3. `.codex\auth.json`

随后调用 `https://chatgpt.com/backend-api/wham/usage` 获取用量数据。

## 构建

```powershell
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

## 安装

将生成的 `CodexUsage.dll` 复制到 TrafficMonitor 的 `plugins` 目录，然后重启 TrafficMonitor，并在插件管理或显示项目设置中启用这两个显示项目。
