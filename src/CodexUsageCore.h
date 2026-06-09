#pragma once

#include <optional>
#include <string>

struct UsageWindow {
    int usedPercent = 0;
    int remainingPercent = 100;
    int windowSeconds = 0;
    int resetAfterSeconds = 0;
    long long resetAtUnixSeconds = 0;
};

struct UsageSnapshot {
    bool success = false;
    std::wstring email;
    std::wstring planType;
    std::wstring errorMessage;
    UsageWindow fiveHour;
    UsageWindow weekly;
};

std::wstring Utf8ToWide(const std::string& input);
UsageSnapshot ParseUsageJson(const std::string& jsonText, std::wstring* errorMessage);
std::wstring FormatRemainingPercent(int remainingPercent);
std::wstring FormatResetAfter(int seconds);
std::wstring BuildUsageTooltip(const UsageSnapshot& snapshot);
