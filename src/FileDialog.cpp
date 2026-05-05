#include "FileDialog.hpp"

#ifdef _WIN32 // fucking hate how much BS I need for Windows compared to Linux
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shobjidl.h>
#include <string>

namespace {
    static std::string WideToUtf8(LPCWSTR wstr) {
        if (!wstr) return {};
        const int len = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, nullptr, 0, nullptr, nullptr);
        if (len <= 1) return {};
        std::string s(static_cast<std::size_t>(len - 1), '\0');
        WideCharToMultiByte(CP_UTF8, 0, wstr, -1, s.data(), len, nullptr, nullptr);
        return s;
    }

    static std::wstring Utf8ToWide(const char* str) {
        if (!str || !*str) return {};
        const int len = MultiByteToWideChar(CP_UTF8, 0, str, -1, nullptr, 0);
        if (len <= 1) return {};
        std::wstring ws(static_cast<std::size_t>(len - 1), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, str, -1, ws.data(), len);
        return ws;
    }
}

std::string OpenNativeFileDialog(const char* title, const char* filter) {
    std::string result;
    const HRESULT hrCom = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    const bool needUninit = (hrCom == S_OK);

    IFileOpenDialog* pDlg = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL,
                                   IID_IFileOpenDialog, reinterpret_cast<void**>(&pDlg)))) {
        if (title) {
            const std::wstring wt = Utf8ToWide(title);
            pDlg->SetTitle(wt.c_str());
        }
        if (filter) {
            const std::wstring wSpec  = Utf8ToWide(filter);
            const std::wstring wLabel = L"Files (" + wSpec + L")";
            const COMDLG_FILTERSPEC specs[2] = {
                {wLabel.c_str(), wSpec.c_str()},
                {L"All Files",   L"*.*"},
            };
            pDlg->SetFileTypes(2, specs);
        }
        if (SUCCEEDED(pDlg->Show(nullptr))) {
            IShellItem* pItem = nullptr;
            if (SUCCEEDED(pDlg->GetResult(&pItem))) {
                PWSTR pszPath = nullptr;
                if (SUCCEEDED(pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszPath))) {
                    result = WideToUtf8(pszPath);
                    CoTaskMemFree(pszPath);
                }
                pItem->Release();
            }
        }
        pDlg->Release();
    }

    if (needUninit) CoUninitialize();
    return result;
}

std::string OpenNativeFolderDialog(const char* title) {
    std::string result;
    const HRESULT hrCom = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    const bool needUninit = (hrCom == S_OK);

    IFileOpenDialog* pDlg = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL,
                                   IID_IFileOpenDialog, reinterpret_cast<void**>(&pDlg)))) {
        FILEOPENDIALOGOPTIONS opts = 0;
        pDlg->GetOptions(&opts);
        pDlg->SetOptions(opts | FOS_PICKFOLDERS);
        if (title) {
            const std::wstring wt = Utf8ToWide(title);
            pDlg->SetTitle(wt.c_str());
        }
        if (SUCCEEDED(pDlg->Show(nullptr))) {
            IShellItem* pItem = nullptr;
            if (SUCCEEDED(pDlg->GetResult(&pItem))) {
                PWSTR pszPath = nullptr;
                if (SUCCEEDED(pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszPath))) {
                    result = WideToUtf8(pszPath);
                    CoTaskMemFree(pszPath);
                }
                pItem->Release();
            }
        }
        pDlg->Release();
    }

    if (needUninit) CoUninitialize();
    return result;
}

#else // Like look how much LESS code I need for Linux/POSIX

#include <cstdio>

namespace {
    static bool ToolExists(const char* name) {
        std::string cmd = std::string("which ") + name + " >/dev/null 2>&1";
        FILE* const p = popen(cmd.c_str(), "r");
        if (!p) return false;
        return pclose(p) == 0;
    }

    static std::string RunDialog(const std::string& cmd) {
        FILE* const pipe = popen(cmd.c_str(), "r");
        if (!pipe) return {};
        std::string result;
        char buf[4096];
        while (std::fgets(buf, sizeof(buf), pipe)) result += buf;
        if (pclose(pipe) != 0) return {};
        while (!result.empty() && (result.back() == '\n' || result.back() == '\r'))
            result.pop_back();
        return result;
    }

    static std::string ShellQuote(const std::string& s) {
        std::string out = "'";
        for (const char c : s) {
            if (c == '\'') out += "'\\''";
            else           out += c;
        }
        return out + "'";
    }
}

std::string OpenNativeFileDialog(const char* title, const char* filter) {
    const std::string t = title ? title : "Open File";

    if (ToolExists("zenity")) {
        std::string cmd = "zenity --file-selection --title=" + ShellQuote(t);
        if (filter && *filter) {
            cmd += " --file-filter=" + ShellQuote(std::string("Files | ") + filter);
            cmd += " --file-filter=" + ShellQuote("All Files | *");
        }
        cmd += " 2>/dev/null";
        return RunDialog(cmd);
    }

    if (ToolExists("kdialog")) {
        std::string cmd = "kdialog --getopenfilename /";
        if (filter && *filter)
            cmd += " " + ShellQuote(filter);
        cmd += " --title " + ShellQuote(t) + " 2>/dev/null";
        return RunDialog(cmd);
    }

    return {};
}

std::string OpenNativeFolderDialog(const char* title) {
    const std::string t = title ? title : "Open Folder";

    if (ToolExists("zenity")) {
        return RunDialog("zenity --file-selection --directory --title=" +
                         ShellQuote(t) + " 2>/dev/null");
    }

    if (ToolExists("kdialog")) {
        return RunDialog("kdialog --getexistingdirectory / --title " +
                         ShellQuote(t) + " 2>/dev/null");
    }

    return {};
}

#endif
