#include "FileDialog.hpp"

#include <SDL3/SDL.h>
#include <atomic>
#include <string>

namespace {
    struct DialogResult {
        std::string       path;
        std::atomic<bool> done{false};
    };

    static void SDLCALL OnDialogResult(void* userdata, const char* const* filelist, int /*filter*/) {
        auto* res = static_cast<DialogResult*>(userdata);
        if (filelist && *filelist)
            res->path = *filelist;
        res->done.store(true, std::memory_order_release);
    }

    static std::string ToSdlPattern(const char* filter) {
        if (!filter || !*filter) return "*";
        std::string pat(filter);
        if (pat.size() >= 2 && pat[0] == '*' && pat[1] == '.')
            pat = pat.substr(2);
        return pat.empty() ? "*" : pat;
    }

    static void PumpUntilDone(DialogResult& res) {
        while (!res.done.load(std::memory_order_acquire)) {
            SDL_PumpEvents();
            SDL_Delay(1);
        }
    }
}

std::string OpenNativeFileDialog(const char* title, const char* filter) {
    DialogResult res;
    if (filter) {
        const std::string pat = ToSdlPattern(filter);
        const SDL_DialogFileFilter filters[] = {
            { filter,      pat.c_str() },
            { "All Files", "*"         },
        };
        SDL_ShowOpenFileDialog(OnDialogResult, &res, nullptr, filters, 2, nullptr, false);
    } else {
        SDL_ShowOpenFileDialog(OnDialogResult, &res, nullptr, nullptr, 0, nullptr, false);
    }
    PumpUntilDone(res);
    return res.path;
}

std::string OpenNativeFolderDialog(const char* title) {
    DialogResult res;
    SDL_ShowOpenFolderDialog(OnDialogResult, &res, nullptr, nullptr, false);
    PumpUntilDone(res);
    return res.path;
}
