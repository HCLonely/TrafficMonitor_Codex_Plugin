#include "CodexUsageFetcher.h"

#include "JsonLite.h"

#include <Windows.h>
#include <winhttp.h>

#include <fstream>
#include <sstream>

namespace {

std::wstring JoinPath(const std::wstring& base, const std::wstring& child) {
    std::wstring result = base;
    if (!result.empty() && result.back() != L'\\' && result.back() != L'/') {
        result.push_back(L'\\');
    }
    result += child;
    return result;
}

std::optional<std::wstring> ReadEnv(const wchar_t* name) {
    const DWORD size = GetEnvironmentVariableW(name, nullptr, 0);
    if (size == 0) {
        return std::nullopt;
    }

    std::wstring value(size - 1, L'\0');
    GetEnvironmentVariableW(name, value.data(), size);
    return value;
}

std::optional<std::string> HttpGetJson(const std::wstring& userAgent,
                                       const std::wstring& host,
                                       const std::wstring& path,
                                       const std::vector<std::wstring>& headers,
                                       std::wstring* errorMessage) {
    if (errorMessage != nullptr) {
        errorMessage->clear();
    }

    HINTERNET session = WinHttpOpen(userAgent.c_str(), WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (session == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = L"WinHttpOpen failed";
        }
        return std::nullopt;
    }

    std::optional<std::string> responseBody;
    HINTERNET connect = nullptr;
    HINTERNET request = nullptr;

    do {
        connect = WinHttpConnect(session, host.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0);
        if (connect == nullptr) {
            if (errorMessage != nullptr) {
                *errorMessage = L"WinHttpConnect failed";
            }
            break;
        }

        request = WinHttpOpenRequest(connect, L"GET", path.c_str(), nullptr,
            WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
        if (request == nullptr) {
            if (errorMessage != nullptr) {
                *errorMessage = L"WinHttpOpenRequest failed";
            }
            break;
        }

        for (const std::wstring& header : headers) {
            if (!WinHttpAddRequestHeaders(request, header.c_str(), static_cast<DWORD>(-1L), WINHTTP_ADDREQ_FLAG_ADD)) {
                if (errorMessage != nullptr) {
                    *errorMessage = L"WinHttpAddRequestHeaders failed";
                }
                break;
            }
        }
        if (errorMessage != nullptr && !errorMessage->empty()) {
            break;
        }

        constexpr DWORD timeout = 15000;
        WinHttpSetTimeouts(request, timeout, timeout, timeout, timeout);

        if (!WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
            if (errorMessage != nullptr) {
                *errorMessage = L"WinHttpSendRequest failed";
            }
            break;
        }

        if (!WinHttpReceiveResponse(request, nullptr)) {
            if (errorMessage != nullptr) {
                *errorMessage = L"WinHttpReceiveResponse failed";
            }
            break;
        }

        DWORD statusCode = 0;
        DWORD statusCodeSize = sizeof(statusCode);
        WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusCodeSize, WINHTTP_NO_HEADER_INDEX);
        if (statusCode != 200) {
            if (errorMessage != nullptr) {
                *errorMessage = host + path + L" returned HTTP " + std::to_wstring(statusCode);
                if (statusCode == 401) {
                    *errorMessage += L"; auth.json access_token may be expired";
                }
            }
            break;
        }

        std::string body;
        for (;;) {
            DWORD available = 0;
            if (!WinHttpQueryDataAvailable(request, &available)) {
                if (errorMessage != nullptr) {
                    *errorMessage = L"WinHttpQueryDataAvailable failed";
                }
                break;
            }
            if (available == 0) {
                responseBody = std::move(body);
                break;
            }

            std::string chunk(static_cast<size_t>(available), '\0');
            DWORD downloaded = 0;
            if (!WinHttpReadData(request, chunk.data(), available, &downloaded)) {
                if (errorMessage != nullptr) {
                    *errorMessage = L"WinHttpReadData failed";
                }
                break;
            }

            chunk.resize(downloaded);
            body.append(chunk);
        }
    } while (false);

    if (request != nullptr) {
        WinHttpCloseHandle(request);
    }
    if (connect != nullptr) {
        WinHttpCloseHandle(connect);
    }
    WinHttpCloseHandle(session);

    return responseBody;
}

}  // namespace

