#pragma once

#include "CodexUsageCore.h"

#include <optional>
#include <string>
#include <vector>

class CodexUsageFetcher {
public:
    UsageSnapshot Fetch() const;

private:
    std::wstring ResolveAuthJsonPath() const;
    std::optional<std::string> ReadAccessToken(std::wstring* errorMessage) const;
    std::optional<std::string> LoadFileUtf8(const std::wstring& path, std::wstring* errorMessage) const;
    std::optional<std::string> HttpGetUsageJson(const std::string& accessToken, std::wstring* errorMessage) const;
};
