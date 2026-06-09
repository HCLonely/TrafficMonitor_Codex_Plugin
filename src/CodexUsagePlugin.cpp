#include "CodexUsageFetcher.h"

#include "PluginInterface.h"

#include <array>
#include <chrono>
#include <mutex>
#include <string>
#include <thread>

namespace {

class CodexUsageItem final : public IPluginItem {
public:
    enum class Window {
        FiveHour,
        Weekly,
    };

    CodexUsageItem(Window window, const wchar_t* name, const wchar_t* id, const wchar_t* label)
        : window_(window), name_(name), id_(id), label_(label) {}

    const wchar_t* GetItemName() const override { return name_; }
    const wchar_t* GetItemId() const override { return id_; }
    const wchar_t* GetItemLableText() const override { return label_; }
    const wchar_t* GetItemValueText() const override { return valueText_.c_str(); }
    const wchar_t* GetItemValueSampleText() const override { return L"100%"; }
    int IsDrawResourceUsageGraph() const override { return 1; }
    float GetResourceUsageGraphValue() const override {
        if (remainingPercent_ < 0) {
            return 0.0f;
        }
        return static_cast<float>(remainingPercent_) / 100.0f;
    }

    void SetSnapshot(const UsageSnapshot& snapshot) {
        if (!snapshot.success) {
            remainingPercent_ = -1;
            valueText_ = L"--";
            return;
        }

        remainingPercent_ = window_ == Window::FiveHour
            ? snapshot.fiveHour.remainingPercent
            : snapshot.weekly.remainingPercent;
        valueText_ = FormatRemainingPercent(remainingPercent_);
    }

private:
    Window window_;
    const wchar_t* name_;
    const wchar_t* id_;
    const wchar_t* label_;
    int remainingPercent_ = -1;
    std::wstring valueText_ = L"--";
};

class CodexUsagePlugin final : public ITMPlugin {
public:
    static CodexUsagePlugin& Instance() {
        static CodexUsagePlugin plugin;
        return plugin;
    }

    IPluginItem* GetItem(int index) override {
        if (index == 0) {
            return &fiveHourItem_;
        }
        if (index == 1) {
            return &weeklyItem_;
        }
        return nullptr;
    }

    void DataRequired() override {
        const auto now = std::chrono::steady_clock::now();
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (fetchInProgress_) {
                return;
            }
            if (lastFetch_.time_since_epoch().count() != 0 && now - lastFetch_ < refreshInterval_) {
                return;
            }
            fetchInProgress_ = true;
            lastFetch_ = now;
        }

        std::thread([this]() {
            UsageSnapshot snapshot = fetcher_.Fetch();
            std::lock_guard<std::mutex> lock(mutex_);
            snapshot_ = std::move(snapshot);
            fiveHourItem_.SetSnapshot(snapshot_);
            weeklyItem_.SetSnapshot(snapshot_);
            tooltip_ = BuildTooltip(snapshot_);
            fetchInProgress_ = false;
        }).detach();
    }

    const wchar_t* GetInfo(PluginInfoIndex index) override {
        switch (index) {
            case TMI_NAME:
                return L"Codex Usage";
            case TMI_DESCRIPTION:
                return L"Shows Codex remaining usage for the 5-hour and weekly windows.";
            case TMI_AUTHOR:
                return L"Codex";
            case TMI_COPYRIGHT:
                return L"MIT";
            case TMI_VERSION:
                return L"0.1.0";
            case TMI_URL:
                return L"https://chatgpt.com";
            default:
                return L"";
        }
    }

    const wchar_t* GetTooltipInfo() override {
        std::lock_guard<std::mutex> lock(mutex_);
        return tooltip_.c_str();
    }

private:
    CodexUsagePlugin() = default;

    std::wstring BuildTooltip(const UsageSnapshot& snapshot) const {
        return BuildUsageTooltip(snapshot);
    }

    CodexUsageFetcher fetcher_;
    CodexUsageItem fiveHourItem_{CodexUsageItem::Window::FiveHour, L"Codex 5h Remaining", L"CodexUsage5h", L"5小时: "};
    CodexUsageItem weeklyItem_{CodexUsageItem::Window::Weekly, L"Codex Weekly Remaining", L"CodexUsageWeek", L"本 周: "};
    mutable std::mutex mutex_;
    UsageSnapshot snapshot_;
    std::wstring tooltip_ = L"Codex Usage: waiting for data";
    bool fetchInProgress_ = false;
    std::chrono::steady_clock::time_point lastFetch_{};
    static constexpr std::chrono::minutes refreshInterval_{1};
};

}  // namespace

extern "C" __declspec(dllexport) ITMPlugin* TMPluginGetInstance() {
    return &CodexUsagePlugin::Instance();
}
