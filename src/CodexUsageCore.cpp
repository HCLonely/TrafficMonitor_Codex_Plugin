#include "CodexUsageCore.h"

#include "JsonLite.h"

#include <Windows.h>

#include <algorithm>

namespace {

bool ExtractWindow(const jsonlite::Value* windowNode, UsageWindow* output) {
    if (windowNode == nullptr || output == nullptr) {
        return false;
    }

    const jsonlite::Value* usedPercent = windowNode->Find("used_percent");
    const jsonlite::Value* limitWindowSeconds = windowNode->Find("limit_window_seconds");
    const jsonlite::Value* resetAfterSeconds = windowNode->Find("reset_after_seconds");
    const jsonlite::Value* resetAt = windowNode->Find("reset_at");
    if (usedPercent == nullptr || limitWindowSeconds == nullptr || resetAfterSeconds == nullptr || resetAt == nullptr) {
        return false;
    }

    auto used = usedPercent->AsInt();
    auto limit = limitWindowSeconds->AsInt();
    auto resetAfter = resetAfterSeconds->AsInt();
    auto resetAtValue = resetAt->AsNumber();
    if (!used.has_value() || !limit.has_value() || !resetAfter.has_value() || !resetAtValue.has_value()) {
        return false;
    }

    output->usedPercent = std::clamp(*used, 0, 100);
    output->remainingPercent = 100 - output->usedPercent;
    output->windowSeconds = std::max(*limit, 0);
    output->resetAfterSeconds = std::max(*resetAfter, 0);
    output->resetAtUnixSeconds = static_cast<long long>(*resetAtValue);
    return true;
}

}  // namespace

std::wstring Utf8ToWide(const std::string& input) {
    if (input.empty()) {
        return {};
    }

    const int size = MultiByteToWideChar(CP_UTF8, 0, input.data(), static_cast<int>(input.size()), nullptr, 0);
    if (size <= 0) {
        return {};
    }

    std::wstring output(static_cast<size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, input.data(), static_cast<int>(input.size()), output.data(), size);
    return output;
}

UsageSnapshot ParseUsageJson(const std::string& jsonText, std::wstring* errorMessage) {
    if (errorMessage != nullptr) {
        errorMessage->clear();
    }

    UsageSnapshot snapshot;
    jsonlite::Parser parser(jsonText);
    std::optional<jsonlite::Value> root = parser.Parse();
    if (!root.has_value()) {
        if (errorMessage != nullptr) {
            *errorMessage = L"usage JSON parse failed: " + Utf8ToWide(parser.Error());
        }
        return snapshot;
    }

    const jsonlite::Value* email = root->Find("email");
    const jsonlite::Value* planType = root->Find("plan_type");
    const jsonlite::Value* rateLimit = root->Find("rate_limit");
    const jsonlite::Value* primaryWindow = rateLimit != nullptr ? rateLimit->Find("primary_window") : nullptr;
    const jsonlite::Value* secondaryWindow = rateLimit != nullptr ? rateLimit->Find("secondary_window") : nullptr;
    if (!ExtractWindow(primaryWindow, &snapshot.fiveHour) || !ExtractWindow(secondaryWindow, &snapshot.weekly)) {
        if (errorMessage != nullptr) {
            *errorMessage = L"usage payload missing rate_limit windows";
        }
        return snapshot;
    }

    if (auto emailString = email != nullptr ? email->AsString() : std::nullopt; emailString.has_value()) {
        snapshot.email = Utf8ToWide(std::string(*emailString));
    }
    if (auto planTypeString = planType != nullptr ? planType->AsString() : std::nullopt; planTypeString.has_value()) {
        snapshot.planType = Utf8ToWide(std::string(*planTypeString));
    }

    snapshot.success = true;
    return snapshot;
}

std::wstring FormatRemainingPercent(int remainingPercent) {
    if (remainingPercent < 0 || remainingPercent > 100) {
        return L"--";
    }
    return std::to_wstring(remainingPercent) + L"%";
}

std::wstring FormatResetAfter(int seconds) {
    if (seconds <= 0) {
        return L"now";
    }

    const int days = seconds / 86400;
    seconds %= 86400;
    const int hours = seconds / 3600;
    seconds %= 3600;
    const int minutes = seconds / 60;

    if (days > 0) {
        return std::to_wstring(days) + L"d " + std::to_wstring(hours) + L"h";
    }
    if (hours > 0) {
        return std::to_wstring(hours) + L"h " + std::to_wstring(minutes) + L"m";
    }
    return std::to_wstring(minutes) + L"m";
}

std::wstring BuildUsageTooltip(const UsageSnapshot& snapshot) {
    if (!snapshot.success) {
        if (snapshot.errorMessage.empty()) {
            return L"Codex Usage: no data";
        }
        return L"Codex Usage error: " + snapshot.errorMessage;
    }

    std::wstring text = L"Codex: 5小时剩余 " + FormatRemainingPercent(snapshot.fiveHour.remainingPercent);
    text += L"(" + FormatResetAfter(snapshot.fiveHour.resetAfterSeconds) + L"后重置)";
    text += L" / 本周剩余 " + FormatRemainingPercent(snapshot.weekly.remainingPercent);
    text += L"(" + FormatResetAfter(snapshot.weekly.resetAfterSeconds) + L"后重置)";
    return text;
}
