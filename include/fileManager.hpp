#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;

class fileManager {
public:
    // Regular files only (kept for backwards compatibility).
    static std::vector<fs::path> getFiles(const fs::path& path, bool recursive);

    // All entries (files *and* directories). Permission-denied directories are
    // skipped instead of aborting the whole scan.
    static std::vector<fs::path> getEntries(const fs::path& path, bool recursive);

    static bool removeFile(const fs::path& path);

    // Remove a file or an entire directory tree. Returns the number of items
    // removed (0 on error / nothing removed).
    static std::uintmax_t removePath(const fs::path& path);

    static bool isDirectory(const fs::path& path);
    static bool exists(const fs::path& path);
};
