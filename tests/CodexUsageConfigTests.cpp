#include <winsock2.h>
#include "CodexUsageConfig.h"
#include "CodexUsageFetcher.h"
#include <Windows.h>
#include <cstdlib>
#include <iostream>
#include <thread>

namespace {
void Check(bool ok, const char* message) {
    if (!ok) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

void CheckManualProxy(const std::wstring& authPath) {
    WSADATA data{};
    Check(WSAStartup(MAKEWORD(2, 2), &data) == 0, "start local proxy test");
    SOCKET listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    Check(listener != INVALID_SOCKET, "create local proxy socket");
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    Check(bind(listener, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0 && listen(listener, 1) == 0,
          "listen on loopback proxy");
    int length = sizeof(address);
    Check(getsockname(listener, reinterpret_cast<sockaddr*>(&address), &length) == 0, "get local proxy port");
    std::string request;
    std::thread proxy([&]() {
        fd_set sockets;
        FD_ZERO(&sockets);
        FD_SET(listener, &sockets);
        timeval timeout{5, 0};
        if (select(0, &sockets, nullptr, nullptr, &timeout) > 0) {
            SOCKET client = accept(listener, nullptr, nullptr);
            if (client != INVALID_SOCKET) {
                DWORD receiveTimeout = 5000;
                setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&receiveTimeout), sizeof(receiveTimeout));
                char buffer[4096]{};
                const int size = recv(client, buffer, sizeof(buffer), 0);
                if (size > 0) request.assign(buffer, size);
                const std::string reply = "HTTP/1.1 502 Bad Gateway\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
                send(client, reply.data(), static_cast<int>(reply.size()), 0);
                closesocket(client);
            }
        }
    });
    const std::string auth = R"({"tokens":{"access_token":"synthetic-test-token"}})";
    HANDLE file = CreateFileW(authPath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    Check(file != INVALID_HANDLE_VALUE, "create synthetic auth file");
    DWORD written = 0;
    Check(WriteFile(file, auth.data(), static_cast<DWORD>(auth.size()), &written, nullptr) && written == auth.size(), "write synthetic token");
    CloseHandle(file);
    CodexUsageConfig config;
    config.authPath = authPath;
    config.proxyMode = ProxyMode::Manual;
    config.proxyServer = L"127.0.0.1:" + std::to_wstring(ntohs(address.sin_port));
    const auto snapshot = CodexUsageFetcher{}.Fetch(config);
    proxy.join();
    closesocket(listener);
    WSACleanup();
    DeleteFileW(authPath.c_str());
    Check(!snapshot.success, "proxy error is surfaced");
    Check(request.find("CONNECT chatgpt.com:443 ") != std::string::npos, "usage fetch reaches configured proxy using HTTPS CONNECT");
    Check(request.find("synthetic-test-token") == std::string::npos, "token is not sent in plaintext to proxy");
}
}

void RunCodexConfigTests() {
    wchar_t temp[MAX_PATH]{};
    wchar_t file[MAX_PATH]{};
    Check(GetTempPathW(MAX_PATH, temp) != 0 && GetTempFileNameW(temp, L"cux", 0, file), "temporary config path");
    const std::wstring path = file;
    DeleteFileW(file);
    auto config = LoadCodexConfig(path);
    Check(config.authPath.empty() && config.proxyMode == ProxyMode::Direct, "missing config defaults to no proxy");
    Check(CodexUsageConfig{}.proxyMode == ProxyMode::Direct, "new configuration defaults to no proxy");
    Check(WritePrivateProfileStringW(L"CodexUsage", L"auth_path", L"custom-auth.json", path.c_str()), "create config without proxy mode");
    Check(LoadCodexConfig(path).proxyMode == ProxyMode::Direct, "missing proxy mode defaults to no proxy");
    Check(WritePrivateProfileStringW(L"CodexUsage", L"proxy_mode", L"unknown", path.c_str()), "write unknown proxy mode");
    Check(LoadCodexConfig(path).proxyMode == ProxyMode::Direct, "unknown proxy mode defaults to no proxy");
    DeleteFileW(file);
    std::wstring error;
    config.authPath = L" C:\\用户目录\\自定义 auth.json ";
    config.proxyMode = ProxyMode::Manual;
    config.proxyServer = L" http://127.0.0.1:7890/ ";
    Check(SaveCodexConfig(config, &error, path), "save Unicode configuration");
    config = LoadCodexConfig(path);
    Check(config.authPath == L"C:\\用户目录\\自定义 auth.json", "Unicode path roundtrip");
    Check(config.proxyMode == ProxyMode::Manual && config.proxyServer == L"127.0.0.1:7890", "manual proxy roundtrip");
    Check(ResolveCodexAuthPath(config) == config.authPath, "custom auth path overrides defaults");
    for (const auto value : {L"", L"socks5://127.0.0.1:1080", L"https://localhost:443", L"http://user:pass@host:80",
                             L"localhost:0", L"localhost:99999", L"http://host:80/path", L"http://host:80?query", L"a;b:80"}) {
        auto invalid = config;
        invalid.proxyServer = value;
        Check(!SaveCodexConfig(invalid, &error, path) && !error.empty(), "reject invalid manual proxy");
        Check(LoadCodexConfig(path).proxyServer == config.proxyServer, "invalid configuration preserves saved values");
    }
    config.proxyServer = L"[::1]:7890";
    Check(NormalizeCodexConfig(&config, &error) && config.proxyServer == L"[::1]:7890", "IPv6 proxy");
    for (auto mode : {ProxyMode::System, ProxyMode::Direct}) {
        config.proxyMode = mode;
        config.proxyServer.clear();
        Check(SaveCodexConfig(config, &error, path) && LoadCodexConfig(path).proxyMode == mode, "system/direct proxy roundtrip");
    }
    config.authPath = L"bad\r\nproxy_mode=manual";
    Check(!SaveCodexConfig(config, &error, path), "reject INI line injection");
    config.authPath.clear();
    Check(!SaveCodexConfig(config, &error, path + L"\\missing.ini"), "save failure is reported");

    // Save and restore process environment without reading any real auth file.
    const DWORD size = GetEnvironmentVariableW(L"CODEX_HOME", nullptr, 0);
    std::wstring previous(size, L'\0');
    if (size) GetEnvironmentVariableW(L"CODEX_HOME", previous.data(), size);
    SetEnvironmentVariableW(L"CODEX_HOME", L"C:\\test-codex-home");
    Check(ResolveCodexAuthPath(config) == L"C:\\test-codex-home\\auth.json", "blank path uses CODEX_HOME");
    SetEnvironmentVariableW(L"CODEX_HOME", nullptr);
    wchar_t profile[32768]{};
    GetEnvironmentVariableW(L"USERPROFILE", profile, 32768);
    Check(ResolveCodexAuthPath(config) == (profile[0] ? std::wstring(profile) + L"\\.codex\\auth.json" : L".codex\\auth.json"),
          "blank path uses user profile fallback");
    SetEnvironmentVariableW(L"CODEX_HOME", size ? previous.c_str() : nullptr);

    config.authPath = path + L".missing-auth.json";
    auto snapshot = CodexUsageFetcher{}.Fetch(config);
    Check(!snapshot.success && snapshot.errorMessage.find(config.authPath) != std::wstring::npos,
          "fetcher reads custom auth path and reports missing file");
    CheckManualProxy(path + L".auth.json");
    DeleteFileW(file);
}
