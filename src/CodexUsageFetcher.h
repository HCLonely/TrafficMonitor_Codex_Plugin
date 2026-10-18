#pragma once

#include "CodexUsageCore.h"
#include "CodexUsageConfig.h"

#include <optional>
#include <string>
#include <vector>

class CodexUsageFetcher {
public:
    UsageSnapshot Fetch(const CodexUsageConfig& config = {}) const;

private:
    std::optional<std::string> ReadAccessToken(const CodexUsageConfig& config, std::wstring* errorMessage) const;
    std::optional<std::string> LoadFileUtf8(const std::wstring& path, std::wstring* errorMessage) const;
    std::optional<std::string> HttpGetUsageJson(const std::string& accessToken, const CodexUsageConfig& config, std::wstring* errorMessage) const;
};
