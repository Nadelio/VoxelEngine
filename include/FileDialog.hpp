#pragma once
#include <string>

// Opens a file-picker dialog and returns the
// selected file path, or an empty string if the user cancels.
// `filter` is a wildcard pattern such as "*.png" (may be nullptr for all files).
std::string OpenNativeFileDialog(const char* title, const char* filter = nullptr);

// Opens a folder-picker dialog and returns the
// selected folder path, or an empty string if the user cancels.
std::string OpenNativeFolderDialog(const char* title);
