#include "CodexUsageCore.h"
#include "CodexUsageVersion.h"

#include <cstdlib>
#include <iostream>
#include <string>

namespace {

void AssertTrue(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

void AssertEqual(int actual, int expected, const char* message) {
    if (actual != expected) {
        std::cerr << message << ": expected " << expected << ", got " << actual << '\n';
        std::exit(1);
    }
}

void AssertEqual(const std::wstring& actual, const std::wstring& expected, const char* message) {
    if (actual != expected) {
        std::wcerr << message << L": expected [" << expected << L"], got [" << actual << L"]\n";
        std::exit(1);
    }
}

void AssertNotContains(const std::wstring& actual, wchar_t needle, const char* message) {
    if (actual.find(needle) != std::wstring::npos) {
        std::wcerr << message << L": unexpected character [" << needle << L"] in [" << actual << L"]\n";
        std::exit(1);
    }
}

void ParsesUsageWindows() {
    const std::string json = R"json({
        "email": "user@example.com",
        "plan_type": "pro",
        "rate_limit": {
            "primary_window": {
                "used_percent": 23,
                "limit_window_seconds": 18000,
                "reset_after_seconds": 3600,
                "reset_at": 1780980000
            },
            "secondary_window": {
                "used_percent": 61,
                "limit_window_seconds": 604800,
                "reset_after_seconds": 86400,
                "reset_at": 1781066400
            }
        }
    })json";

    std::wstring error;
    UsageSnapshot snapshot = ParseUsageJson(json, &error);

    AssertTrue(snapshot.success, "snapshot should parse");
    AssertEqual(snapshot.fiveHour.remainingPercent, 77, "five-hour remaining percent");
    AssertEqual(snapshot.weekly.remainingPercent, 39, "weekly remaining percent");
    AssertEqual(snapshot.email, L"user@example.com", "email");
    AssertEqual(snapshot.planType, L"pro", "plan type");
}

void FormatsDisplayText() {
    AssertEqual(FormatRemainingPercent(77), L"77%", "remaining percent text");
    AssertEqual(FormatRemainingPercent(-1), L"--", "negative percent text");
    AssertEqual(FormatRemainingPercent(101), L"--", "out-of-range percent text");
}

void BuildsSingleLineSuccessTooltip() {
    UsageSnapshot snapshot;
    snapshot.success = true;
    snapshot.fiveHour.remainingPercent = 77;
    snapshot.fiveHour.resetAfterSeconds = 3600;
    snapshot.weekly.remainingPercent = 39;
    snapshot.weekly.resetAfterSeconds = 86400;

    const std::wstring tooltip = BuildUsageTooltip(snapshot);

    AssertNotContains(tooltip, L'\n', "success tooltip should stay on one line");
    AssertEqual(
        tooltip,
        L"Codex: 5\u5c0f\u65f6\u5269\u4f59 77%(1h 0m\u540e\u91cd\u7f6e) / \u672c\u5468\u5269\u4f59 39%(1d 0h\u540e\u91cd\u7f6e)",
        "compact success tooltip");
}

void HasGeneratedVersion() {
    AssertTrue(std::wstring(CODEX_USAGE_VERSION_WIDE).size() > 0, "generated version should not be empty");
}

}  // namespace

int main() {
    ParsesUsageWindows();
    FormatsDisplayText();
    BuildsSingleLineSuccessTooltip();
    HasGeneratedVersion();
    return 0;
}