UsageSnapshot CodexUsageFetcher::Fetch() const {
    UsageSnapshot snapshot;

    std::wstring errorMessage;
    std::optional<std::string> accessToken = ReadAccessToken(&errorMessage);
    if (!accessToken.has_value()) {
        snapshot.errorMessage = errorMessage;
        return snapshot;
    }

    std::optional<std::string> usageJson = HttpGetUsageJson(*accessToken, &errorMessage);
    if (!usageJson.has_value()) {
        snapshot.errorMessage = errorMessage;
        return snapshot;
    }

    snapshot = ParseUsageJson(*usageJson, &errorMessage);
    if (!snapshot.success) {
        snapshot.errorMessage = errorMessage;
    }
    return snapshot;
}

std::wstring CodexUsageFetcher::ResolveAuthJsonPath() const {
    if (auto codexHome = ReadEnv(L"CODEX_HOME"); codexHome.has_value() && !codexHome->empty()) {
        return JoinPath(*codexHome, L"auth.json");
    }

    if (auto userProfile = ReadEnv(L"USERPROFILE"); userProfile.has_value() && !userProfile->empty()) {
        return JoinPath(JoinPath(*userProfile, L".codex"), L"auth.json");
    }

    return L".codex\\auth.json";
}

std::optional<std::string> CodexUsageFetcher::ReadAccessToken(std::wstring* errorMessage) const {
    const std::wstring authPath = ResolveAuthJsonPath();
    std::optional<std::string> jsonText = LoadFileUtf8(authPath, errorMessage);
    if (!jsonText.has_value()) {
        return std::nullopt;
    }

    jsonlite::Parser parser(*jsonText);
    std::optional<jsonlite::Value> root = parser.Parse();
    if (!root.has_value()) {
        if (errorMessage != nullptr) {
            *errorMessage = L"auth.json parse failed: " + Utf8ToWide(parser.Error());
        }
        return std::nullopt;
    }

    const jsonlite::Value* tokens = root->Find("tokens");
    const jsonlite::Value* accessToken = tokens != nullptr ? tokens->Find("access_token") : nullptr;
    auto token = accessToken != nullptr ? accessToken->AsString() : std::nullopt;
    if (!token.has_value() || token->empty()) {
        if (errorMessage != nullptr) {
            *errorMessage = L"auth.json missing tokens.access_token";
        }
        return std::nullopt;
    }

    return std::string(*token);
}

std::optional<std::string> CodexUsageFetcher::LoadFileUtf8(const std::wstring& path, std::wstring* errorMessage) const {
    HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        if (errorMessage != nullptr) {
            *errorMessage = L"cannot open " + path;
        }
        return std::nullopt;
    }

    LARGE_INTEGER size{};
    if (!GetFileSizeEx(file, &size) || size.QuadPart < 0 || size.QuadPart > 16 * 1024 * 1024) {
        CloseHandle(file);
        if (errorMessage != nullptr) {
            *errorMessage = L"cannot read " + path;
        }
        return std::nullopt;
    }

    std::string contents(static_cast<size_t>(size.QuadPart), '\0');
    DWORD read = 0;
    const BOOL ok = contents.empty() || ReadFile(file, contents.data(), static_cast<DWORD>(contents.size()), &read, nullptr);
    CloseHandle(file);
    if (!ok || read != contents.size()) {
        if (errorMessage != nullptr) {
            *errorMessage = L"cannot read " + path;
        }
        return std::nullopt;
    }

    return contents;
}

std::optional<std::string> CodexUsageFetcher::HttpGetUsageJson(const std::string& accessToken, std::wstring* errorMessage) const {
    return HttpGetJson(
        L"TrafficMonitorCodexUsage/0.1",
        L"chatgpt.com",
        L"/backend-api/wham/usage",
        { L"Authorization: Bearer " + Utf8ToWide(accessToken) },
        errorMessage);
}
