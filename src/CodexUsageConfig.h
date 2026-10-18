#pragma once

#include <string>

enum class ProxyMode { System, Direct, Manual };

struct CodexUsageConfig {
    std::wstring authPath;
    ProxyMode proxyMode = ProxyMode::Direct;
    std::wstring proxyServer;
};

std::wstring ResolveCodexConfigPath();
CodexUsageConfig LoadCodexConfig(const std::wstring& path = ResolveCodexConfigPath());
bool SaveCodexConfig(const CodexUsageConfig& config, std::wstring* error,
                     const std::wstring& path = ResolveCodexConfigPath());
bool NormalizeCodexConfig(CodexUsageConfig* config, std::wstring* error);
std::wstring ResolveCodexAuthPath(const CodexUsageConfig& config);
