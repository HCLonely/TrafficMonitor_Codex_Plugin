#include "CodexUsageConfig.h"

#include <Windows.h>
#include <winhttp.h>
#include <cwctype>
#include <vector>

extern "C" IMAGE_DOS_HEADER __ImageBase;

namespace {
std::wstring Trim(const std::wstring& text) {
    const auto first = text.find_first_not_of(L" \t\r\n");
    return first == std::wstring::npos ? L"" : text.substr(first, text.find_last_not_of(L" \t\r\n") - first + 1);
}

std::wstring ReadEnv(const wchar_t* name) {
    const DWORD size = GetEnvironmentVariableW(name, nullptr, 0);
    if (!size) return {};
    std::vector<wchar_t> buffer(size);
    const DWORD length = GetEnvironmentVariableW(name, buffer.data(), size);
    return length && length < size ? std::wstring(buffer.data(), length) : L"";
}

std::wstring ReadValue(const std::wstring& path, const wchar_t* key, const wchar_t* fallback = L"") {
    std::vector<wchar_t> buffer(32768);
    GetPrivateProfileStringW(L"CodexUsage", key, fallback, buffer.data(), static_cast<DWORD>(buffer.size()), path.c_str());
    return buffer.data();
}
}

std::wstring ResolveCodexConfigPath() {
    std::vector<wchar_t> buffer(32768);
    const DWORD length = GetModuleFileNameW(reinterpret_cast<HMODULE>(&__ImageBase), buffer.data(), static_cast<DWORD>(buffer.size()));
    const std::wstring module(buffer.data(), length);
    return module.substr(0, module.find_last_of(L"\\/") + 1) + L"CodexUsage.ini";
}

CodexUsageConfig LoadCodexConfig(const std::wstring& path) {
    CodexUsageConfig config;
    config.authPath = ReadValue(path, L"auth_path");
    config.proxyServer = ReadValue(path, L"proxy_server");
    const auto mode = ReadValue(path, L"proxy_mode", L"direct");
    if (mode == L"system") config.proxyMode = ProxyMode::System;
    if (mode == L"direct") config.proxyMode = ProxyMode::Direct;
    if (mode == L"manual") config.proxyMode = ProxyMode::Manual;
    return config;
}

bool NormalizeCodexConfig(CodexUsageConfig* config, std::wstring* error) {
    if (error) error->clear();
    if (config->proxyMode != ProxyMode::System && config->proxyMode != ProxyMode::Direct && config->proxyMode != ProxyMode::Manual) {
        if (error) *error = L"请选择有效的代理模式。";
        return false;
    }
    config->authPath = Trim(config->authPath);
    config->proxyServer = Trim(config->proxyServer);
    if (config->authPath.find_first_of(L"\r\n") != std::wstring::npos ||
        config->proxyServer.find_first_of(L"\r\n") != std::wstring::npos) {
        if (error) *error = L"路径和代理地址不能包含换行。";
        return false;
    }
    if (config->proxyMode != ProxyMode::Manual) return true;
    std::wstring url = config->proxyServer;
    if (url.find(L"://") == std::wstring::npos) url = L"http://" + url;
    URL_COMPONENTS parts{};
    parts.dwStructSize = sizeof(parts);
    parts.dwHostNameLength = parts.dwUserNameLength = parts.dwPasswordLength =
        parts.dwUrlPathLength = parts.dwExtraInfoLength = static_cast<DWORD>(-1);
    if (!WinHttpCrackUrl(url.c_str(), 0, 0, &parts) || parts.nScheme != INTERNET_SCHEME_HTTP ||
        !parts.dwHostNameLength || !parts.nPort || parts.dwUserNameLength || parts.dwPasswordLength ||
        parts.dwExtraInfoLength || (parts.dwUrlPathLength && std::wstring(parts.lpszUrlPath, parts.dwUrlPathLength) != L"/")) {
        if (error) *error = L"请输入 HTTP 代理地址，例如 127.0.0.1:7890 或 http://127.0.0.1:7890。";
        return false;
    }
    std::wstring host(parts.lpszHostName, parts.dwHostNameLength);
    for (wchar_t ch : host) {
        if (iswspace(ch) || ch == L';' || ch == L'=') {
            if (error) *error = L"代理主机名无效。";
            return false;
        }
    }
    if (host.find(L':') != std::wstring::npos && host.front() != L'[') host = L"[" + host + L"]";
    config->proxyServer = host + L":" + std::to_wstring(parts.nPort);
    return true;
}

bool SaveCodexConfig(const CodexUsageConfig& input, std::wstring* error, const std::wstring& path) {
    auto config = input;
    if (!NormalizeCodexConfig(&config, error)) return false;
    const wchar_t* mode = config.proxyMode == ProxyMode::Manual ? L"manual" :
        config.proxyMode == ProxyMode::Direct ? L"direct" : L"system";
    const std::wstring text = L"\ufeff[CodexUsage]\r\nauth_path=" + config.authPath +
        L"\r\nproxy_mode=" + mode + L"\r\nproxy_server=" + config.proxyServer + L"\r\n";
    const auto temporary = path + L".tmp";
    HANDLE file = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        if (error) *error = L"无法保存配置，请检查目录写入权限：" + path;
        return false;
    }
    DWORD written = 0;
    const DWORD size = static_cast<DWORD>(text.size() * sizeof(wchar_t));
    const bool saved = WriteFile(file, text.data(), size, &written, nullptr) && written == size;
    CloseHandle(file);
    if (!saved || !MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        DeleteFileW(temporary.c_str());
        if (error) *error = L"配置写入失败：" + path;
        return false;
    }
    WritePrivateProfileStringW(nullptr, nullptr, nullptr, path.c_str());
    return true;
}

std::wstring ResolveCodexAuthPath(const CodexUsageConfig& config) {
    if (!config.authPath.empty()) return config.authPath;
    auto home = ReadEnv(L"CODEX_HOME");
    if (home.empty()) {
        home = ReadEnv(L"USERPROFILE");
        home = home.empty() ? L".codex" : home + L"\\.codex";
    }
    if (home.back() != L'\\' && home.back() != L'/') home += L'\\';
    return home + L"auth.json";
}
