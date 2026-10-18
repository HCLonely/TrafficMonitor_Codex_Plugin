#include "CodexUsageOptions.h"
#include "resource.h"
#include <commdlg.h>
#include <vector>

extern "C" IMAGE_DOS_HEADER __ImageBase;

namespace {
std::wstring ReadEdit(HWND dialog, int id) {
    HWND edit = GetDlgItem(dialog, id);
    std::vector<wchar_t> text(static_cast<size_t>(GetWindowTextLengthW(edit)) + 1);
    GetWindowTextW(edit, text.data(), static_cast<int>(text.size()));
    return text.data();
}

INT_PTR CALLBACK OptionsProc(HWND dialog, UINT message, WPARAM wparam, LPARAM lparam) {
    auto* config = reinterpret_cast<CodexUsageConfig*>(GetWindowLongPtrW(dialog, DWLP_USER));
    if (message == WM_INITDIALOG) {
        config = reinterpret_cast<CodexUsageConfig*>(lparam);
        SetWindowLongPtrW(dialog, DWLP_USER, lparam);
        SetDlgItemTextW(dialog, IDC_AUTH_PATH, config->authPath.c_str());
        SetDlgItemTextW(dialog, IDC_PROXY_SERVER, config->proxyServer.c_str());
        for (auto label : {L"系统代理（自动读取）", L"直连（不使用代理）", L"手动代理"}) {
            SendDlgItemMessageW(dialog, IDC_PROXY_MODE, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label));
        }
        SendDlgItemMessageW(dialog, IDC_PROXY_MODE, CB_SETCURSEL, static_cast<WPARAM>(config->proxyMode), 0);
        EnableWindow(GetDlgItem(dialog, IDC_PROXY_SERVER), config->proxyMode == ProxyMode::Manual);
        return TRUE;
    }
    if (message == WM_COMMAND) {
        switch (LOWORD(wparam)) {
        case IDC_PROXY_MODE:
            if (HIWORD(wparam) == CBN_SELCHANGE) {
                EnableWindow(GetDlgItem(dialog, IDC_PROXY_SERVER),
                    SendDlgItemMessageW(dialog, IDC_PROXY_MODE, CB_GETCURSEL, 0, 0) == 2);
            }
            return TRUE;
        case IDC_BROWSE_AUTH: {
            std::vector<wchar_t> file(32768);
            OPENFILENAMEW ofn{};
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = dialog;
            ofn.lpstrFilter = L"JSON 文件 (*.json)\0*.json\0所有文件\0*.*\0";
            ofn.lpstrFile = file.data();
            ofn.nMaxFile = static_cast<DWORD>(file.size());
            ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
            if (GetOpenFileNameW(&ofn)) SetDlgItemTextW(dialog, IDC_AUTH_PATH, file.data());
            return TRUE;
        }
        case IDOK: {
            auto next = *config;
            next.authPath = ReadEdit(dialog, IDC_AUTH_PATH);
            next.proxyServer = ReadEdit(dialog, IDC_PROXY_SERVER);
            next.proxyMode = static_cast<ProxyMode>(SendDlgItemMessageW(dialog, IDC_PROXY_MODE, CB_GETCURSEL, 0, 0));
            std::wstring error;
            if (!NormalizeCodexConfig(&next, &error) || !SaveCodexConfig(next, &error)) {
                MessageBoxW(dialog, error.c_str(), L"Codex Usage", MB_OK | MB_ICONERROR);
                return TRUE;
            }
            *config = std::move(next);
            EndDialog(dialog, IDOK);
            return TRUE;
        }
        case IDCANCEL:
            EndDialog(dialog, IDCANCEL);
            return TRUE;
        }
    }
    if (message == WM_CLOSE) {
        EndDialog(dialog, IDCANCEL);
        return TRUE;
    }
    return FALSE;
}
}

bool ShowCodexOptions(HWND parent, CodexUsageConfig* config) {
    const auto result = DialogBoxParamW(reinterpret_cast<HMODULE>(&__ImageBase), MAKEINTRESOURCEW(IDD_OPTIONS),
        parent, OptionsProc, reinterpret_cast<LPARAM>(config));
    if (result == -1) MessageBoxW(parent, L"无法打开插件选项。", L"Codex Usage", MB_OK | MB_ICONERROR);
    return result == IDOK;
}
