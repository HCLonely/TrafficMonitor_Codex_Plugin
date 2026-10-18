# CodexUsage TrafficMonitor 插件

这是一个 TrafficMonitor 插件，用于从 ChatGPT 的用量接口获取并显示 Codex 剩余用量。

## 显示项目

- `Codex 5h Remaining`：5 小时用量窗口的剩余百分比。
- `Codex Weekly Remaining`：周用量窗口的剩余百分比。

鼠标提示会显示剩余百分比、重置倒计时，或最近一次获取失败的错误信息。

## 数据来源

插件读取 `auth.json` 中的 `tokens.access_token`。可在插件选项中指定文件路径；留空时，按以下规则确定默认路径（不会在文件读取失败后尝试其他位置）：

1. 设置了 `CODEX_HOME` 时，使用 `%CODEX_HOME%\auth.json`。
2. 否则，使用 `%USERPROFILE%\.codex\auth.json`。
3. 两个环境变量均不可用时，使用 `.codex\auth.json`。

随后调用 `https://chatgpt.com/backend-api/wham/usage` 获取用量数据。

## 插件选项

在 TrafficMonitor 的插件管理中选中 **Codex Usage**，打开插件选项：

- **访问令牌路径**：填写 `auth.json` 的完整文件路径，也可点击“浏览”选择文件；留空恢复默认路径。
- **系统代理（自动读取）**：选择后自动使用 Windows 的系统及当前用户代理设置，支持自动检测和 PAC 自动配置。修改系统代理后，后续刷新会读取新设置。
- **直连（不使用代理）**：默认模式，忽略系统代理，直接连接用量接口。
- **手动代理**：填写 HTTP 代理地址，如 `127.0.0.1:7890`、`http://127.0.0.1:7890` 或 `[::1]:7890`。HTTPS 用量请求通过 HTTP CONNECT 隧道传输。不支持 SOCKS、HTTPS 代理端点或带账号密码的代理地址。

点击“确定”后，设置保存到 `CodexUsage.dll` 同目录的 `CodexUsage.ini`，自动重新获取用量；如果旧请求仍在进行，会在旧请求结束后的下一次轮询使用新设置。点击“取消”不保存修改。插件正常情况下每分钟刷新一次。

配置文件只保存令牌文件的路径，不保存令牌内容。请确保插件目录可写；保存失败时会显示错误，并保留原配置。自定义路径不展开 `%变量%` 或 `~`，建议使用完整路径。

## 安装

将生成的 `CodexUsage.dll` 复制到 TrafficMonitor 的 `plugins` 目录，然后重启 TrafficMonitor，并在插件管理或显示项目设置中启用这两个显示项目。

插件与 TrafficMonitor 的架构需要匹配。令牌过期或缺少 `tokens.access_token` 时，需要先通过 Codex 重新登录。构建、测试和排错方法见 [开发说明](./Development.md)。

## 示例

![示例1](./example1.png?raw=true)
![示例2](./example2.png?raw=true)

## LICENSE

[MIT LICENSE](./LICENSE)
